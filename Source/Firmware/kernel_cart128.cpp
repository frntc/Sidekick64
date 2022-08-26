/*
  _________.__    .___      __   .__        __       _________                __   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   ___ \_____ ________/  |_ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /    \  \/\__  \\_  __ \   __\
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    \     \____/ __ \|  | \/|  |  
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \______  (____  /__|   |__|  
        \/         \/    \/     \/       \/     \/           \/     \/             

 kernel_cart128.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - example how to implement a simple C128 cartridge and Megabit 128 ROM bank switching 
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
#include "kernel_cart128.h"

static const char DRIVE[] = "SD:";
static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_cart.tga";

// for rebooting the RPi
static u64 c128CycleCount = 0;
static u32 resetCounter = 0;

// ... flash/ROM
extern u8 *flash_cacheoptimized_pool;
static unsigned char *externalROM;

static u8 isMegabitROM = 0;
static u32 releaseDMA = 0;

#ifdef COMPILE_MENU
void KernelCart128FIQHandler( void *pParam );

void KernelCartRun128( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr )
#else
void CKernelCart128::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	u32 size;
	isMegabitROM = 0;
	externalROM = &flash_cacheoptimized_pool[ 0 ];
	memset( externalROM, 0, 32768 );
	readFile( logger, DRIVE, FILENAME, externalROM, &size );

	if ( size > 32768 )
	{
		isMegabitROM = 1;
		// we assume this is a Megabit 128 ROM
		// ... and we need to patch it to run from external function ROM instead of internal

		for ( u32 i = 0; i < size - 6; i++ )
		{
			const u8 seq1[ 5 ] = { 0xa9, 0x06, 0x8d, 0x00, 0xff };
			const u8 rep1[ 5 ] = { 0xa9, 0x0a, 0x8d, 0x00, 0xff };
			const u8 seq2[ 6 ] = { 0x8d, 0x00, 0xd7, 0x8d, 0x00, 0xd7 };
			const u8 rep2[ 6 ] = { 0x8d, 0x00, 0xdf, 0xea, 0xea, 0xea };
			const u8 seq3[ 5 ] = { 0xa2, 0x16, 0x8e, 0x01, 0xd5 };
			const u8 rep3[ 5 ] = { 0xa2, 0x2a, 0x8e, 0x01, 0xd5 };
			if ( memcmp( &externalROM[ i ], seq1, 5 ) == 0 ) memcpy( &externalROM[ i ], rep1, 5 );
			if ( memcmp( &externalROM[ i ], seq2, 6 ) == 0 ) memcpy( &externalROM[ i ], rep2, 6 );
			if ( memcmp( &externalROM[ i ], seq3, 5 ) == 0 ) memcpy( &externalROM[ i ], rep3, 5 );
		}
	}

	if ( screenType == 0 )
	{
		char fn[ 1024 ];
		// attention: this assumes that the filename ending is always ".???"!
		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, ".logo" );
		if ( !splashScreenFile( (char*)DRIVE, fn ) )
			splashScreen( raspi_cart_splash );
	} else
	if ( screenType == 1 )
	{
		char fn[ 1024 ];
		// attention: this assumes that the filename ending is always ".???"!
		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, ".tga" );

		if ( !tftLoadBackgroundTGA( (char*)DRIVE, fn ) )
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
			u32 c1 = rgb24to16( 166, 250, 128 );
			//u32 c2 = rgb24to16( 98, 216, 204 );
			u32 c3 = rgb24to16( 233, 114, 36 );
			
			char b1[512];
			int p = strlen( FILENAME ) - 1;
			while ( p > 0 && FILENAME[ p ] != '/' && FILENAME[ p ] != '\\' ) p--;
			p ++;
			if ( p > 0 )
				strcpy( b1, &FILENAME[ p ] ); else
				sprintf( b1, "ext. function ROM" );

			for ( size_t i = 0; i < strlen( b1 ); i++ )
			{
				if ( b1[i] >= 65 && b1[i] <= 90 )
				{
					b1[ i ] += 97-65;
				} else
				if ( b1[i] >= 97 && b1[i] <= 122 )
				{
					b1[ i ] -= 97-65;
				} 
			}
			int charWidth = 16;
			int l = strlen( b1 );
			if ( l * 16 >= 240 )
				charWidth = 13;
			if ( l * 13 >= 240 )
				b1[ 18 ] = 0;
	
			int sx = max( 0, ( 240 - charWidth * l ) / 2 - 1 );
			tftPrint( b1, sx, 220-16, c3, charWidth == 16 ? 0 : -3 );	

			if ( isMegabitROM )
			{
				sprintf( b1, "mEGABIT 128 rom" );
				int charWidth = 13;
				int l = strlen( b1 );
				int sx = max( 0, ( 240 - charWidth * l ) / 2 - 1 );
				tftPrint( b1, sx, 220-16-20, c1, charWidth == 16 ? 0 : -3 );	
			}
		}

		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	} 

	// set GAME and EXROM high = inactive (+ set NMI, DMA and latch outputs)
	u32 set = bNMI | bDMA | bEXROM | bGAME, clr = 0;
	SETCLR_GPIO( set, clr )

	DisableIRQs();

	// preload caches, and try to convince the RPi that we really want the data to reside in caches (by reading it again)
	SyncDataAndInstructionCache();
	CACHE_PRELOAD_DATA_CACHE( &externalROM[ 0 ], 32768, CACHE_PRELOADL2KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 2536 );
	FORCE_READ_LINEAR32a( externalROM, 32768, 32768 * 4 );
	FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 2536 );

	// setup FIQ
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );
	m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );

	latchSetClearImm( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );

	c128CycleCount = resetCounter = 0;
	releaseDMA = 0;

	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU2FIQs( c128CycleCount, resetCounter )
		#endif

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

#ifdef COMPILE_MENU
void KernelCart128FIQHandler( void *pParam )
#else
void CKernelCart128::FIQHandler( void *pParam )
#endif
{
	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// update some counters
	UPDATE_COUNTERS_MIN( c128CycleCount, resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	u32 addr = GET_ADDRESS0to13;

	if ( ROMH_ACCESS )
		addr += 16384;

	static u32 bank = 0;

	if ( CPU_READS_FROM_BUS && ROML_OR_ROMH_ACCESS )
	{
		WRITE_D0to7_TO_BUS( externalROM[ addr + bank ] );
	}

	// implementation of the Megabit ROM bank switching
	if ( isMegabitROM )
	{
		if ( CPU_RESET ) 
		{
			if ( bank != 0 )
			{
				bank = 0;
				CACHE_PRELOAD_DATA_CACHE( &externalROM[ 0 ], 32768, CACHE_PRELOADL2STRM )
				FORCE_READ_LINEAR64( &externalROM[ 0 ], 32768 )
			}
		}

		if ( CPU_WRITES_TO_BUS && IO2_ACCESS && GET_IO12_ADDRESS == 0x00 )
		{
			u8 D;
			READ_D0to7_FROM_BUS( D )
			bank = (u32)(D&31) * 32768;

			// cache prefetching
			WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
			CLR_GPIO( bDMA ); 
			releaseDMA = NUM_DMA_CYCLES + 10000;
			CACHE_PRELOAD_DATA_CACHE( &externalROM[ bank ], 32768, CACHE_PRELOADL2STRM )
			FORCE_READ_LINEAR64( &externalROM[ bank ], 32768 * 20 )
		}

		if ( releaseDMA > 0 && --releaseDMA == 0 )
		{
			WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
			SET_GPIO( bDMA ); 
		}
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

