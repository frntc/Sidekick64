/*
  _________.__    .___      __   .__        __     ___________                                      ____  __._________   _________
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __ \_   _____/______   ____   ____ ________ ____   |    |/ _|\_   ___ \ /   _____/
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /  |    __) \_  __ \_/ __ \_/ __ \\___   // __ \  |      <  /    \  \/ \_____  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <   |     \   |  | \/\  ___/\  ___/ /    /\  ___/  |    |  \ \     \____/        \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \___  /   |__|    \___  >\___  >_____ \\___  > |____|__ \ \______  /_______  /
        \/         \/    \/     \/       \/     \/      \/                \/     \/      \/    \/          \/        \/        \/ 
		
 kernel_kcs.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Freeze: example how to implement a KCS Power Cartridge compatible 
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

#include "kernel_kcs.h"

// we will read this .CRT file 
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:Freezer/kcs.crt";

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_kcs.tga";

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	// KCS register
	u8	kcsReg;
	// extra RAM
	u8  ram[ 128 ];

	u32 active, nROMBanks, curBank, ultimax;

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

	u32 releaseDMA;

	u32 LONGBOARD;

	u32 hasKernal;
} __attribute__((packed)) KCSSTATE;
#pragma pack(pop)

static volatile KCSSTATE kcs AAA;

static unsigned char kernalROM[ 8192 ] AAA;

// ... flash/ROM
extern u8 *flash_cacheoptimized_pool;

static __attribute__( ( always_inline ) ) inline void kcsConfig( u8 c )
{
	if ( c == 3 )
		latchSetClear( 0, LATCH_LED0 ); else
		latchSetClear( LATCH_LED0, 0 ); 

	kcs.ultimax = 0;
	switch ( c )
	{
		default:
		case 0:	SETCLR_GPIO( bDMA | bNMI, bGAME | bEXROM );	break;
		case 1:	SETCLR_GPIO( bGAME | bDMA | bNMI, bEXROM );	break;
		case 2:	SETCLR_GPIO( bDMA | bNMI | bEXROM, bGAME );	kcs.ultimax = 1; break;
		case 3:	SET_GPIO( bGAME | bEXROM | bDMA | bNMI );	break;
	}
}

static void initKCS()
{
	kcs.kcsReg = 0;
	kcs.curBank = 0;
    kcs.active = 1;
	kcs.ultimax = 0; 

	kcs.resetCounter = kcs.resetPressed = 
	kcs.resetReleased = kcs.cyclesSinceReset = 
	kcs.freezeNMICycles = kcs.lastFreezeButton = 	
	kcs.countWrites = kcs.c64CycleCount = 0;

	kcs.releaseDMA = 0;

	SETCLR_GPIO( bDMA | bNMI, bGAME | bEXROM | bCTRL257 );
}

__attribute__( ( always_inline ) ) inline void callbackReset()
{
	initKCS();
	latchSetClearImm( LATCH_LED0, 0 );
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
void KernelKCSRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME, const char *FILENAME_KERNAL = NULL )
#else
void CKernelKCS::Run( void )
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

	// load kernal is any
	u32 kernalSize = 0; // should always be 8192
	if ( FILENAME_KERNAL != NULL )
	{
		kcs.hasKernal = 1;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_KERNAL, flash_cacheoptimized_pool, &kernalSize );
		memcpy( kernalROM, flash_cacheoptimized_pool, 8192 );
	} else
		kcs.hasKernal = 0;

	kcs.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[0] + 128 ) & ~127 );

	CRT_HEADER header;
	u32 ROM_LH;
	u8 bankswitchType;

	u32 nBanks;
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)kcs.flash_cacheoptimized, &bankswitchType, &ROM_LH, &nBanks, true );
	kcs.nROMBanks = nBanks;

	// todo hack?
	// kcs.nROMBanks = 16;

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

	// reset the KCS and warm caches
	callbackReset();

	SyncDataAndInstructionCache();

	CACHE_PRELOAD_DATA_CACHE( kcs.flash_cacheoptimized, 8192 * 2 * kcs.nROMBanks, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEARa( kcs.flash_cacheoptimized, 8192 * 2 * kcs.nROMBanks, 8192 * 2 * kcs.nROMBanks * 1 )

	CACHE_PRELOAD_DATA_CACHE( (u8*)&kcs, ( sizeof( KCSSTATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2048 )

	FORCE_READ_LINEAR32a( &kcs, sizeof( KCSSTATE ), 65536 );
	FORCE_READ_LINEAR32a( &FIQ_HANDLER, 2048, 65536 );

	FORCE_READ_LINEARa( kcs.flash_cacheoptimized, 8192 * 2 * 4, 8192 * 2 * 4 * 32 )

	if ( kcs.hasKernal )
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP );

	// different timing C64-longboards and C128 compared to 469-boards
	kcs.LONGBOARD = 0;
	if ( modeC128 || modeVIC == 0 )
		kcs.LONGBOARD = 1; 

	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );
	m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );

	// ready to go
	DELAY(10);

	if ( kcs.hasKernal )
		latchSetClear( LATCH_RESET | LATCH_ENABLE_KERNAL, LED_ALL_BUT_0 ); else
		latchSetClear( LATCH_RESET, LED_ALL_BUT_0 | LATCH_ENABLE_KERNAL );

	// main loop
	while ( true ) {
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU2FIQs( (kcs.c64CycleCount*2), (kcs.resetCounter*2) )
		#endif

		if ( kcs.resetCounter > 30 && kcs.resetReleased )
			callbackReset();

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt2();
	m_InputPin.DisableInterrupt();
}


#ifdef COMPILE_MENU
void KernelKCSFIQHandler( void *pParam )
#else
void CKernelKCS::FIQHandler (void *pParam)
#endif
{
	register u8 *flashBank = &kcs.flash_cacheoptimized[ kcs.curBank * 8192 * 2 ];
	register u32 D;

	START_AND_READ_ADDR0to7_RW_RESET_CS_NO_MULTIPLEX

	if ( VIC_HALF_CYCLE )
	{
		READ_ADDR0to7_RW_RESET_CS_AND_MULTIPLEX

		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA_VIC2

		D = *(u8*)&flashBank[ GET_ADDRESS * 2 + 1 ];

		if ( ROMH_ACCESS )
			WRITE_D0to7_TO_BUS_VIC( D )

		FINISH_BUS_HANDLING
		return;
	}  

	SET_GPIO( bCTRL257 );	

	// starting from here we are in the CPU-half cycle
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	UPDATE_COUNTERS( kcs.c64CycleCount, kcs.resetCounter, kcs.resetPressed, kcs.resetReleased, kcs.cyclesSinceReset )

	if ( kcs.hasKernal && ROMH_ACCESS && KERNAL_ACCESS && !kcs.ultimax )
	{
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );
		FINISH_BUS_HANDLING
		return;
	}

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		// get both ROML and ROMH with one read
		D = *(u32*)&flashBank[ GET_ADDRESS * 2 ];

		if ( ROMH_ACCESS ) D >>= 8; 

		WRITE_D0to7_TO_BUS ( D )

		FINISH_BUS_HANDLING
		return;
	} 

	// read from KCS's ROM visible in IO1
	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		D = *(u8*)&flashBank[ ( (30 << 8) | GET_IO12_ADDRESS ) * 2 ];
		WRITE_D0to7_TO_BUS( D )

		kcs.kcsReg = ( GET_IO12_ADDRESS & 2 ) | 1;
		kcsConfig( kcs.kcsReg );

		FINISH_BUS_HANDLING
		return;
	}

	// read from KCS's ROM visible in IO2 
	if ( CPU_READS_FROM_BUS && IO2_ACCESS )
	{
		if ( !( GET_IO12_ADDRESS & 0x80 ) )
		{
			WRITE_D0to7_TO_BUS( kcs.ram[ GET_IO12_ADDRESS & 0x7f ] );
		} else
		{
			WRITE_D0to7_TO_BUS( kcs.kcsReg << 6 );
		}

		FINISH_BUS_HANDLING
		return;
	}

	if ( CPU_WRITES_TO_BUS )
		kcs.countWrites ++; else
		kcs.countWrites = 0;

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS /*&& kcs.active*/ )
	{
		kcs.kcsReg = GET_IO12_ADDRESS & 2;
		kcsConfig( kcs.kcsReg );
	}

	if ( CPU_WRITES_TO_BUS && IO2_ACCESS /*&& kcs.active*/ )
	{
		if ( !( GET_IO12_ADDRESS & 0x80 ) )
		{
			READ_D0to7_FROM_BUS( D )
				kcs.ram[ GET_IO12_ADDRESS & 0x7f ] = D;
		}

		//SET_GPIO( bNMI ); 
        return;
	}

	// freeze
	if ( BUTTON_PRESSED && ( kcs.lastFreezeButton == 0 ) )
	{
		kcs.lastFreezeButton = 500000; // debouncing
		kcs.freezeNMICycles  = 10;
		kcs.countWrites      = 0;

        kcs.kcsReg = 2;
		kcsConfig( 2 );

		CLR_GPIO( bNMI | bCTRL257 ); 
		RESET_CPU_CYCLE_COUNTER
		return;
	}

	// debouncing
	if ( kcs.lastFreezeButton > 0 )
		kcs.lastFreezeButton --;

	// and go to ultimax mode once the CPU is ready
	if ( kcs.freezeNMICycles && kcs.countWrites == 3 )
	{
		SETCLR_GPIO( bEXROM, bGAME | bCTRL257 ); 
		kcs.ultimax = 1;
		kcs.freezeNMICycles = 0;
		RESET_CPU_CYCLE_COUNTER
		return;
	} 

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

