/*
  _________.__    .___      __   .__        __         ___________                                        ____________________ ________  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     \_   _____/______   ____   ____ ________ ____      \_   _____/\_   ___ \\_____  \ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      |    __) \_  __ \_/ __ \_/ __ \\___   // __ \      |    __)  /    \  \/  _(__  < 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <       |     \   |  | \/\  ___/\  ___/ /    /\  ___/      |     \   \     \____/       \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \      \___  /   |__|    \___  >\___  >_____ \\___  >     \___  /    \______  /______  /
        \/         \/    \/     \/       \/     \/          \/                \/     \/      \/    \/          \/            \/       \/ 

 kernel_fc3.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Freeze: example how to implement a Final Cartridge 3(+) compatible cartridge
 Copyright (c) 2019 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

// we will read this .CRT file 
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:Freezer/fc3.crt";

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

	u32 releaseDMA;

	u32 LONGBOARD;
} __attribute__((packed)) FC3STATE;
#pragma pack(pop)

static volatile FC3STATE fc3 AAA;

// ... flash/ROM
static u8 flash_cacheoptimized_pool[ 16 * 8192 * 2 + 128 ] AAA;

static void initFC3()
{
    fc3.active = 1;
	fc3.curBank = 0;

	fc3.resetCounter = fc3.resetPressed = 
	fc3.resetReleased = fc3.cyclesSinceReset = 
	fc3.freezeNMICycles = fc3.lastFreezeButton = 	
	fc3.countWrites = fc3.c64CycleCount = 0;

	fc3.releaseDMA = 0;

	write32( ARM_GPIO_GPSET0, bDMA ); 
	write32( ARM_GPIO_GPCLR0, bNMI | bGAME | bEXROM | bCTRL257 ); 
}

__attribute__( ( always_inline ) ) inline void callbackReset()
{
	initFC3();
	latchSetClearImm( LATCH_LED0, 0 );
}

#ifdef COMPILE_MENU
void KernelFC3Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME )
#else
void CKernelFC3::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();
	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	SET_GPIO( bGAME | bEXROM | bNMI | bDMA );
	CLR_GPIO( bCTRL257 ); 

	//
	// load ROM and convert to cache-friendly format
	//
	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	fc3.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[0] + 128 ) & ~127 );

	CRT_HEADER header;
	u32 ROM_LH;
	u8 bankswitchType;

	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)fc3.flash_cacheoptimized, &bankswitchType, &ROM_LH, &fc3.nROMBanks, true );

	// todo hack?
	// fc3.nROMBanks = 16;

	#ifdef USE_OLED
	splashScreen( raspi_freeze_splash );
	#endif

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );

	// reset the FC3 and warm caches
	callbackReset();

	CleanDataCache();
	InvalidateDataCache();
	InvalidateInstructionCache();

	CACHE_PRELOAD_DATA_CACHE( fc3.flash_cacheoptimized, 8192 * 2 * fc3.nROMBanks, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEARa( fc3.flash_cacheoptimized, 8192 * 2 * fc3.nROMBanks, 8192 * 2 * fc3.nROMBanks * 1 )

	CACHE_PRELOAD_DATA_CACHE( (u8*)&fc3, ( sizeof( FC3STATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2048 )

	FORCE_READ_LINEAR32a( &fc3, sizeof( FC3STATE ), 65536 );
	FORCE_READ_LINEAR32a( &FIQ_HANDLER, 2048, 65536 );

	FORCE_READ_LINEARa( fc3.flash_cacheoptimized, 8192 * 2 * 4, 8192 * 2 * 4 * 32 )

	// different timing C64-longboards and C128 compared to 469-boards
	fc3.LONGBOARD = 0;
	if ( modeC128 || modeVIC == 0 )
		fc3.LONGBOARD = 1; 

	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );
	m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );

	// ready to go
	DELAY(10);
	latchSetClearImm( LATCH_RESET, LATCH_LED0to3 | LATCH_ENABLE_KERNAL );

	// main loop
	while ( true ) {
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU2FIQs( (fc3.c64CycleCount*2), (fc3.resetCounter*2) )
		#endif

		if ( fc3.resetCounter > 30 && fc3.resetReleased )
			callbackReset();

		asm volatile ("wfi");
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

	// VIC2 read during badline?
	if ( VIC_BADLINE )
	{
		// get ROMH data (not reading anything else here)
		D = *(u8*)&flashBank[ GET_ADDRESS * 2 + 1 ];

		// read again (some time passed, had to wait to signals)
		if ( !fc3.LONGBOARD )
		{
			// do not do this: WAIT_UP_TO_CYCLE( WAIT_CYCLE_MULTIPLEXER_VIC2 ); 
			READ_ADDR8to12_ROMLH_IO12_BA 
		}

		if ( ROMH_ACCESS ) 
			WRITE_D0to7_TO_BUS_BADLINE( D )

		FINISH_BUS_HANDLING
		return;
	}

	UPDATE_COUNTERS( fc3.c64CycleCount, fc3.resetCounter, fc3.resetPressed, fc3.resetReleased, fc3.cyclesSinceReset )

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		// get both ROML and ROMH with one read
		D = *(u32*)&flashBank[ GET_ADDRESS * 2 ];

		if ( ROMH_ACCESS ) D >>= 8; 

		WRITE_D0to7_TO_BUS ( D )

		FINISH_BUS_HANDLING
		return;
	} 

	// read from FC3's ROM visible in IO1/2 
	if ( CPU_READS_FROM_BUS && IO1_OR_IO2_ACCESS )
	{
		// an attempt to make the read more cache-friendly/aligned
		if ( !( g3 & bIO1 ) )
			D = *(u8*)&flashBank[ ( (30 << 8) | GET_ADDRESS0to7 ) * 2 ]; else
			D = *(u8*)&flashBank[ ( (31 << 8) | GET_ADDRESS0to7 ) * 2 ]; 

		WRITE_D0to7_TO_BUS( D )

		FINISH_BUS_HANDLING
		return;
	}

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

		fc3.curBank = D & ( fc3.nROMBanks - 1 );

		fc3.active = ( (D >> 7) & 1 ) ^ 1;
		if ( !fc3.active )
			latchSetClearImm( 0, LATCH_LED0 );

		RESET_CPU_CYCLE_COUNTER
		return;
	}

	// freeze
	if ( BUTTON_PRESSED && ( fc3.lastFreezeButton == 0 ) )
	{
		fc3.lastFreezeButton = 500000; // debouncing
		fc3.freezeNMICycles  = 10;
		fc3.countWrites      = 0;

		CLR_GPIO( bNMI | bCTRL257 ); 
		RESET_CPU_CYCLE_COUNTER
		return;
	}

	// debouncing
	if ( fc3.lastFreezeButton > 0 )
		fc3.lastFreezeButton --;

	// and go to ultimax mode once the CPU is ready
	if ( fc3.freezeNMICycles && fc3.countWrites == 3 )
	{
		SETCLR_GPIO( bEXROM, bGAME | bCTRL257 ); 
		fc3.freezeNMICycles = 0;
		RESET_CPU_CYCLE_COUNTER
		return;
	} 

	FINISH_BUS_HANDLING
}

