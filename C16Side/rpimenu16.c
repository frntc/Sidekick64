/*
  _________.__    .___      __   .__        __        _____                            ___ ________  ________   _____   ___    
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   /     \   ____   ____  __ __     /  / \_____  \/  _____/  /  |  |  \  \   
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /  /  \ /  \_/ __ \ /    \|  |  \   /  /   /  ____/   __  \  /   |  |_  \  \  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <  /    Y    \  ___/|   |  \  |  /  (  (   /       \  |__\  \/    ^   /   )  ) 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \ \____|__  /\___  >___|  /____/    \  \  \_______ \_____  /\____   |   /  /  
        \/         \/    \/     \/       \/     \/         \/     \/     \/           \__\         \/     \/      |__|  /__/   

 rpimenu16.cpp

 RasPiC64 - A framework for interfacing the C64 (and the C16/+4) and a Raspberry Pi 3B/3B+
          - simple menu code running on the 264-series (NTSC/PAL and memory size detection, sending keypress, downloading screen)
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

#include <stdlib.h>
#include <string.h>
#include <conio.h>

#define CHARSET     0x6000
#define CHARADR     ((CHARSET >> 8) & 0xFC)

unsigned char test;

void updateScreen()
{
    // wait for rasterline 
    __asm__ ("wait:"); 
    __asm__ ("lda $ff1c");
    __asm__ ("beq wait");
    __asm__ ("lda $ff1d");
    __asm__ ("cmp #50");
    __asm__ ("bne wait");

	// copy color RAM
    __asm__ ("lda #$01");
    __asm__ ("sta $fde5");

    __asm__ ("lda #$00");
    __asm__ ("sta $fc");
    __asm__ ("lda #$08");
    __asm__ ("sta $fd");

    __asm__ ("ldy #$00");
__asm__ ("loopc:");
    __asm__ ("lda $fde5");
    __asm__ ("sta ($fc),y");
    __asm__ ("iny");
    __asm__ ("bne loopc");

    __asm__ ("inc $fd");
    __asm__ ("lda $fd");
    __asm__ ("cmp #$0c");
    __asm__ ("bne loopc");	

    __asm__ ("ldy #$00");
__asm__ ("loop2c:");
    __asm__ ("lda $fde5");
    __asm__ ("sta ($fc),y");
    __asm__ ("iny");
    __asm__ ("tya");
    __asm__ ("cmp #232");
	__asm__ ("bne loop2c");

	// copy screen RAM
    __asm__ ("lda #$00");
    __asm__ ("sta $fde5");

    __asm__ ("lda #$00");
    __asm__ ("sta $fc");
    __asm__ ("lda #$0c");
    __asm__ ("sta $fd");

    __asm__ ("ldy #$00");
__asm__ ("loop:");
    __asm__ ("lda $fde5");
    __asm__ ("sta ($fc),y");
    __asm__ ("iny");
    __asm__ ("bne loop");

    __asm__ ("inc $fd");
    __asm__ ("lda $fd");
    __asm__ ("cmp #$0f");
    __asm__ ("bne loop");	

    __asm__ ("ldy #$00");
__asm__ ("loop2:");
    __asm__ ("lda $fde5");
    __asm__ ("sta ($fc),y");
    __asm__ ("iny");
    __asm__ ("tya");
    __asm__ ("cmp #232");
	__asm__ ("bne loop2");

    __asm__ ("lda $fde5");
    __asm__ ("sta $ff13");	// char address in RAM
    __asm__ ("lda $fde5");
    __asm__ ("sta $ff15");	// background color
    __asm__ ("lda $fde5");
    __asm__ ("sta $ff19");	// border color
}

void copyCharset()
{
    __asm__ ("lda #$02");
    __asm__ ("sta $fde5");

    __asm__ ("lda #$00");
    __asm__ ("sta $fc");
    __asm__ ("lda #$60");
    __asm__ ("sta $fd");

    __asm__ ("ldy #$00");
__asm__ ("loop:");
    __asm__ ("lda $fde5");
    __asm__ ("sta ($fc),y");
    __asm__ ("iny");
    __asm__ ("bne loop");

    __asm__ ("inc $fd");
    __asm__ ("lda $fd");
    __asm__ ("cmp #$70");
    __asm__ ("bne loop");
}

extern void wait4IRQ();

int main (void)
{
	unsigned char machine, a, b, c, d;

	// force single clock mode
	*(unsigned char*)(0xff13) |= 2;

    __asm__ ("lda #$0");
    __asm__ ("sta $ff15");	// background color
    __asm__ ("sta $ff19");	// border color

	// detect 16 or 64kb
	machine = 0;
	*(unsigned char*)(0x3fff) = 0xff;
	*(unsigned char*)(0x7fff) = 0xff;
	*(unsigned char*)(0x3fff) = 0x00;
	if ( *(unsigned char*)(0x7fff) == 0 )
		machine = 0; else  	// 16k
		machine = 1;		// 64k

	// read PAL/NTSC flag
	a = *(unsigned char*)(0xff07) >> 6; // 0 == PAL, 1 == NTSC
	machine |= a << 1;

	*(unsigned char*)(0xfde5) = machine + 252;

	copyCharset();

	// charset in ram
	*(unsigned char*)(0xff12) &= ~4;

	// charset address
	*(unsigned char*)(0xff13) = CHARADR;

	// at first run: upload dummy keypress to refresh screen
	a = 20;

	while ( 1 )
	{
        __asm__ ("lda %v", a); 
        wait4IRQ(); // also uploads the keypress stored in A

        // this is a very hacky solution:
        // - on the C16, Sidekick264 can reset the machine on the expansion port
        // - on the +4 this is not possible, and thus the following code checks whether the menu is still running...
        //   + if not, then it tests for a while if one of the sub-kernels on the RPi have launched and then resets the machine
        if ( *(unsigned char*)(0xfd91) != 1 || *(unsigned char*)(0xfd92) != 2 )
        {
            d = 3;
            for ( a = 0; a < 255; a++ )
            {
                for ( b = 0; b < 32; b++ )
                {
                    for ( c = 0; c < 8; c++ )
                    {
                        __asm__ ("wait:");
                        __asm__ ("lda $ff1d");
                        __asm__ ("cmp #250");
                        __asm__ ("bne wait");
                    }

                    // fade out color RAM
                    __asm__ ("lda #$01");
                    __asm__ ("sta $fde5");

                    __asm__ ("lda #$00");
                    __asm__ ("sta $fc");
                    __asm__ ("lda #$08");
                    __asm__ ("sta $fd");

                    __asm__ ("ldy #$00");
                __asm__ ("loopfc:");
                    __asm__ ("lda ($fc),y");
                    __asm__ ("and #15");
                    __asm__ ("sta $ff");
                    __asm__ ("lda ($fc),y");
                    __asm__ ("cmp #16");
                    __asm__ ("bmi skipfc");
                    __asm__ ("clc");
                    __asm__ ("sbc #16");
                __asm__ ("skipfc:");
                    __asm__ ("ora $ff");
                    __asm__ ("sta ($fc),y");
                    
                    __asm__ ("iny");
                    __asm__ ("bne loopfc");

                    __asm__ ("inc $fd");
                    __asm__ ("lda $fd");
                    __asm__ ("cmp #$0c");
                    __asm__ ("bne loopfc");	

                    if ( *(unsigned char*)(0xfd91) == 2 
                        && *(unsigned char*)(0xfd92) == 3
                        && *(unsigned char*)(0xfd93) == 4
                        && *(unsigned char*)(0xfd94) == 5
                        && *(unsigned char*)(0xfd95) == 6 
                        && d -- == 0 )
                        goto doReset;
                }
            }
           

        doReset:

            // on a Plus4 we take the chance and warm up the RPi's cache before we reset
            __asm__ ("lda #$02");
            __asm__ ("sta $FDD2");
            {
                __asm__ ("ldy #$00");
            __asm__ ("loop_hackyread1:");
                __asm__ ("ldx #$00");
            __asm__ ("loop_hackyread:");
                __asm__ ("lda $8000,x");
                __asm__ ("inx");
                __asm__ ("bne loop_hackyread");
                __asm__ ("iny");
                __asm__ ("bne loop_hackyread1");
            }

            __asm__ ("jmp $f2a4" ); 
        }

		updateScreen();
		a = cgetc();
	}

    return EXIT_SUCCESS;
}
