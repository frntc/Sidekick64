/*
  _________.__    .___      __   .__        __        __      __                                                     .___
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   /  \    /  \_____ _____________  ____________   ____   ____   __| _/
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   \   \/\/   /\__  \\_  __ \____ \/  ___/\____ \_/ __ \_/ __ \ / __ | 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \        /  / __ \|  | \/  |_> >___ \ |  |_> >  ___/\  ___// /_/ | 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \__/\  /  (____  /__|  |   __/____  >|   __/ \___  >\___  >____ | 
        \/         \/    \/     \/       \/     \/          \/        \/      |__|       \/ |__|        \/     \/     \/ 

 kernel_warpspeed.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - example how to implement a Warpspeed Cartridge for C64 and C128
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

#include "kernel_warpspeed.h"

// we will read this .CRT file 
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:Freezer/ws.crt";

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_ws.tga";

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	u32 active;
	u32 runInC128Mode;

	// counts the #cycles when the C64-reset line is pulled down (to detect a reset), cycles since release and status
	u32 resetCounter, cyclesSinceReset,  
		resetPressed, resetReleased;

	// total count of C64 cycles (not set to zero upon reset)
	u64 c64CycleCount;

	// pointer to ROM data
	u8 *cartridgeROM;

	// orchestration of freezing
	u32 buttonPressed, lastButtonPress;

	u32 hasKernal;
} __attribute__((packed)) WSSTATE;
#pragma pack(pop)

static volatile WSSTATE ws AAA;

// ... flash/ROM
extern u8 flash_cacheoptimized_pool[ 1024 * 1024 + 1024 ] AAA;

//static unsigned char kernalROM[ 8192 ] AAA;
static unsigned char *kernalROM = &flash_cacheoptimized_pool[ 1024 * 1024 - 8192 ];

__attribute__( ( always_inline ) ) inline void initWS()
{
    ws.active = 1;
	ws.resetCounter = ws.resetPressed = 
	ws.resetReleased = ws.cyclesSinceReset = 
	ws.lastButtonPress = 	
	ws.c64CycleCount = 0;
	ws.buttonPressed = 0;

	if ( ws.runInC128Mode )
		{SETCLR_GPIO( bEXROM | bGAME | bNMI | bDMA, bCTRL257 );} else
		{SETCLR_GPIO( bNMI | bDMA, bEXROM | bGAME | bCTRL257 );}

	latchSetClear( LATCH_LED0, 0 );
}

__attribute__( ( always_inline ) ) inline void callbackReset()
{
	initWS();
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
void KernelWSRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME, const char *FILENAME_KERNAL = NULL )
#else
void CKernelWS::Run( void )
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

	memset( (void*)&ws, 0, sizeof( WSSTATE ) );

	// load kernal if any
	u32 kernalSize = 0; // should always be 8192
	if ( FILENAME_KERNAL != NULL )
	{
		ws.hasKernal = 1;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_KERNAL, flash_cacheoptimized_pool, &kernalSize );
		memcpy( kernalROM, flash_cacheoptimized_pool, 8192 );
	} else
		ws.hasKernal = 0;

	ws.cartridgeROM = (u8 *)( ( (u64)&flash_cacheoptimized_pool[0] + 128 ) & ~127 );

	CRT_HEADER header;
	u32 ROM_LH;
	u8 bankswitchType;
	logger->Write( "RaspiFlash", LogNotice, "read" );

	u32 nBanks;
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)ws.cartridgeROM, &bankswitchType, &ROM_LH, &nBanks, true );

	#ifdef COMPILE_MENU
	if ( screenType == 0 )
	{
		splashScreen( raspi_freeze_splash );
	} else
	if ( screenType == 1 )
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
		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}
	#endif

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );
	logger->Write( "RaspiFlash", LogNotice, "done" );


	// reset the FC3 and warm caches
	callbackReset();
	ws.runInC128Mode = 0;

	SyncDataAndInstructionCache();

	CACHE_PRELOAD_DATA_CACHE( &ws.cartridgeROM[ 0 ], 16384, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32_SKIP( ws.cartridgeROM, 16384 )

	CACHE_PRELOAD_DATA_CACHE( (u8*)&ws, ( sizeof( WSSTATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2600 )

	FORCE_READ_LINEAR32a( &ws, sizeof( WSSTATE ), 65536 );
	FORCE_READ_LINEAR32a( &FIQ_HANDLER, 2600, 65536 );

	FORCE_READ_LINEAR32_SKIP( ws.cartridgeROM, 16384 * 4 )

	if ( ws.hasKernal )
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP );

	if ( ws.hasKernal )
		latchSetClear( LATCH_RESET | LATCH_ENABLE_KERNAL, LED_ALL_BUT_0 ); else
		latchSetClear( LATCH_RESET, LED_ALL_BUT_0 | LATCH_ENABLE_KERNAL );

	// main loop
	ws.c64CycleCount = 0;

	logger->Write( "RaspiFlash", LogNotice, "go" );
	while ( true ) {
		#ifdef COMPILE_MENU
		{
			TEST_FOR_JUMP_TO_MAINMENU( (ws.c64CycleCount*2), (ws.resetCounter*2) )
		}
		#endif

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt2();
	m_InputPin.DisableInterrupt();
}


#ifdef COMPILE_MENU
void KernelWSFIQHandler( void *pParam )
#else
void CKernelWS::FIQHandler (void *pParam)
#endif
{
	register u8 *flashBank = &ws.cartridgeROM[ 0 ];
	register u8 D;

	START_AND_READ_ADDR0to7_RW_RESET_CS
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( ws.hasKernal && ROMH_ACCESS && KERNAL_ACCESS /*&& !(ws.active & 256)*/ )
	{
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );
		goto quitFIQandHandleReset;
	} 

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS && ws.active )
	{
		D = flashBank[ GET_ADDRESS0to13 ];
		WRITE_D0to7_TO_BUS ( D )
		goto quitFIQandHandleReset;
	} 
/*	if ( CPU_READS_FROM_BUS && ROMH_ACCESS && ws.active )
	{
		D = flashBank[ GET_ADDRESS + 8192 ];
		WRITE_D0to7_TO_BUS ( D )
		goto quitFIQandHandleReset;
	} */

	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		D = flashBank[ 0x1e00 + GET_IO12_ADDRESS ];
		WRITE_D0to7_TO_BUS ( D )
		goto quitFIQandHandleReset;
	}

	if ( CPU_READS_FROM_BUS && IO2_ACCESS )
	{
		D = flashBank[ 0x1f00 + GET_IO12_ADDRESS ];
		WRITE_D0to7_TO_BUS ( D )
		goto quitFIQandHandleReset;
	}

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		ws.active = 1;
		CLR_GPIO( bEXROM | bGAME ); 
		latchSetClear( LATCH_LED0, 0 );
		goto quitFIQandHandleReset;
	}

	if ( CPU_WRITES_TO_BUS && IO2_ACCESS )
	{
		ws.active = 0;
		SET_GPIO( bEXROM | bGAME ); 
		latchSetClear( 0, LATCH_LED0 );
		goto quitFIQandHandleReset;
	}

	if ( BUTTON_PRESSED && modeC128 && ( ws.lastButtonPress == 0 ) )
	{
		ws.lastButtonPress = 500000; // debouncing
		ws.buttonPressed = 1;
		latchSetClear( 0, LATCH_RESET );
	}

	if ( !BUTTON_PRESSED && ws.buttonPressed )
	{
		ws.buttonPressed = 0;
		ws.runInC128Mode = 1 - ws.runInC128Mode;
		callbackReset();
		latchSetClear( LATCH_RESET, 0 );
	}

	// debouncing
	if ( ws.lastButtonPress > 0 )
		ws.lastButtonPress --;

quitFIQandHandleReset:
	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
	UPDATE_COUNTERS( ws.c64CycleCount, ws.resetCounter, ws.resetPressed, ws.resetReleased, ws.cyclesSinceReset )
	if ( ws.resetCounter > 30 && ws.resetReleased )	callbackReset();
}
