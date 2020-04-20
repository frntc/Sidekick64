/*
  _________.__    .___      __   .__        __       .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/           \/    \/           \/     \/     \/ 

 kernel_launch264.cpp

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - Sidekick Launch 264: example how to implement a .PRG dropper
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
#include "kernel_launch264.h"

static u32	resetCounter, c64CycleCount, exitToMainMenu;

// hack
u32 h_nRegOffset;
u32 h_nRegMask;


void KernelLaunchFIQHandler( void *pParam );


#ifdef COMPILE_MENU
void KernelLaunchRun( CGPIOPinFIQ2 m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 )
#else
void CKernelLaunch::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	#ifdef USE_OLED
	splashScreen( sidekick_launch_oled );
	#endif

	// read launch code and .PRG
	#ifdef COMPILE_MENU
	if ( FILENAME == NULL && !hasData )
	{
		launchPrg_l264 = 0;
		disableCart_l264 = 1;
		logger->Write( "", LogNotice, "launch nothing" );
	} else
	{
		launchPrg_l264 = 1;
		if ( launchGetProgram( FILENAME, hasData, prgDataExt, prgSizeExt ) )
			launchInitLoader( false ); else
			launchPrg_l264 = 0;
		logger->Write( "", LogNotice, "launch '%s' (%d bytes)", FILENAME, prgSizeExt );
	}
	#endif
	for ( u32 i = prgSize_l264; i > 0; i-- )
		prgData_l264[ i ] = prgData_l264[ i - 1 ];
	prgData_l264[0] = ( ( prgSize_l264 + 255 ) >> 8 );

	
	// setup FIQ
	DisableIRQs();
	//m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.ConnectInterrupt( KernelLaunchFIQHandler, FIQ_PARENT );
	h_nRegOffset = m_InputPin.nRegOffset();
	h_nRegMask = m_InputPin.nRegMask();

	// warm caches
	launchPrepareAndWarmCache();

	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	// warm caches
	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 1024 );
	FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 1024 );

	exitToMainMenu = 0;
	c64CycleCount = resetCounter = 0;
	disableCart_l264 = transferStarted_l264 = currentOfs_l264 = 0;

	// ready to go
	latchSetClearImm( LATCH_RESET, 0 );


	// wait forever
	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( c64CycleCount, resetCounter )

		/*if ( exitToMainMenu )
		{
			EnableIRQs();
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			return;
		}*/
		#endif

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}


#ifdef COMPILE_MENU

#define REGOFFSET h_nRegOffset
#define REGMASK h_nRegMask

void KernelLaunchFIQHandler( void *pParam )
#else

#define REGOFFSET h_nRegOffset
#define REGMASK h_nRegMask

void CKernelLaunch::FIQHandler (void *pParam) {};

void CGPIOPinFIQ2::FIQHandler( void *pParam ) {};

void KernelLaunchFIQHandler( void *pParam )
#endif
{
	#ifdef COMPILE_MENU
	if ( launchPrg_l264 && !disableCart_l264 )
	{
		LAUNCH_FIQ( c64CycleCount, resetCounter,  h_nRegOffset, h_nRegMask )
	}
	#endif

	START_AND_READ_EXPPORT264
 
	// this is an ugly hack to signal the menu code that it can reset (only necessary on a +4)
	if ( BUS_AVAILABLE264 && CPU_READS_FROM_BUS && GET_ADDRESS264 >= 0xfd90 && GET_ADDRESS264 <= 0xfd97 )
	{
		PUT_DATA_ON_BUS_AND_CLEAR257( GET_ADDRESS264 - 0xfd90 + 1 )
	}

	PeripheralEntry();
	write32( ARM_GPIO_GPEDS0 + h_nRegOffset, h_nRegMask );
	PeripheralExit();

	/*if ( BUTTON_PRESSED )
	{
		latchSetClearImm( LATCH_LED_ALL, LATCH_RESET | LATCH_ENABLE_KERNAL );
		exitToMainMenu = 1;
	}*/

	if ( CPU_RESET ) 
		resetCounter ++;

	if ( resetCounter > 3 )//&& geo.resetReleased == 1 )
	{
		resetCounter = 0;
		disableCart_l264 = transferStarted_l264 = 0;
	}

	FINISH_BUS_HANDLING
}

