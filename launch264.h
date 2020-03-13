/*
  _________.__    .___      __   .__        __       .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/           \/    \/           \/     \/     \/ 

 launch264.h

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - launch/.PRG dropper routines to be inlined in some other kernel
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

#ifndef __LAUNCH264__H
#define __LAUNCH264__H

#include <circle/logger.h>
#include <circle/types.h>

extern u32 disableCart_l264, transferStarted_l264, currentOfs_l264;
extern u32 prgSize_l264, launchPrg_l264;
extern unsigned char prgData_l264[ 65536 + 64 ];
extern unsigned char launchCode_l264[ 65536 ];
extern unsigned char *curTransfer_l264;

extern int launchGetProgram( const char *FILENAME, bool hasData = false, u8 *prgDataExt = 0, u32 prgSizeExt = 0 );
extern void launchInitLoader( bool ultimax );
extern void launchPrepareAndWarmCache();

#define LAUNCH_FIQ( cycleCountC64, resetCounter, REGOFFSET, REGMASK )			\
		register u32 g2, g3, D;													\
		BEGIN_CYCLE_COUNTER														\
		write32( ARM_GPIO_GPSET0, bCTRL257 ); 									\
		g2 = read32( ARM_GPIO_GPLEV0 );											\
		register u32 addr2 = ( g2 >> 5 ) & 255;									\
		g3 = read32( ARM_GPIO_GPLEV0 );											\
		if ( disableCart_l264  ) goto get_out_launch264;						\
		addr2 |= (( g3 >> A8 ) & 255)<<8;										\
		if ( BUS_AVAILABLE264 && CPU_READS_FROM_BUS && addr2 == 0xfde5 ) {		\
			D = *( curTransfer_l264 );											\
			PUT_DATA_ON_BUS_AND_CLEAR257( D )									\
			curTransfer_l264++;													\
			CACHE_PRELOADL2KEEP( curTransfer_l264 );							\
			goto get_out_launch264;												\
		} 																		\
		if ( BUS_AVAILABLE264 && !( g3 & bROMH ) && CPU_READS_FROM_BUS ) {		\
			D = launchCode_l264[ addr2 & 0x1fff ];								\
			PUT_DATA_ON_BUS_AND_CLEAR257( D )									\
			goto get_out_launch264;												\
		} 																		\
		if ( CPU_WRITES_TO_BUS && addr2 == 0xfde5 ) {							\
			READ_D0to7_FROM_BUS( D )											\
			if ( D == 123 )	disableCart_l264 = 1;								\
			curTransfer_l264 = &prgData_l264[0]; 								\
			CACHE_PRELOADL2KEEP( curTransfer_l264 );							\
			goto get_out_launch264;												\
		}																		\
	get_out_launch264:															\
		PeripheralEntry();														\
		write32( ARM_GPIO_GPEDS0 + REGOFFSET, REGMASK );						\
		PeripheralExit();														\
	UPDATE_COUNTERS_MIN( cycleCountC64, resetCounter )							\
	if ( resetCounter > 3  ) { disableCart_l264 = transferStarted_l264 = 0; }	\
	write32( ARM_GPIO_GPCLR0, bCTRL257 );										\
		RESET_CPU_CYCLE_COUNTER													\
		return;														

#endif