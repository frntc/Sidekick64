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
static u32	disableCart, transferStarted, currentOfs, transferPart;

u32 prgSize;
unsigned char prgData[ 65536 ] AAA;
static u32 startAddr, prgSizeAboveA000, prgSizeBelowA000;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static unsigned char launchCode[ 65536 ] AAA;

static u32 resetFromCodeState = 0;
static u32 _playingPSID = 0;

extern volatile u8 forceReadLaunch;

void prepareOnReset( bool refresh = false )
{
	if ( !refresh )
	{
		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();
	}

	if ( _playingPSID )
	{
		extern unsigned char charset[ 4096 ];
		CACHE_PRELOADL2STRM( &charset[ 2048 ] );
		FORCE_READ_LINEAR32( (void*)&charset[ 2048 ], 1024 );
	}

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], prgSize, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( prgData, prgSize, prgSize * 8 );

	// launch code / CBM80
	CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 512, CACHE_PRELOADL1KEEP )
	FORCE_READ_LINEAR32a( launchCode, 512, 512 * 16 );

	for ( u32 i = 0; i < prgSizeAboveA000; i++ )
		forceReadLaunch = prgData[ prgSizeBelowA000 + i + 2 ];

	for ( u32 i = 0; i < prgSizeBelowA000 + 2; i++ )
		forceReadLaunch = prgData[ i ];

	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 1024 );
	FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 1024 );
}


#ifdef COMPILE_MENU
void KernelLaunchRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0 )
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
	_playingPSID = playingPSID;

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

	startAddr = prgData[ 0 ] + prgData[ 1 ] * 256;
	prgSizeBelowA000 = 0xa000 - startAddr;
	if ( prgSizeBelowA000 > prgSize - 2 )
	{
		prgSizeBelowA000 = prgSize - 2;
		prgSizeAboveA000 = 0;
	} else
		prgSizeAboveA000 = prgSize - prgSizeBelowA000;

	resetFromCodeState = 0;

	// setup FIQ
	prepareOnReset();
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	c64CycleCount = resetCounter = 0;
	disableCart = transferStarted = currentOfs = 0;
	transferPart = 1;

	// warm caches
	prepareOnReset( true );
	DELAY(1<<18);
	prepareOnReset( true );
	DELAY(1<<18);
	prepareOnReset( true );

	// ready to go
	latchSetClear( LATCH_RESET, 0 );

	c64CycleCount = resetCounter = 0;
	disableCart = transferStarted = currentOfs = 0;
	transferPart = 1;
	CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 ] );
	CACHE_PRELOADL2KEEP( &prgData[ 0 ] );

	// wait forever
	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( c64CycleCount, resetCounter )

		if ( resetFromCodeState == 2 )
		{
			EnableIRQs();
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			return;		
		}
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

	if ( resetCounter > 3 && resetFromCodeState != 2 )
	{
		disableCart = transferStarted = 0;
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
		FINISH_BUS_HANDLING
		return;
	}

	if ( disableCart )
	{
		if ( _playingPSID )
		{
			if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x55 )
			{
				static u32 oc = 0;
				extern unsigned char charset[ 4096 ];
				u32 D = charset[ 2048 + oc ];
				oc ++; oc &= 1023;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2STRM( &charset[ 2048 + oc ] );
			}
			if ( resetFromCodeState == 0 && IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x11 )
			{
				READ_D0to7_FROM_BUS( D )
				if ( D == 0x22 )
					resetFromCodeState = 1;
			}
			if ( resetFromCodeState == 1 && IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x33 )
			{
				READ_D0to7_FROM_BUS( D )
				if ( D == 0x44 )
				{
					resetFromCodeState = 2;
					latchSetClear( 0, LATCH_RESET );
				}
			}
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		}
	}

	// access to CBM80 ROM (launch code)
	if ( CPU_READS_FROM_BUS && ACCESS( ROM_LH ) )
		WRITE_D0to7_TO_BUS( launchCode[ GET_ADDRESS + LAUNCH_BYTES_TO_SKIP ] );

	if ( IO1_ACCESS ) 
	{
		if ( CPU_WRITES_TO_BUS ) 
		{
			transferStarted = 1;

			// any write to IO1 will (re)start the PRG transfer
			if ( GET_IO12_ADDRESS == 2 )
			{
				currentOfs = prgSizeBelowA000 + 2;
				transferPart = 1; 
				CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ prgSizeBelowA000 + 2 ];
			} else
			{
				currentOfs = 0;
				transferPart = 0;
				CACHE_PRELOADL2KEEP( &prgData[ 0 ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ 0 ];
			}
			return;
		} else
		// if ( CPU_READS_FROM_BUS ) 
		{
			if ( GET_IO12_ADDRESS == 1 )	
			{
				// $DE01 -> get number of 256-byte pages
				if ( transferPart == 1 ) // PRG part above $a000
					D = ( prgSizeAboveA000 + 255 ) >> 8;  else
					D = ( prgSizeBelowA000 + 255 ) >> 8; 
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			{
				// $DE00 -> get next byte
/*				if ( transferPart == 1 ) // PRG part above $a000
				{
					//D = prgData[ prgSizeBelowA000 + 2 + currentOfs++ ]; 
					D = forceReadLaunch;	currentOfs ++;
					//D = (currentOfs ++) & 255;
					WRITE_D0to7_TO_BUS( D )
					CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 + currentOfs ] );
					FINISH_BUS_HANDLING
					forceReadLaunch = prgData[ prgSizeBelowA000 + 2 + currentOfs ];
				} else*/
				{
					D = forceReadLaunch;	currentOfs ++;
					//D = prgData[ currentOfs++ ];
					WRITE_D0to7_TO_BUS( D )
					CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
					FINISH_BUS_HANDLING
					forceReadLaunch = prgData[ currentOfs ];
				}
			}
				
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

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

