/*
  _________.__    .___      __   .__        __         ___________                                        ____________________ ________  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     \_   _____/______   ____   ____ ________ ____      \_   _____/\_   ___ \\_____  \ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      |    __) \_  __ \_/ __ \_/ __ \\___   // __ \      |    __)  /    \  \/  _(__  < 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <       |     \   |  | \/\  ___/\  ___/ /    /\  ___/      |     \   \     \____/       \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \      \___  /   |__|    \___  >\___  >_____ \\___  >     \___  /    \______  /______  /
        \/         \/    \/     \/       \/     \/          \/                \/     \/      \/    \/          \/            \/       \/ 

 kernel_fc3.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Freeze: example how to implement a Final Cartridge 3(+) compatible cartridge
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

#include "kernel_fc3.h"

// this is an ugly hack: for some weird reasons the FC3 freezer occassionally
// crashes on the first freeze (possibly due to caching behavior). This hack
// launches the machine in freeze mode for a few hundred milliseconds, then
// does a normal reset. To hide this, the freeze menu color is patched to black.
//#define INITAL_LAUNCH_FREEZER_HACK

// we will read this .CRT file 
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:Freezer/fc3.crt";

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_fc3.tga";

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	// FC3 active or disables, #banks (4 for FC3, 16 for FC3+), and currently selected bank
	u32 active, nROMBanks, curBank;

	// counts the #cycles when the C64-reset line is pulled down (to detect a reset), cycles since release and status
	u32 resetCounter, cyclesSinceReset,  
		resetPressed, resetReleased;
	u32 countWrites;

	// total count of C64 cycles (not set to zero upon reset)
	u64 c64CycleCount;

	// pointer to ROM data
	u8 *flash_cacheoptimized;

	// orchestration of freezing
	u32 freezeNMICycles, lastFreezeButton;
	u32 statusGAMEEXROM;

	u32 releaseDMA;

	u32 LONGBOARD;

	u32 hasKernal;
} __attribute__((packed)) FC3STATE;
#pragma pack(pop)

static volatile FC3STATE fc3 AAA;

// ... flash/ROM
extern u8 *flash_cacheoptimized_pool;

//static unsigned char kernalROM[ 8192 ] AAA;
static unsigned char *kernalROM;

static void initFC3()
{
    fc3.active = 1;
	fc3.curBank = 0;

	fc3.resetCounter = fc3.resetPressed = 
	fc3.resetReleased = fc3.cyclesSinceReset = 
	fc3.freezeNMICycles = fc3.lastFreezeButton = 	
	fc3.countWrites = fc3.c64CycleCount = 0;

	fc3.releaseDMA = 0;
	fc3.statusGAMEEXROM = 0;

	write32( ARM_GPIO_GPSET0, bDMA ); 
	write32( ARM_GPIO_GPCLR0, bNMI | bGAME | bEXROM | bCTRL257 ); 
}

__attribute__( ( always_inline ) ) inline void callbackReset()
{
	initFC3();
	latchSetClear( LATCH_LED0, 0 );
}

static u32 LED_CLEAR, LED_ALL_BUT_0;

static void initScreenAndLEDCodes()
{
	#ifndef COMPILE_MENU
	int screenType = 0;
	#endif
	if ( screenType == 0 ) // OLED with SCL and SDA (i.e. 2 Pins) -> 4 LEDs
	{
		LED_ALL_BUT_0	= LATCH_LED1to3;
		LED_CLEAR		= LATCH_LED_ALL;
	} else
	if ( screenType == 1 ) // RGB TFT with SCL, SDA, DC, RES -> 2 LEDs
	{
		LED_ALL_BUT_0	= LATCH_LED1;
		LED_CLEAR		= LATCH_LED0to1;
	}
}


bool loadCustomLogoIfAvailable( char *FILENAME )
{
	char fn[ 1024 ];
	// attention: this assumes that the filename ending is always ".crt"!
	memset( fn, 0, 1024 );
	strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
	strcat( fn, ".tga" );

	logger->Write( "RaspiFlash", LogNotice, "trying to load: '%s'", fn );

	if ( tftLoadBackgroundTGA( (char*)DRIVE, fn ) )
		return true;

	return false;
}


#ifdef COMPILE_MENU
void KernelFC3Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME, const char *FILENAME_KERNAL = NULL )
#else
void CKernelFC3::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();
	initScreenAndLEDCodes();

	latchSetClearImm( 0, LATCH_RESET | LED_CLEAR | LATCH_ENABLE_KERNAL );

	SET_GPIO( bGAME | bEXROM | bNMI | bDMA );
	CLR_GPIO( bCTRL257 ); 

	//
	// load ROM and convert to cache-friendly format
	//
	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	memset( (void*)&fc3, 0, sizeof( FC3STATE ) );
	kernalROM = &flash_cacheoptimized_pool[ 1024 * 1024 - 8192 ];

	// load kernal if any
	u32 kernalSize = 0; // should always be 8192
	if ( FILENAME_KERNAL != NULL )
	{
		fc3.hasKernal = 1;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_KERNAL, flash_cacheoptimized_pool, &kernalSize );
		memcpy( kernalROM, flash_cacheoptimized_pool, 8192 );
	} else
		fc3.hasKernal = 0;

	fc3.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[0] + 128 ) & ~127 );

	CRT_HEADER header;
	u32 ROM_LH;
	u8 bankswitchType;

	u32 nBanks;
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)fc3.flash_cacheoptimized, &bankswitchType, &ROM_LH, &nBanks, false );
	fc3.nROMBanks = nBanks;

	#ifdef COMPILE_MENU
	if ( screenType == 0 )
	{
		splashScreen( raspi_freeze_splash );
	} else
	if ( screenType == 1 )
	{
		if ( !loadCustomLogoIfAvailable( FILENAME ) )
		{
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 );

			int w, h; 
			extern char FILENAME_LOGO_RGBA[128];
			extern unsigned char tempTGA[ 256 * 256 * 4 ];

			if ( tftLoadTGA( DRIVE, FILENAME_LOGO_RGBA, tempTGA, &w, &h, true ) )
			{
				tftBlendRGBA( tempTGA, tftBackground, 0 );
			}
		}

		tftCopyBackground2Framebuffer();
		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}
	#endif

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );
	m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );

	// reset the FC3 and warm caches
	callbackReset();

	SyncDataAndInstructionCache();

	//CACHE_PRELOAD_DATA_CACHE( fc3.flash_cacheoptimized, 8192 * 2 * 4, CACHE_PRELOADL1KEEP )
	//CACHE_PRELOAD_DATA_CACHE( &fc3.flash_cacheoptimized[ 8192 * 2 * 3 ], 8192 * 2, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_DATA_CACHE( &fc3.flash_cacheoptimized[ 0 ], 8192 * 2 * (fc3.nROMBanks), CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32_SKIP( fc3.flash_cacheoptimized, 8192 * 2 * fc3.nROMBanks )

	CACHE_PRELOAD_DATA_CACHE( (u8*)&fc3, ( sizeof( FC3STATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2600 )

	FORCE_READ_LINEAR32a( &fc3, sizeof( FC3STATE ), 65536 );
	FORCE_READ_LINEAR32a( &FIQ_HANDLER, 2600, 65536 );

	FORCE_READ_LINEAR32_SKIP( fc3.flash_cacheoptimized, 8192 * 2 * 4 )

	if ( fc3.hasKernal )
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP );

	// different timing C64-longboards and C128 compared to 469-boards
	fc3.LONGBOARD = 0;
	if ( modeC128 || modeVIC == 0 )
		fc3.LONGBOARD = 1; 

	//if ( C128 using RPi Zero 2W )
	if ( modeC128 )
		fc3.LONGBOARD = 0; 

	//DELAY(1<<24);

	if ( fc3.hasKernal )
		latchSetClear( LATCH_RESET | LATCH_ENABLE_KERNAL, LED_ALL_BUT_0 ); else
		latchSetClear( LATCH_RESET, LED_ALL_BUT_0 | LATCH_ENABLE_KERNAL );

	// main loop
	fc3.c64CycleCount = 0;
	while ( true ) {
		#ifdef COMPILE_MENU
		{
			TEST_FOR_JUMP_TO_MAINMENU2FIQs( (fc3.c64CycleCount*2), (fc3.resetCounter*2) )
		}
		#endif

		if ( fc3.resetCounter > 30 && fc3.resetReleased )
			callbackReset();

		asm volatile ("wfi");

		CACHE_PRELOAD_DATA_CACHE( &fc3.flash_cacheoptimized[ 8192 * 2 * 3 ], 8192 * 2, CACHE_PRELOADL1KEEP )
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt2();
	m_InputPin.DisableInterrupt();
}


#ifdef COMPILE_MENU
void KernelFC3FIQHandler( void *pParam )
#else
void CKernelFC3::FIQHandler (void *pParam)
#endif
{
	register u8 *flashBank = &fc3.flash_cacheoptimized[ fc3.curBank * 8192 * 2 ];
	register u16 D;

	START_AND_READ_ADDR0to7_RW_RESET_CS_NO_MULTIPLEX

	if ( VIC_HALF_CYCLE )
	{
		READ_ADDR0to7_RW_RESET_CS_AND_MULTIPLEX

		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA_VIC2

		D = *(u8*)&flashBank[ GET_ADDRESS_CACHEOPT * 2 + 1 ];

		if ( ROMH_ACCESS )
			WRITE_D0to7_TO_BUS_VIC( D )

		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
		return;
	}  

	SET_GPIO( bCTRL257 );	

	// starting from here we are in the CPU-half cycle
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	// VIC2 read during badline?
	if ( VIC_BADLINE )
	{
		// get ROMH data (not reading anything else here)
		D = *(u8*)&flashBank[ GET_ADDRESS_CACHEOPT * 2 + 1 ];

		// read again (some time passed, had to wait to signals)
		if ( !fc3.LONGBOARD )
		{
			// do not do this: WAIT_UP_TO_CYCLE( WAIT_CYCLE_MULTIPLEXER_VIC2 ); 
			READ_ADDR8to12_ROMLH_IO12_BA 
		}

		if ( ROMH_ACCESS ) 
			WRITE_D0to7_TO_BUS_BADLINE( D )

		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
		return;
	}

	if ( fc3.hasKernal && ROMH_ACCESS && KERNAL_ACCESS && !(fc3.active & 256) )
	{
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );
		FINISH_BUS_HANDLING
		UPDATE_COUNTERS( fc3.c64CycleCount, fc3.resetCounter, fc3.resetPressed, fc3.resetReleased, fc3.cyclesSinceReset )
		return;
	}

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		// get both ROML and ROMH with one read
		D = *(u16*)&flashBank[ GET_ADDRESS_CACHEOPT * 2 ];

		if ( ROMH_ACCESS ) D >>= 8; 

		WRITE_D0to7_TO_BUS ( D )

		FINISH_BUS_HANDLING
		UPDATE_COUNTERS( fc3.c64CycleCount, fc3.resetCounter, fc3.resetPressed, fc3.resetReleased, fc3.cyclesSinceReset )
		return;
	} 

	// read from FC3's ROM visible in IO1/2 
	if ( CPU_READS_FROM_BUS && IO1_OR_IO2_ACCESS )
	{
		// an attempt to make the read more cache-friendly/aligned
		if ( !( g3 & bIO1 ) )
			D = *(u8*)&flashBank[ ( (30) | ( GET_ADDRESS0to7 << 5 ) ) * 2 ]; else
			D = *(u8*)&flashBank[ ( (31) | ( GET_ADDRESS0to7 << 5 ) ) * 2 ]; 
		WRITE_D0to7_TO_BUS( D )

		FINISH_BUS_HANDLING
		UPDATE_COUNTERS( fc3.c64CycleCount, fc3.resetCounter, fc3.resetPressed, fc3.resetReleased, fc3.cyclesSinceReset )
		return;
	}

	UPDATE_COUNTERS( fc3.c64CycleCount, fc3.resetCounter, fc3.resetPressed, fc3.resetReleased, fc3.cyclesSinceReset )

	if ( CPU_WRITES_TO_BUS )
		fc3.countWrites ++; else
		fc3.countWrites = 0;

	// write to FC3's IO2 which contains...
	// ... the FC3 control register at $DFFF
	// Bits
	// 0-1	current bank at $8000 (0 = BASIC/Monitor, 1 = Notebad/Basic Bar, 2 = Desktop, 3 = Freezer)
	// 2-3	unused for FC3, additional banks for FC3+
	// 4-6	control EXROM, GAME and NMI
	// 7	disable FC3 and hide register
	if ( CPU_WRITES_TO_BUS && IO2_ACCESS && fc3.active && GET_IO12_ADDRESS == 0xff )
	{
		READ_D0to7_FROM_BUS( D )

		SET_GPIO( ((D&(1<<4)) ? bEXROM : 0) | ((D&(1<<5)) ? bGAME : 0) | ((D&(1<<6)) ? bNMI : 0) );
		CLR_GPIO( ((D&(1<<4)) ? 0 : bEXROM) | ((D&(1<<5)) ? 0 : bGAME) | ((D&(1<<6)) ? 0 : bNMI) );

		fc3.statusGAMEEXROM = ( D >> 4 ) & 3;

		fc3.curBank = D & ( fc3.nROMBanks - 1 );

		fc3.active = ( (D >> 7) & 1 ) ^ 1;
		if ( !fc3.active )
			latchSetClear( 0, LATCH_LED0 );

		// Ultimax mode (EXROM high and GAME low) -> kernal replacement on/off
		if ( (D&(1<<4)) && !(D&(1<<5)) )
			fc3.active |= 256; else
			fc3.active &= ~256;

		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
		return;
	}

	// freeze
	if ( BUTTON_PRESSED && ( fc3.lastFreezeButton == 0 ) )
	{
		fc3.lastFreezeButton = 500000; // debouncing
		CLR_GPIO( bNMI | bCTRL257 ); 

		fc3.freezeNMICycles  = 10;
		fc3.countWrites      = 0;
	
		RESET_CPU_CYCLE_COUNTER
		return;
	}

	// debouncing
	if ( fc3.lastFreezeButton > 0 )
		fc3.lastFreezeButton --;

	// and go to ultimax mode once the CPU is ready
	if ( fc3.freezeNMICycles && fc3.countWrites == 3 )
	{
		CLR_GPIO( bGAME | bCTRL257 ); 
		fc3.active |= 256;
		fc3.freezeNMICycles = 0;
		latchSetClear( LATCH_LED0, 0 );
		RESET_CPU_CYCLE_COUNTER
		return;
	} 

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}
