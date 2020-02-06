/*
  _________.__    .___      __   .__        __       ___________.__                .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   _____/|  | _____    _____|  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |    __)  |  | \__  \  /  ___/  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |     \   |  |__/ __ \_\___ \|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \___  /   |____(____  /____  >___|  /
        \/         \/    \/     \/       \/     \/        \/              \/     \/     \/ 

 kernel_ef.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Flash: example how to implement an generic/magicdesk/easyflash-compatible cartridge
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
#include "kernel_ef.h"

// use this, it you want LEDs to show EF accesses
#define LED

// we will read this .CRT file
static const char DRIVE[] = "SD:";
#ifdef COMPILE_MENU
extern char *FILENAME;
#else
static const char FILENAME[] = "SD:test.crt";
#endif

// temporary buffers to read cartridge data from SD
static CRT_HEADER header;

//
// easyflash (note: generic and magic desk carts are "mapped" to easyflash carts)
//
#define EASYFLASH_BANKS			64
#define EASYFLASH_BANK_MASK		( EASYFLASH_BANKS - 1 )

typedef struct
{
	// EF extra RAM
	u8  ram[ 256 ];

	// flash ROM
	u32 nBanks;
	u8	bankswitchType;					// EF, Magic Desk, CBM80, ...
	u32 ROM_LH;							// are we reacting to ROML and/or ROMH?
	u8	*flashBank;						// ptr to currently selected bank
	u8	*flash_cacheoptimized;

	// Easyflash registers and jumper
	u8  reg0, reg0old, reg2;			// $DE00 EasyFlash Bank and $DE02 EasyFlash Control registers
	u32 jumper;							// EasyFlash Jumper
	u8	memconfig[ 16 ];				// see Table 2.5 of EF ProgRef

	// counts the #cycles when the C64-reset line is pulled down (to detect a reset), cycles since release and status
	u32 resetCounter, resetCounter2, 
		cyclesSinceReset,  
		resetPressed, resetReleased, resetEFRAM;

	// total count of C64 cycles (not set to zero upon reset)
	u64 c64CycleCount;

	// this is for orchestration of DMA-enforced stalling of the C64 to warm up caches
	u32 releaseDMA;

	u32 flashFitsInCache;

	// todo
	u8 padding[ 384 - 344 ];
} __attribute__((packed)) EFSTATE;

static volatile EFSTATE ef AAA;

// ... flash
static u8 flash_cacheoptimized_pool[ 1024 * 1024 + 1024 ] AAA;

// table with EF memory configurations adapted from Vice
#define M_EXROM	2
#define M_GAME	1
static const u8 memconfig_table[] = {
    (M_EXROM|M_GAME), (M_EXROM|M_GAME), (M_GAME), (M_GAME),
    (M_EXROM),		  (M_EXROM|M_GAME), 0,		  (M_GAME),
    (M_EXROM),		  (M_EXROM|M_GAME), 0,		  (M_GAME), 
    (M_EXROM),		  (M_EXROM|M_GAME), 0,		  (M_GAME),
};

__attribute__( ( always_inline ) ) inline void setGAMEEXROM()
{
	register u32 mode = ef.memconfig[ ( ef.jumper << 3 ) | ( ef.reg2 & 7 ) ];

	register u32 set = bNMI, clr = 0;

	if ( mode & 2 )
		set |= bEXROM; else
		clr |= bEXROM;

	if ( mode & 1 )
		clr |= bGAME; else
		set |= bGAME;

	SETCLR_GPIO( set, clr );
}

__attribute__( ( always_inline ) ) inline u8 easyflash_IO1_Read( u32 addr )
{
	return ( addr & 2 ) ? ef.reg2 : ef.reg0;
}

__attribute__( ( always_inline ) ) inline void easyflash_IO1_Write( u32 addr, u8 value )
{
	if ( ( addr & 2 ) == 0 )
	{
		ef.reg0old = ef.reg0;
		ef.reg0 = (u8)( value & EASYFLASH_BANK_MASK );
		ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];
	} else
	{
		ef.reg2 = value & 0x87;
	}
}


void initEF()
{
	ef.jumper  = 
	ef.reg0    = 
	ef.reg0old = 
	ef.reg2    = 0;

	ef.resetCounter     = 
	ef.resetPressed     = 
	ef.resetReleased    = 
	ef.cyclesSinceReset = 0;

	ef.releaseDMA = 0;

	if ( ef.bankswitchType == BS_NONE )
	{
		ef.reg2 = 4;
		if ( !header.exrom ) ef.reg2 |= 2;
		if ( !header.game )  ef.reg2 |= 1;
	}
	if ( ef.bankswitchType == BS_MAGICDESK )
	{
		ef.reg2 = 4 + 2;
	}

	ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 * 2 ];

	setGAMEEXROM();
	SET_GPIO( bDMA | bNMI ); 
}


__attribute__( ( always_inline ) ) inline void prefetchHeuristic()
{
	//prefetchBank( ef.reg0, 1 );
	CACHE_PRELOAD_DATA_CACHE( ef.flashBank, 16384, CACHE_PRELOADL2STRM )
	CACHE_PRELOAD_DATA_CACHE( ef.ram, 256, CACHE_PRELOADL1KEEP )

	// the RPi is not really convinced enough to preload data into caches when calling "prfm PLD*", so we access the data (and a bit more than our current bank)
	FORCE_READ_LINEAR( ef.flashBank, 16384 * 2 )
	FORCE_READ_LINEAR32( ef.ram, 256 )
/*	FORCE_READ_LINEAR( ef.flashBank, 16384 )
	FORCE_READ_LINEAR( ef.flashBank, 16384 )
	FORCE_READ_LINEAR32( ef.ram, 256 )*/

//	FORCE_READ_LINEARa( ef.flashBank, 16384, 16384 * 8 )
//	FORCE_READ_LINEARa( ef.ram, 256, 256 * 8 )
	//FORCE_READ_LINEAR32( ef.ram, 256 )
}


__attribute__( ( always_inline ) ) inline void prefetchComplete()
{
	CACHE_PRELOAD_DATA_CACHE( ef.flash_cacheoptimized, ef.nBanks * ( ef.bankswitchType == BS_EASYFLASH ? 2 : 1 ) * 8192, CACHE_PRELOADL2KEEP )
	CACHE_PRELOAD_DATA_CACHE( ef.ram, 256, CACHE_PRELOADL1KEEP )

	//the RPi is again not convinced enough to preload data into cache, this time we need to simulate random access to the data
	FORCE_READ_LINEAR( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks )
	FORCE_READ_RANDOM( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 8192 * 2 * ef.nBanks * 16 )
}

#ifdef COMPILE_MENU
static void KernelEFFIQHandler( void *pParam );
void KernelEFRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME )
#else
void CKernelEF::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	#ifdef USE_OLED
	char fn[ 1024 ];
	// attention: this assumes that the filename ending is always ".crt"!
	memset( fn, 0, 1024 );
	strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
	strcat( fn, ".logo" );
	if ( !splashScreenFile( (char*)DRIVE, fn ) )
		splashScreen( raspi_flash_splash );
	#endif

	// read .CRT
	ef.flash_cacheoptimized = (u8 *)( ( (u64)&flash_cacheoptimized_pool[ 0 ] + 128 ) & ~127 );
	readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)ef.flash_cacheoptimized, &ef.bankswitchType, &ef.ROM_LH, &ef.nBanks );

	// first initialization of EF
	for ( u32 i = 0; i < sizeof( memconfig_table ); i++ )
		ef.memconfig[ i ] = memconfig_table[ i ];

	for ( u32 i = 0; i < 256; i++ )
		ef.ram[ i ] = 0;

	initEF();

	DisableIRQs();

	// setup FIQ
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );
	m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );

	// determine how to and preload caches
	if ( ef.bankswitchType == BS_MAGICDESK )
		ef.flashFitsInCache = ( ef.nBanks <= 64 ) ? 1 : 0; else // Magic Desk only uses ROML-banks -> different memory layout
		ef.flashFitsInCache = ( ef.nBanks <= 32 ) ? 1 : 0;

	CleanDataCache();
	InvalidateDataCache();
	InvalidateInstructionCache();

	// TODO
	ef.flashFitsInCache = 0;

	if ( ef.flashFitsInCache )
		prefetchComplete(); else
		prefetchHeuristic();

	// additional first-time cache preloading
	CACHE_PRELOAD_DATA_CACHE( (u8*)&ef, ( sizeof( EFSTATE ) + 63 ) / 64, CACHE_PRELOADL1KEEP )
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 3072 )

	if ( ef.flashFitsInCache )
		FORCE_READ_RANDOM( ef.flash_cacheoptimized, 8192 * 2 * ef.nBanks, 8192 * 2 * ef.nBanks * 256 )

	FORCE_READ_LINEAR32a( &ef, sizeof( EFSTATE ), 65536 );
	FORCE_READ_LINEAR32a( &FIQ_HANDLER, 3072, 65536 );

	prefetchHeuristic();

	// ready to go...
	latchSetClearImm( LATCH_RESET, 0 );

	ef.c64CycleCount = ef.resetCounter2 = 0;

	// wait forever
	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU2FIQs( ef.c64CycleCount, ef.resetCounter2 )
		#endif
		asm volatile ("wfi");
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}


#ifdef COMPILE_MENU
static void KernelEFFIQHandler( void *pParam )
#else
void CKernelEF::FIQHandler (void *pParam)
#endif
{
	register u32 D, addr;
	register u8 *flashBankR = ef.flashBank;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// we got the A0..A7 part of the address which we will access
	addr = GET_ADDRESS0to7 << 5;

	CACHE_PRELOADL2STRM( &flashBankR[ addr * 2 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 0 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 64 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 128 ] );
	CACHE_PRELOADL1KEEP( &ef.ram[ 192 ] );

	UPDATE_COUNTERS_MIN( ef.c64CycleCount, ef.resetCounter2 )

	if ( modeC128 && VIC_HALF_CYCLE )
		WAIT_CYCLE_MULTIPLEXER += 20;

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( modeC128 && VIC_HALF_CYCLE )
		WAIT_CYCLE_MULTIPLEXER -= 20;

	// make our address complete
	addr |= GET_ADDRESS8to12;

	//
	//
	//
	if ( VIC_HALF_CYCLE )
	{

		if ( ROML_OR_ROMH_ACCESS )
		{
			// get both ROML and ROMH with one read
			D = *(u32*)&flashBankR[ addr * 2 ];
			if ( ROMH_ACCESS ) D >>= 8;

			WRITE_D0to7_TO_BUS_VIC( D )
			LED0_ON
		} else
		if ( IO2_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			WRITE_D0to7_TO_BUS_VIC( ef.ram[ GET_IO12_ADDRESS ] );
			LED1_ON
		} else

		if ( IO1_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			WRITE_D0to7_TO_BUS_VIC( easyflash_IO1_Read( GET_IO12_ADDRESS ) );
			LED1_ON
		}
		
		FINISH_BUS_HANDLING
		return;
	} 



	// VIC2 read during badline?
	if ( VIC_BADLINE )
	{
		if ( ef.bankswitchType == BS_MAGICDESK )
		{
			D = *(u32*)&flashBankR[ addr ];
		} else
		{
			D = *(u32*)&flashBankR[ addr * 2 ];
			if ( ROMH_ACCESS )
				D >>= 8; 
		}

		READ_ADDR8to12_ROMLH_IO12_BA
		WRITE_D0to7_TO_BUS_BADLINE( D )
		FINISH_BUS_HANDLING
		return;
	}

	//
	// starting from here: CPU communication
	//
	if ( CPU_READS_FROM_BUS )
	{
		if ( ( ~g3 & ef.ROM_LH ) )
		{
			if ( ef.bankswitchType == BS_MAGICDESK )
			{
				D = *(u32*)&flashBankR[ addr ];
			} else
			{
				D = *(u32*)&flashBankR[ addr * 2 ];
				if ( ROMH_ACCESS )
					D >>= 8; 
			}

			WRITE_D0to7_TO_BUS( D )
			LED0_ON
			goto cleanup;
		}

		if ( IO2_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			WRITE_D0to7_TO_BUS( ef.ram[ GET_IO12_ADDRESS ] )
			LED2_ON
			goto cleanup;
		}

		if ( IO1_ACCESS && ( ef.bankswitchType == BS_EASYFLASH ) )
		{
			PUT_DATA_ON_BUS_AND_CLEAR257( easyflash_IO1_Read( GET_IO12_ADDRESS ) )
			LED3_ON
			goto cleanup;
		}
	}

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )

		if ( ef.bankswitchType == BS_EASYFLASH )
		{
			// easyflash register in IO1
			easyflash_IO1_Write( GET_IO12_ADDRESS, D );

			if ( ( GET_IO12_ADDRESS & 2 ) == 0 )
			{
				// if the EF-ROM does not fit into the RPi's cache: stall the CPU with a DMA and prefetch the data
				if ( !ef.flashFitsInCache )
				{
					WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
					CLR_GPIO( bDMA ); 
					ef.releaseDMA = NUM_DMA_CYCLES;
					prefetchHeuristic();
				}
			} else 
				setGAMEEXROM();
		} else
		{
			// Magic Desk
			ef.reg0 = (u8)( D & 63 );
			ef.flashBank = &ef.flash_cacheoptimized[ ef.reg0 * 8192 ];

			// if the EF-ROM does not fit into the RPi's cache: stall the CPU with a DMA and prefetch the data
			if ( !ef.flashFitsInCache )
			{
				WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
				CLR_GPIO( bDMA ); 
				ef.releaseDMA = NUM_DMA_CYCLES;
				prefetchHeuristic();
			}

			if ( !(D & 128) )
				ef.reg2 = 128 + 4 + 2; else
				ef.reg2 = 4 + 0;
					
			setGAMEEXROM();
		}

		LED3_ON
		goto cleanup;
	}

	if ( CPU_WRITES_TO_BUS && IO2_ACCESS )
	{
		READ_D0to7_FROM_BUS( D )
		ef.ram[ GET_IO12_ADDRESS ] = D;
		LED2_ON
		goto cleanup;
	}

	// reset handling: when button #2 is pressed together with #1 then the EF ram is erased, DMA is released as well
	if ( !( g2 & bRESET ) ) { ef.resetCounter ++; } else { ef.resetCounter = 0; }
	if ( !( g3 & ( 1 << BUTTON ) ) && ef.resetCounter > 3 ) { ef.resetEFRAM = 1; }

	if ( ef.resetCounter > 3 && ef.resetCounter < 0x8000000 )
	{
		ef.resetCounter = 0x8000000;
		initEF();
		if ( ef.resetEFRAM )
			memset( (void*)ef.ram, 0, 256 );
		ef.resetEFRAM = 0;
		SET_GPIO( bDMA ); 
		FINISH_BUS_HANDLING
		return;
	}

cleanup:

	if ( ef.releaseDMA > 0 && --ef.releaseDMA == 0 )
	{
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
	}

	CLEAR_LEDS_EVERY_8K_CYCLES
	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

