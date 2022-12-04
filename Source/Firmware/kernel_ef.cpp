/*
  _________.__    .___      __   .__        __       ___________.__                .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   _____/|  | _____    _____|  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |    __)  |  | \__  \  /  ___/  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |     \   |  |__/ __ \_\___ \|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \___  /   |____(____  /____  >___|  /
        \/         \/    \/     \/       \/     \/        \/              \/     \/     \/ 

 kernel_ef.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - example how to implement generic/magicdesk/easyflash-compatible and others cartridges
 Copyright (c) 2019-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

 Logo created with http://patorjk.com/software/taag/
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "kernel_ef.h"

// use this, it you want LEDs to show EF accesses
#define LED

// we will read this .CRT file
static const char DRIVE[] = "SD:";
#ifdef COMPILE_MENU
extern char *FILENAME;
#else
static const char FILENAME[] = "SD:test.crt";
#endif

static const char FILENAME_EAPI[] = "SD:C64/eapi.prg";

static u8 eapiC64Code[ 0x300 + 16384 ];

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_ef3_bg.tga";

// temporary buffers to read cartridge data from SD
static CRT_HEADER header;

//
// easyflash (note: generic and magic desk carts are "mapped" to easyflash carts)
//
#define EASYFLASH_BANKS			64
#define EASYFLASH_BANK_MASK		( EASYFLASH_BANKS - 1 )

#define EAPI_OFFSET 0x1800
#define EAPI_SIZE   0x300

typedef struct
{
	// EF extra RAM
	u8  ram[ 256 ];

	// flash ROM
	u32 nBanks;
	u8	bankswitchType;					// EF, Magic Desk, CBM80, ...
	u32 ROM_LH;							// are we reacting to ROML and/or ROMH?
	u8	*flashBank;						// ptr to currently selected bank
	u8	*flash_cacheoptimized;

	// Easyflash registers and jumper
	u8  reg0, reg0old, reg2;			// $DE00 EasyFlash Bank and $DE02 EasyFlash Control registers
	u32 jumper;							// EasyFlash Jumper
	u8	memconfig[ 16 ];				// see Table 2.5 of EF ProgRef

	// counts the #cycles when the C64-reset line is pulled down (to detect a reset), cycles since release and status
	u32 resetCounter, resetCounter2, 
		cyclesSinceReset,  
		resetPressed, resetReleased, resetEFRAM;

	// total count of C64 cycles (not set to zero upon reset)
	u64 c64CycleCount;

	// this is for orchestration of DMA-enforced stalling of the C64 to warm up caches
	u32 releaseDMA;

	u32 flashFitsInCache;

	// EAPI
	u32 eapiState;

	u8 eapiBufferIn[ 4 ];
	u8 eapiBufferOut[ 4 ];
	u32 eapiBufCountIn;
	u32 eapiBufCountOut;
	u32 eapiBufPosOut;

	u32 mainloopCount;
	u32 eapiCRTModified;

	s32 eeprom_cs, eeprom_data, eeprom_clock;
	u32 eeprom_next_data;
	s32 eeprom_delayed_write;
	u32 triggerDMA, dmaCountWrites;

	u32 LONGBOARD;

	u32 hasKernal;
	//u8 padding[ 384 - 345 ];
} __attribute__((packed)) EFSTATE;

static unsigned char kernalROM[ 8192 ] AAA;

// ... flash
u8 *flash_cacheoptimized_pool;//[ 1024 * 1024 + 8 * 1024 ] AAA;

static volatile EFSTATE ef AAA;

// table with EF memory configurations adapted from Vice
#define M_EXROM	2
#define M_GAME	1
static const u8 memconfig_table[] = {
    (M_EXROM|M_GAME), (M_EXROM|M_GAME), (M_GAME), (M_GAME),
    (M_EXROM),		  (M_EXROM|M_GAME), 0,		  (M_GAME),
    (M_EXROM),		  (M_EXROM|M_GAME), 0,		  (M_GAME), 
    (M_EXROM),		  (M_EXROM|M_GAME), 0,		  (M_GAME),
};

__attribute__( ( always_inline ) ) inline void setGAMEEXROM()
{
	register u32 mode = ef.memconfig[ ( ef.jumper << 3 ) | ( ef.reg2 & 7 ) ];

	register u32 set = bNMI, clr = 0;

	if ( mode & 2 )
		set |= bEXROM; else
		clr |= bEXROM;

	if ( mode & 1 )
		clr |= bGAME; else
		set |= bGAME;

	SETCLR_GPIO( set, clr );
}

//
// very simple/simplified EAPI implementation
//
#define EAPI_STATE_MAIN			0
#define EAPI_STATE_WRITE_FLASH	1
#define EAPI_STATE_ERASE_SECTOR	2

#define CMD_EAPI_INIT			0xf0
#define CMD_WRITE_FLASH			0xf1
#define CMD_ERASE_SECTOR		0xf2

#define EAPI_REPLY_OK			0x00
#define EAPI_REPLY_WRITE_ERROR	0xf1

__attribute__( ( always_inline ) ) inline void eapiSendReply( u8 d )
{
	ef.eapiBufferOut[ 0 ] = d;
	ef.eapiBufPosOut      = 0;
	ef.eapiBufCountOut    = 1;
}

__attribute__( ( always_inline ) ) inline void eapiEraseSector( u8 bank, u32 addr )
{
	if ( bank >= 64 || ( bank % 8 ) )
    {
        // invalid sector to erase
		eapiSendReply( EAPI_REPLY_WRITE_ERROR );
		return;
    }

	u8 *p = &ef.flash_cacheoptimized[ bank * 8192 * 2 ];
	if ( ( addr & 0xff00 ) != 0x8000 )
		p++;

	for ( u32 i = 0; i < 8192 * 8; i++, p += 2 )
		*p = 0xff;

	eapiSendReply( EAPI_REPLY_OK );
}

__attribute__( ( always_inline ) ) inline void eapiWriteFlash( u32 addr, u8 value )
{
	u32 ofs = ( ADDR_LINEAR2CACHE( addr & 0x3fff ) ) * 2 + ( addr < 0xe000 ? 0 : 1 );

	ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 + ofs ] &= value;

	eapiSendReply( EAPI_REPLY_OK );
}


__attribute__( ( always_inline ) ) inline void eapiReceiveByte( u8 d )
{
	switch ( ef.eapiState )
	{
	case EAPI_STATE_MAIN:
		if ( d == CMD_EAPI_INIT )
		{
			ef.eapiBufferOut[ 0 ] = EAPI_REPLY_OK;
			ef.eapiBufPosOut      = 0;
			ef.eapiBufCountOut    = 1;
		} else
		if ( d == CMD_WRITE_FLASH )
		{
			ef.eapiState      = EAPI_STATE_WRITE_FLASH;
			ef.eapiBufCountIn = 0;
		} else
		if ( d == CMD_ERASE_SECTOR )
		{
			ef.eapiState      = EAPI_STATE_ERASE_SECTOR;
			ef.eapiBufCountIn = 0;
		}
		break;

	case EAPI_STATE_WRITE_FLASH:
		ef.eapiBufferIn[ ef.eapiBufCountIn++ ] = d;
		if ( ef.eapiBufCountIn == 3 )
		{
			u32 addr = 0;
			((u8*)&addr)[0] = ef.eapiBufferIn[ 0 ];
			((u8*)&addr)[1] = ef.eapiBufferIn[ 1 ];
			u8 value = ef.eapiBufferIn[ 2 ];
			eapiWriteFlash( addr, value );
			ef.eapiCRTModified = 1;
			ef.eapiState = EAPI_STATE_MAIN;
		}
		break;
	case EAPI_STATE_ERASE_SECTOR:
		ef.eapiBufferIn[ ef.eapiBufCountIn++ ] = d;
		if ( ef.eapiBufCountIn == 3 )
		{
			u32 addr = 0;
			((u8*)&addr)[0] = ef.eapiBufferIn[ 0 ];
			((u8*)&addr)[1] = ef.eapiBufferIn[ 1 ];
			u8 value = ef.eapiBufferIn[ 2 ];
			eapiEraseSector( value, addr );
			ef.eapiCRTModified = 1;
			ef.eapiState = EAPI_STATE_MAIN;
		}
		break;

	default:
		ef.eapiState = EAPI_STATE_MAIN;
		break;
	}
}

__attribute__( ( always_inline ) ) inline u8 easyflash_IO1_Read( u32 addr )
{
	if ( (addr & 0xff) == 0x09 ) // EF3 USB control 
	{
		return 0x40 + 0x80; // always ready
	} 
	if ( (addr & 0xff) == 0x0a ) // EF3 USB data 
	{
		u8 d = 0;
		if ( ef.eapiBufPosOut < ef.eapiBufCountOut )
			d = ef.eapiBufferOut[ ef.eapiBufPosOut ++ ];
		return d;
	} 
	return ( addr & 2 ) ? ef.reg2 : ef.reg0;
}

__attribute__( ( always_inline ) ) inline void easyflash_IO1_Write( u32 addr, u8 value )
{
	if ( (addr & 0xff) == 0x0a ) // EF3 USB data
	{
		eapiReceiveByte( value );
		return;
	}

	if ( ( addr & 2 ) == 0 )
	{
		ef.reg0old = ef.reg0;
		ef.reg0 = (u8)( value & EASYFLASH_BANK_MASK );
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];
	} else
	{
		ef.reg2 = value & 0x87;
	}
}

static u32 epyxDisable = 0;

void initEF()
{
	ef.jumper  = 
	ef.reg0    = 
	ef.reg0old = 
	ef.reg2    = 0;

	ef.resetCounter     = 
	ef.resetPressed     = 
	ef.resetReleased    = 
	ef.cyclesSinceReset = 0;

	ef.releaseDMA = 0;

	ef.eapiState = EAPI_STATE_MAIN;
	ef.eapiBufCountIn = 0;
	ef.eapiBufCountOut = 0;
	ef.eapiBufPosOut = 0;

	if ( ef.bankswitchType == BS_NONE )
	{
		ef.reg2 = 4;
		if ( !header.exrom ) ef.reg2 |= 2;
		if ( !header.game )  ef.reg2 |= 1;
	}
	if ( ef.bankswitchType == BS_MAGICDESK )
	{
		ef.reg2 = 128 + 4 + 2;
	}

	ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];

	setGAMEEXROM();
	SET_GPIO( bDMA | bNMI ); 

	if ( ef.bankswitchType == BS_OCEAN )
	{
		if ( ef.nBanks > 32 )
			{SETCLR_GPIO( bDMA | bNMI | bGAME, bEXROM );} else
			{SETCLR_GPIO( bDMA | bNMI, bEXROM | bGAME );} 
	}
	if ( ef.bankswitchType == BS_ZAXXON || ef.bankswitchType == BS_COMAL80 || ef.bankswitchType == BS_SIMONSBASIC )
	{
		SETCLR_GPIO( bDMA | bNMI, bEXROM | bGAME );
	} else
	if ( ef.bankswitchType == BS_DINAMIC || ef.bankswitchType == BS_C64GS || ef.bankswitchType == BS_PROPHET || ef.bankswitchType == BS_GMOD2 || ef.bankswitchType == BS_RGCD || ef.bankswitchType == BS_HUCKY )
	{
		SETCLR_GPIO( bDMA | bNMI | bGAME, bEXROM );
	} else
	if ( ef.bankswitchType == BS_EPYXFL )
	{
		epyxDisable = 512 * 2 * 0;
		SETCLR_GPIO( bDMA | bNMI | bGAME, bEXROM );
	}

	if ( ef.bankswitchType == BS_GMOD2 )
	{
		static int ff = 1;
		if ( ff )
		{
			ff = 0;
			ef.eeprom_cs = 0;
			ef.eeprom_data = 0;
			ef.eeprom_clock = 0;
			ef.eeprom_next_data = 0;
			ef.eeprom_delayed_write = -1;
			m93c86_write_select((uint8_t)ef.eeprom_cs);
		}
	}

	ef.triggerDMA = 0;
	ef.dmaCountWrites = 0;
}


__attribute__( ( always_inline ) ) inline void prefetchHeuristic()
{
	//prefetchBank( ef.reg0, 1 );
	CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 16384, CACHE_PRELOADL2STRM )
	CACHE_PRELOAD_DATA_CACHE( ef.ram, 256, CACHE_PRELOADL1KEEP )

	// the RPi is not really convinced enough to preload data into caches when calling "prfm PLD*", so we access the data (and a bit more than our current bank)
	// the following two lines are too slow on a RPi 2 Zero
	//FORCE_READ_LINEAR( ef.flashBank, 16384 * 2 )
	//FORCE_READ_LINEAR32( ef.ram, 256 )

	// RPi 2 Zero
	//FORCE_READ_LINEAR32_SKIP( ef.flashBank, 16384*4 )
	FORCE_READ_LINEAR64( ef.flashBank, 16384*2 )
	FORCE_READ_LINEAR32( ef.ram, 256 )

	if ( ef.hasKernal )
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP );
}

__attribute__( ( always_inline ) ) inline void prefetchHeuristic8()
{
	CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
	FORCE_READ_LINEAR( ef.flashBank, 8192 )
}


__attribute__( ( always_inline ) ) inline void prefetchComplete()
{
	// todo: check if all carts covered correctly
	CACHE_PRELOAD_DATA_CACHE( ef.flash_cacheoptimized, ef.nBanks * ( (ef.bankswitchType == BS_EASYFLASH || ef.bankswitchType == BS_NONE) ? 2 : 1 ) * 8192, CACHE_PRELOADL2KEEP )

	if ( ef.bankswitchType == BS_EASYFLASH )
		CACHE_PRELOAD_DATA_CACHE( ef.ram, 256, CACHE_PRELOADL1KEEP )

	//the RPi is again not convinced enough to preload data into cache, this time we need to simulate random access to the data
	FORCE_READ_LINEARa( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 16384 * 16 )
	FORCE_READ_RANDOM( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 8192 * 2 * ef.nBanks * 16 )
	FORCE_READ_LINEARa( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 16384 * 16 )

	if ( ef.hasKernal )
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP );
}

static u32 LED_INIT1_HIGH;	
static u32 LED_INIT1_LOW;	
static u32 LED_INIT2_HIGH;	
static u32 LED_INIT2_LOW;	
static u32 LED_ROM_ACCESS, LED_RAM_ACCESS, LED_IO1, LED_IO2, LED_CLEAR;

static void initScreenAndLEDCodes()
{
	#ifndef COMPILE_MENU
	int screenType = 0;
	#endif
	if ( screenType == 0 ) // OLED with SCL and SDA (i.e. 2 Pins) -> 4 LEDs
	{
		LED_INIT1_HIGH				= LATCH_LED_ALL;
		LED_INIT1_LOW				= 0;
		LED_INIT2_HIGH				= 0;
		LED_INIT2_LOW				= LATCH_LED_ALL;
		LED_ROM_ACCESS				= LATCH_LED0;
		LED_RAM_ACCESS				= LATCH_LED1;
		LED_IO1						= LATCH_LED3;
		LED_IO2						= LATCH_LED2;
		LED_CLEAR					= LATCH_LED_ALL;
	} else
	if ( screenType == 1 ) // RGB TFT with SCL, SDA, DC, RES -> 2 LEDs
	{
		LED_INIT1_HIGH				= LATCH_LED0to1;
		LED_INIT1_LOW				= 0;
		LED_INIT2_HIGH				= 0;
		LED_INIT2_LOW				= LATCH_LED0to1;
		LED_ROM_ACCESS				= LATCH_LED0;
		LED_RAM_ACCESS				= LATCH_LED1;
		LED_IO1						= LATCH_LED0to1;
		LED_IO2						= LATCH_LED1;
		LED_CLEAR					= LATCH_LED0to1;
	}
}

static bool irqFallingEdge = true;

#ifdef COMPILE_MENU

static void KernelEFFIQHandler_nobank( void *pParam );
static void KernelEFFIQHandler_Zaxxon( void *pParam );
static void KernelEFFIQHandler_Prophet( void *pParam );
static void KernelEFFIQHandler_Ocean( void *pParam );
static void KernelEFFIQHandler_RGCD( void *pParam );
static void KernelEFFIQHandler_GMOD2( void *pParam );
static void KernelEFFIQHandler_C64GS( void *pParam );
static void KernelEFFIQHandler_Dinamic( void *pParam );
static void KernelEFFIQHandler_SimonsBasic( void *pParam );
static void KernelEFFIQHandler_Comal80( void *pParam );
static void KernelEFFIQHandler_EpyxFL( void *pParam );

#define LRU_CACHE_ENTRIES	16

u8 lruCache[ LRU_CACHE_ENTRIES ];
u8 lruCurPreloadSlot = 0;
u32 lruPreloadAddr = 0;

static u8 isEyeOfTheBeholder = 0;


// looks for parts of "The dwarf gasps out" and "Prince Keirgar"
const u32 eobString1[] = { 0x64206568, 0x66726177, 0x73616720 };
const u32 eobString2[] = { 0x65636e69, 0x69654b20, 0x72616772 };

static void checkForEyeOfTheBeholder()
{
	extern u8 tempRAWCRTBuffer[ 1032 * 1024 ];

	isEyeOfTheBeholder = 0;

	u32 *r = (u32*)&tempRAWCRTBuffer[ 0 ];

	u8 s1 = 0, s2 = 0;

	for ( int i = 0; i < 512 * 1024 / 4; i ++ )
	{
		if ( r[ i   ] == eobString1[ 0 ] &&
			 r[ i+1 ] == eobString1[ 1 ] &&
			 r[ i+2 ] == eobString1[ 2 ] )
			s1 = 1;
		if ( r[ i   ] == eobString2[ 0 ] &&
			 r[ i+1 ] == eobString2[ 1 ] &&
			 r[ i+2 ] == eobString2[ 2 ] )
			s2 = 1;

		if ( s1 && s2 )
		{
			isEyeOfTheBeholder = 1;
			return;
		}
	}
}

static void KernelEFFIQHandler( void *pParam );
void KernelEFRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr, const char *FILENAME_KERNAL = NULL )
#else
void CKernelEF::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	initScreenAndLEDCodes();

	latchSetClearImm( LED_INIT1_HIGH, LATCH_RESET | LED_INIT1_LOW | LATCH_ENABLE_KERNAL );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	// load kernal is any
	u32 kernalSize = 0; // should always be 8192
	if ( FILENAME_KERNAL != NULL )
	{
		ef.hasKernal = 1;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_KERNAL, flash_cacheoptimized_pool, &kernalSize );
		memcpy( kernalROM, flash_cacheoptimized_pool, 8192 );
	} else
		ef.hasKernal = 0;

	bool getRAW = false;

	// read .CRT
	ef.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[ 0 ] + 128 ) & ~127 );
	u32 nBanks, romLH;
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, &ef.bankswitchType, &romLH, &nBanks, getRAW );
	ef.ROM_LH = romLH; 	ef.nBanks = nBanks;

	ef.eapiCRTModified = 0;

	// EAPI in EF CRT? replace
	if ( ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+0) * 2 + 1 ] == 0x65 &&
		 ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+1) * 2 + 1 ] == 0x61 &&
		 ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+2) * 2 + 1 ] == 0x70 &&
		 ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+3) * 2 + 1 ] == 0x69 )
	{
		logger->Write( "RaspiFlash", LogNotice, "replacing EAPI" );
		u32 size;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_EAPI, eapiC64Code, &size );

		for ( u32 i = 0; i < 0x300; i++ )
			ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+i) * 2 + 1 ] = eapiC64Code[ i ];
	}

	checkForEyeOfTheBeholder();

	#ifdef COMPILE_MENU
	if ( screenType == 0 )
	{
		char fn[ 1024 ];
		// attention: this assumes that the filename ending is always ".crt"!
		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, ".logo" );
		if ( !splashScreenFile( (char*)DRIVE, fn ) )
			splashScreen( raspi_flash_splash );
	} else
	if ( screenType == 1 )
	{
		char fn[ 1024 ];
		// attention: this assumes that the filename ending is always ".crt"!
		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, ".tga" );

		logger->Write( "RaspiFlash", LogNotice, "trying to load: '%s'", fn );

		if ( !tftLoadBackgroundTGA( (char*)DRIVE, fn ) )
		{
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 );

			int w, h; 
			extern char FILENAME_LOGO_RGBA[128];
			extern unsigned char tempTGA[ 256 * 256 * 4 ];

			if ( tftLoadTGA( DRIVE, FILENAME_LOGO_RGBA, tempTGA, &w, &h, true ) )
			{
				tftBlendRGBA( tempTGA, tftBackground, 0 );
			}

			tftCopyBackground2Framebuffer();

			//
			u32 c1 = rgb24to16( 166, 250, 128 );
			//u32 c2 = rgb24to16( 98, 216, 204 );
			u32 c3 = rgb24to16( 233, 114, 36 );
			
			char b1[512];
			strcpy( b1, menuItemStr );
			for ( size_t i = 0; i < strlen( b1 ); i++ )
			{
				if ( b1[i] >= 65 && b1[i] <= 90 )
				{
					b1[ i ] += 97-65;
				} else
				if ( b1[i] >= 97 && b1[i] <= 122 )
				{
					b1[ i ] -= 97-65;
				} 
			}
			int charWidth = 16;
			int l = strlen( b1 );
			if ( l * 16 >= 240 )
				charWidth = 13;
			if ( l * 13 >= 240 )
				b1[ 18 ] = 0;
	
			int sx = max( 0, ( 240 - charWidth * l ) / 2 - 1 );
			tftPrint( b1, sx, 220-16, c1, charWidth == 16 ? 0 : -3 );	

			if ( ef.bankswitchType == BS_MAGICDESK )
				sprintf( b1, "md %d BANKS (%d KB)", ef.nBanks, ef.nBanks * 8192 / 1024 );
			if ( ef.bankswitchType == BS_EASYFLASH )
				sprintf( b1, "%d BANKS (%d KB)", ef.nBanks, ef.nBanks * 16384 / 1024 );
			if ( ef.bankswitchType == BS_NONE )
				sprintf( b1, "cbm80 cARTRIDGE" );
			if ( ef.bankswitchType == BS_ZAXXON )
				sprintf( b1, "(Super) Zaxxon" );
			if ( ef.bankswitchType == BS_PROPHET )
				sprintf( b1, "pROPHET64" );
			if ( ef.bankswitchType == BS_FUNPLAY )
				sprintf( b1, "fUNPLAY/pOWERPLAY" );
			if ( ef.bankswitchType == BS_OCEAN )
				sprintf( b1, "oCEAN cARTRIDGE" );
			if ( ef.bankswitchType == BS_RGCD )
				sprintf( b1, "rgcd cARTRIDGE" );
			if ( ef.bankswitchType == BS_HUCKY )
				sprintf( b1, "hUCKY cARTRIDGE" );
			if ( ef.bankswitchType == BS_GMOD2 )
				sprintf( b1, "gmOD2 cARTRIDGE" );
			if ( ef.bankswitchType == BS_C64GS )
				sprintf( b1, "c64 gAME sYSTEM" );
			if ( ef.bankswitchType == BS_DINAMIC )
				sprintf( b1, "dINAMIC cARTRIDGE" );
			if ( ef.bankswitchType == BS_COMAL80 )
				sprintf( b1, "cOMAL80 cARTRIDGE" );
			if ( ef.bankswitchType == BS_EPYXFL )
				sprintf( b1, "ePYX fASTLOADER" );
			if ( ef.bankswitchType == BS_SIMONSBASIC )
				sprintf( b1, "sIMONS bASIC" );
			charWidth = 16;
			l = strlen( b1 );
			if ( l * 16 >= 240 )
				charWidth = 13;
			if ( l * 13 >= 240 )
				b1[ 18 ] = 0;
	
			sx = max( 0, ( 240 - charWidth * l ) / 2 - 1 );
			tftPrint( b1, sx, 220, c3, charWidth == 16 ? 0 : -3 );	

		} else
		{
			tftCopyBackground2Framebuffer();
		}

		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	} 
	#endif


	// first initialization of EF
	for ( u32 i = 0; i < sizeof( memconfig_table ); i++ )
		ef.memconfig[ i ] = memconfig_table[ i ];

	for ( u32 i = 0; i < 256; i++ )
		ef.ram[ i ] = 0;

	initEF();

	DisableIRQs();

	// setup FIQ
	TGPIOInterruptHandler *myHandler = FIQ_HANDLER;
	#ifdef COMPILE_MENU
	if ( ef.bankswitchType == BS_NONE )
		myHandler = KernelEFFIQHandler_nobank;
	if ( ef.bankswitchType == BS_ZAXXON )
		myHandler = KernelEFFIQHandler_Zaxxon;
	if ( ef.bankswitchType == BS_PROPHET )
		myHandler = KernelEFFIQHandler_Prophet;
	if ( ef.bankswitchType == BS_FUNPLAY )
		ef.bankswitchType = BS_MAGICDESK;
	if ( ef.bankswitchType == BS_OCEAN )
		myHandler = KernelEFFIQHandler_Ocean;
	if ( ef.bankswitchType == BS_RGCD || ef.bankswitchType == BS_HUCKY )
		myHandler = KernelEFFIQHandler_RGCD;
	if ( ef.bankswitchType == BS_GMOD2 )
		myHandler = KernelEFFIQHandler_GMOD2;
	if ( ef.bankswitchType == BS_C64GS )
		myHandler = KernelEFFIQHandler_C64GS;
	if ( ef.bankswitchType == BS_DINAMIC )
		myHandler = KernelEFFIQHandler_Dinamic;
	if ( ef.bankswitchType == BS_COMAL80 )
		myHandler = KernelEFFIQHandler_Comal80;
	if ( ef.bankswitchType == BS_EPYXFL )
		myHandler = KernelEFFIQHandler_EpyxFL;
	if ( ef.bankswitchType == BS_SIMONSBASIC )
		myHandler = KernelEFFIQHandler_SimonsBasic;
	#endif

	if ( !isEyeOfTheBeholder )
		m_InputPin.ConnectInterrupt( myHandler, FIQ_PARENT );

	// different timing C64-longboards and C128 compared to 469-boards
	ef.LONGBOARD = 0;
	if ( modeC128 || modeVIC == 0 )
		ef.LONGBOARD = 1; 

	if ( !isEyeOfTheBeholder )
		m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	irqFallingEdge = true;

	if ( ef.bankswitchType == BS_C64GS || ef.bankswitchType == BS_GMOD2 || ef.bankswitchType == BS_OCEAN || ef.bankswitchType == BS_HUCKY || ef.bankswitchType == BS_RGCD || ef.bankswitchType == BS_COMAL80 || ef.bankswitchType == BS_FUNPLAY || ef.bankswitchType == BS_EPYXFL || ef.bankswitchType == BS_SIMONSBASIC || ef.bankswitchType == BS_DINAMIC )
		irqFallingEdge = false;

	if ( !isEyeOfTheBeholder )
	{
		if ( irqFallingEdge )
			m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );
	}

	// determine how to and preload caches
	if ( ef.bankswitchType == BS_MAGICDESK /*|| ef.bankswitchType == BS_GMOD2*/ )
		ef.flashFitsInCache = ( ef.nBanks <= 64 ) ? 1 : 0; else // Magic Desk only uses ROML-banks -> different memory layout
		ef.flashFitsInCache = ( ef.nBanks <= 32 ) ? 1 : 0;

	// TODO
	ef.flashFitsInCache = 0;
	if ( ef.bankswitchType == BS_NONE || ef.bankswitchType == BS_ZAXXON || ef.bankswitchType == BS_FUNPLAY || ef.bankswitchType == BS_COMAL80 || ef.bankswitchType == BS_EPYXFL || ef.bankswitchType == BS_SIMONSBASIC || ef.bankswitchType == BS_DINAMIC )
		ef.flashFitsInCache = 1;

	if ( ef.bankswitchType == BS_HUCKY || ef.bankswitchType == BS_RGCD )
		ef.reg0 = 7;

	if ( ef.bankswitchType == BS_GMOD2 )
	{
		ef.flashFitsInCache = 0;
		extern uint8_t m93c86_data[M93C86_SIZE];
		char fn[ 4096 ];
		sprintf( fn, "%s.eeprom", FILENAME );
		u32 size;
		extern CLogger *logger;
		CACHE_PRELOAD_DATA_CACHE( m93c86_data, 2048, CACHE_PRELOADL1STRMW )
		memset( m93c86_data, 0, 2048 );
		readFile( logger, DRIVE, fn, m93c86_data, &size );
	}

	// wait forever
	ef.mainloopCount = 0;

	CleanDataCache();
	InvalidateDataCache();
	InvalidateInstructionCache();


	if ( ef.flashFitsInCache )
		prefetchComplete(); else
		prefetchHeuristic();

	// additional first-time cache preloading
	CACHE_PRELOAD_DATA_CACHE( (u8*)&ef, ( sizeof( EFSTATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)myHandler, 4096 )

	FORCE_READ_LINEAR32a( &ef, sizeof( EFSTATE ), 65536 );
	FORCE_READ_LINEAR32a( myHandler, 4096, 65536 );

	if ( ef.flashFitsInCache )
		prefetchComplete(); else
		prefetchHeuristic();

	// ready to go...

	ef.c64CycleCount = ef.resetCounter2 = 0;

	if ( isEyeOfTheBeholder )
	{
		void efPollingHandler();
		void initLRUCache();

		initLRUCache();

		// this is actually unnecessary, but anyways...
		#if 1
		// const u8 nLRUInit = 6;
		// const u8 lruInit[nLRUInit] = { 0x01, 0x00, 0x25, 0x12, 0x1f, 0x18 };
		const u8 nLRUInit = 3;
		const u8 lruInit[nLRUInit] = { 0x12, 0x18, 0x00 };

		CACHE_PRELOAD_INSTRUCTION_CACHE( efPollingHandler, 4096 )

		for ( int j = 0; j < nLRUInit; j++ )
		{
			void addLRUCache( u8 k );
			addLRUCache( lruInit[ j ] );
			CACHE_PRELOAD_DATA_CACHE( &ef.flash_cacheoptimized[ lruInit[ j ] * 16384 ], 16384, CACHE_PRELOADL2KEEP )
			FORCE_READ_LINEAR64( &ef.flash_cacheoptimized[ lruInit[ j ] * 16384 ], 16384 )
			CACHE_PRELOAD_DATA_CACHE( &ef.flash_cacheoptimized[ lruInit[ j ] * 16384 ], 16384, CACHE_PRELOADL2KEEP )
			FORCE_READ_LINEAR64( &ef.flash_cacheoptimized[ lruInit[ j ] * 16384 ], 16384 )
		}

		lruCurPreloadSlot = 0;
		lruPreloadAddr = 0;

		//u8 lruCurPreloadBank = lruCache[ lruCurPreloadSlot ];
		//CACHE_PRELOAD_DATA_CACHE( &ef.flash_cacheoptimized[ lruCurPreloadBank * 16384 ], 16384, CACHE_PRELOADL2KEEP )

		#endif

		CACHE_PRELOAD_INSTRUCTION_CACHE( efPollingHandler, 4096 )
		latchSetClear( LED_INIT2_HIGH, LED_INIT2_LOW );
		efPollingHandler();

		if ( ef.eapiCRTModified ) 
		{
			writeChanges2CRTFile( logger, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, false );
		}

		return;
	}

	if ( ef.hasKernal )
		latchSetClear( LATCH_RESET | LED_INIT2_HIGH | LATCH_ENABLE_KERNAL, LED_INIT2_LOW ); else
		latchSetClear( LATCH_RESET | LED_INIT2_HIGH, LED_INIT2_LOW );

	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU2FIQs_CB( ef.c64CycleCount, ef.resetCounter2, 
		{ if ( ef.eapiCRTModified ) {			/*logger->Write( "RaspiFlash", LogNotice, "EF-CRT saved!" );*/
		writeChanges2CRTFile( logger, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, false );}} 
		{ if ( ef.bankswitchType == BS_GMOD2 ) { extern uint8_t m93c86_data[M93C86_SIZE]; char fn[ 4096 ]; sprintf( fn, "%s.eeprom", FILENAME ); writeFile( logger, DRIVE, fn, m93c86_data, 2048 ); } } )
		#endif


	#if 1
		// if the C64 is turned off and the CRT has been modified => write back to SD
		// I omitted any notification as this is real quick

		if ( ef.mainloopCount > 10000 && ef.bankswitchType == BS_GMOD2 ) 
		{
			extern uint8_t m93c86_data[M93C86_SIZE];
			char fn[ 4096 ];
			sprintf( fn, "%s.eeprom", FILENAME );
			writeFile( logger, DRIVE, fn, m93c86_data, 2048 );
		}

		if ( ef.mainloopCount++ > 10000 && ef.eapiCRTModified ) 
		{
			writeChanges2CRTFile( logger, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, false );
			/*logger->Write( "RaspiFlash", LogNotice, "EF-CRT saved, c64 switched off!" );*/
			ef.eapiCRTModified = 0;
			/*{
				u32 c1 = rgb24to16( 166, 250, 128 );
			
				char b1[512];
				sprintf( b1, "CRT saved!" );
				u32 l = strlen( b1 );
				u32 sx = max( 0, ( 240 - 16 * l ) / 2 - 1 );
				tftPrint( b1, sx, 180, c1, 0 );	
				tftInitImm();
				tftSendFramebuffer16BitImm( tftFrameBuffer );
			}*/
		}
	#endif
		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}


#define TRIGGER_CAN_ASSERT_DMA ( ef.dmaCountWrites == 3 ? true : false )
#define TRIGGER_DMA_COUNT_WRITES { 	if ( CPU_WRITES_TO_BUS ) ef.dmaCountWrites ++; else ef.dmaCountWrites = 0; }
#define TRIGGER_DMA( cycles ) { ef.triggerDMA = cycles; }
#define	HANDLE_DMA_TRIGGER_RELEASE \
	if ( ef.triggerDMA ) {							\
		ef.releaseDMA = ef.triggerDMA;				\
		ef.triggerDMA = 0;							\
		WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA );		\
		CLR_GPIO( bDMA );							\
		setLatchFIQ( LATCH_LED0 );					\
	} else											\
	if ( ef.releaseDMA > 0 && --ef.releaseDMA == 0 )\
	{												\
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA );		\
		SET_GPIO( bDMA );							\
		clrLatchFIQ( LATCH_LED0 );					\
	}	


#define HANDLE_KERNAL_IF_REQUIRED \
	if ( ef.hasKernal && ROMH_ACCESS && KERNAL_ACCESS ) {	\
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );		\
		FINISH_BUS_HANDLING									\
		return;												\
	}


//#ifdef COMPILE_MENU
#if 1
static void KernelEFFIQHandler_Prophet( void *pParam )
{
	register u32 D, addr;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	addr = GET_ADDRESS0to7 << 5;
	CACHE_PRELOADL2STRM( &ef.flashBank[ addr ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		D = ef.flashBank[ addr ];
		WRITE_D0to7_TO_BUS( D )
	} else
	if ( CPU_WRITES_TO_BUS && IO2_ACCESS && GET_IO12_ADDRESS == 0 )
	{
		READ_D0to7_FROM_BUS( D )
		setLatchFIQ( LATCH_LED0 );

		if ( ( D >> 5 ) & 1 )
		{ // cartridge off 
			SET_GPIO( bGAME | bEXROM );
		} else
		{ // cartridge on
			SETCLR_GPIO( bGAME, bEXROM );
		}

		ef.reg0 = D & 0x1f;
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
	} 
		
	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		ef.releaseDMA = 0;
		ef.reg0 = 0;
		ef.flashBank = &ef.flash_cacheoptimized[ 0 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
		SETCLR_GPIO( bGAME | bDMA | bNMI, bEXROM );
		FINISH_BUS_HANDLING
		return;
	}

	//CLEAR_LEDS_EVERY_8K_CYCLES
	static u32 cycleCount = 0;
	if ( !((++cycleCount)&8191) )
		clrLatchFIQ( LATCH_LED0 );

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}
#endif


static volatile u32 forceRead; 

static void KernelEFFIQHandler_GMOD2( void *pParam )
{
	register u32 D, addr;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	addr = GET_ADDRESS0to7 << 5;
	CACHE_PRELOADL2STRM( &ef.flashBank[ addr ] );

	extern int m93c86_addr;
	extern uint8_t m93c86_data[M93C86_SIZE];
	CACHE_PRELOADL2STRMW( &m93c86_data[ m93c86_addr * 2 ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	ef.mainloopCount = 0;

	TRIGGER_DMA_COUNT_WRITES

	addr = GET_ADDRESS_CACHEOPT;
	

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		D = ef.flashBank[ addr ];
		WRITE_D0to7_TO_BUS( D )
	} else
	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		if (ef.eeprom_cs) {
			D = m93c86_read_data() << 7;
			WRITE_D0to7_TO_BUS( D )
		} else
			WRITE_D0to7_TO_BUS( 0 )
	} else
	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )

		if ( ( D & 0xc0 ) == 0xc0 ) {
			SETCLR_GPIO( bEXROM, bGAME ); 
		} else if ( ( D & 0x40 ) == 0x00 ) {
			SETCLR_GPIO( bGAME, bEXROM ); 
		} else if ( ( D & 0x40 ) == 0x40 ) {
			SET_GPIO( bGAME | bEXROM ); 
		}
		ef.eeprom_cs = ( D >> 6 ) & 1;
		ef.eeprom_data = ( D >> 4 ) & 1;
		ef.eeprom_clock = ( D >> 5 ) & 1;
		m93c86_write_select( (uint8_t)ef.eeprom_cs );
		if ( ef.eeprom_cs ) {
			m93c86_write_data( (uint8_t)( ef.eeprom_data ) );
			m93c86_write_clock( (uint8_t)( ef.eeprom_clock ) );
		}

		if ( ef.reg0 != (D & 0x3f) )
		{
			ef.reg0 = D & 0x3f;
			ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];
			CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
		}
	} 


	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		ef.releaseDMA = 0;
		ef.reg0 = 0;
		ef.flashBank = &ef.flash_cacheoptimized[ 0 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
		SETCLR_GPIO( bGAME | bDMA | bNMI, bEXROM );
		FINISH_BUS_HANDLING
		return;
	}

	HANDLE_DMA_TRIGGER_RELEASE

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

static void KernelEFFIQHandler_Ocean( void *pParam )
{
	register u32 D, addr;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	addr = GET_ADDRESS0to7 << 5;
	CACHE_PRELOADL2STRM( &ef.flashBank[ addr ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		D = ef.flashBank[ addr ];
		WRITE_D0to7_TO_BUS( D )
	} 
	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )
		setLatchFIQ( LATCH_LED0 );
		ef.reg0 = D & 0x3f;
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
	} 

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		ef.releaseDMA = 0;
		ef.reg0 = 0;
		ef.flashBank = &ef.flash_cacheoptimized[ 0 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
		if ( ef.nBanks > 32 )
			{SETCLR_GPIO( bDMA | bNMI | bGAME, bEXROM );} else
			{SETCLR_GPIO( bDMA | bNMI, bEXROM | bGAME );} 
		FINISH_BUS_HANDLING
		return;
	}

	//CLEAR_LEDS_EVERY_8K_CYCLES
	static u32 cycleCount = 0;
	if ( !((++cycleCount)&8191) )
		clrLatchFIQ( LATCH_LED0 );

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

static void KernelEFFIQHandler_RGCD( void *pParam )
{
	register u32 D, addr;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	addr = GET_ADDRESS0to7 << 5;
	CACHE_PRELOADL2STRM( &ef.flashBank[ addr ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS && !ef.reg2 )
	{
		D = ef.flashBank[ addr ];
		WRITE_D0to7_TO_BUS( D )
	} 

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )
		setLatchFIQ( LATCH_LED0 );
		if ( D & 8 )
		{
			ef.reg2 = 1;
			SET_GPIO( bDMA | bNMI | bGAME | bEXROM );
		}
		D &= 7;
		if ( ef.bankswitchType == BS_HUCKY )
			ef.reg0 = (D ^ 7) & (ef.nBanks - 1); else
			ef.reg0 = D & (ef.nBanks - 1); 
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
	} 

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		ef.releaseDMA = 0;
		ef.reg0 = ef.reg2 = 0;
		if ( ef.bankswitchType == BS_HUCKY )
			ef.reg0 = 7 & (ef.nBanks - 1);
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
		SETCLR_GPIO( bDMA | bNMI | bGAME, bEXROM );
		FINISH_BUS_HANDLING
		return;
	}

	//CLEAR_LEDS_EVERY_8K_CYCLES
	static u32 cycleCount = 0;
	if ( !((++cycleCount)&8191) )
		clrLatchFIQ( LATCH_LED0 );

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}


static void KernelEFFIQHandler_Zaxxon( void *pParam )
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;
	register u8 *bank0 = &ef.flash_cacheoptimized[ 0 ];

	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		D = *(u8*)&bank0[ (addr&0b1111111101111) * 2 + 0 ];
		ef.reg0 = GET_ADDRESS & 0x1000 ? 1 : 0;
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];
		WRITE_D0to7_TO_BUS( D )
	} else
	if ( CPU_READS_FROM_BUS && ROMH_ACCESS )
	{
		D = *(u8*)&flashBankR[ ( addr & 0x3fff ) * 2 + 1 ];
		WRITE_D0to7_TO_BUS( D )
	} 

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		SET_GPIO( bDMA | bNMI ); 
		FINISH_BUS_HANDLING
		return;
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}


static void KernelEFFIQHandler_C64GS( void *pParam )
{
	register u32 D, addr;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	addr = GET_ADDRESS0to7 << 5;
	CACHE_PRELOADL2STRM( &ef.flashBank[ addr ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA


	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		D = ef.flashBank[ addr ];
		WRITE_D0to7_TO_BUS( D )
	} else

	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		setLatchFIQ( LATCH_LED0 | LATCH_LED1 );
		ef.reg0 = 0;
		ef.flashBank = &ef.flash_cacheoptimized[ 0 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
	} else

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )
		setLatchFIQ( LATCH_LED0 | LATCH_LED1 );
		ef.reg0 = GET_IO12_ADDRESS & 0x3f;
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];
		CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 8192, CACHE_PRELOADL2STRM )
	} 

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		ef.releaseDMA = 0;
		ef.reg0 = 0;
		ef.flashBank = &ef.flash_cacheoptimized[ 0 ];
		clrLatchFIQ( LATCH_LED1 );
		SETCLR_GPIO( bGAME | bDMA | bNMI, bEXROM );
		FINISH_BUS_HANDLING
		return;
	}

	if ( ef.releaseDMA > 0 && --ef.releaseDMA == 0 )
	{
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
		clrLatchFIQ( LATCH_LED1 );
	}

	//CLEAR_LEDS_EVERY_8K_CYCLES
	static u32 cycleCount = 0;
	if ( !((++cycleCount)&8191) )
		clrLatchFIQ( LATCH_LED0 );

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}



static void KernelEFFIQHandler_Dinamic( void *pParam )
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		D = *(u32*)&flashBankR[ addr * 2 ];

		WRITE_D0to7_TO_BUS( D )
	} else

	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		addr = GET_IO12_ADDRESS;
		if ( ( addr & 0x0f ) == addr )
		{
			ef.reg0 = addr & 0x0f;
			ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];
		}
	} 

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		SET_GPIO( bDMA | bNMI ); 
		FINISH_BUS_HANDLING
		return;
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}



static void KernelEFFIQHandler_Comal80( void *pParam )
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		D = *(u32*)&flashBankR[ addr * 2 ];
		if ( ROMH_ACCESS )
			D >>= 8; 

		WRITE_D0to7_TO_BUS( D )
	} else

	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		WRITE_D0to7_TO_BUS( ef.reg2 )
	} else

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )

		ef.reg2 = D & 0xc7;
		ef.reg0 = D & 3;
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];

		if ( D & 0x40 )
			{SET_GPIO( bEXROM | bGAME );} else
			{CLR_GPIO( bEXROM | bGAME );}
	}


	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		SET_GPIO( bDMA | bNMI ); 
		FINISH_BUS_HANDLING
		return;
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}


static void KernelEFFIQHandler_SimonsBasic( void *pParam )
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		D = *(u32*)&flashBankR[ addr * 2 ];
		if ( ROMH_ACCESS )
			D >>= 8; 

		WRITE_D0to7_TO_BUS( D )
	} else

	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		SETCLR_GPIO( bGAME, bEXROM );
	} else

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		CLR_GPIO( bGAME | bEXROM );
	}

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		SETCLR_GPIO( bDMA | bNMI, bGAME | bEXROM );
		FINISH_BUS_HANDLING
		return;
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}



static void KernelEFFIQHandler_EpyxFL( void *pParam )
{
	register u32 D, addr;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		D = *(u32*)&ef.flash_cacheoptimized[ addr * 2 ];
		epyxDisable = 512 * 2;
		WRITE_D0to7_TO_BUS( D )
	} else

	if ( CPU_READS_FROM_BUS && IO2_ACCESS )
	{
		addr |= 0b0000000011111; // access 0x1f00 + IO_ADDRESS
		D = *(u32*)&ef.flash_cacheoptimized[ addr * 2 ];
		WRITE_D0to7_TO_BUS( D )
	} else

	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		SETCLR_GPIO( bGAME, bEXROM );
		epyxDisable = 512 * 2;
		WRITE_D0to7_TO_BUS( 0 )
	} 

	if ( CPU_RESET )
	{
		epyxDisable = 512 * 2;
		SETCLR_GPIO( bDMA | bNMI | bGAME, bEXROM );
	} else
	if ( epyxDisable && --epyxDisable == 0 )
    {
		SET_GPIO( bEXROM | bGAME );
    }

	if ( CPU_RESET ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		FINISH_BUS_HANDLING
		return;
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}




#if 0
this is an example of how to slow down the C64 using DMA
static void KernelEFFIQHandler_nobank( void *pParam )
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;
	static int firstByte = 0;
	//
	// starting from here: CPU communication
	//
	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		D = *(u32*)&flashBankR[ addr * 2 ];
		if ( ROMH_ACCESS )
			D >>= 8; 

		WRITE_D0to7_TO_BUS( D )
		firstByte = 1;

		goto cleanup;
	}

	TRIGGER_DMA_COUNT_WRITES

	static int bremse = 0;
	static int triggerDMA = 0;
	if ( firstByte )
	if ( ++bremse > 100000 && TRIGGER_CAN_ASSERT_DMA )
	{
		TRIGGER_DMA( 50000 )
		bremse = 0;
		goto cleanup;
	}

	// reset handling: when button #2 is pressed together with #1 then the EF ram is erased, DMA is released as well
	if ( !( g2 & bRESET ) ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		SET_GPIO( bDMA | bNMI ); 
		FINISH_BUS_HANDLING
		return;
	}

cleanup:
	HANDLE_DMA_TRIGGER_RELEASE

	//CLEAR_LEDS_EVERY_8K_CYCLES
	/*static u32 cycleCount = 0;
	if ( !((++cycleCount)&8191) )
		clrLatchFIQ( LATCH_LED0 );*/

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}
#endif




#ifdef COMPILE_MENU
static void KernelEFFIQHandler_nobank( void *pParam )
#else
void CKernelEF::FIQHandler (void *pParam)
#endif
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS_NO_MULTIPLEX

	addr = GET_ADDRESS0to7 << 5;
	CACHE_PRELOADL2KEEP( &flashBankR[ addr * 2 ] );

	if ( VIC_HALF_CYCLE )
	{
		// FINETUNED = experimental code to explore VIC2-timings
		#ifdef FINETUNED
		WAIT_UP_TO_CYCLE( 100 );
		g2 = read32( ARM_GPIO_GPLEV0 );	
		SET_GPIO( bCTRL257 );	
		register u32 t;
		READ_CYCLE_COUNTER( t );
		WAIT_UP_TO_CYCLE_AFTER( 75+50, t );
		g3 = read32( ARM_GPIO_GPLEV0 );	
		#else
		READ_ADDR0to7_RW_RESET_CS_AND_MULTIPLEX
		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA_VIC2
		#endif

		if ( ROMH_ACCESS ) 
		{
			D = *(u8*)&flashBankR[ GET_ADDRESS_CACHEOPT * 2 + 1 ];
			WRITE_D0to7_TO_BUS_VIC( D )
		}

		FINISH_BUS_HANDLING
		return;
	}  

	SET_GPIO( bCTRL257 );	

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	addr = GET_ADDRESS_CACHEOPT;

	// VIC2 read during badline?
	if ( VIC_BADLINE )
	{
		if ( !ef.LONGBOARD )
			READ_ADDR8to12_ROMLH_IO12_BA

		if ( ROMH_ACCESS ) 
		{
			D = *(u8*)&flashBankR[ addr * 2 + 1 ];
			WRITE_D0to7_TO_BUS_BADLINE( D )
		}
		FINISH_BUS_HANDLING
		return;
	}

	//
	// starting from here: CPU communication
	//
	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		D = *(u32*)&flashBankR[ addr * 2 ];
		if ( ROMH_ACCESS )
			D >>= 8; 

		WRITE_D0to7_TO_BUS( D )
		goto cleanup;
	}

	// reset handling: when button #2 is pressed together with #1 then the EF ram is erased, DMA is released as well
	if ( !( g2 & bRESET ) ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	
	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		SET_GPIO( bDMA | bNMI ); 
		FINISH_BUS_HANDLING
		return;
	}

cleanup:

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}











#ifdef COMPILE_MENU
static void KernelEFFIQHandler( void *pParam )
#else
void CKernelEF::FIQHandler (void *pParam)
#endif
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	//START_AND_READ_ADDR0to7_RW_RESET_CS
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// we got the A0..A7 part of the address which we will access
	addr = GET_ADDRESS0to7 << 5;

	CACHE_PRELOADL2STRM( &flashBankR[ addr * 2 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 0 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 64 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 128 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 192 ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	//
	//
	//
	if ( VIC_HALF_CYCLE )
	{
		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA
//		READ_ADDR0to7_RW_RESET_CS_AND_MULTIPLEX
//		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA_VIC2

		addr |= GET_ADDRESS8to12;

		if ( ROML_OR_ROMH_ACCESS )
		{
			// get both ROML and ROMH with one read
			D = *(u32*)&flashBankR[ addr * 2 ];
			if ( ROMH_ACCESS ) D >>= 8;
			WRITE_D0to7_TO_BUS_VIC( D )
			setLatchFIQ( LED_ROM_ACCESS );
		} else
		if ( IO2_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			WRITE_D0to7_TO_BUS_VIC( ef.ram[ GET_IO12_ADDRESS ] );
			setLatchFIQ( LED_IO2 );
		} else
		if ( IO1_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			WRITE_D0to7_TO_BUS_VIC( easyflash_IO1_Read( GET_IO12_ADDRESS ) );
			setLatchFIQ( LED_IO1 );
		}

		FINISH_BUS_HANDLING
		return;
	}  

//	if ( modeC128 && VIC_HALF_CYCLE )
//		WAIT_CYCLE_MULTIPLEXER += 15;
	
	ef.mainloopCount = 0;

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

//	if ( modeC128 && VIC_HALF_CYCLE )
//		WAIT_CYCLE_MULTIPLEXER -= 15;

	// make our address complete
	addr |= GET_ADDRESS8to12;


	// VIC2 read during badline?
	if ( VIC_BADLINE )
	{
		if ( !ef.LONGBOARD )
			READ_ADDR8to12_ROMLH_IO12_BA

		if ( ROMH_ACCESS ) 
		{
			if ( ef.bankswitchType == BS_MAGICDESK )
				D = *(u32*)&flashBankR[ addr ]; else
				D = *(u8*)&flashBankR[ addr * 2 + 1 ];
			WRITE_D0to7_TO_BUS_BADLINE( D )
		}
		FINISH_BUS_HANDLING
		return;
	}

	//
	// starting from here: CPU communication
	//
	if ( CPU_READS_FROM_BUS )
	{
		if ( ( ~g3 & ef.ROM_LH ) )
		{
			if ( ef.bankswitchType == BS_MAGICDESK )
			{
				D = *(u32*)&flashBankR[ addr ];
			} else
			{
				D = *(u32*)&flashBankR[ addr * 2 ];
				if ( ROMH_ACCESS )
					D >>= 8; 
			}

			WRITE_D0to7_TO_BUS( D )
			setLatchFIQ( LED_ROM_ACCESS );
			goto cleanup;
		}

		if ( IO2_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			WRITE_D0to7_TO_BUS( ef.ram[ GET_IO12_ADDRESS ] )
			setLatchFIQ( LED_IO2 );
			goto cleanup;
		}

		if ( IO1_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			PUT_DATA_ON_BUS_AND_CLEAR257( easyflash_IO1_Read( GET_IO12_ADDRESS ) )
			setLatchFIQ( LED_IO1 );
			goto cleanup;
		}
	}

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )

		if ( ef.bankswitchType == BS_EASYFLASH )
		{
			// easyflash register in IO1
			if ( GET_IO12_ADDRESS == 0x0a )
			{
				WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
				CLR_GPIO( bDMA ); 
				ef.releaseDMA = NUM_DMA_CYCLES;

				easyflash_IO1_Write( GET_IO12_ADDRESS, D );

				prefetchHeuristic();
			} else
			{
				easyflash_IO1_Write( GET_IO12_ADDRESS, D );

				if ( ( GET_IO12_ADDRESS & 2 ) == 0 )
				{
					// if the EF-ROM does not fit into the RPi's cache: stall the CPU with a DMA and prefetch the data
					if ( !ef.flashFitsInCache )
					{
						WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
						CLR_GPIO( bDMA ); 
						ef.releaseDMA = NUM_DMA_CYCLES;
						prefetchHeuristic();
					}
				} else 
					setGAMEEXROM();
			}
		} else
		{
			// Magic Desk
			if( GET_IO12_ADDRESS == 0 )
			{
				if ( !( D & 128 ) )
				{
					if ( ef.nBanks <= 64 )
						ef.reg0 = (u8)( D & 63 ); else
						ef.reg0 = (u8)( D & 127 ); 
					ef.reg2 = 128 + 4 + 2; 
				} else
					ef.reg2 = 4 + 0;
			}		

			ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];

			// if the EF-ROM does not fit into the RPi's cache: stall the CPU with a DMA and prefetch the data
			if ( !ef.flashFitsInCache )
			{
				WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
				CLR_GPIO( bDMA ); 
				ef.releaseDMA = NUM_DMA_CYCLES;
				prefetchHeuristic();
			}

			setGAMEEXROM();
		}

		setLatchFIQ( LED_IO1 );
		goto cleanup;
	}

	if ( CPU_WRITES_TO_BUS && IO2_ACCESS && ef.bankswitchType == BS_EASYFLASH )
	{
		READ_D0to7_FROM_BUS( D )
		ef.ram[ GET_IO12_ADDRESS ] = D;
		setLatchFIQ( LED_IO2 );
		goto cleanup;
	}

	// reset handling: when button #2 is pressed together with #1 then the EF ram is erased, DMA is released as well
	if ( !( g2 & bRESET ) ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	if ( !( g3 & ( 1 << BUTTON ) ) && ef.resetCounter > 3 ) { ef.resetEFRAM = 1; }

	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		initEF();
		if ( ef.resetEFRAM )
			memset( (void*)ef.ram, 0, 256 );
		ef.resetEFRAM = 0;
		SET_GPIO( bDMA ); 
		FINISH_BUS_HANDLING
		return;
	}

cleanup:

	if ( ef.releaseDMA > 0 && --ef.releaseDMA == 0 )
	{
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
	}

	//CLEAR_LEDS_EVERY_8K_CYCLES
	static u32 cycleCount = 0;
	if ( !((++cycleCount)&8191) )
		clrLatchFIQ( LED_CLEAR );

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}


#define PRELOAD_BUCKET_SIZE (128*4)
#define PRELOAD_TOTAL_SIZE 16384

static u32 lastEFAddr = 0;


void initLRUCache()
{
	lruCurPreloadSlot = 0;
	lruPreloadAddr = 0;
	for ( int i = 0; i < LRU_CACHE_ENTRIES; i ++ )
		lruCache[ i ] = 0xff;
}

u8 isInLRUCache( u8 k )
{
	for ( int i = 0; i < LRU_CACHE_ENTRIES; i ++ )
		if ( lruCache[ i ] == k )
		return i + 1;
		
	return 0;
}

void addLRUCache( u8 k )
{
	for ( int i = LRU_CACHE_ENTRIES - 1; i > 0; i -- )
		lruCache[ i ] = lruCache[ i - 1 ];

	lruCache[ 0 ] = k;
}

u8 addOrMoveFrontLRUCache( u8 k )
{
	u8 pos = isInLRUCache( k );
	if ( pos == 0 )
	{
		addLRUCache( k );
		return 1;
	} else
	if ( pos > 1 ) // = in cache, but not at front-most position
	{
		pos -= 1;
		u8 t = lruCache[ 0 ];
		lruCache[ 0 ] = lruCache[ pos ];
		lruCache[ pos ] = t;
	} 
	return 0;
}

void efPollingHandler()
{
	register u32 g2, g3;
	register u32 rs2 = 0;
	
	while ( 1 )
	{
		register u32 D, addr;
		register u8 *flashBankR = ef.flashBank;

		#define WAIT_FOR_CPU_HALFCYCLE {do { g2 = read32( ARM_GPIO_GPLEV0 ); } while ( VIC_HALF_CYCLE );}
		#define WAIT_FOR_VIC_HALFCYCLE {do { g2 = read32( ARM_GPIO_GPLEV0 ); } while ( !VIC_HALF_CYCLE ); }

		u8 lruCurPreloadBank = lruCache[ lruCurPreloadSlot ];

		WAIT_FOR_VIC_HALFCYCLE

		BEGIN_CYCLE_COUNTER
		WAIT_UP_TO_CYCLE( WAIT_FOR_SIGNALS + 40-20 );
		g2 = read32( ARM_GPIO_GPLEV0 );
		write32( ARM_GPIO_GPSET0, bCTRL257 );

		addr = GET_ADDRESS0to7 << 5;

		WAIT_UP_TO_CYCLE( WAIT_CYCLE_MULTIPLEXER + 40-20 );
		g3 = read32( ARM_GPIO_GPLEV0 );					

		addr |= GET_ADDRESS8to12;
		
		if ( !VIC_BADLINE && ( ROML_OR_ROMH_ACCESS || IO1_ACCESS || IO2_ACCESS ) )
		{
			if ( ROML_OR_ROMH_ACCESS )
			{
				// get both ROML and ROMH with one read
				D = *(u16*)&flashBankR[ addr * 2 ];
				if ( ROMH_ACCESS ) D >>= 8;
			} else
			if ( IO2_ACCESS )
			{
				D = ef.ram[ GET_IO12_ADDRESS ];
			} else
			//if ( IO1_ACCESS )
			{
				D = easyflash_IO1_Read( GET_IO12_ADDRESS );
			} 
			WRITE_D0to7_TO_BUS( D );
			FINISH_BUS_HANDLING
		} else
		{
			write32( ARM_GPIO_GPCLR0, bCTRL257 );
			if ( lruCurPreloadBank != 0xff )
			{
				u8 *curPrefetchAddr = &ef.flash_cacheoptimized[ lruCurPreloadBank * 16384 + lruPreloadAddr ];
				FORCE_READ_LINEAR64( curPrefetchAddr, PRELOAD_BUCKET_SIZE )

				u8 nextPreloadSlot = ( lruCurPreloadSlot + 1 ) % LRU_CACHE_ENTRIES;
				u8 lruNextPreloadBank = lruCache[ nextPreloadSlot ];
				CACHE_PRELOAD_DATA_CACHE( &ef.flash_cacheoptimized[ lruNextPreloadBank * 16384 + lruPreloadAddr ], PRELOAD_BUCKET_SIZE, CACHE_PRELOADL2KEEP )

				lruPreloadAddr += PRELOAD_BUCKET_SIZE;
				if ( lruPreloadAddr >= 16384 )
				{
					lruPreloadAddr = 0;
					lruCurPreloadSlot = nextPreloadSlot;
				}
			} else
				lruCurPreloadSlot = ( lruCurPreloadSlot + 1 ) % LRU_CACHE_ENTRIES;
		}

		WAIT_FOR_CPU_HALFCYCLE

		// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
		RESTART_CYCLE_COUNTER
		WAIT_UP_TO_CYCLE( WAIT_FOR_SIGNALS + 40 );
		g2 = read32( ARM_GPIO_GPLEV0 );
		write32( ARM_GPIO_GPSET0, bCTRL257 );

		// we got the A0..A7 part of the address which we will access
		addr = GET_ADDRESS0to7 << 5;

		CACHE_PRELOADL1STRM( &flashBankR[ addr * 2 ] );
		static u8 plRAM = 0;
		CACHE_PRELOADL1KEEP( &ef.ram[ plRAM ] );
		plRAM += 64; plRAM &= 255;

		UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

		if ( ef.c64CycleCount == 100000 || ef.c64CycleCount == 200000 )
			latchSetClearImm( LATCH_RESET | LED_INIT2_HIGH, LED_INIT2_LOW );
		if ( ef.c64CycleCount == 150000 )
			latchSetClearImm( LED_INIT2_HIGH, LATCH_RESET | LED_INIT2_LOW );

	//	if ( modeC128 && VIC_HALF_CYCLE )
	//		WAIT_CYCLE_MULTIPLEXER += 15;
		
		ef.mainloopCount = 0;

		// read the rest of the signals
		//WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA
		// +35, +25 works, +70 not
		WAIT_UP_TO_CYCLE( WAIT_CYCLE_MULTIPLEXER + 40 );
		g3 = read32( ARM_GPIO_GPLEV0 );					

//40 170
//120 235

	//	if ( modeC128 && VIC_HALF_CYCLE )
	//		WAIT_CYCLE_MULTIPLEXER -= 15;

		// make our address complete
		addr |= GET_ADDRESS8to12;

		//
		// starting from here: CPU communication
		//
		if ( CPU_READS_FROM_BUS )
		{
			if ( ( ~g3 & ef.ROM_LH ) )
			{
				D = *(u16*)&flashBankR[ addr * 2 ];
				if ( ROMH_ACCESS )
					D >>= 8; 

				WRITE_D0to7_TO_BUS( D )
				setLatchFIQ( LED_ROM_ACCESS );
				lastEFAddr = addr;

				goto cleanup;
			}

			if ( IO2_ACCESS )
			{
				WRITE_D0to7_TO_BUS( ef.ram[ GET_IO12_ADDRESS ] )
				setLatchFIQ( LED_IO2 );
				goto cleanup;
			}

			if ( IO1_ACCESS )
			{
				WRITE_D0to7_TO_BUS( easyflash_IO1_Read( GET_IO12_ADDRESS ) )
				setLatchFIQ( LED_IO1 );
				goto cleanup;
			}
		}

		if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
		{
			READ_D0to7_FROM_BUS( D )

			// easyflash register in IO1
			u8 *oldBank = ef.flashBank;
			easyflash_IO1_Write( GET_IO12_ADDRESS, D );

			if ( ( GET_IO12_ADDRESS & 2 ) == 0 && oldBank != ef.flashBank )
			{
				u8 newBank = ef.reg0;

				/*u8 allFull = 1;

				for ( int i = 0; i < LRU_CACHE_ENTRIES; i ++ )
					if ( lruCache[ i ] == 0xff )
						allFull = 0;
				if ( allFull )
				{
					writeFile( logger, DRIVE, "firstcacheeob.bin", lruCache, 16 );
					return;
				}*/

				u8 requiresPreload = addOrMoveFrontLRUCache( newBank );

				if ( requiresPreload )
				{
					CLR_GPIO( bDMA ); 
					// (4-1) cycles seem to be enough, 10 would be super-safe
					ef.releaseDMA = 4 + 6;

					if ( ef.c64CycleCount < 500000 )
						ef.releaseDMA += 20;

					for ( int i = 0; i < 32; i++ )
						CACHE_PRELOADL1STRM( &oldBank[ lastEFAddr + ( i << 6 ) ] );

					CACHE_PRELOAD_DATA_CACHE( ef.flashBank, PRELOAD_TOTAL_SIZE, CACHE_PRELOADL2KEEP )
					
					__attribute__((unused)) volatile u64 forceRead;
					for ( int i = 0; i < 32; i++ )
						forceRead = ((u64*)&oldBank[ lastEFAddr + ( i << 6 ) ])[ 0 ];

					FORCE_READ_LINEAR64( &ef.flashBank[ 0 ], 16384 )

					lruCurPreloadSlot = 1;
					lruPreloadAddr = 0;

					u8 lruCurPreloadBank = lruCache[ lruCurPreloadSlot ];
					CACHE_PRELOAD_DATA_CACHE( &ef.flash_cacheoptimized[ lruCurPreloadBank * 16384 ], PRELOAD_TOTAL_SIZE, CACHE_PRELOADL2KEEP )
				}
			} else 
				setGAMEEXROM();

			setLatchFIQ( LED_IO1 );
			goto cleanup;
		}

		if ( CPU_WRITES_TO_BUS && IO2_ACCESS )
		{
			READ_D0to7_FROM_BUS( D )
			ef.ram[ GET_IO12_ADDRESS ] = D;
			setLatchFIQ( LED_IO2 );
			goto cleanup;
		}

		// reset handling: when button #2 is pressed together with #1 then the EF ram is erased, DMA is released as well
		if ( !( g2 & bRESET ) ) 
		{ 
			ef.resetCounter ++; 
			rs2 ++;
		} else 
		{ 
			if ( rs2 > 500000 ) return; 
			rs2 = ef.resetCounter = 0; 
		}
		if ( !( g3 & ( 1 << BUTTON ) ) && ef.resetCounter > 3 ) { ef.resetEFRAM = 1; }

		if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
		{
			ef.resetCounter = 0x8000000;
			initEF();
			if ( ef.resetEFRAM )
				memset( (void*)ef.ram, 0, 256 );
			ef.resetEFRAM = 0;
			SET_GPIO( bDMA ); 
			FINISH_BUS_HANDLING
			continue;
		}

	cleanup:

		if ( ef.releaseDMA > 0 && --ef.releaseDMA == 0 )
		{
			WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
			SET_GPIO( bDMA ); 
		}

		//CLEAR_LEDS_EVERY_8K_CYCLES
		static u32 cycleCount = 0;
		if ( !((++cycleCount)&8191) )
			clrLatchFIQ( LED_CLEAR );

		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
	}
}
