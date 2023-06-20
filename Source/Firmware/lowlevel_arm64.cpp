/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 lowlevel_arm64.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - low-level ARM code 
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
#include "lowlevel_arm64.h"


u16 WAIT_FOR_SIGNALS = 40;
u16 WAIT_CYCLE_MULTIPLEXER = 200;
u16 WAIT_CYCLE_READ = 475;
u16 WAIT_CYCLE_WRITEDATA = 470;
u16 WAIT_CYCLE_READ_BADLINE = 400;
u16 WAIT_CYCLE_READ_VIC2 = 445;
u16 WAIT_CYCLE_WRITEDATA_VIC2 = 505;
u16 WAIT_CYCLE_MULTIPLEXER_VIC2 = 265;
u16 WAIT_TRIGGER_DMA = 600;
u16 WAIT_RELEASE_DMA = 600;

u16 POLL_FOR_SIGNALS_VIC = 60;
u16 POLL_FOR_SIGNALS_CPU = 80;
u16 POLL_CYCLE_MULTIPLEXER_VIC = 180; 
u16 POLL_CYCLE_MULTIPLEXER_CPU = 200;
u16 POLL_READ = 450;
u16 POLL_READ_VIC2 = 413;
u16 POLL_WAIT_CYCLE_WRITEDATA = 440;
u16	POLL_TRIGGER_DMA = 475;
u16 POLL_RELEASE_DMA = 475;

u32 modeC128 = 0;
u32 modeVIC = 0;		// 0 = old (6596), 1 = new (6572R0,8565R2)
u32 modePALNTSC = 0; // 1 = [NTSC: 6567R56A VIC], 2 = [NTSC: 6567R8 VIC], 3 = [PAL VIC]
u32 hasSIDKick = 0;

// for C16/+4
u32 machine264 = 0;

// initialize what we need for the performance counters
void initCycleCounter()
{
	unsigned long rControl;
	unsigned long rFilter;
	unsigned long rEnableSet;

	asm volatile( "mrs %0, PMCCFILTR_EL0" : "=r" ( rFilter ) );
	asm volatile( "mrs %0, PMCR_EL0" : "=r" ( rControl ) );

	// enable PMU filter to count cycles
	rFilter |= ( 1 << PMCCFILTR_NSH_EN_BIT );
	asm volatile( "msr PMCCFILTR_EL0, %0" : : "r" ( rFilter ) );
	asm volatile( "mrs %0, PMCCFILTR_EL0" : "=r" ( rFilter ) );

	// enable cycle count register
	asm volatile( "mrs %0, PMCNTENSET_EL0" : "=r" ( rEnableSet ) );
	rEnableSet |= ( 1 << PMCNTENSET_C_EN_BIT );
	asm volatile( "msr PMCNTENSET_EL0, %0" : : "r" ( rEnableSet ) );
	asm volatile( "mrs %0, PMCNTENSET_EL0" : "=r" ( rEnableSet ) );

	// enable long cycle counter and reset it
	rControl = ( 1 << PMCR_LC_EN_BIT ) | ( 1 << PMCR_C_RESET_BIT ) | ( 1 << PMCR_EN_BIT );
	asm volatile( "msr PMCR_EL0, %0" : : "r" ( rControl ) );
	asm volatile( "mrs %0, PMCR_EL0" : "=r" ( rControl ) );
}

void setDefaultTimings( int mode )
{
	switch ( mode )
	{
	// Raspberry Pi 3A+/3B+, standard clocking
	default:
	case AUTO_TIMING_RPI3PLUS_C64:
		WAIT_FOR_SIGNALS = 40;
		WAIT_CYCLE_MULTIPLEXER = 200;
		WAIT_CYCLE_READ = 475;
		WAIT_CYCLE_WRITEDATA = 470;
		WAIT_CYCLE_READ_BADLINE = 400;
		WAIT_CYCLE_READ_VIC2 = 445;
		WAIT_CYCLE_WRITEDATA_VIC2 = 505;
		WAIT_CYCLE_MULTIPLEXER_VIC2 = 265;
		WAIT_TRIGGER_DMA = 600;
		WAIT_RELEASE_DMA = 600;

		// todo: needs to be tested!
		POLL_FOR_SIGNALS_VIC = 60;
		POLL_FOR_SIGNALS_CPU = 80;
		POLL_CYCLE_MULTIPLEXER_VIC = 180; 
		POLL_CYCLE_MULTIPLEXER_CPU = 200;
		POLL_READ = 450;
		POLL_READ_VIC2 = 413;
		POLL_WAIT_CYCLE_WRITEDATA = 440;
		POLL_TRIGGER_DMA = 475;
		POLL_RELEASE_DMA = 475;
		
		break;

	// Raspberry Pi 3A+/3B+, 1.5GHz, Core 600MHz, SD 550MHz, over voltage 2
	case AUTO_TIMING_RPI3PLUS_C64C128:
		WAIT_FOR_SIGNALS = 60;
		WAIT_CYCLE_MULTIPLEXER = 220;
		WAIT_CYCLE_READ = 475;
		WAIT_CYCLE_WRITEDATA = 470;
		WAIT_CYCLE_READ_BADLINE = 400;
		WAIT_CYCLE_READ_VIC2 = 445;
		WAIT_CYCLE_WRITEDATA_VIC2 = 505;
		WAIT_CYCLE_MULTIPLEXER_VIC2 = 265;
		WAIT_TRIGGER_DMA = 600;
		WAIT_RELEASE_DMA = 600;

		// todo: needs to be tested!
		POLL_FOR_SIGNALS_VIC = 60;
		POLL_FOR_SIGNALS_CPU = 80;
		POLL_CYCLE_MULTIPLEXER_VIC = 180; 
		POLL_CYCLE_MULTIPLEXER_CPU = 200;
		POLL_READ = 450;
		POLL_READ_VIC2 = 413;
		POLL_WAIT_CYCLE_WRITEDATA = 440;
		POLL_TRIGGER_DMA = 475;
		POLL_RELEASE_DMA = 475;
		break;

	// Raspberry Pi Zero 2, C64 1200mhz core 550, sdram 550, overvoltage 6
	case AUTO_TIMING_RPI0_C64:
		WAIT_FOR_SIGNALS			= 35;
		WAIT_CYCLE_MULTIPLEXER		= 160;
		WAIT_CYCLE_READ				= 370;
		WAIT_CYCLE_WRITEDATA		= 380;
		WAIT_CYCLE_READ_BADLINE		= 360;
		WAIT_CYCLE_READ_VIC2		= 350;
		WAIT_CYCLE_WRITEDATA_VIC2	= 404;
		WAIT_CYCLE_MULTIPLEXER_VIC2	= 230;
		WAIT_TRIGGER_DMA			= 480;
		WAIT_RELEASE_DMA			= 480;

		// todo: needs to be tested!
		POLL_FOR_SIGNALS_VIC = 60;
		POLL_FOR_SIGNALS_CPU = 80;
		POLL_CYCLE_MULTIPLEXER_VIC = 180; 
		POLL_CYCLE_MULTIPLEXER_CPU = 200;
		POLL_READ = 450;
		POLL_READ_VIC2 = 413;
		POLL_WAIT_CYCLE_WRITEDATA = 440;
		POLL_TRIGGER_DMA = 475;
		POLL_RELEASE_DMA = 475;
		break;

	// Raspberry Pi Zero 2, C64/C128, 1300mhz, core/sd=550, overvoltage 6 
	case AUTO_TIMING_RPI0_C64C128:
		WAIT_FOR_SIGNALS			= 40;
		WAIT_CYCLE_MULTIPLEXER		= 170;
		WAIT_CYCLE_READ				= 441;
		WAIT_CYCLE_WRITEDATA		= 436;
		WAIT_CYCLE_READ_BADLINE		= 371;
		WAIT_CYCLE_READ_VIC2		= 413;
		WAIT_CYCLE_WRITEDATA_VIC2	= 469;
		WAIT_CYCLE_MULTIPLEXER_VIC2	= 246;
		WAIT_TRIGGER_DMA			= 520;
		WAIT_RELEASE_DMA			= 520;

		POLL_FOR_SIGNALS_VIC = 60;
		POLL_FOR_SIGNALS_CPU = 80;
		POLL_CYCLE_MULTIPLEXER_VIC = 180; 
		POLL_CYCLE_MULTIPLEXER_CPU = 200;
		POLL_READ = 450;
		POLL_READ_VIC2 = 413;
		POLL_WAIT_CYCLE_WRITEDATA = 440;
		POLL_TRIGGER_DMA = 475;
		POLL_RELEASE_DMA = 475;
		break;
	};

/*	const int dt = +5;
	WAIT_FOR_SIGNALS += dt;
	WAIT_CYCLE_MULTIPLEXER += dt;
	WAIT_CYCLE_READ += dt;
	WAIT_CYCLE_WRITEDATA += dt;
	WAIT_CYCLE_READ_BADLINE += dt;
	WAIT_CYCLE_READ_VIC2 += dt;
	WAIT_CYCLE_WRITEDATA_VIC2 += dt;
	WAIT_CYCLE_MULTIPLEXER_VIC2 += dt;
	WAIT_TRIGGER_DMA += dt;
	WAIT_RELEASE_DMA += dt;*/

}

void setDefaultTimings264( int mode )
{
	switch ( mode )
	{
	// Raspberry Pi 3A+/3B+
	default:
	case AUTO_TIMING_RPI3_264:
		WAIT_CYCLE_READ = (350);
		WAIT_CYCLE_WRITEDATA = (400);
		break;

	// Raspberry Pi Zero 2
	case AUTO_TIMING_RPI0_264:
		WAIT_CYCLE_READ = (293-3);
		WAIT_CYCLE_WRITEDATA = (335-5);
		break;
	}
}

__attribute__( ( always_inline ) ) inline void LDNP_2x32( unsigned long addr, u32 &val1, u32 &val2 )
{
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val1), "=r" (val2) : "r" (addr) : "memory");
}

__attribute__( ( always_inline ) ) inline u32 LDNP_1x32( unsigned long addr )
{
	u32 val1, val2;
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val1), "=r" (val2) : "r" (addr) : "memory");
	return val1;
}

__attribute__( ( always_inline ) ) inline u16 LDNP_1x16( unsigned long addr )
{
	u32 val1, val2;
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val1), "=r" (val2) : "r" (addr) : "memory");
	return val1 & 65535;
}

/*__attribute__( ( always_inline ) ) inline u8 LDNP_1x8( void *addr )
{
	u32 val1, val2;
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val1), "=r" (val2) : "r" ((unsigned long)addr) : "memory");
	return val1 & 255;
}*/
