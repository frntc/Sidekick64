/*
  _________.__    .___      __   .__        __       __________    _____      _____   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \______   \  /  _  \    /     \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |       _/ /  /_\  \  /  \ /  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |    |   \/    |    \/    Y    \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    |____|_  /\____|__  /\____|__  /
        \/         \/    \/     \/       \/     \/           \/         \/         \/ 

 kernel_georam.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick RAM: example how to implement a GeoRAM/NeoRAM compatible memory expansion
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
#include "kernel_georam.h"

// let them blink
#define LED

// size of GeoRAM/NeoRAM in Kilobytes
const int geoSizeKB = 2048;

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	// GeoRAM registers
	// $dffe : selection of 256 Byte-window in 16 Kb-block
	// $dfff : selection of 16 Kb-nlock
	u8  reg[ 2 ];	

	// for rebooting the RPi
	u64 c64CycleCount;
	u32 resetCounter;

	u8 *RAM AA;

	u8 padding[ 10 ];
} __attribute__((packed)) GEOSTATE;
#pragma pack(pop)

volatile static GEOSTATE geo AAA;

// geoRAM memory pool 
static u8  geoRAM_Pool[ geoSizeKB * 1024 + 128 ] AA;

// u8* to current window
#define GEORAM_WINDOW (&geo.RAM[ ( geo.reg[ 1 ] * 16384 ) + ( geo.reg[ 0 ] * 256 ) ])

// geoRAM helper routines
void geoRAM_Init()
{
	geo.reg[ 0 ] = geo.reg[ 1 ] = 0;
	geo.RAM = (u8*)( ( (u64)&geoRAM_Pool[0] + 128 ) & ~127 );
	memset( geo.RAM, 0, geoSizeKB * 1024 );

	geo.c64CycleCount = 0;
	geo.resetCounter = 0;
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

void CKernelGeoRAM::Run( void )
{
	// setup control lines, initialize latch and software I2C buffer
	initLatch();
	SETCLR_GPIO( bDMA | bEXROM | bNMI | bGAME, 0 ); 
	latchSetClearImm( LATCH_RESET, LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	#ifdef USE_OLED
	splashScreen( raspi_ram_splash );
	#endif

	// GeoRAM initialization
	geoRAM_Init();

	DisableIRQs();

	// setup FIQ
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	geo.c64CycleCount = geo.resetCounter = 0;

	while ( true )
	{
		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}


void CKernelGeoRAM::FIQHandler( void *pParam )
{
	register u32 D;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// let's use this time to preload the cache (L1-cache, streaming)
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 0 ] );
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 64 ] );
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 128 ] );
	CACHE_PRELOADL1STRM( GEORAM_WINDOW[ 192 ] );

	// ... and update some counters
	UPDATE_COUNTERS_MIN( geo.c64CycleCount, geo.resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	// access to IO1 or IO2 => GeoRAM 
	if ( IO1_OR_IO2_ACCESS )
	{
		if ( CPU_READS_FROM_BUS )	// CPU reads from memory page or register
		{
			if ( IO1_ACCESS )	
				// GeoRAM read from memory page
				D = GEORAM_WINDOW[ GET_IO12_ADDRESS ]; else
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
				// GeoRAM write to memory page
				GEORAM_WINDOW[ GET_IO12_ADDRESS ] = D; else
				// GeoRAM write register (IO2_ACCESS)
				geoRAM_IO2_Write( GET_IO12_ADDRESS, D );
		}

		#ifdef LED
		// turn off one of the 4 LEDs indicating reads and writes from/to the GeoRAM register and memory pages
		LED_ON( ( IO2_ACCESS ? 1 : 0 ) + ( CPU_WRITES_TO_BUS ? 2 : 0 ) )
		#endif
	}

	#ifdef LED
	CLEAR_LEDS_EVERY_8K_CYCLES
	#endif

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

int main()
{
	CKernelGeoRAM kernel;
	if ( kernel.Initialize() )
		kernel.Run();

	halt();
	return EXIT_HALT;
}
