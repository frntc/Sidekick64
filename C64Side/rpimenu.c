/*
  _________.__    .___      __   .__        __           _____                            ___ _________   ________   _____   ___    
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /     \   ____   ____  __ __     /  / \_   ___ \ /  _____/  /  |  |  \  \   
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     /  \ /  \_/ __ \ /    \|  |  \   /  /  /    \  \//   __  \  /   |  |_  \  \  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     /    Y    \  ___/|   |  \  |  /  (  (   \     \___\  |__\  \/    ^   /   )  ) 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \____|__  /\___  >___|  /____/    \  \   \______  /\_____  /\____   |   /  /  
        \/         \/    \/     \/       \/     \/            \/     \/     \/           \__\         \/       \/      |__|  /__/    

 rpimenu.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - simple menu code running on the C64 (C128 and VIC detection, sending keypress, downloading screen)
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

#include <conio.h>
#include <joystick.h>
#include <string.h>

char *SIDKICK_VERSION = (char *)0xc000;
const char VERSION_STR[7] = {0x53, 0x49, 0x44, 0x4b, 0x09, 0x03, 0x0b };

extern void detectC128();
extern void detectVIC();

void updateScreen()
{
    __asm__ ("pha");
    __asm__ ("lda $fb");
    __asm__ ("pha");
    __asm__ ("lda $fc");
    __asm__ ("pha");
    __asm__ ("lda $fd");
    __asm__ ("pha");
    __asm__ ("lda $fe");
    __asm__ ("pha");

    __asm__ ("lda #250");
    __asm__ ("wait: cmp $d012");
    __asm__ ("bne wait");

    // copy screen (with color RAM)
    __asm__ ("lda #$01");
    __asm__ ("sta $df00");

    __asm__ ("lda #$00");
    __asm__ ("sta $fb");
    __asm__ ("lda #$04");
    __asm__ ("sta $fc");

    __asm__ ("lda #$00");
    __asm__ ("sta $fd");
    __asm__ ("lda #$d8");
    __asm__ ("sta $fe");


	__asm__ ("ldy #$00");
__asm__ ("loop:");
    __asm__ ("lda $df00");
    __asm__ ("sta ($fb),y");
    __asm__ ("lda $df01");
    __asm__ ("sta ($fd),y");
    __asm__ ("iny");
    __asm__ ("lsr");
    __asm__ ("lsr");
    __asm__ ("lsr");
    __asm__ ("lsr");
    __asm__ ("sta ($fd),y");
    __asm__ ("lda $df00");
    __asm__ ("sta ($fb),y");
    __asm__ ("iny");
    __asm__ ("bne loop");

    __asm__ ("inc $fe");
    __asm__ ("inc $fc");
    __asm__ ("lda $fc");
    __asm__ ("cmp #7");
    __asm__ ("bne loop");

__asm__ ("loop2:");
    __asm__ ("lda $df00");
    __asm__ ("sta ($fb),y");
    __asm__ ("lda $df01");
    __asm__ ("sta ($fd),y");
    __asm__ ("iny");
    __asm__ ("lsr");
    __asm__ ("lsr");
    __asm__ ("lsr");
    __asm__ ("lsr");
    __asm__ ("sta ($fd),y");
    __asm__ ("lda $df00");
    __asm__ ("sta ($fb),y");
    __asm__ ("iny");

    __asm__ ("tya");
    __asm__ ("cmp #232");
    __asm__ ("bne loop2");

    __asm__ ("pla");
    __asm__ ("sta $fe");
    __asm__ ("pla");
    __asm__ ("sta $fd");
    __asm__ ("pla");
    __asm__ ("sta $fc");
    __asm__ ("pla");
    __asm__ ("sta $fb");
    __asm__ ("pla");

    // execute code on the RPi (changed upper/lower case, border/bg color etc.)
    __asm__ ("jsr $df10");
}


void copyCharset()
{
    __asm__ ("pha");
    __asm__ ("lda $fb");
    __asm__ ("pha");
    __asm__ ("lda $fc");
    __asm__ ("pha");
    __asm__ ("lda $fd");
    __asm__ ("pha");
    __asm__ ("lda $fe");
    __asm__ ("pha");

    __asm__ ("sta $df04");

    __asm__ ("lda #$00");
    __asm__ ("sta $fb");
    __asm__ ("lda #$30");
    __asm__ ("sta $fc");

    __asm__ ("ldy #$00");
__asm__ ("loop:");
    __asm__ ("lda $df04");
    __asm__ ("sta ($fb),y");
    __asm__ ("iny");
    __asm__ ("bne loop");

    __asm__ ("inc $fc");
    __asm__ ("lda $fc");
    __asm__ ("cmp #$40");
    __asm__ ("bne loop");

    __asm__ ("pla");
    __asm__ ("sta $fe");
    __asm__ ("pla");
    __asm__ ("sta $fd");
    __asm__ ("pla");
    __asm__ ("sta $fc");
    __asm__ ("pla");
    __asm__ ("sta $fb");
    __asm__ ("pla");
}

// helps Sidekick64 detect if extra wires are connected
void wireDetection()
{
    __asm__ ("sta $df10");  // enable kernal replacement
    __asm__ ("lda $e000");  // access kernal (Sidekick64 will notice if wire connected)
    __asm__ ("sta $df11");  // disable kernal replacement

    __asm__ ("lda #$00");
    __asm__ ("sta $d4ff");  // some access to the SID address range
}

void readSIDKick_Version()
{
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

int main (void)
{
    char key, x, firstHit;
	unsigned char /*joy1, joy1prev, */joy2, joy2prev;

    *(unsigned char*)(0x01) = 15;

    detectC128();
    __asm__ ("sta $df02");

    detectVIC();
    __asm__ ("sta $df03");

    readSIDKick_Version();
    if ( memcmp( (char*)0xc000, (char*)VERSION_STR, 7 ) == 0 )
    {
        __asm__ ("lda #1");
        __asm__ ("sta $df05");
    } else
    {
        __asm__ ("lda #0");
        __asm__ ("sta $df05");
    }

    copyCharset();
    updateScreen();

	joy_install( joy_static_stddrv );

    wireDetection();
    *((char *)(0xdf01)) = 0; // dummy keypress
    //updateScreen();

    wireDetection();
    *((char *)(0xdf01)) = 0; // dummy keypress
    updateScreen();

	joy2prev = 255;
    while ( 1 )
    {
		key = 0;
		firstHit = 0;
		//joy1 = joy_read( JOY_1 );
		joy2 = joy_read( JOY_2 );
		if ( /*JOY_DOWN( joy1 )  || */JOY_DOWN( joy2 )  ) key = VK_DOWN; else
		if ( /*JOY_UP( joy1 )    || */JOY_UP( joy2 )    ) key = VK_UP; else
		if ( /*JOY_LEFT( joy1 )  || */JOY_LEFT( joy2 )  ) key = VK_LEFT; else
		if ( /*JOY_RIGHT( joy1 ) || */JOY_RIGHT( joy2 ) ) key = VK_RIGHT; else
		if ( /*JOY_BTN_1( joy1 ) || */JOY_BTN_1( joy2 ) ) key = 92;  // pound sign

		if ( /*joy1 == joy1prev || */joy2 == joy2prev && joy2 != 0 )
		{
			for ( x = 0; x < 2; x++ )
				waitvsync();
		} else
		if ( key )
			firstHit = 1;

		//joy1prev = joy1; 
		joy2prev = joy2;

		if ( key )
		{
			*(unsigned char*)0xc6 = 0;
		} else
		{
			if ( *(unsigned char*)0xc6 )	// buffer contains key?
			{
				 __asm__ ("jsr $e5b4");		// get it!
				 __asm__ ("sta %v", key ); 
			} else
                continue;
		}

        //*(unsigned char*)0xc6 = 0;

        // sendKeypress
        //key = cgetc();
        if ( key == 29 && *((char *)(0x0427)) != 0 )
        {
            __asm__ ("lda #$0a");
            __asm__ ("ldx #$20");
            __asm__ ("loop:");
            __asm__ ("sta $d850,x");
            __asm__ ("dex");
            __asm__ ("bne loop");
        }

        wireDetection();
        *((char *)(0xdf01)) = key;
		waitvsync();
        updateScreen();

		if ( firstHit )
			for ( x = 0; x < 15; x++ )
				waitvsync();
    }

    return 0;
}
