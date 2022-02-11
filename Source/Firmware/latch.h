/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 latch.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - code for driving the latch (more ouputs)
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

#ifndef _latch_h
#define _latch_h

#include <circle/bcm2835.h>
#include <circle/types.h>
#include <circle/gpiopin.h>
#include <circle/memio.h>
#include "gpio_defs.h"
#include "lowlevel_arm64.h"

#define LATCH_RESET			(1<<D0)

/*#define LATCH_LEDO 			(1<<D1)
#define LATCH_LEDG 			(1<<D2)
#define LATCH_LEDR 			(1<<D3)
#define LATCH_LEDB 			(1<<D4)*/

#define LATCH_LED0 			(1<<D1)
#define LATCH_LED1 			(1<<D2)
#define LATCH_LED2 			(1<<D3)
#define LATCH_LED3 			(1<<D4)
#define LATCH_LED_ALL		(LATCH_LED0|LATCH_LED1|LATCH_LED2|LATCH_LED3)
#define LATCH_LED0to3		LATCH_LED_ALL
#define LATCH_LED1to3		(LATCH_LED1|LATCH_LED2|LATCH_LED3)
#define LATCH_LED2to3		(LATCH_LED2|LATCH_LED3)
#define LATCH_LED0to2		(LATCH_LED0|LATCH_LED1|LATCH_LED2)
#define LATCH_LED0to1		(LATCH_LED0|LATCH_LED1)

const u32 LATCH_ON[5] =  {             0, LATCH_LED0,    LATCH_LED0to1, LATCH_LED0to2, LATCH_LED0to3 };
const u32 LATCH_OFF[5] = { LATCH_LED_ALL, LATCH_LED1to3, LATCH_LED2to3, LATCH_LED3,    0 };

#define LATCH_ENABLE_KERNAL (1<<D5)
#define LATCH_SDA			(1<<D7)
#define LATCH_SCL			(1<<D6)

#define LED_ON(n)	setLatchFIQ( LATCH_LED0 << (n) );
#define LED0_ON		setLatchFIQ( LATCH_LED0 );
#define LED1_ON		setLatchFIQ( LATCH_LED1 );
#define LED2_ON		setLatchFIQ( LATCH_LED2 );
#define LED3_ON		setLatchFIQ( LATCH_LED3 );

#define LED_OFF(n)	clrLatchFIQ( LATCH_LED0 << (n) );
#define LED0_OFF	clrLatchFIQ( LATCH_LED0 );
#define LED1_OFF	clrLatchFIQ( LATCH_LED1 );
#define LED2_OFF	clrLatchFIQ( LATCH_LED2 );
#define LED3_OFF	clrLatchFIQ( LATCH_LED3 );

// ugly, need to make this inlining nicer...

// current and last status of the data lines
// connected to the latch input (GPIO D0-D7 -> 74LVX573D D0-D7)
extern u32 latchD, latchClr, latchSet;
extern u32 latchDOld;

extern void initLatch();

// a ring buffer for simple I2C output via the latch
#define FAKE_I2C_BUF_SIZE ( 65536 )
extern u8 i2cBuffer[ FAKE_I2C_BUF_SIZE ];
extern u32 i2cBufferCountLast, i2cBufferCountCur;

#define DELAY(rounds) \
	for ( int i = 0; i < rounds; i++ ) { \
		asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" ); }


static __attribute__( ( always_inline ) ) inline void outputLatch( bool withoutCycleCounter = false )
{
	if ( latchD != latchDOld )
	{
		latchDOld = latchD;

		write32( ARM_GPIO_GPSET0, ( D_FLAG & latchD ) | (1 << GPIO_OE) );
		write32( ARM_GPIO_GPCLR0, ( D_FLAG & ( ~latchD ) ) );

		if ( withoutCycleCounter )
		{
			//DELAY(8);
			write32( ARM_GPIO_GPSET0, (1 << LATCH_CONTROL) );
			DELAY(8);
			write32( ARM_GPIO_GPCLR0, (1 << LATCH_CONTROL) ); 
			DELAY(8);
		} else
		{
			BEGIN_CYCLE_COUNTER
			WAIT_UP_TO_CYCLE( 50 );
			write32( ARM_GPIO_GPSET0, (1 << LATCH_CONTROL) );
			WAIT_UP_TO_CYCLE( 50 + 50 );
			write32( ARM_GPIO_GPCLR0, (1 << LATCH_CONTROL) ); 
		}
	}
}

static __attribute__( ( always_inline ) ) inline void setLatchFIQ( u32 f )
{
	latchD |= f;
}

static __attribute__( ( always_inline ) ) inline void clrLatchFIQ( u32 f )
{
	latchD &= ~f;
}

static __attribute__( ( always_inline ) ) inline void latchSetClear( u32 s, u32 c )
{
	latchD |= s;
	latchD &= ~c;
}

static __attribute__( ( always_inline ) ) inline void latchSetClearImm( u32 s, u32 c )
{
	latchD |= s;
	latchD &= ~c;
	outputLatch( true );
}

static __attribute__( ( always_inline ) ) inline void putI2CCommand( u32 c )
{
	u32 memOfs = i2cBufferCountCur >> 2;
	u8  bitOfs = (i2cBufferCountCur & 3 ) << 1; 
	u8  v = i2cBuffer[ memOfs ];
	v &= ~( 3 << bitOfs );
	v |= ( c & 3 ) << bitOfs;
	i2cBuffer[ memOfs ] = v;

	i2cBufferCountCur++;
	i2cBufferCountCur &= ( FAKE_I2C_BUF_SIZE - 1 );
}

static __attribute__( ( always_inline ) ) inline u32 getI2CCommand()
{
	u32 memOfs = i2cBufferCountLast >> 2;
	u8  bitOfs = (i2cBufferCountLast & 3 ) << 1; 
	u8  ret = ( ( i2cBuffer[ memOfs ] >> bitOfs ) & 3 );

	i2cBufferCountLast++;
	i2cBufferCountLast &= ( FAKE_I2C_BUF_SIZE - 1 );
	return ret;
}

static __attribute__( ( always_inline ) ) inline void put4BitCommand( u32 c )
{
	u32 memOfs = i2cBufferCountCur >> 1;
	u8  bitOfs = (i2cBufferCountCur & 1 ) << 2; 
	u8  v = i2cBuffer[ memOfs ];
	v &= ~( 15 << bitOfs );
	v |= ( c & 15 ) << bitOfs;
	i2cBuffer[ memOfs ] = v;

	i2cBufferCountCur++;
	i2cBufferCountCur &= ( FAKE_I2C_BUF_SIZE - 1 );
}

static __attribute__( ( always_inline ) ) inline u32 get4BitCommand()
{
	u32 memOfs = i2cBufferCountLast >> 1;
	u8  bitOfs = (i2cBufferCountLast & 1 ) << 2; 
	u8  ret = ( ( i2cBuffer[ memOfs ] >> bitOfs ) & 15 );

	i2cBufferCountLast++;
	i2cBufferCountLast &= ( FAKE_I2C_BUF_SIZE - 1 );
	return ret;
}


static __attribute__( ( always_inline ) ) inline boolean bufferEmptyI2C()
{
	return ( i2cBufferCountLast == i2cBufferCountCur );
}

static __attribute__( ( always_inline ) ) inline u32 bufferIsFreeI2C()
{
	return ( i2cBufferCountLast - i2cBufferCountCur + FAKE_I2C_BUF_SIZE - 1 ) % FAKE_I2C_BUF_SIZE;
}

static __attribute__( ( always_inline ) ) inline void prefetchI2C()
{
	CACHE_PRELOADL2STRM( &i2cBufferCountLast );
	CACHE_PRELOADL2STRM( &i2cBufferCountLast );
	CACHE_PRELOADL2STRM( &i2cBufferCountCur );
	CACHE_PRELOADL2STRM( &i2cBuffer[ i2cBufferCountLast ] );
}

static __attribute__( ( always_inline ) ) inline void prepareOutputLatch()
{
	if ( !bufferEmptyI2C() )
	{
		NextCommand:
		extern u32 getI2CCommand();
		u32 c = getI2CCommand();
	#if 1
		static u32 lastSDAValue = 255;

		if ( !( c & 2 ) ) // SDA
		{
			// optimization: if SDA didn't change, no need to output it to I2C
			if ( ( c & 1 ) == lastSDAValue )
			{
				if ( !bufferEmptyI2C() )
					goto NextCommand; else
					goto NothingToDo; 
			}
			lastSDAValue = c & 1;

			if ( c & 1 ) setLatchFIQ( LATCH_SDA ); else clrLatchFIQ( LATCH_SDA );
		} else
		// SCL
		{
			if ( c & 1 ) setLatchFIQ( LATCH_SCL ); else clrLatchFIQ( LATCH_SCL );
		}
	#endif

	#if 0
		register u32 flag = LATCH_SDA;
		if ( c & 2 )
			flag = LATCH_SCL;

		if ( c & 1 )
			setLatchFIQ( flag ); else 
			clrLatchFIQ( flag );
	#endif
	} else
	{
		i2cBufferCountLast = i2cBufferCountCur = 0;
	}
NothingToDo:;

}


static __attribute__( ( always_inline ) ) inline void flushI2CBuffer( bool withoutCycleCounter = false )
{
	u64 armCycleCounter = 0;
	if ( !withoutCycleCounter )
	{
		RESTART_CYCLE_COUNTER
	}
	while ( !bufferEmptyI2C() )
	{
		prepareOutputLatch();
		outputLatch( withoutCycleCounter );
		if ( withoutCycleCounter )
		{
			DELAY( 512 );
		} else
		{
			RESTART_CYCLE_COUNTER
			WAIT_UP_TO_CYCLE( 7000 )
		}
	}
	write32( ARM_GPIO_GPCLR0, ( 1 << LATCH_CONTROL ) );
	i2cBufferCountLast = i2cBufferCountCur = 0;
}

static __attribute__( ( always_inline ) ) inline void prepareOutputLatch4Bit()
{
	test:
	if ( !bufferEmptyI2C() )
	{
		u32 v = get4BitCommand();

		const u32 tab[4] = { LATCH_SCL, LATCH_SDA, LATCH_LED3, LATCH_LED2 };
		u32 c = tab[ v >> 2 ]; 

		u32 oldLatchD = latchD;
		if ( v & 1 )
			latchD |= c; else
			latchD &= ~c;
		if ( oldLatchD == latchD )
			goto test;
	} else
	{
		i2cBufferCountLast = i2cBufferCountCur = 0;
	}
}

static __attribute__( ( always_inline ) ) inline void flush4BitBuffer( bool withoutCycleCounter = false )
{
	u64 armCycleCounter = 0;
	if ( !withoutCycleCounter )
	{
		RESTART_CYCLE_COUNTER
	}
	while ( !bufferEmptyI2C() )
	{
		prepareOutputLatch4Bit();
		outputLatch( withoutCycleCounter );
		if ( withoutCycleCounter )
		{
			//DELAY( 512 );
		} else
		{
			RESTART_CYCLE_COUNTER
			WAIT_UP_TO_CYCLE( 7000 )
		}
	}
	write32( ARM_GPIO_GPCLR0, ( 1 << LATCH_CONTROL ) );
	i2cBufferCountLast = i2cBufferCountCur = 0;
}

 
#endif

 