/*
  _________.__    .___      __   .__        __       _________                __   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   ___ \_____ ________/  |_ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /    \  \/\__  \\_  __ \   __\
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    \     \____/ __ \|  | \/|  |  
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \______  (____  /__|   |__|  
        \/         \/    \/     \/       \/     \/           \/     \/             

 kernel_cart.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Cart: example how to implement a simple CBM80 cartridge
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
#include "kernel_cart.h"

// setting EXROM and GAME (low = 0, high = 1)
#define SET_EXROM	0
#define SET_GAME	1

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

// simply defines "const unsigned char cart[8192]", a binary dump of a 8k cartridge
#include "Cartridges/cart_d020.h"		// ROML, EXROM closed
//#include "Cartridges/cart_1541.h"		// ROML, EXROM closed
//#include "Cartridges/cart_deadtest.h" // ROMH, GAME closed 
//#include "Cartridges/cart_diag41.h"	// ROML, EXROM and GAME closed
//#include "Cartridges/cart_64erdisc.h" // ROML, EXROM closed


// for rebooting the RPi
static u64 c64CycleCount = 0;
static u32 resetCounter = 0;

#ifdef COMPILE_MENU
void KernelCartRun( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu )
#else
void CKernelCart::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	#ifdef USE_OLED
	// I know this is a gimmick, but I couldn't resist ;-)
	splashScreen( raspi_cart_splash );
	#endif
	
	// set GAME and EXROM as defined above (+ set NMI, DMA and latch outputs)
	u32 set = bNMI | bDMA, clr = 0;

	if ( SET_EXROM == 0 )
		clr |= bEXROM; else
		set |= bEXROM; 

	if ( SET_GAME == 0 )
		clr |= bGAME; else
		set |= bGAME; 

	SETCLR_GPIO( set, clr )

	DisableIRQs();

	// setup FIQ
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

	// preload caches, and try to convince the RPi that we really want the data to reside in caches (by reading it again)
	CACHE_PRELOAD_DATA_CACHE( &cart[ 0 ], 8192, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 1536 );
	FORCE_READ_LINEAR32( cart, 8192 );
	FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 1536 );

	latchSetClearImm( LATCH_RESET, LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	c64CycleCount = resetCounter = 0;

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
void KernelCartFIQHandler( void *pParam )
#else
void CKernelCart::FIQHandler( void *pParam )
#endif
{
	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// update some counters
	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( CPU_READS_FROM_BUS && ACCESS( ROM_LH ) )
		WRITE_D0to7_TO_BUS( cart[ GET_ADDRESS ] );

	FINISH_BUS_HANDLING
}

int main( void )
{
	CKernelCart kernel;
	if ( kernel.Initialize() )
		kernel.Run();

	halt();
	return EXIT_REBOOT;
}

