/*
  _________.__    .___      __   .__        __       __________ ____  __.____     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \______   \    |/ _|    |    
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |       _/      < |    |    
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |    |   \    |  \|    |___ 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    |____|_  /____|__ \_______ \
        \/         \/    \/     \/       \/     \/           \/        \/       \/ 
 
 kernel_rkl.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick RKL: a merger of the Geo/NeoRAM emulation, Kernal cart, and .PRG dropper
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

#include "kernel_rkl.h"

// let them blink
//#define LED

// provide a well hidden way to differentiate Sidekick's NeoRAM from a real physical Neo/GeoRAM
#define HIDDEN_DETECTION
const u8 DETECTION_MAGIC_STRING[ 8 ] = { 83, 73, 68, 69, 75, 73, 67, 75 };

//
// launcher
//

// we will read this .PRG file
static const char DRIVE[] = "SD:";
#ifndef COMPILE_MENU
static const char FILENAME[] = "SD:C64/test.prg";		// .PRG to start
#endif
static const char FILENAME_CBM80_ULTIMAX[] = "SD:C64/launch_ultimax.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM80[] = "SD:C64/launch.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM128[] = "SD:C64/launch128.cbm80";	// launch code (CBM80 8k cart)

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_neoram.tga";

static u32 allUsedLEDs = 0;

// setting EXROM and GAME (low = 0, high = 1)
#define EXROM_ACTIVE	1
#define GAME_ACTIVE		0

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

static u32	configGAMEEXROMSet, configGAMEEXROMClr;
static u32	disableCart, ultimaxDisabled, transferStarted, currentOfs, transferPart;

extern volatile u8 forceReadLaunch;

static u32 launchPrg = 0, hasKernal = 0;
static u32 prgSize;
static unsigned char prgData[ 65536 ] AAA;
static u32 startAddr, prgSizeAboveA000, prgSizeBelowA000, endAddr;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static unsigned char launchCode[ 65536 ] AAA;

//
// Kernal
//
static unsigned char kernalROM[ 65536 ];


//
// GeoRAM
//

// size of GeoRAM/NeoRAM in Kilobytes
#define MAX_GEORAM_SIZE 4096
u32 geoSizeKB = 4096;

typedef struct
{
	// GeoRAM registers
	// $dffe : selection of 256 Byte-window in 16 Kb-block
	// $dfff : selection of 16 Kb-nlock
	u8  reg[ 2 ];	

	// for rebooting the RPi
	u64 c64CycleCount;
	u32 resetCounter;
	u32 nBytesRead, stage;

	u8 *RAM AA;

	u32 saveRAM, releaseDMA;

	u8 skHiddenDetectionPos[ 10 ];
	u8 skHiddenDetectionMagic[ 8 ];

	u8 padding[ 52 - 10 ];
} __attribute__((packed)) GEOSTATE;

volatile static GEOSTATE geo AAA;

// geoRAM memory pool 
static u8  geoRAM_Pool[ MAX_GEORAM_SIZE * 1024 + 128 ] AA;

// u8* to current window
#define GEORAM_WINDOW (&geo.RAM[ ( geo.reg[ 1 ] * 16384 ) + ( geo.reg[ 0 ] * 256 ) ])

// geoRAM helper routines
static void geoRAM_Init()
{
	geo.reg[ 0 ] = geo.reg[ 1 ] = 0;
	geo.RAM = (u8*)( ( (u64)&geoRAM_Pool[0] + 128 ) & ~127 );
	memset( geo.RAM, 0, geoSizeKB * 1024 );

	geo.c64CycleCount = 0;
	geo.resetCounter = 0;
	geo.nBytesRead = 0;
	geo.stage = 0;
	geo.saveRAM = 0;
	geo.releaseDMA = 0;

	geo.skHiddenDetectionPos[ 0 ] = 0;
	geo.skHiddenDetectionPos[ 1 ] = 0;

	memcpy( (void*)geo.skHiddenDetectionMagic, DETECTION_MAGIC_STRING, 8 );
}

__attribute__( ( always_inline ) ) inline u8 geoRAM_IO2_Read( u32 A )
{
    if ( A < 2 )
		return geo.reg[ A & 1 ];

	#ifdef HIDDEN_DETECTION
	if ( geo.skHiddenDetectionPos[ 1 ] && A >= 0x58 && A <= 0x5f )
	{
		if ( (0x5f - A + 1) == geo.skHiddenDetectionPos[ 1 ] ++ )
			return geo.skHiddenDetectionMagic[ 0x5f - A ];
	}
	#endif
	
	return 0;
}

__attribute__( ( always_inline ) ) inline void geoRAM_IO2_Write( u32 A, u8 D )
{
	if ( ( A & 1 ) == 1 )
		geo.reg[ 1 ] = D & ( ( geoSizeKB / 16 ) - 1 ); else
		geo.reg[ 0 ] = D & 63;

	#ifdef HIDDEN_DETECTION
	if ( A >= 0x50 && A < 0x58 )
	{	
		if ( A == 0x50 )
			geo.skHiddenDetectionPos[ 0 ] = 0;

		if ( geo.skHiddenDetectionMagic[ A - 0x50 ] == D &&  ++ geo.skHiddenDetectionPos[ 0 ] == 8 )
			geo.skHiddenDetectionPos[ 1 ] = 1;
	} else
		geo.skHiddenDetectionPos[ 1 ] = 0;
	#endif
}

static void saveGeoRAM( const char *FILENAME_RAM )
{
	// lazy, we always store max size files
	writeFile( logger, DRIVE, FILENAME_RAM, geo.RAM, geoSizeKB * 1024 );
}

#ifdef COMPILE_MENU
static void KernelRKLFIQHandler( void *pParam );

void KernelRKLRun( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME_KERNAL, const char *FILENAME, const char *FILENAME_RAM, u32 sizeRAM, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 )
#else
void CKernelRKL::Run( void )
#endif
{
	// setup control lines, initialize latch and software I2C buffer
	initLatch();
	latchSetClearImm( LATCH_ENABLE_KERNAL, LATCH_RESET );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	if ( screenType == 0 )
	{
		allUsedLEDs = LATCH_LED_ALL;
		splashScreen( raspi_ram_splash );
	} else
	if ( screenType == 1 )
	{
		allUsedLEDs = LATCH_LED0to1;

		char fn[ 1024 ];
		// attention: this assumes that the filename ending is always ".crt"!
		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, ".tga" );

		logger->Write( "RaspiFlash", LogNotice, "trying to load: '%s'", fn );

		if ( tftLoadBackgroundTGA( (char*)DRIVE, fn ) )
		{
			tftCopyBackground2Framebuffer();
		} else
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

			//
			//u32 c1 = rgb24to16( 166, 250, 128 );
			//u32 c2 = rgb24to16( 98, 216, 204 );
			u32 c3 = rgb24to16( 233, 114, 36 );

			char b1[64];
			if ( sizeRAM < 1024 )
				sprintf( b1, "size: %dkb", sizeRAM ); else
				sprintf( b1, "size: %dmb", sizeRAM/1024 ); 

			u32 charWidth = 16;
			u32 l = strlen( b1 );
			if ( l * 16 >= 240 )
				charWidth = 13;
	
			u32 sx = max( 0, ( 240 - charWidth * l ) / 2 - 1 );
			tftPrint( b1, sx, 204, c3, charWidth == 16 ? 0 : -3 );	
		}

		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	} 

	// GeoRAM initialization
	geoSizeKB = sizeRAM;
	geoRAM_Init();

	if ( FILENAME_RAM )
	{
		u32 size;
		readFile( logger, DRIVE, FILENAME_RAM, geo.RAM, &size );
		geoSizeKB = size / 1024;
	}

	// read launch code and .PRG

	// program to launch might be from D64 -> support handing over data directly
	if ( FILENAME == NULL && !hasData )
	{
		launchPrg = 0;
	} else
	{
		launchPrg = 1;
		if ( !hasData )
		{
			readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData, &prgSize );
		} else
		{
			prgSize = prgSizeExt;
			memcpy( prgData, prgDataExt, prgSize );
		}

		startAddr = prgData[ 0 ] + prgData[ 1 ] * 256;
		endAddr = startAddr + prgSize - 2;
		prgSizeBelowA000 = 0xa000 - startAddr;
		if ( prgSizeBelowA000 > prgSize - 2 )
		{
			prgSizeBelowA000 = prgSize - 2;
			prgSizeAboveA000 = 0;
		} else
			prgSizeAboveA000 = prgSize - prgSizeBelowA000;
	}


	u32 kernalSize = 0; // should always be 8192
	if ( FILENAME_KERNAL != NULL && c128PRG == 0 )
	{
		hasKernal = 1;
		readFile( logger, (char*)DRIVE, (char*)FILENAME_KERNAL, kernalROM, &kernalSize );
	} else
		hasKernal = 0;

	u32 size;
	if ( launchPrg )
	{
		// if kernal
		if ( hasKernal )
		{
			// we need to start with ultimax mode
			ultimaxDisabled = 0;
			configGAMEEXROMSet = bNMI | bDMA;
			configGAMEEXROMClr = bGAME | bEXROM | bCTRL257; 
			SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );

			readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80_ULTIMAX, launchCode, &size );
		} else
		{
			// use 8k cart launcher
			if ( c128PRG )
			{
				readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM128, launchCode, &size );
				ultimaxDisabled = 1;
				configGAMEEXROMSet = bGAME | bEXROM | bNMI | bDMA;
				configGAMEEXROMClr = bCTRL257; 
				SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );
			} else
			{
				readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, launchCode, &size );
				ultimaxDisabled = 1;
				configGAMEEXROMSet = bGAME | bNMI | bDMA;
				configGAMEEXROMClr = bEXROM | bCTRL257; 
				SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );
			}
		}
	} else
	{
		ultimaxDisabled = 1;
		configGAMEEXROMSet = bGAME | bEXROM | bNMI | bDMA;
		configGAMEEXROMClr = bCTRL257; 
		SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );
	}

	disableCart = transferStarted = currentOfs = 0;
	transferPart = 1;
	geo.c64CycleCount = geo.resetCounter = 0;
	geo.nBytesRead = 0;
	geo.stage = 1;

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	// warm caches

	for ( u32 i = 0; i < 4; i++ )
	{
		// kernal
		CACHE_PRELOAD_DATA_CACHE( kernalROM, 8192, CACHE_PRELOADL2KEEP )

		if ( launchPrg )
		{
			// .PRG data
			CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], prgSize, CACHE_PRELOADL2KEEP )
			FORCE_READ_LINEAR32a( prgData, prgSize, prgSize * 8 );

			// launch code / CBM80
			CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 512, CACHE_PRELOADL2KEEP )
			FORCE_READ_LINEAR32a( launchCode, 512, 512 * 16 );

			for ( u32 i = 0; i < prgSizeAboveA000; i++ )
				forceReadLaunch = prgData[ prgSizeBelowA000 + i + 2 ];

			for ( u32 i = 0; i < prgSizeBelowA000 + 2; i++ )
				forceReadLaunch = prgData[ i ];
		}

		// FIQ handler
		CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 3*1024 );

		FORCE_READ_LINEAR32a( kernalROM, 8192, 8192 * 8 );

		FORCE_READ_LINEAR32a( (void*)&FIQ_HANDLER, 3*1024, 3*1024*8 );
	}

	// ready to go
	DELAY(10000);
	if ( hasKernal )
		latchSetClearImm( LATCH_RESET | LATCH_ENABLE_KERNAL, 0 ); else
		latchSetClearImm( LATCH_RESET, LATCH_ENABLE_KERNAL ); 

	while ( true )
	{
		if ( ( ( geo.stage == 1 ) && geo.nBytesRead > 0 ) ||
			 ( ( geo.stage == 2 ) && geo.nBytesRead < 32 && geo.c64CycleCount > 1000000 ) )
		{
			if ( geo.stage == 1 ) geo.stage = 2; else geo.stage = 0;
			latchSetClearImm( 0, LATCH_RESET );
			DELAY(1<<10);
			latchSetClearImm( LATCH_RESET, 0 );
		}

		TEST_FOR_JUMP_TO_MAINMENU_CB( geo.c64CycleCount, geo.resetCounter, saveGeoRAM(FILENAME_RAM) )

		if ( geo.saveRAM )
		{
			saveGeoRAM( FILENAME_RAM );
			geo.saveRAM = 0;
		}

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

u32 c64_state_01 = 0;

//static u32 deferredWrite = 0;

#ifdef COMPILE_MENU
static void KernelRKLFIQHandler( void *pParam )
#else
void CKernelRKL::FIQHandler( void *pParam )
#endif
{
	register u32 D;
	register u8 payload;

/*	if ( deferredWrite )
	{
		GEORAM_WINDOW[ ( deferredWrite >> 8 ) & 255 ] = deferredWrite & 255;
		deferredWrite = 0;
		return;
	}*/

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// let's use this time to preload the cache (L1-cache, streaming)
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 0 ] );
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 64 ] );
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 128 ] );
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 192 ] );
	//CACHE_PRELOADL1STRM( &GEORAM_WINDOW[ GET_IO12_ADDRESS ] );
	payload = GEORAM_WINDOW[ GET_IO12_ADDRESS ];

	// ... and update some counters
	UPDATE_COUNTERS_MIN( geo.c64CycleCount, geo.resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( ( disableCart || ultimaxDisabled ) && ROMH_ACCESS && KERNAL_ACCESS )
	{
		WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );
		FINISH_BUS_HANDLING
		return;
	}
					
		
#if 1
	// launch -->
	if ( launchPrg )
	{
		if ( geo.resetCounter > 3  )
		{
			disableCart = transferStarted = 0;
			ultimaxDisabled = 0;
			SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );

			FINISH_BUS_HANDLING
			return;
		}

		if ( !disableCart )
		{
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
					if ( GET_IO12_ADDRESS == 4 ) // full 256-byte pages 
					{
						D = ( prgSize - 2 ) >> 8;
						WRITE_D0to7_TO_BUS( D )
						CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
						FINISH_BUS_HANDLING
						forceReadLaunch = prgData[ currentOfs ];
					} else
					if ( GET_IO12_ADDRESS == 5 ) // bytes on last non-full 256-byte page
					{
						D = ( prgSize - 2 ) & 255;
						WRITE_D0to7_TO_BUS( D )
						CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
						FINISH_BUS_HANDLING
						forceReadLaunch = prgData[ currentOfs ];
					} else
					if ( GET_IO12_ADDRESS == 2 )	
					{
						// $DE02 -> get BASIC end address
						WRITE_D0to7_TO_BUS( (u8)( endAddr & 255 ) )
						FINISH_BUS_HANDLING
					} else
					if ( GET_IO12_ADDRESS == 3 )	
					{
						// $DE02 -> get BASIC end address
						WRITE_D0to7_TO_BUS( (u8)( (endAddr>>8) & 255 ) )
						FINISH_BUS_HANDLING
					} else
					{
						// $DE00 -> get next byte
						D = forceReadLaunch;	currentOfs ++;
						WRITE_D0to7_TO_BUS( D )
						CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
						FINISH_BUS_HANDLING
						forceReadLaunch = prgData[ currentOfs ];
					}
				
					return;
				}
			}

			if ( CPU_WRITES_TO_BUS && IO2_ACCESS ) // writing #123 to $df00 (IO2) will disable the cartridge
			{
				READ_D0to7_FROM_BUS( D )

				if ( GET_IO12_ADDRESS == 0 && D == 1 )
				{
					SET_GPIO( bGAME | bEXROM | bNMI );
					ultimaxDisabled = 1;
					FINISH_BUS_HANDLING
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 0 ] );
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 64 ] );
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 128 ] );
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 192 ] );
					return;
				}
				if ( GET_IO12_ADDRESS == 0 && D == 123 )
				{
					LED_ON( 1 );
					disableCart = 1;
					SET_GPIO( bGAME | bEXROM | bNMI );
					FINISH_BUS_HANDLING
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 0 ] );
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 64 ] );
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 128 ] );
					CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 192 ] );
					return;
				}
			}

			// access to CBM80 ROM (launch code)
			if ( CPU_READS_FROM_BUS && ROML_ACCESS )
			{
				WRITE_D0to7_TO_BUS( launchCode[ GET_ADDRESS + LAUNCH_BYTES_TO_SKIP ] );
				geo.nBytesRead ++;
			}

			if ( !ultimaxDisabled )
			{
				if ( CPU_READS_FROM_BUS && ROMH_ACCESS )
				{
					WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );
					geo.nBytesRead ++;
				}
			} 

			FINISH_BUS_HANDLING
			return;
		}
		// <-- launch
	}
#endif

	if ( IO1_OR_IO2_ACCESS )
	{
		if ( CPU_READS_FROM_BUS )	// CPU reads from memory page or register
		{
			if ( IO1_ACCESS )	
				// GeoRAM read from memory page
				D = payload; /* GEORAM_WINDOW[ GET_IO12_ADDRESS ];*/ else
				// GeoRAM read register (IO2_ACCESS)
				D = geoRAM_IO2_Read( GET_IO12_ADDRESS );

			// write D0..D7 to bus
			WRITE_D0to7_TO_BUS( D )
		} else
		// CPU writes to memory page or register // CPU_WRITES_TO_BUS is always true here
		{
			// read D0..D7 from bus
			READ_D0to7_FROM_BUS( D )

			if ( IO1_ACCESS )	
			{
				// GeoRAM write to memory page
				GEORAM_WINDOW[ GET_IO12_ADDRESS ] = D; 
				//deferredWrite = (1<<24) | ((GET_IO12_ADDRESS)<<8) | D;
			} else
			{
				// GeoRAM write register (IO2_ACCESS)
				geoRAM_IO2_Write( GET_IO12_ADDRESS, D );
				CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 0 ] );
				CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 64 ] );
				CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 128 ] );
				CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 192 ] );
				/*WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
				CLR_GPIO( bDMA ); 
				geo.releaseDMA = NUM_DMA_CYCLES;
				FINISH_BUS_HANDLING
				FORCE_READ_LINEAR32a( GEORAM_WINDOW, 256, 512 );
				return;*/
				FORCE_READ_LINEAR32_SKIP( GEORAM_WINDOW, 256 );
			}
		}

		#ifdef LED
		// turn off one of the 4 LEDs indicating reads and writes from/to the GeoRAM register and memory pages
		LED_ON( ( IO2_ACCESS ? 1 : 0 ) + ( CPU_WRITES_TO_BUS ? 2 : 0 ) )
		#endif
	}

	/*if ( BUTTON_PRESSED )
	{
		geo.saveRAM = 1;
		WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
		CLR_GPIO( bDMA ); 
		geo.releaseDMA = NUM_DMA_CYCLES;
		FINISH_BUS_HANDLING
		return;
	}

	if ( geo.saveRAM == 0 && geo.releaseDMA > 0 && --geo.releaseDMA == 0 )
	{
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
		FINISH_BUS_HANDLING
		return;
	} else*/
	/*if ( geo.releaseDMA > 0 && --geo.releaseDMA == 0 )
	{
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
		FINISH_BUS_HANDLING
		return;
	} */

	#ifdef LED
	if ( screenType == 0 )
		CLEAR_LEDS_EVERY_8K_CYCLES
	#endif

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

