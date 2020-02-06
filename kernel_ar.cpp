/*
  _________.__    .___      __   .__        __       ___________                                          _____ __________ 
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   _____/______   ____   ____ ________ ____       /  _  \\______   \
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |    __) \_  __ \_/ __ \_/ __ \\___   // __ \     /  /_\  \|       _/
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |     \   |  | \/\  ___/\  ___/ /    /\  ___/    /    |    \    |   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \___  /   |__|    \___  >\___  >_____ \\___  >   \____|__  /____|_  /
        \/         \/    \/     \/       \/     \/        \/                \/     \/      \/    \/            \/       \/ 

 kernel_ar.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Freeze: example how to implement an Action Replay compatible cartridge
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

#include "kernel_ar.h"
#include "crt.h"
#ifdef COMPILE_MENU
#include "kernel_menu.h"
#endif

// we will read this .CRT file
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:Freezer/ar6.crt";

typedef struct
{
	u32 active;
	u32 statusRegister;
	u32 exportRAM;
	u32 ofsROMBank;

	// counts the #cycles when the C64-reset line is pulled down (to detect a reset), cycles since release and status
	u32 resetCounter,
		cyclesSinceReset,  
		resetPressed, resetReleased,
		countWrites;

	// total count of C64 cycles (not set to zero upon reset)
	u64 c64CycleCount;

	u32 freezeNMICycles;
	u32 lastFreezeButton;

	u8 *flash_cacheoptimized;
	u8 *ramAR;
} __attribute__((packed)) ARSTATE;

volatile ARSTATE ar AAA;

// ... flash/ROM (32k) and RAM (8k)
u8 flash_cacheoptimized_pool[ 5 * 8192 + 1024 ] AAA;

__attribute__( ( always_inline ) ) inline void setGAMEEXROM( u32 f )
{
	SET_GPIO( ((f&2)?bEXROM:0) | ((f&1)?0:bGAME) );
	CLR_GPIO( ((f&2)?0:bEXROM) | ((f&1)?bGAME:0) );
}


//  Action Replay 4.2, 5, 6, 7 Control Register IO1
//	0    1 = /GAME low
//  1    1 = /EXROM high
//  2    1 = disable cartridge (turn off $DE00)
//  3-4  ROM bank
//  5    enable RAM for R/W at $8000 and I/O2 ($df00-$dfff mirrors $9f00-$9fff)
//  6    reset freeze-mode
__attribute__( ( always_inline ) ) inline void ar_IO1_Write( u32 addr, u8 value )
{
	ar.statusRegister = value;
	ar.exportRAM = ( value >> 5 ) & 1;
	ar.ofsROMBank = ( ( value >> 3 ) & 3 ) * 8192;

	if ( value & 4 )
		ar.active = 0; else
		ar.active = 1;

	setGAMEEXROM( value & 3 );
}

void initAR()
{
	ar_IO1_Write( 0, 0 );
	ar.resetCounter = ar.resetPressed = 
	ar.resetReleased = ar.cyclesSinceReset = 
	ar.freezeNMICycles = ar.lastFreezeButton = 	
	ar.countWrites = ar.c64CycleCount = 0;
}

__attribute__( ( always_inline ) ) inline void callbackReset()
{
	initAR();
	latchSetClearImm( LATCH_LED0, 0 );
}


#ifdef COMPILE_MENU
void KernelAR6Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL )
#else
void CKernel::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();
	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	SET_GPIO( bGAME | bEXROM | bNMI | bDMA );

	//
	// load ROM and convert to cache-friendly format
	//
	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	CRT_HEADER header;
	u32 ROM_LH, nBanks;
	u8 bankswitchType, temp[ 8192 * 4 * 2 ];
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)temp, &bankswitchType, &ROM_LH, &nBanks, true );

	ar.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[0] + 128 ) & ~127 );
	for ( u32 b = 0; b < 4; b++ )
		for ( u32 i = 0; i < 8192; i++ )
			ar.flash_cacheoptimized[ ( b * 8192 + ADDR_LINEAR2CACHE( i ) ) ] = temp[ (b * 8192 + i) * 2 ];

	ar.ramAR = &ar.flash_cacheoptimized[ 4 * 8192 ];
	memset( ar.ramAR, 0, 8192 );

	#ifdef USE_OLED
	splashScreen( raspi_freeze_splash );
	#endif

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

	// reset the AR and warm caches
	callbackReset();

	CACHE_PRELOAD_DATA_CACHE( ar.flash_cacheoptimized, 8192 * 4, CACHE_PRELOADL2KEEP )
	CACHE_PRELOAD_DATA_CACHE( ar.ramAR, 8192, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_DATA_CACHE( &ar, ( sizeof( ARSTATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	FORCE_READ_LINEAR32a( ar.flash_cacheoptimized, 8192 * 4, 8192 * 32 )
	FORCE_READ_LINEAR32a( ar.ramAR, 8192, 8192 * 8 )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2048 );
	FORCE_READ_LINEAR32a( (void*)&FIQ_HANDLER, 2048, 65536 );

	// ready to go
	latchSetClearImm( LATCH_LED0 | LATCH_RESET, LATCH_LED1to3 | LATCH_ENABLE_KERNAL );

	// wait forever
	while ( true )
	{
		// blinking activity LED
		// clrLatchFIQ( LATCH_LEDR );	if ( ( ar.c64CycleCount >> 18 ) & 1 ) setLatchFIQ( LATCH_LEDR ); outputLatch();			

		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( ar.c64CycleCount, ar.resetCounter )
		#endif

		if ( ar.resetCounter > 30 && ar.resetReleased )
			callbackReset();

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

#ifdef COMPILE_MENU
void KernelAR6FIQHandler( void *pParam )
#else
void CKernel::FIQHandler (void *pParam)
#endif
{
	register u8 *flashBank = &ar.flash_cacheoptimized[ ar.ofsROMBank ];

	START_AND_READ_ADDR0to7_RW_RESET_CS

	// we got the A0..A7 part of the address which we will access
	register u32 addr = GET_ADDRESS0to7 << 5;

	CACHE_PRELOADL2STRM( &flashBank[ addr ] );
	if ( ar.exportRAM )
	{
		if ( g2 & bRW )
			{ CACHE_PRELOADL2STRM( &ar.ramAR[ addr ] ); } else
			{ CACHE_PRELOADL2STRMW( &ar.ramAR[ addr ] ); }
	}

	UPDATE_COUNTERS( ar.c64CycleCount, ar.resetCounter, ar.resetPressed, ar.resetReleased, ar.cyclesSinceReset )
	
	if ( CPU_WRITES_TO_BUS )
		ar.countWrites ++; else
		ar.countWrites = 0;

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	// if AR is not active we might still freeze
	if ( !ar.active )
		goto freezer;

	// make our address complete
	addr |= GET_ADDRESS8to12;

	// read access to ROM at $8000/$a000 
	if ( CPU_READS_FROM_BUS && ( ( ROML_ACCESS && !ar.exportRAM ) || ROMH_ACCESS ) )
	{
		WRITE_D0to7_TO_BUS( flashBank[ addr ] );
		FINISH_BUS_HANDLING
		return;
	} 

	// read access to RAM at $8000
	if ( CPU_READS_FROM_BUS && ROML_ACCESS && ar.exportRAM )
	{
		WRITE_D0to7_TO_BUS( ar.ramAR[ addr ] );
		FINISH_BUS_HANDLING
		return;
	} 

	// write access to RAM at $8000
	if ( CPU_WRITES_TO_BUS && ROML_ACCESS && ar.exportRAM )
	{	
		READ_D0to7_FROM_BUS( ar.ramAR[ addr ] );
		FINISH_BUS_HANDLING
		return;
	} 

	// read status register in IO1-range
	if ( CPU_READS_FROM_BUS && IO1_ACCESS )
	{
		WRITE_D0to7_TO_BUS( ar.statusRegister )
		FINISH_BUS_HANDLING
		return;
	}

	// read part of RAM or ROM in IO2-range
	if ( CPU_READS_FROM_BUS && IO2_ACCESS )
	{
		if ( ar.exportRAM )
			WRITE_D0to7_TO_BUS( ar.ramAR[ addr ] ) else
			WRITE_D0to7_TO_BUS( flashBank[ addr ] )

		FINISH_BUS_HANDLING
		return;
	}

	// write status register
	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		register u32 D;

		READ_D0to7_FROM_BUS( D );
		ar_IO1_Write( GET_IO12_ADDRESS, D );

		if ( !ar.active )
			latchSetClearImm( 0, LATCH_LED0 );

		FINISH_BUS_HANDLING
		return;
	}

	// write to part of the AR-RAM in IO2-range
	if ( CPU_WRITES_TO_BUS && IO2_ACCESS && ar.exportRAM )
	{
		READ_D0to7_FROM_BUS( ar.ramAR[ addr | 31 ] );
		FINISH_BUS_HANDLING
		return;
	}

freezer:
	// initialize freezing
	if ( BUTTON_PRESSED && ( ar.lastFreezeButton == 0 ) )
	{
		ar.lastFreezeButton = 500000; // debouncing
		ar.freezeNMICycles  = 10;

		CLR_GPIO( bNMI | bCTRL257 ); 
		RESET_CPU_CYCLE_COUNTER
		return;
	}

	if ( ar.lastFreezeButton )
		ar.lastFreezeButton --;

	// and go to ultimax mode once the CPU is ready
	if ( ar.freezeNMICycles && ar.countWrites == 3 )
	{
		SET_GPIO( bEXROM | bNMI );
		CLR_GPIO( bGAME | bCTRL257 ); 

		ar.statusRegister = 3;
		ar.active = 1;
		ar.freezeNMICycles = ar.ofsROMBank = ar.exportRAM = 0;

		latchSetClearImm( LATCH_LED0, 0 );

		RESET_CPU_CYCLE_COUNTER
		return;
	}

	FINISH_BUS_HANDLING
}
