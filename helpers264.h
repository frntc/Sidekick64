/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 helpers264.h

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - some macros for the C16/+4
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

#ifndef _helpers264_h
#define _helpers264_h

#include <SDCard/emmc.h>
#include <fatfs/ff.h>

extern int readFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size );
extern int writeFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 size );

#define START_AND_READ_EXPPORT264			\
	register u32 g2, g3, addr2;				\
	BEGIN_CYCLE_COUNTER						\
	write32( ARM_GPIO_GPSET0, bCTRL257 );	\
	g2 = read32( ARM_GPIO_GPLEV0 );			\
	addr2 = ( g2 >> A0 ) & 255;				\
	g3 = read32( ARM_GPIO_GPLEV0 );			\
	addr2 |= (( g3 >> A8 ) & 255)<<8;		

#define GET_ADDRESS264		(addr2)

#define BUS_AVAILABLE264	((g3&bIO1))
#define C1LO_ACCESS			(!(g3&bROMH))
#define C1HI_ACCESS			(!(g2&bCS))

// fixed timings
//#define WAIT_CYCLE_READ (265*14/10)
//#define WAIT_CYCLE_WRITEDATA (300*14/10)
#define WAIT_CYCLE_READ (350)
#define WAIT_CYCLE_WRITEDATA (400)

#endif
