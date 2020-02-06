/*
  _________.__    .___      __   .__        __        ____  __.                         .__   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |/ _|___________  ____ _____  |  |  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |      <_/ __ \_  __ \/    \\__  \ |  |  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |  \  ___/|  | \/   |  \/ __ \|  |__
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |____|__ \___  >__|  |___|  (____  /____/
        \/         \/    \/     \/       \/     \/           \/   \/           \/     \/      

 kernel_kernal.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Kernal: example how to implement a kernal cartridge, based on http://blog.worldofjani.com/?p=3736
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
#include "kernel_kernal.h"

// we will read this kernal .bin file
static const char DRIVE[] = "SD:";
static const char FILENAME[] = "SD:KERNAL/sd2iec.bin";
static unsigned char kernalROM[ 65536 ];

// for rebooting the RPi
static u64 c64CycleCount = 0;
static u32 resetCounter = 0;

#ifdef COMPILE_MENU
void KernelKernalRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME )
#else
void CKernelKernal::Run( void )
#endif
{
	latchSetClearImm( LATCH_ENABLE_KERNAL, LATCH_LED_ALL | LATCH_RESET );

	SETCLR_GPIO( bEXROM | bNMI | bGAME, bCTRL257 ); 

	#ifdef USE_OLED
	splashScreen( sidekick_kernal_oled );
	#endif

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	// read kernal rom 
	u32 kernalSize = 0; // should always be 8192
	readFile( logger, (char*)DRIVE, (char*)FILENAME, kernalROM, &kernalSize );

	DisableIRQs();

	// setup FIQ
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	// warm cache
	CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2048 )

	FORCE_READ_LINEAR( kernalROM, 8192 );
	FORCE_READ_LINEAR( &FIQ_HANDLER, 2048 );

	c64CycleCount = resetCounter = 0;

	// ready to go
	latchSetClearImm( LATCH_ENABLE_KERNAL | LATCH_RESET, LATCH_LED_ALL );

	// wait forever
	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( c64CycleCount, resetCounter )
		#endif

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

#ifdef COMPILE_MENU
void KernelKernalFIQHandler( void *pParam )
#else
void CKernelKernal::FIQHandler (void *pParam)
#endif
{
	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// update some counters
	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( ROMH_ACCESS && KERNAL_ACCESS )
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );

	FINISH_BUS_HANDLING
}

