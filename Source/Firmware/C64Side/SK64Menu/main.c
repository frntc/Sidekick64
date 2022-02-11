/*
  _________.__    .___      __   .__        __           _____                         
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /     \   ____   ____  __ __   
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     /  \ /  \_/ __ \ /    \|  |  \  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     /    Y    \  ___/|   |  \  |  /  
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \____|__  /\___  >___|  /____/   
        \/         \/    \/     \/       \/     \/            \/     \/     \/         

 main.c

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
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
#include <conio.h>
#include <joystick.h>
#include <string.h>
#include <unistd.h>

#include "vdc.h"

#define RELOC_BASE	0x7000

// TODO diese Adresse ist vermutlich anderweitig belegt!
char *SIDKICK_VERSION = (char *)0xc800;
const char VERSION_STR_1[10] = {0x53, 0x49, 0x44, 0x4b, 0x09, 0x03, 0x0b, '0', '.', '1' };
const char VERSION_STR_2[10] = {0x53, 0x49, 0x44, 0x4b, 0x09, 0x03, 0x0b, '0', '.', '2' };

extern void detectC128();
extern void detectVIC();
extern void detectC64Model();

// currently not used
//unsigned char *jumpTable = (unsigned char *)0x3d0;

void readSIDKick_Version()
{
	__asm__ ("lda #255" );
    __asm__ ("sta $d41f");	// enter config mode

	__asm__ ("ldx #0");
	__asm__ ("ldy #224");

  __asm__ ("nextchar:");
	__asm__ ("tya");
    __asm__ ("sta $d41e");	
	__asm__ ("lda $d41d");
	__asm__ ("sta $c000,x");

	__asm__ ("iny");
	__asm__ ("inx");
	__asm__ ("cpx #$10");
	__asm__ ("bne nextchar");

	__asm__ ("lda #0");
	__asm__ ("sta $c000,x");
}


#define VK_LEFT		157
#define VK_RIGHT	29
#define VK_UP		145
#define VK_DOWN		17
#define VK_RETURN	13

const unsigned char readySignal[ 6 ] = { 0xf0, 0x0f, 0x88, 0x77, 0xaa, 0x55 };

int i, j, k, l;
char key, x, firstHit, ntsc_or_pal, vic_new_old;
unsigned char /*joy1, joy1prev, */joy2, joy2prev, sk64Command, a;

#define poke(addr,val)        (*(unsigned char*)(addr)) = (val)
#define peek(addr)        (*(unsigned char*)(addr))

//3->14->6->0
//13->5->11->0
const unsigned char fadeTab[ 16 ] = 
// 3 color ramp
{ 0, 15, 9, 14, 2, 11, 0, 13, 9, 0, 2, 0, 11, 5, 6, 12 };
//{ 0, 15, 0, 14, 6,11, 0, 5, 0, 0, 0, 0, 11, 7, 4, 12 };
// 4 color-ramp
// { 0, 15, 0, 14, 6,11, 0, 5, 0, 0, 0, 0, 11, 15, 4, 5 };

/*const unsigned char fadeTab2[ 16 ] = 
// 3 color ramp
{ 0, 15, 0, 14, 0,11, 6, 0, 0, 0, 0, 11, 11, 5, 6, 12 };*/

const unsigned char fadeInTab[ 16 ] = 
// 3 color ramp
{ 0, 15, 0, 3, 0,13, 14, 0, 0, 0, 0, 5, 11, 13, 3, 12 };
//{ 0, 15, 0, 14, 6,11, 0, 5, 0, 0, 0, 0, 11, 7, 4, 12 };
// 4 color-ramp
// { 0, 15, 0, 14, 6,11, 0, 5, 0, 0, 0, 0, 11, 15, 4, 5 };


extern void callSidekickCommand();
extern void loop();
extern void singleFrameUpdate();

extern void setupIRQ();
extern void setupNMI();
extern void restoreIRQ();
extern void detectPALNTSC();
extern void myIRQ();
extern void myNMI();
extern void minimalReset();
extern void initCharset();

extern void setupSpriteLogo();

typedef void (*func)();


const char col[] = { 10, 7, 7, 10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10 };

int main (void)
{
    *(unsigned char*)(0x01) = 55;
	__asm__ ("lda #$8" );
	__asm__ ("sta $d016");	

	// VIC bank
	x = peek( 0xdd00 );
	poke( 0xdd00, x | 3 );
	poke( 0xd018, 0x1c );

	// copy some code from cartridge to RAM
	// (the SK64 emulating the cartridge will not always be able to respond)	
	memcpy( (void*)RELOC_BASE, loop, 0x0280 );
	memcpy( (void*)(RELOC_BASE+0x2e0), callSidekickCommand, 0x0020 );
	memcpy( (void*)(RELOC_BASE+0x300), myIRQ, 0x0300 );
	memcpy( (void*)(RELOC_BASE+0x600), myNMI, 0x0020 );
	memcpy( (void*)(RELOC_BASE+0x620), singleFrameUpdate, 0x0080 );
	memcpy( (void*)(RELOC_BASE+0xd00), minimalReset, 0x0030 );
//	memcpy( (void*)0x5000, transferScreenVDC, 0x0400 );
	memcpy( (void*)0x5600, switch4080table, 28 );

/*	__asm__ ("ldx #15");
	__asm__ ("cpyloop:");
	__asm__ ("lda %v, x",vdcColorConversion);
	__asm__ ("sta $5400,x");
	__asm__ ("sta $5410,x");
	__asm__ ("sta $5420,x");
	__asm__ ("sta $5430,x");
	__asm__ ("sta $5440,x");
	__asm__ ("sta $5450,x");
	__asm__ ("sta $5460,x");
	__asm__ ("sta $5470,x");
	__asm__ ("sta $5480,x");
	__asm__ ("sta $5490,x");
	__asm__ ("sta $54a0,x");
	__asm__ ("sta $54b0,x");
	__asm__ ("sta $54c0,x");
	__asm__ ("sta $54d0,x");
	__asm__ ("sta $54e0,x");
	__asm__ ("sta $54f0,x");
	__asm__ ("ora #$80");
	__asm__ ("sta $5500,x");
	__asm__ ("sta $5510,x");
	__asm__ ("sta $5520,x");
	__asm__ ("sta $5530,x");
	__asm__ ("sta $5540,x");
	__asm__ ("sta $5550,x");
	__asm__ ("sta $5560,x");
	__asm__ ("sta $5570,x");
	__asm__ ("sta $5580,x");
	__asm__ ("sta $5590,x");
	__asm__ ("sta $55a0,x");
	__asm__ ("sta $55b0,x");
	__asm__ ("sta $55c0,x");
	__asm__ ("sta $55d0,x");
	__asm__ ("sta $55e0,x");
	__asm__ ("sta $55f0,x");
	__asm__ ("dex");
	__asm__ ("bpl cpyloop");*/

	if ( peek( 0x9ff6 ) ) // first time execution?
	{
  		detectC128();
		__asm__ ("tax");
		__asm__ ("lda $9ffe");		;// signal that we're sending a command + data
		__asm__ ("ldy #11");		;// command to send C128 detection result
		__asm__ ("lda $9d00,x");	;// implicitly send data
		__asm__ ("lda $9e00,y");	;// implicitly send command

		readSIDKick_Version();

		x = 0;
		if ( memcmp( (char*)0xc000, (char*)VERSION_STR_1, 10 ) == 0 ) x = 1;
		if ( memcmp( (char*)0xc000, (char*)VERSION_STR_2, 10 ) == 0 ) x = 2;

		{
			__asm__ ("ldx %v", x);
			__asm__ ("lda $9ffe");		;// signal that we're sending a command + data
			__asm__ ("lda $9d00,x");	;// implicitly send data
			__asm__ ("lda $9e0d");	    ;// implicitly send command "SIDKick detection"
		}

		// this value is used for timing-critical VIC-ultimax accesses on SK64
		detectVIC(); // returns 0 or 8 in A
		__asm__ ("sta %v", vic_new_old ); 
		__asm__ ("lda #$0b" );
		__asm__ ("sta $d011");	

		detectC64Model();
		__asm__ ("sta %v", ntsc_or_pal ); 
		if ( ntsc_or_pal == 0 )
			ntsc_or_pal = 4;
		
		// now we have for 'ntsc_or_pal'
		// 1 = old NTSC (262 rasterlines, 64cyc/line), 2 = new NTSC (263, 65), 3 = Drean PAL-N (312, 65), 4 = PAL (312, 63)

		vic_new_old |= ntsc_or_pal << 4;
		__asm__ ("ldx %v", vic_new_old ); 
		__asm__ ("lda $9ffe");		;// signal that we're sending a command + data
		__asm__ ("ldy #12");		;// command to send C128 detection result
		__asm__ ("lda $9d00,x");	;// implicitly send data
		__asm__ ("lda $9e00,y");	;// implicitly send command
	}

	//jumpTable[ 0 ] = 0x20; // JSR
	//jumpTable[ 0 ] = ( (unsigned int)testCall ) & 255;
	//jumpTable[ 1 ] = ( (unsigned int)testCall ) >> 8;
	//jumpTable[ 3 ] = 0x60; // RTS

	setupNMI();
	initCharset();
	setupSpriteLogo();

	if ( peek( 0x9fe1 ) )
	{
		initVDCOutput();
		copyCharsetVDC();
	}

	// y-coordinates of multiplexed sprites
	#define MPLEX_Y (99)
	for ( x = 0; x < 7; x++ )
		poke( RELOC_BASE+0x800 + 4 + x, MPLEX_Y + x * 21 );

	// SK logo top-right
	poke( RELOC_BASE+0x800 + 4 + 7, 50 );

	// table with addresses of sprites in the multiplexed background animation
	for ( x = 0; x < 16; x++ )
		poke( RELOC_BASE+0x820 + x, (x-6)*8+192-64 );

	// copy some lookup tables
	memset( (void*)(RELOC_BASE + 0x0e00 + 32 - 2), 0, 16 );
	memset( (void*)(RELOC_BASE + 0x0e00 + 78 - 2), 0, 16 );
	memset( (void*)(RELOC_BASE + 0x0e00 + 92), 0, 6 );
	poke( RELOC_BASE + 0x0e00 + 98, 0 );

	__asm__ ("ldx #2");	
	__asm__( "jsr %w", RELOC_BASE + 0x620 );

	// raster-IRQ will turn on screen
	setupIRQ();

	//
	// jump to mainloop (must reside in RAM)
	//
	__asm__ ("lda #1");	
	__asm__ ("sta $33e");	
	__asm__ ("lda #0");	
	__asm__ ("sta $33d");	
	__asm__ ("lda #1");	
	__asm__ ("sta $8ff");	
	__asm__ ("lda #0");	
	__asm__ ("sta $8fe");	
	__asm__ ("lda #0");	
	__asm__ ("sta $8fd");	
	__asm__ ( "jmp %w", RELOC_BASE );

    return 0;
}
