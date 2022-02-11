/*
  _________.__    .___      __   .__        __       ________            __________    _____      _____        ___ ________  ________   _____   ___    
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __  /  _____/  ____  ____\______   \  /  _  \    /     \      /  / \_____  \/  _____/  /  |  |  \  \   
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ / /   \  ____/ __ \/  _ \|       _/ /  /_\  \  /  \ /  \    /  /   /  ____/   __  \  /   |  |_  \  \  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <  \    \_\  \  ___(  <_> )    |   \/    |    \/    Y    \  (  (   /       \  |__\  \/    ^   /   )  ) 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \______  /\___  >____/|____|_  /\____|__  /\____|__  /   \  \  \_______ \_____  /\____   |   /  /  
        \/         \/    \/     \/       \/     \/         \/     \/             \/         \/         \/     \__\         \/     \/      |__|  /__/   

 neoramc16.cpp

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - simple test code for the GeoRAM-like memory expansion on the C16
 Copyright (c) 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

#define TEST_PATTERN 0x2000     // here we store some pattern which is copied to GeoRAM
#define CMP_PATTERN  0x2100     // and to this location we download it for comparison

#define NEORAM_WINDOW   0xfe00  // 128-byte window
#define NEORAM_R0       0xfde8  // chooses window within 16k block (0-63)
#define NEORAM_R1       0xfde9  // chooses 16k block (max. 4 MB i.e. 0-255)
#define NEORAM_R2       0xfdea  // a GeoRAM window is 256-byte, bit 7 in $fdea determines whether the lower or upper 128 byte are visible at $fe00

unsigned char hex[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

void neoSelect( unsigned char block, unsigned char window )
{
    *((unsigned char *)(NEORAM_R1)) = block;        // choose 16k block
    *((unsigned char *)(NEORAM_R0)) = window & 63;  // choose 256-byte window within block
    *((unsigned char *)(NEORAM_R2)) = 0;
}

void neoWriteWindow( unsigned char *src )
{
    *((unsigned char *)(NEORAM_R2)) = 0;
    memcpy( (void*)NEORAM_WINDOW, src, 128 );
    *((unsigned char *)(NEORAM_R2)) = 128;
    memcpy( (void*)NEORAM_WINDOW, src + 128, 128 );
}

void neoReadWindow( unsigned char *dst )
{
    *((unsigned char *)(NEORAM_R2)) = 0;
    memcpy( dst, (void*)NEORAM_WINDOW, 128 );
    *((unsigned char *)(NEORAM_R2)) = 128;
    memcpy( dst+128, (void*)NEORAM_WINDOW, 128 );
}

void eorWindow( unsigned char b )
{
    *((unsigned char *)(0xfd)) = b;
    *((unsigned char *)(0xfb)) = NEORAM_WINDOW & 255;
    *((unsigned char *)(0xfc)) = NEORAM_WINDOW >> 8;

    __asm__ ("ldx #$00");
__asm__ ("loopE:");
    __asm__ ("stx $fdea");  // select correct 128-byte sub-window
    
    __asm__ ("txa");        // offset in 128-byte window: x & 127
    __asm__ ("and #127");
    __asm__ ("tay");
    __asm__ ("lda ($fb),y");
    __asm__ ("eor $fd" ); 
    __asm__ ("sta ($fb),y");

    __asm__ ("inx");
    __asm__ ("bne loopE");
}

unsigned char compareEOR( unsigned char v )
{
    *((char *)(0x61)) = v;
    *((char *)(0x63)) = 1;
    *((char *)(0x62)) = 0;

    *((unsigned char *)(0xfb)) = TEST_PATTERN & 255;
    *((unsigned char *)(0xfc)) = TEST_PATTERN >> 8;
    *((unsigned char *)(0xfd)) = CMP_PATTERN & 255;
    *((unsigned char *)(0xfe)) = CMP_PATTERN >> 8;

    __asm__ ("ldy #$00");
__asm__ ("loop:");
    __asm__ ("lda ($fd),y");
    __asm__ ("eor $61" ); 
    __asm__ ("cmp ($fb),y");
    __asm__ ("bne error");
    
    __asm__ ("iny");
    __asm__ ("bne loop");

    return  *((char *)(0x63));

__asm__ ("error:");
    __asm__ ("sty $62");
    __asm__ ("lda #0");
    __asm__ ("sta $63");

    return  *((char *)(0x63));
}

int main (void)
{
    unsigned char b, w, c, ok, i,t;
    unsigned char *p1;
    
    const unsigned char nBlocks = 32; // 512kb
    const unsigned char nWindows = 64;

	// force single clock mode
	//*(unsigned char*)(0xff13) |= 2;

    (void) textcolor (COLOR_BLACK);
    clrscr();

    // generate some test pattern (will be xor-ed for every window below)
    b = 19;
    p1 = (unsigned char*)TEST_PATTERN;
    for ( w = 0; w < 128; w++ )
    {
        *( p1 ++ ) = b;
        b += 17;
        *( p1 ++ ) = b;
        b += 17;
    }

    gotoxy( 1, 1 );
    cprintf ("GeoRAM264 Test (512kb)" );

    for ( b = 0; b < nBlocks; b++ )
    {
        gotoxy( 1, 2 );
        cprintf ("block %d kb - %d kb", b * 16, b * 16 + 16 );
        gotoxy( 1, 4 ); cprintf( "................................" );
        gotoxy( 1, 5 ); cprintf( "................................" );
        gotoxy( 1, 6 ); cprintf( "................................" );

        for ( w = 0; w < nWindows; w++ )
        {
            neoSelect( b, w );
            gotoxy( 1+w/2, 4 ); cprintf( "w" ); 

            neoWriteWindow( (unsigned char*)TEST_PATTERN );
            eorWindow( w ); // xor all bytes in this window
        }

        for ( w = 0; w < nWindows; w++ )
        {
            neoSelect( b, w );

            // use this to enforce an error
            //*((char *)(NEORAM_WINDOW+33)) = 1;

            gotoxy( 1+w/2, 5 ); cprintf( "r" );
            neoReadWindow( (unsigned char*)CMP_PATTERN );

            gotoxy( 1+w/2, 6 ); cprintf( "c" );

            // compare (on-the-fly values in the reference are xor-ed, as the window has been before)
			ok = compareEOR( w );

            if ( !ok )
            {
				t = w;
                gotoxy( 1, 8 );
                cprintf ("error in block %d, window %d at ofs %d", b, w, *((char *)(0x62)) );

				i = *((char *)(0x62));
				if ( i < 6 ) i = 0; else i -= 6;
				if ( i > 256 - 13 ) i = 256 - 13;
                gotoxy( 1, 10 );
				cprintf( "reference:");
                gotoxy( 1, 11 );
				for ( w = i; w < i+13; w++ )
				{
					c = *((unsigned char *)(TEST_PATTERN+w)) ^ t;
					cprintf( "%c%c ", hex[c>>4], hex[c&15] );
				}
                gotoxy( 1, 13 );
				cprintf( "memory:");
                gotoxy( 1, 14 );
				for ( w = i; w < i+13; w++ )
				{
					cprintf( "%c%c ", hex[*((unsigned char *)(CMP_PATTERN+w))>>4], hex[*((unsigned char *)(CMP_PATTERN+w))&15] );
				}

                gotoxy( 1, 16 );
				cprintf( "offset:");
				gotoxy( 1, 17 );
				for ( w = i; w < i+13; w++ )
				{
					cprintf( "%c%c ", hex[(w)>>4], hex[(w)&15] );
				}

                return 1;
            }
        }
    }
    gotoxy( 0, 8 );
    cprintf ("passed!" );

    return 0;
}
