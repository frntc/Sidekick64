/*
  _________.__    .___      __   .__        __       .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/           \/    \/           \/     \/     \/ 

 kernel_launch.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Launch: example how to implement a .PRG dropper
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
#include "kernel_launch.h"

// we will read this .PRG file
static const char DRIVE[] = "SD:";
#ifndef COMPILE_MENU
static const char FILENAME[] = "SD:C64/test.prg";		// .PRG to start
static const bool c128PRG = false;
#endif
static const char FILENAME_CBM80[] = "SD:C64/launch.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM128[] = "SD:C64/launch128.cbm80";	// launch code (C128 cart)

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_launch.tga";

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

static u32	configGAMEEXROMSet, configGAMEEXROMClr;
static u32	resetCounter, c64CycleCount;
static u32	disableCart, transferStarted, currentOfs;

u32 prgSize;
unsigned char prgData[ 65536 ] AAA;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static unsigned char launchCode[ 65536 ] AAA;


#ifdef COMPILE_MENU
void KernelLaunchRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 )
#else
void CKernelLaunch::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	if ( c128PRG )
	{
		configGAMEEXROMSet = bGAME | bEXROM | bNMI | bDMA;
		configGAMEEXROMClr = 0; 
	} else
	{
		// set GAME and EXROM as defined above (+ set NMI, DMA and latch outputs)
		configGAMEEXROMSet = bGAME | bNMI | bDMA;
		configGAMEEXROMClr = bEXROM; 
	}
	SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	#ifdef COMPILE_MENU
	if ( screenType == 0 )
	{
		splashScreen( sidekick_launch_oled );
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

	// read launch code and .PRG
	u32 size;
	if ( c128PRG )
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM128, launchCode, &size ); else
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, launchCode, &size );

	#ifdef COMPILE_MENU
	if ( !hasData )
	{
		readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData, &prgSize );
	} else
	{
		prgSize = prgSizeExt;
		memcpy( prgData, prgDataExt, prgSize );
	}
	#else
	readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData, &prgSize );
	#endif


	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	// warm caches

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], 65536, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( prgData, prgSize, 65536 * 8 );

	// launch code / CBM80
	CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 8192, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( launchCode, 8192, 65536 * 8 );

	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 1024 );
	FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 1024 );

	c64CycleCount = resetCounter = 0;
	disableCart = transferStarted = currentOfs = 0;

	// ready to go
	latchSetClearImm( LATCH_RESET, 0 );

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
void KernelLaunchFIQHandler( void *pParam )
#else
void CKernelLaunch::FIQHandler (void *pParam)
#endif
{
	register u32 D;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// update some counters
	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( resetCounter > 3  )
	{
		disableCart = transferStarted = 0;
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
		FINISH_BUS_HANDLING
		return;
	}

	if ( disableCart )
	{
		FINISH_BUS_HANDLING
		return;
	}

	if ( IO1_ACCESS ) 
	{
		if ( CPU_WRITES_TO_BUS ) 
		{
			// any write to IO1 will (re)start the PRG transfer
			currentOfs = 0;
			transferStarted = 1;
			FINISH_BUS_HANDLING
			return;
		} else
		// if ( CPU_READS_FROM_BUS ) 
		{
			if ( GET_IO12_ADDRESS == 1 )	
				// $DE01 -> get number of 256-byte pages
				D = ( prgSize + 255 ) >> 8; else
				// $DE00 -> get next byte
				D = prgData[ currentOfs++ ];
				
			WRITE_D0to7_TO_BUS( D )
			FINISH_BUS_HANDLING
			return;
		}
	}

	if ( CPU_WRITES_TO_BUS && IO2_ACCESS ) // writing #123 to $df00 (IO2) will disable the cartridge
	{
		READ_D0to7_FROM_BUS( D )

		if ( GET_IO12_ADDRESS == 0 && D == 123 )
		{
			disableCart = 1;
			SET_GPIO( bGAME | bEXROM | bNMI );
			FINISH_BUS_HANDLING
			return;
		}
	}

	// access to CBM80 ROM (launch code)
	if ( CPU_READS_FROM_BUS && ACCESS( ROM_LH ) )
		WRITE_D0to7_TO_BUS( launchCode[ GET_ADDRESS + LAUNCH_BYTES_TO_SKIP ] );

	FINISH_BUS_HANDLING
}

