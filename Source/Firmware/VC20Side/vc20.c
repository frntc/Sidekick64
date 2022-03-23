/*
  _________.__    .___      __   .__        __           _____                         
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /     \   ____   ____  __ __   
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     /  \ /  \_/ __ \ /    \|  |  \  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     /    Y    \  ___/|   |  \  |  /  
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \____|__  /\___  >___|  /____/   
        \/         \/    \/     \/       \/     \/            \/     \/     \/         

 vc20.c

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

#include <string.h>

#include "waitnmi.h"

#define POKE(a, v) *((unsigned char *)(a)) = (v);

void setupScreen()
{
    __asm__ ("waitnz: ");
    __asm__ ("lda $9004");
    __asm__ ("beq waitnz");
    __asm__ ("waitlast:");
    __asm__ ("lda $9004");
    __asm__ ("beq gotlast");
    __asm__ ("tay");
    __asm__ ("bne waitlast");
    __asm__ ("gotlast:"); 
    __asm__ ("cpy #(312  - 1) / 2");
    __asm__ ("beq PALmachine");

    // screen positioning for NTSC
    __asm__ ("lda #2");
    __asm__ ("sta 36864");
    __asm__ ("lda #32");
    __asm__ ("sta 36865");
    
    return;

	// screen positioning for PAL
    __asm__ ("PALmachine:");
    __asm__ ("lda #10");
    __asm__ ("sta 36864");
    __asm__ ("lda #44");
    __asm__ ("sta 36865");
}

int main (void)
{
    POKE( 36866, 24 );  // # columns + video matrix address (bit 7)
    POKE( 36867, 21 );  // # rows, 16x8 chars (bit 0)
    POKE( 36869, 204 ); // characters in RAM at 4096

    memset( 4096, 0, 240 );
    memset( 37888, 0, 240 );

    POKE( 36879, 8+0*27 ); // background + border

    setupScreen();

    // main loop runs in casette buffer :-)
    memcpy( 0x33c, waitnmi, waitnmi_size );
    __asm__ ("jmp $33c"); 
}
