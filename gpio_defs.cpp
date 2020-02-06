/*
  __________________.___________   
 /  _____/\______   \   \_____  \  
/   \  ___ |     ___/   |/   |   \ 
\    \_\  \|    |   |   /    |    \
 \______  /|____|   |___\_______  /
        \/                      \/ 
 
 gpio_defs.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
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
#include "gpio_defs.h"
#include "lowlevel_arm64.h"

static void INP_GPIO( int pin )
{
	unsigned nSelReg = ARM_GPIO_GPFSEL0 + ( pin / 10 ) * 4;
	unsigned nShift = ( pin % 10 ) * 3;

	u32 nValue = read32 ( nSelReg );
	nValue &= ~( 7 << nShift );
	write32 ( nSelReg, nValue );
}

static void OUT_GPIO( int pin )
{
	unsigned nSelReg = ARM_GPIO_GPFSEL0 + ( pin / 10 ) * 4;
	unsigned nShift = ( pin % 10 ) * 3;

	u32 nValue = read32 ( nSelReg );
	nValue &= ~( 7 << nShift );
	nValue |= 1 << nShift;
	write32 ( nSelReg, nValue );
}

#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

#define GPPUD     37
#define GPPUDCLK0 38

#define ARM_GPIO_GPPUD		(ARM_GPIO_BASE + 0x94)
#define ARM_GPIO_GPPUDCLK0	(ARM_GPIO_BASE + 0x98)

#define PI_BANK (gpio>>5)
#define PI_BIT  (1<<(gpio&0x1F))

static void PULLUPDOWN_GPIO( u32 gpio, u32 pud )
{
	write32( ARM_GPIO_GPPUD, pud );

	BEGIN_CYCLE_COUNTER
	WAIT_UP_TO_CYCLE( 20000 )

	write32( ARM_GPIO_GPPUDCLK0 + PI_BANK, PI_BIT );

	RESTART_CYCLE_COUNTER
	WAIT_UP_TO_CYCLE( 20000 )

	write32( ARM_GPIO_GPPUD, 0 );
	write32( ARM_GPIO_GPPUDCLK0 + PI_BANK, 0 );
}

void gpioInit()
{
	// D0 - D7
	INP_GPIO( D0 );	INP_GPIO( D1 ); INP_GPIO( D2 );	INP_GPIO( D3 );
	INP_GPIO( D4 );	INP_GPIO( D5 );	INP_GPIO( D6 );	INP_GPIO( D7 );
	SET_BANK2_INPUT

	// A0-A7 (A8-A12, ROML, ROMH use the same GPIOs)
	INP_GPIO( A0 );	INP_GPIO( A1 );	INP_GPIO( A2 );	INP_GPIO( A3 );
	INP_GPIO( A4 ); INP_GPIO( A5 ); INP_GPIO( A6 ); INP_GPIO( A7 );

	// RW, PHI2, CS etc.
	INP_GPIO( RW );		INP_GPIO( CS );
	INP_GPIO( PHI2 );	INP_GPIO( RESET );
	INP_GPIO( IO1 );	INP_GPIO( IO2 );

	// GPIOs for controlling the level shifter
	INP_GPIO( GPIO_OE );
	OUT_GPIO( GPIO_OE );
	write32( ARM_GPIO_GPSET0, 1 << GPIO_OE );

	// ... and the multiplexer
	INP_GPIO( DIR_CTRL_257 );
	OUT_GPIO( DIR_CTRL_257 );
	write32( ARM_GPIO_GPCLR0, 1 << DIR_CTRL_257 );

	INP_GPIO( LATCH_CONTROL );
	OUT_GPIO( LATCH_CONTROL );
	write32( ARM_GPIO_GPCLR0, 1 << LATCH_CONTROL );

	INP_GPIO( EXROM );
	OUT_GPIO( EXROM );
	INP_GPIO( GAME );
	OUT_GPIO( GAME );
	
	INP_GPIO( NMI );
	OUT_GPIO( NMI );
	write32( ARM_GPIO_GPSET0, bNMI ); 

	INP_GPIO( DMA );
	OUT_GPIO( DMA );
	write32( ARM_GPIO_GPSET0, bDMA ); 

	for ( u32 i = 0; i < 32; i++ )
		PULLUPDOWN_GPIO( i, PI_PUD_OFF );
	
	write32( ARM_GPIO_GPSET0, bEXROM | bNMI | bGAME );
	write32( ARM_GPIO_GPCLR0, bCTRL257 ); 

	SET_BANK2_OUTPUT 
}

// decodes SID address and data from GPIOs using the above mapping
void decodeGPIO( u32 g, u8 *a, u8 *d )
{
	u32 A, D;

	A = ( g >> A0 ) & 31;
	D = ( g >> D0 ) & 255;

	#ifdef EMULATE_OPL2
	A |= ( ( g & bIO2 ) >> IO2 ) << 6;
	#endif

	*a = A; *d = D;
}

void decodeGPIOData( u32 g, u8 *d )
{
	*d = ( g >> D0 ) & 255;
}

// encodes one byte for the D0-D7-GPIOs using the above mapping
u32 encodeGPIO( u32 v )
{
	return ( v & 255 ) << D0;
}
