/*
  _________.__    .___      __   .__        __     ___________                                                         .__    .__               
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __ \_   _____/______   ____   ____ ________ ____   _____ _____    ____ |  |__ |__| ____   ____  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /  |    __) \_  __ \_/ __ \_/ __ \\___   // __ \ /     \\__  \ _/ ___\|  |  \|  |/    \_/ __ \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <   |     \   |  | \/\  ___/\  ___/ /    /\  ___/|  Y Y  \/ __ \\  \___|   Y  \  |   |  \  ___/ 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \___  /   |__|    \___  >\___  >_____ \\___  >__|_|  (____  /\___  >___|  /__|___|  /\___  >
        \/         \/    \/     \/       \/     \/      \/                \/     \/      \/    \/      \/     \/     \/     \/        \/     \/ 
		
 kernel_freezemachine.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Freeze: example how to implement a Freeze Frame/Machine compatible cartridge
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

#include "kernel_freezemachine.h"

// we will read this .CRT file 
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:Freezer/fm.crt";

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_fm.tga";

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	u32 active, nROMBanks, curBank;
	u32 isFreezeMachine, a13;

	// counts the #cycles when the C64-reset line is pulled down (to detect a reset), cycles since release and status
	u32 resetCounter, cyclesSinceReset,  
		resetPressed, resetReleased;
	u32 countWrites;

	// total count of C64 cycles (not set to zero upon reset)
	u64 c64CycleCount;

	// pointer to ROM data
	u8 *cartridgeROM;

	// orchestration of freezing
	u32 freezeNMICycles, lastFreezeButton;
	u32 statusGAMEEXROM;

	u32 hasKernal;
} __attribute__((packed)) FMSTATE;
#pragma pack(pop)

static volatile FMSTATE fm AAA;

// ... flash/ROM
extern u8 *flash_cacheoptimized_pool;

//static unsigned char kernalROM[ 8192 ] AAA;
static unsigned char *kernalROM;

__attribute__( ( always_inline ) ) inline void initFM()
{
    fm.active = 1;
	fm.resetCounter = fm.resetPressed = 
	fm.resetReleased = fm.cyclesSinceReset = 
	fm.freezeNMICycles = fm.lastFreezeButton = 	
	fm.countWrites = fm.c64CycleCount = 0;
	fm.a13 = 0;

	fm.statusGAMEEXROM = bGAME;
	SETCLR_GPIO( bNMI | bDMA | bGAME, bEXROM | bCTRL257 ); 
	latchSetClear( LATCH_LED0, 0 );

	if ( fm.isFreezeMachine == BS_FREEZEMACHINE )
	{
		static int toggleBankInit = 1;
		if ( toggleBankInit )
		{
			fm.curBank = 1;
			toggleBankInit = 0;
		}
		fm.curBank = 1 - fm.curBank;
	} else
		fm.curBank = 0;
}

__attribute__( ( always_inline ) ) inline void callbackReset()
{
	initFM();
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

#ifdef COMPILE_MENU
void KernelFMRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME, const char *FILENAME_KERNAL = NULL )
#else
void CKernelFM::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();
	initScreenAndLEDCodes();

	latchSetClearImm( 0, LATCH_RESET | LED_CLEAR | LATCH_ENABLE_KERNAL );

	SETCLR_GPIO( bGAME | bEXROM | bNMI | bDMA, bCTRL257 ); 

	//
	// load ROM and convert to cache-friendly format
	//
	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	memset( (void*)&fm, 0, sizeof( FMSTATE ) );
	kernalROM = &flash_cacheoptimized_pool[ 1024 * 1024 - 8192 ];

	// load kernal if any
	u32 kernalSize = 0; // should always be 8192
	if ( FILENAME_KERNAL != NULL )
	{
		fm.hasKernal = 1;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_KERNAL, flash_cacheoptimized_pool, &kernalSize );
		memcpy( kernalROM, flash_cacheoptimized_pool, 8192 );
	} else
		fm.hasKernal = 0;

	fm.cartridgeROM = (u8 *)( ( (u64)&flash_cacheoptimized_pool[0] + 128 ) & ~127 );

	CRT_HEADER header;
	u32 ROM_LH;
	u8 bankswitchType;

	u32 nBanks;
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)fm.cartridgeROM, &bankswitchType, &ROM_LH, &nBanks, true );
	fm.nROMBanks = nBanks;
	fm.isFreezeMachine = bankswitchType;

	#ifdef COMPILE_MENU
	if ( screenType == 0 )
	{
		splashScreen( raspi_freeze_splash );
	} else
	if ( screenType == 1 )
	{
		extern bool loadCustomLogoIfAvailable( char *FILENAME );

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

	// reset the FC3 and warm caches
	callbackReset();

	SyncDataAndInstructionCache();

	CACHE_PRELOAD_DATA_CACHE( &fm.cartridgeROM[ 0 ], 8192 * 2 * (fm.nROMBanks), CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32_SKIP( fm.cartridgeROM, 8192 * 2 * fm.nROMBanks )

	CACHE_PRELOAD_DATA_CACHE( (u8*)&fm, ( sizeof( FMSTATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2600 )

	FORCE_READ_LINEAR32a( &fm, sizeof( FMSTATE ), 65536 );
	FORCE_READ_LINEAR32a( &FIQ_HANDLER, 2600, 65536 );

	FORCE_READ_LINEAR32_SKIP( fm.cartridgeROM, 8192 * 2 * 4 )

	if ( fm.hasKernal )
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP );

	if ( fm.hasKernal )
		latchSetClear( LATCH_RESET | LATCH_ENABLE_KERNAL, LED_ALL_BUT_0 ); else
		latchSetClear( LATCH_RESET, LED_ALL_BUT_0 | LATCH_ENABLE_KERNAL );

	// main loop
	fm.c64CycleCount = 0;
	while ( true ) {
		#ifdef COMPILE_MENU
		{
			TEST_FOR_JUMP_TO_MAINMENU( (fm.c64CycleCount*2), (fm.resetCounter*2) )
		}
		#endif

		asm volatile ("wfi");

		CACHE_PRELOAD_DATA_CACHE( &fm.cartridgeROM[ 8192 * 2 * 3 ], 8192 * 2, CACHE_PRELOADL1KEEP )
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt2();
	m_InputPin.DisableInterrupt();
}


#ifdef COMPILE_MENU
void KernelFMFIQHandler( void *pParam )
#else
void CKernelFM::FIQHandler (void *pParam)
#endif
{
	register u8 *flashBank = &fm.cartridgeROM[ fm.curBank * 8192 * 2 + fm.a13 * 8192 ];
	register u8 D;

	START_AND_READ_ADDR0to7_RW_RESET_CS
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( fm.hasKernal && ROMH_ACCESS && KERNAL_ACCESS && !(fm.active & 256) )
	{
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );
		goto quitFIQandHandleReset;
	} 

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		D = flashBank[ GET_ADDRESS ];
		WRITE_D0to7_TO_BUS ( D )
		goto quitFIQandHandleReset;
	} 

	if ( CPU_WRITES_TO_BUS )
		fm.countWrites ++; else
		fm.countWrites = 0;

	if ( CPU_READS_FROM_BUS && IO1_ACCESS && fm.active )
	{
		if ( fm.isFreezeMachine == BS_FREEZEFRAME || fm.statusGAMEEXROM == bEXROM )
		{
			SETCLR_GPIO( bNMI | bGAME, bEXROM );
			fm.statusGAMEEXROM = bGAME;
		} else
		{
			SETCLR_GPIO( bNMI, bEXROM | bGAME );
			fm.statusGAMEEXROM = 0;
		}
		goto quitFIQandHandleReset;
	}

	if ( CPU_READS_FROM_BUS && IO2_ACCESS && fm.active )
	{
		SET_GPIO( bGAME | bEXROM );
		fm.statusGAMEEXROM = bGAME | bEXROM;
		if ( fm.isFreezeMachine == BS_FREEZEMACHINE ) fm.a13 = 1;
		fm.active = 0;
		latchSetClear( 0, LATCH_LED0 );
		goto quitFIQandHandleReset;
	}

	// freeze
	if ( BUTTON_PRESSED && ( fm.lastFreezeButton == 0 ) )
	{
		fm.lastFreezeButton = 500000; // debouncing
		CLR_GPIO( bNMI | bCTRL257 ); 

		fm.freezeNMICycles  = 10;
		fm.countWrites      = 0;
	}

	// debouncing
	if ( fm.lastFreezeButton > 0 )
		fm.lastFreezeButton --;

	// and go to ultimax mode once the CPU is ready
	if ( fm.freezeNMICycles && fm.countWrites == 3 )
	{
		SETCLR_GPIO( bEXROM, bGAME | bCTRL257 ); 
		fm.statusGAMEEXROM = bEXROM;
		fm.active |= 256;
		fm.freezeNMICycles = 0;
		latchSetClear( LATCH_LED0, 0 );
	} 

quitFIQandHandleReset:
	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
	UPDATE_COUNTERS( fm.c64CycleCount, fm.resetCounter, fm.resetPressed, fm.resetReleased, fm.cyclesSinceReset )
	if ( fm.resetCounter > 30 && fm.resetReleased )	callbackReset();
}
