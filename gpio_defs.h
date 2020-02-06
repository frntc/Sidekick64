/*
  __________________.___________   
 /  _____/\______   \   \_____  \  
/   \  ___ |     ___/   |/   |   \ 
\    \_\  \|    |   |   /    |    \
 \______  /|____|   |___\_______  /
        \/                      \/ 
 
 gpio_defs.h

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
#ifndef _gpio_defs_h
#define _gpio_defs_h


// GPIO List									IC			GPIO name	comment
//  0 = EXROM									74LVC07		ID_SD		used to control EXROM
//  1 =	NMI										74LVC07		ID_SC		used to control NMI
//  2 = RESET (+ ROMH)							257 #3		SDA
//  3 = CS (SID+Kernal)							257 #3		SCL
//  4 = BUTTON (and PHI2)						257 #3		GCLK
//  5 = A0 (+ A8)    							257 #2					(freezer) button	
//  6 = A1 (+ A9)  								257 #2
//  7 = A2 (+ A10) 								257 #2		CE1
//  8 = A3 (+ A11)								257 #2		CE0
//  9 = A4 (+ A12)								257 #1		SPI_MISO
// 10 = A5 (+ ROML)								257 #1		SPI_MOSI
// 11 = A6 (+ BA)								257 #1		SPI_CLK
// 12 = A7 (+ IO2)								257 #1
// 13 = DMA		 								74LVC07	
// 14 = GPIO_OE												TXD0		(OE for D0-D7 LVC245 level shifter, direction controlled via C64-R/W)
// 15 = GAME									74LVC07		RXD0		used to control GAME
// 16 = latch LE (LATCH_CONTROL)
// 17 = PHI2									245 #1
// 18 = control multiplexers
// 19 = RW										257 #3
// 20 = D0										245 #2
// 21 = D1										245 #2
// 22 = D2										245 #2
// 23 = D3										245 #2
// 24 = D4										245 #2
// 25 = D5										245 #2
// 26 = D6										245 #2
// 27 = D7										245 #2

#define EXROM		0
#define GAME		15
#define NMI			1

// phi2 also on gpio4 via 257 (this may change something in the future)
#define PHI2 		17
#define RW			19
#define RESET		2
#define CS			3
#define IO1			RW

#define DMA			13
#define bDMA		(1<<DMA)

#define GPIO_OE			14	
#define LATCH_CONTROL	16
#define DIR_CTRL_257	18

// important: all data pins D0-D7 are assumed to be in GPIO-bank #2 (for faster read/write toggle)
#define D0	20
#define D1	21
#define D2	22
#define D3	23
#define D4	24
#define D5	25
#define D6	26
#define D7	27
#define D_FLAG ( (1<<D0)|(1<<D1)|(1<<D2)|(1<<D3)|(1<<D4)|(1<<D5)|(1<<D6)|(1<<D7) )

#define A0	5
#define A1	6
#define A2	7
#define A3	8
#define A4	9
#define A5	10	 
#define A6	11	 
#define A7	12	 
#define BUTTON	4
#define A8  A0
#define A9  A1
#define A10 A2
#define A11 A3
#define A12 A4
#define A_FLAG ( (1<<A0)|(1<<A1)|(1<<A2)|(1<<A3)|(1<<A4) )

#define IO2			A7

#define BA			A6
#define bBA			(1<<BA)

#define ROML		A5
#define ROMH		RESET


#define bRESET	(1<<RESET)
#define bCS		(1<<CS)
#define bRW		(1<<RW)
#define bPHI	(1<<PHI2)
#define bIO1	(1<<IO1)
#define bIO2	(1<<IO2)
#define bROML	(1<<ROML)
#define bROMH	(1<<ROMH)
#define bEXROM	(1<<EXROM)
#define bGAME	(1<<GAME)
#define bNMI 	(1<<NMI)
#define bCTRL257 (1<<DIR_CTRL_257)
#define bBUTTON  (1<<BUTTON)

#define ARM_GPIO_GPFSEL2	(ARM_GPIO_BASE + 0x08) 

// set bank 2 GPIOs to input (D0-D7)
#define SET_BANK2_INPUT { \
		const unsigned int b1 = ~( ( 7 << 0 ) | ( 7 << 3 ) | ( 7 << 6 ) | ( 7 << 9 ) | ( 7 << 12 ) | ( 7 << 15 ) | ( 7 << 18 ) | ( 7 << 21 ) ); \
		u32 t2 = read32( ARM_GPIO_GPFSEL2 ) & b1; \
		write32( ARM_GPIO_GPFSEL2, t2 ); }

// set bank 2 GPIOs to output (D0-D7)
#define SET_BANK2_OUTPUT { \
			const unsigned int b1 = ~( ( 7 << 0 ) | ( 7 << 3 ) | ( 7 << 6 ) | ( 7 << 9 ) | ( 7 << 12 ) | ( 7 << 15 ) | ( 7 << 18 ) | ( 7 << 21 ) ); \
			const unsigned int b2 = ( 1 << 0 ) | ( 1 << 3 ) | ( 1 << 6 ) | ( 1 << 9 ) | ( 1 << 12 ) | ( 1 << 15 ) | ( 1 << 18 ) | ( 1 << 21 ); \
			u32 t2 = read32( ARM_GPIO_GPFSEL2 ) & b1; \
			write32( ARM_GPIO_GPFSEL2, t2 | b2 ); }

void gpioInit();
void decodeGPIOData( u32 g, u8 *d );
void decodeGPIO( u32 g, u8 *a, u8 *d );
u32  encodeGPIO( u32 v );

#endif
