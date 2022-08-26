/*
  _________.__    .___      __   .__        __       __________.____    ________  ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \______   \    |   \_____  \/  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |       _/    |    /  ____/   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |    |   \    |___/       \  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    |____|_  /_______ \_______ \_____  /\____   | 
        \/         \/    \/     \/       \/     \/           \/        \/       \/     \/      |__| 

 kernel_ramlaunch264.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick RAM Launch 264: a merger of an experimental Geo/NeoRAM emulation and .PRG dropper
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


#include "kernel_ramlaunch264.h"

static const char DRIVE[] = "SD:";

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk264_neocrt.tga";
static const char FILENAME_SPLASH_RGB_RAMONLY[] = "SD:SPLASH/sk64_neoram.tga";
// let them blink
//#define LED

//
// GeoRAM
//
#define REGISTER_BASE 0xfde8

// size of GeoRAM/NeoRAM in Kilobytes
#define MAX_GEORAM_SIZE 4096
u32 geoSizeKB = 4096;

typedef struct
{
	// GeoRAM registers
	// $fde8 : selection of 256 Byte-window in 16 Kb-block
	// $fde9 : selection of 16 Kb-nlock
	// $fdea : bit 7 indicates if lower or higher 128-byte of GeoRAM window is mapped to $fe00-fe7f
	u8 reg[ 3 + 1 ];	

	// for rebooting the RPi
	u64 c64CycleCount;
	u32 resetCounter, resetPressed, resetReleased;

	u8 *RAM AA;

	u32 saveRAM, releaseDMA;

	u16 nextAddr;
	u8 nextByte;

	u8 padding[ 23 ];
} __attribute__((packed)) GEOSTATE;

volatile static GEOSTATE geo AAA;

// geoRAM memory pool 
static u8  geoRAM_Pool[ MAX_GEORAM_SIZE * 1024 + 128 ] AA;

// u8* to current window
#define GEORAM_WINDOW (&geo.RAM[ ( geo.reg[ 1 ] * 16384 ) + ( geo.reg[ 0 ] * 256 ) ])

// hack
static u32 h_nRegOffset;
static u32 h_nRegMask;

// geoRAM helper routines
static void geoRAM_Init()
{
	geo.reg[ 0 ] = geo.reg[ 1 ] = geo.reg[ 2 ] = 0;
	geo.RAM = (u8*)( ( (u64)&geoRAM_Pool[0] + 128 ) & ~127 );
	memset( geo.RAM, 0, geoSizeKB * 1024 );

	geo.c64CycleCount = 0;
	geo.resetCounter = 0;
	geo.saveRAM = 0;
	geo.releaseDMA = 0;
	geo.nextByte = 0;
	geo.nextAddr = 0xffff;
}

__attribute__( ( always_inline ) ) inline u8 geoRAM_IO2_Read( u32 A )
{
    if ( A < 2 )
		return geo.reg[ A & 1 ];
	return 0;
}

__attribute__( ( always_inline ) ) inline void geoRAM_IO2_Write( u32 A, u8 D )
{
	if ( ( A & 1 ) == 1 )
		geo.reg[ 1 ] = D & ( ( geoSizeKB / 16 ) - 1 ); else
		geo.reg[ 0 ] = D & 63;
}

static void saveGeoRAM( const char *FILENAME_RAM )
{
	// lazy, we always store max size files
	writeFile( logger, DRIVE, FILENAME_RAM, geo.RAM, MAX_GEORAM_SIZE * 1024 );
}

#ifdef COMPILE_MENU
static void KernelRLFIQHandler( void *pParam );

void KernelRLRun( CGPIOPinFIQ2	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME_KERNAL, const char *FILENAME, const char *FILENAME_RAM, u32 sizeRAM, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 )
#else
void CKernelRL::Run( void )
#endif
{

	#ifndef COMPILE_MENU
	// setup control lines, initialize latch and software I2C buffer
	initLatch();
	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	m_EMMC.Initialize();
	#endif

	if ( screenType == 0 )
	{
		char fn[ 1024 ];
		// attention: this assumes that the filename ending is always ".neocrt"!
		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME_RAM, strlen( FILENAME_RAM ) - 7 );
		strcat( fn, ".logo" );
		if ( !splashScreenFile( (char*)DRIVE, fn ) )
			splashScreen( raspi_ram_splash );
	} else
	if ( screenType == 1 )
	{
		char fn[ 2048 ];
		// attention: this assumes that the filename ending is always ".neocrt"!
		//memset( fn, 0, 1024 );
		strncpy( fn, FILENAME_RAM, strlen( FILENAME_RAM ) - 7 );
		strcat( fn, ".tga" );
		logger->Write( "ramlaunch", LogNotice, "looking for: %s", fn );

		if ( !tftLoadBackgroundTGA( (char*)DRIVE, fn ) )
		{
			if ( strstr( FILENAME_RAM, ".neocrt" ) == 0 )
				tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB_RAMONLY, 8 ); else
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
	// GeoRAM initialization
	geoSizeKB = sizeRAM;
	geoRAM_Init();

	if ( FILENAME_RAM )
	{
		u32 size;
		//logger->Write( "ramlaunch", LogNotice, "ram image file name: %s", FILENAME_RAM );
		readFile( logger, DRIVE, FILENAME_RAM, geo.RAM, &size );
	}

	// launch code and .PRG
	#ifdef COMPILE_MENU
	if ( FILENAME == NULL && !hasData )
	{
		launchPrg_l264 = 0;
		disableCart_l264 = 1;
		//logger->Write( "", LogNotice, "launch nothing" );
	} else
	{
		launchPrg_l264 = 1;
		if ( launchGetProgram( FILENAME, hasData, prgDataExt, prgSizeExt ) )
			launchInitLoader( false ); else
			launchPrg_l264 = 0;
		//logger->Write( "", LogNotice, "launch '%s' (%d bytes)", FILENAME, prgSizeExt );
	}
	#endif
	for ( u32 i = prgSize_l264; i > 0; i-- )
		prgData_l264[ i ] = prgData_l264[ i - 1 ];
	prgData_l264[0] = ( ( prgSize_l264 + 255 ) >> 8 );
	int startAddr = prgData_l264[ 1 ] + prgData_l264[ 2 ] * 256;
	prgData_l264[ prgData_l264[0] * 256 + 3 ] = ( startAddr + prgSize_l264 - 2 ) >> 8;
	prgData_l264[ prgData_l264[0] * 256 + 3 + 1 ] = ( startAddr + prgSize_l264 - 2 ) & 255;

	disableCart_l264 = transferStarted_l264 = currentOfs_l264 = 0;
	geo.c64CycleCount = geo.resetCounter = 0;
	geo.resetPressed = 0;
	geo.resetReleased = 0;

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );

	h_nRegOffset = m_InputPin.nRegOffset();
	h_nRegMask = m_InputPin.nRegMask();

	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	// warm caches
	SyncDataAndInstructionCache();
	launchPrepareAndWarmCache264();

	for ( u32 i = 0; i < 4; i++ )
	{
		// FIQ handler
		CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 3*1024 );
		CACHE_PRELOAD_DATA_CACHE( &geo, 64, CACHE_PRELOADL1KEEP );

		FORCE_READ_LINEAR32a( (void*)&FIQ_HANDLER, 3*1024, 3*1024*8 );
		FORCE_READ_LINEAR32a( &geo, 64, 64*16 );
	}

	// ready to go
	DELAY(10000);
	latchSetClearImm( LATCH_RESET, LATCH_ENABLE_KERNAL ); 

	while ( true )
	{
		TEST_FOR_JUMP_TO_MAINMENU_CB( geo.c64CycleCount, geo.resetCounter, saveGeoRAM(FILENAME_RAM) )

		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

#ifdef COMPILE_MENU
static void KernelRLFIQHandler( void *pParam )
#else
void CKernelRL::FIQHandler( void *pParam )
#endif
{
	register u32 D;

	#ifdef COMPILE_MENU
	if ( !disableCart_l264 && launchPrg_l264 )
	{
		LAUNCH_FIQ( geo.c64CycleCount, geo.resetCounter,  h_nRegOffset, h_nRegMask )
	}
	#endif

	//START_AND_READ_EXPPORT264
	register u32 g2, g3, addr2;				
	BEGIN_CYCLE_COUNTER						
	write32( ARM_GPIO_GPSET0, bCTRL257 );	
	g2 = read32( ARM_GPIO_GPLEV0 );			
	addr2 = ( g2 >> A0 ) & 255;				

	// some optimizations such as early-reads in hope of getting all run stable on a RPi Zero 2 -- not yet there.
	if ( CPU_READS_FROM_BUS )
	{
		if ( geo.nextAddr == geo.reg[2] + (addr2 & 127) )
		{
			D = geo.nextByte;
		} else
		{
			D = GEORAM_WINDOW[ geo.reg[2] + (addr2 & 127) ];
		}
	}

	g3 = read32( ARM_GPIO_GPLEV0 );			
	addr2 |= (( g3 >> A8 ) & 255)<<8;		

	if ( CPU_READS_FROM_BUS && ( GET_ADDRESS264 >= 0xfe00 && GET_ADDRESS264 <= 0xfe7f ) )
	{
		WRITE_D0to7_TO_BUS( D );
		geo.nextByte = GEORAM_WINDOW[ geo.reg[2] + ((addr2+1) & 127) ];
		geo.nextAddr = geo.reg[2] + ((addr2+1) & 127);
	} else
	if ( CPU_WRITES_TO_BUS && ( GET_ADDRESS264 >= 0xfe00 && GET_ADDRESS264 <= 0xfe7f ) )
	{
		READ_D0to7_FROM_BUS( D )
		GEORAM_WINDOW[ geo.reg[2] + (addr2 & 127) ] = D;
	} else
	if ( CPU_READS_FROM_BUS && ( GET_ADDRESS264 >= REGISTER_BASE && GET_ADDRESS264 <= REGISTER_BASE+2 ) )
	{
		WRITE_D0to7_TO_BUS( geo.reg[ addr2 - REGISTER_BASE ] );
	} else
	// write to registers
	if ( CPU_WRITES_TO_BUS && ( GET_ADDRESS264 >= REGISTER_BASE && GET_ADDRESS264 <= REGISTER_BASE+2 ) )
	{
		READ_D0to7_FROM_BUS( D )
		if ( addr2 == REGISTER_BASE )
			geo.reg[ 0 ] = D & 63;
		if ( addr2 == REGISTER_BASE+1 )
			geo.reg[ 1 ] = D & ( ( geoSizeKB / 16 ) - 1 );
		if ( addr2 == REGISTER_BASE+2 )
			geo.reg[ 2 ] = D & 128;
		CACHE_PRELOADL2KEEP( GEORAM_WINDOW[ geo.reg[2] ] );
		CACHE_PRELOADL2KEEP( GEORAM_WINDOW[ geo.reg[2] + 64 ] );
		CACHE_PRELOADL2KEEP( GEORAM_WINDOW[ geo.reg[2] + 32 ] );
		CACHE_PRELOADL2KEEP( GEORAM_WINDOW[ geo.reg[2] + 64 + 32 ] );
		geo.nextByte = GEORAM_WINDOW[ geo.reg[2] ];
		geo.nextAddr = 0;
	} else
	// this is an ugly hack to signal the menu code that it can reset (only necessary on a +4)
	if ( BUS_AVAILABLE264 && CPU_READS_FROM_BUS && GET_ADDRESS264 >= 0xfd90 && GET_ADDRESS264 <= 0xfd97 )
	{
		PUT_DATA_ON_BUS_AND_CLEAR257( GET_ADDRESS264 - 0xfd90 + 1 )
	}

	PeripheralEntry();
	write32( ARM_GPIO_GPEDS0 + h_nRegOffset, h_nRegMask );
	PeripheralExit();

	static u8 updCache = 0;
	CACHE_PRELOADL2KEEP( GEORAM_WINDOW[ geo.reg[2] + updCache ] );
	updCache += 32; updCache &= 127;


	// ... and update some counters
	geo.c64CycleCount ++;

	if ( CPU_RESET ) {
		geo.resetReleased = 0; geo.resetPressed = 1; geo.resetCounter ++;
	} else {
		if ( geo.resetPressed )	geo.resetReleased = 1;
		geo.resetPressed = 0;
	}

	if ( geo.resetCounter > 3 )
	{
		geo.resetReleased = 0;
		geo.resetCounter = 0;
		geo.reg[ 0 ] = geo.reg[ 1 ] = geo.reg[ 2 ] = 0;
		disableCart_l264 = transferStarted_l264 = 0;
	}

	if ( BUTTON_PRESSED )
	{
		geo.saveRAM = 1;
		geo.resetCounter = 2000000;
		FINISH_BUS_HANDLING
		return;
	}

	#ifdef LED
	//CLEAR_LEDS_EVERY_8K_CYCLES
	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
	#else
	FINISH_BUS_HANDLING
	#endif

}

