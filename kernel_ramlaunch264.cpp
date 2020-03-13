/*
  _________.__    .___      __   .__        __       __________.____    ________  ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \______   \    |   \_____  \/  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |       _/    |    /  ____/   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |    |   \    |___/       \  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    |____|_  /_______ \_______ \_____  /\____   | 
        \/         \/    \/     \/       \/     \/           \/        \/       \/     \/      |__| 

 kernel_ramlaunch264.cpp

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - Sidekick RAM Launch 264: a merger of an experimental Geo/NeoRAM emulation and .PRG dropper
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


#include "kernel_ramlaunch264.h"

static const char DRIVE[] = "SD:";

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
	u8 reg[ 3 ];	
	u8 padding1[ 5 ];

	// for rebooting the RPi
	u64 c64CycleCount;
	u32 resetCounter, resetPressed, resetReleased;

	u8 *RAM AA;

	u32 saveRAM, releaseDMA;

	u8 padding[ 20 ];
} __attribute__((packed)) GEOSTATE;

volatile static GEOSTATE geo AAA;
volatile static u32 forceRead;

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
	geo.reg[ 0 ] = geo.reg[ 1 ] = 0;
	geo.RAM = (u8*)( ( (u64)&geoRAM_Pool[0] + 128 ) & ~127 );
	memset( geo.RAM, 0, geoSizeKB * 1024 );

	geo.c64CycleCount = 0;
	geo.resetCounter = 0;
	geo.saveRAM = 0;
	geo.releaseDMA = 0;
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
	// setup control lines, initialize latch and software I2C buffer
	initLatch();
	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	#ifdef USE_OLED
	splashScreen( raspi_ram_splash );
	#endif

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

	disableCart_l264 = transferStarted_l264 = currentOfs_l264 = 0;
	geo.c64CycleCount = geo.resetCounter = 0;
	geo.resetPressed = 0;
	geo.resetReleased = 0;

	// setup FIQ
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	h_nRegOffset = m_InputPin.nRegOffset();
	h_nRegMask = m_InputPin.nRegMask();

	// warm caches
	launchPrepareAndWarmCache();

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
		/*if ( geo.c64CycleCount > 2000000 && geo.resetCounter > 500000 ) {
				EnableIRQs();
				m_InputPin.DisableInterrupt();
				m_InputPin.DisconnectInterrupt();
				saveGeoRAM(FILENAME_RAM);
				return;
			}*/


/*		if ( geo.saveRAM )
		{
			saveGeoRAM( FILENAME_RAM );
			geo.saveRAM = 0;
		}*/

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
	if ( launchPrg_l264 && !disableCart_l264 )
	{
		LAUNCH_FIQ( geo.c64CycleCount, geo.resetCounter,  h_nRegOffset, h_nRegMask )
	}
	#endif

	START_AND_READ_EXPPORT264

	if ( CPU_READS_FROM_BUS && ( GET_ADDRESS264 >= 0xfe00 && GET_ADDRESS264 <= 0xfe7f ) )
	{
		WRITE_D0to7_TO_BUS( GEORAM_WINDOW[ geo.reg[2] + (addr2 & 127) ] );
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
		CACHE_PRELOADL1STRM( GEORAM_WINDOW[ geo.reg[2] ] );
		CACHE_PRELOADL1STRM( GEORAM_WINDOW[ geo.reg[2] + 64 ] );
	} else
	if ( CPU_WRITES_TO_BUS && ( GET_ADDRESS264 >= 0xfe00 && GET_ADDRESS264 <= 0xfe7f ) )
	{
		READ_D0to7_FROM_BUS( D )
		GEORAM_WINDOW[ geo.reg[2] + (addr2 & 127) ] = D;
	} 

	PeripheralEntry();
	write32( ARM_GPIO_GPEDS0 + h_nRegOffset, h_nRegMask );
	PeripheralExit();

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
	CLEAR_LEDS_EVERY_8K_CYCLES
	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
	#else
	FINISH_BUS_HANDLING
	#endif

}

