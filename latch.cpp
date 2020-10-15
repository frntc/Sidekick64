/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 latch.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - code for driving the latch (more ouputs)
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
#include <circle/bcm2835.h>
#include <circle/gpiopin.h>
#include <circle/memio.h>
#include "latch.h"

// current and last status of the data lines
// connected to the latch input (GPIO D0-D7 -> 74LVX573D D0-D7)
u32 latchD, latchClr, latchSet;
u32 latchDOld;

// a ring buffer for simple I2C output via the latch
// (since we do not have enough GPIOs available)
u8 i2cBuffer[ FAKE_I2C_BUF_SIZE ] AAA;
u32 i2cBufferCountLast, i2cBufferCountCur;

void initLatch()
{
	latchD = 0;
	latchClr = latchSet = 0;
	latchDOld = 0xFFFFFFFF;
	i2cBufferCountLast = i2cBufferCountCur = 0;
	//putI2CCommand( 0 );
}

