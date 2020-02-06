/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 lowlevel_arm64.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - low-level ARM code 
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
#include "lowlevel_arm64.h"


u32 WAIT_FOR_SIGNALS = 40;
u32 WAIT_CYCLE_MULTIPLEXER = 200;
u32 WAIT_CYCLE_READ = 475;
u32 WAIT_CYCLE_WRITEDATA = 470;
u32 WAIT_CYCLE_READ_BADLINE = 400;
u32 WAIT_CYCLE_READ_VIC2 = 445;
u32 WAIT_CYCLE_WRITEDATA_VIC2 = 505;
u32 WAIT_CYCLE_MULTIPLEXER_VIC2 = 265;
u32 WAIT_TRIGGER_DMA = 600;
u32 WAIT_RELEASE_DMA = 600;

u32 modeC128 = 0;
u32 modeVIC = 0;		// 0 = old (6596), 1 = new (6572R0,8565R2)

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

