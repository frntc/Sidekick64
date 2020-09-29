/*
  _________.__    .___      __   .__        __       ___________.__                .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   _____/|  | _____    _____|  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |    __)  |  | \__  \  /  ___/  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |     \   |  |__/ __ \_\___ \|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \___  /   |____(____  /____  >___|  /
        \/         \/    \/     \/       \/     \/        \/              \/     \/     \/ 

 kernel_ef.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Flash: example how to implement an generic/magicdesk/easyflash-compatible cartridge
 Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

	u32 LONGBOARD;

	//u8 padding[ 384 - 345 ];
} __attribute__((packed)) EFSTATE;

// ... flash
static u8 flash_cacheoptimized_pool[ 1024 * 1024 + 1024 ] AAA;

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
		ef.reg2 = 4 + 2;
	}

	ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];

	setGAMEEXROM();
	SET_GPIO( bDMA | bNMI ); 
}


__attribute__( ( always_inline ) ) inline void prefetchHeuristic()
{
	//prefetchBank( ef.reg0, 1 );
	CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 16384, CACHE_PRELOADL2STRM )
	CACHE_PRELOAD_DATA_CACHE( ef.ram, 256, CACHE_PRELOADL1KEEP )

	// the RPi is not really convinced enough to preload data into caches when calling "prfm PLD*", so we access the data (and a bit more than our current bank)
	FORCE_READ_LINEAR( ef.flashBank, 16384 * 2 )
	FORCE_READ_LINEAR32( ef.ram, 256 )
}


__attribute__( ( always_inline ) ) inline void prefetchComplete()
{
	CACHE_PRELOAD_DATA_CACHE( ef.flash_cacheoptimized, ef.nBanks * ( (ef.bankswitchType == BS_EASYFLASH || ef.bankswitchType == BS_NONE) ? 2 : 1 ) * 8192, CACHE_PRELOADL2KEEP )

	if ( ef.bankswitchType == BS_EASYFLASH )
		CACHE_PRELOAD_DATA_CACHE( ef.ram, 256, CACHE_PRELOADL1KEEP )

	//the RPi is again not convinced enough to preload data into cache, this time we need to simulate random access to the data
	FORCE_READ_LINEARa( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 16384 * 16 )
	FORCE_READ_RANDOM( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 8192 * 2 * ef.nBanks * 16 )
	FORCE_READ_LINEARa( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 16384 * 16 )
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

#ifdef COMPILE_MENU
static void KernelEFFIQHandler_nobank( void *pParam );
static void KernelEFFIQHandler( void *pParam );
void KernelEFRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr )
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

	// read .CRT
	ef.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[ 0 ] + 128 ) & ~127 );
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, &ef.bankswitchType, &ef.ROM_LH, &ef.nBanks );

	ef.eapiCRTModified = 0;

	// EAPI in EF CRT? replace
	if ( ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+0) * 2 + 1 ] == 0x65 &&
		 ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+1) * 2 + 1 ] == 0x61 &&
		 ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+2) * 2 + 1 ] == 0x70 &&
		 ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+3) * 2 + 1 ] == 0x69 )
	{
		//logger->Write( "RaspiFlash", LogNotice, "replacing EAPI" );
		u32 size;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_EAPI, eapiC64Code, &size );

		for ( u32 i = 0; i < 0x300; i++ )
			ef.flash_cacheoptimized[ ADDR_LINEAR2CACHE(EAPI_OFFSET+i) * 2 + 1 ] = eapiC64Code[ i ];
	}

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
			charWidth = 16;
			l = strlen( b1 );
			if ( l * 16 >= 240 )
				charWidth = 13;
			if ( l * 13 >= 240 )
				b1[ 18 ] = 0;
	
			sx = max( 0, ( 240 - charWidth * l ) / 2 - 1 );
			tftPrint( b1, sx, 220, c3, charWidth == 16 ? 0 : -3 );	

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
	#endif
	m_InputPin.ConnectInterrupt( myHandler, FIQ_PARENT );

	// different timing C64-longboards and C128 compared to 469-boards
	ef.LONGBOARD = 0;
	if ( modeC128 || modeVIC == 0 )
		ef.LONGBOARD = 1; 

	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );
	m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );

	// determine how to and preload caches
	if ( ef.bankswitchType == BS_MAGICDESK )
		ef.flashFitsInCache = ( ef.nBanks <= 64 ) ? 1 : 0; else // Magic Desk only uses ROML-banks -> different memory layout
		ef.flashFitsInCache = ( ef.nBanks <= 32 ) ? 1 : 0;

	// TODO
	ef.flashFitsInCache = 0;
	if ( ef.bankswitchType == BS_NONE )
		ef.flashFitsInCache = 1;

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
	latchSetClearImm( LATCH_RESET | LED_INIT2_HIGH, LED_INIT2_LOW );

	ef.c64CycleCount = ef.resetCounter2 = 0;

	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU2FIQs_CB( ef.c64CycleCount, ef.resetCounter2, {if ( ef.eapiCRTModified ) writeChanges2CRTFile( logger, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, false );} );
		#endif

		// if the C64 is turned off and the CRT has been modified => write back to SD
		// I omitted any notification as this is real quick
		if ( ef.mainloopCount++ > 10000 && ef.eapiCRTModified ) 
		{
			writeChanges2CRTFile( logger, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, false );
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

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}


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
			ef.reg0 = (u8)( D & 63 );
			ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];

			// if the EF-ROM does not fit into the RPi's cache: stall the CPU with a DMA and prefetch the data
			if ( !ef.flashFitsInCache )
			{
				WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
				CLR_GPIO( bDMA ); 
				ef.releaseDMA = NUM_DMA_CYCLES;
				prefetchHeuristic();
			}

			if ( !(D & 128) )
				ef.reg2 = 128 + 4 + 2; else
				ef.reg2 = 4 + 0;
					
			setGAMEEXROM();
		}

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

