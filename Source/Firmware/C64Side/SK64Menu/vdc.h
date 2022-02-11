/*
  _________.__    .___      __   .__        __           _____                         
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /     \   ____   ____  __ __   
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     /  \ /  \_/ __ \ /    \|  |  \  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     /    Y    \  ___/|   |  \  |  /  
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \____|__  /\___  >___|  /____/   
        \/         \/    \/     \/       \/     \/            \/     \/     \/         

 vdc.h

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

unsigned char _reg, _d;

extern void transferScreenVDC();

void vdcWrite( unsigned char reg, unsigned char d )
{
    _reg = reg; _d = d;
    __asm__ ("ldx %v", _reg );
    __asm__ ("lda %v", _d );
    __asm__ ("stx $d600" );
    __asm__ ("wait:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl wait" );
    __asm__ ( "sta $d601" );
}

unsigned char vdcRead( unsigned char reg )
{
    _reg = reg; 
    __asm__ ("ldx %v", _reg );
    __asm__ ("stx $d600" );
    __asm__ ("wait2:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl wait2" );
    __asm__ ( "lda $d601" );
    __asm__ ( "sta %v", _d );
    return _d;
}

// interpret Y-X (X = low byte) as 16 bit value and shift left one bit
#define SHL_YX \
        __asm__ ( "txa" ); \
        __asm__ ( "clc" ); \
        __asm__ ( "rol" ); \
        __asm__ ( "tax" ); \
        __asm__ ( "tya" ); \
        __asm__ ( "rol" ); \
        __asm__ ( "tay" ); 


const unsigned char vdcInitTable[37] = {
    0x7e, 0x30, 0x56, 0x49, 0x20, 0x00, 0x19, 0x1d, 
    0x00, 0x07, 0xa0, 0xe7, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x08, 0x20, //undef
    0x08, 0x00, 0x78, 0xe8, 0x20, 0x40+7, 0xf0, 0x00, 
    0x2f, 0xe7, 
    0x00, 0x00, 0x00, 0x00, //undef
    0x7d, 0x64, 0x05    
};

const unsigned char switch4080table[28] = 
{
    0, 63, 1, 40, 2, 55, 3, 0x45, 22, 0x89, 25, 64+16+7, 27, 0,
    0, 0x7e, 1, 0x30*0+40, 2, 0x56, 3, 0x49, 22, 0x78, 25, 0x40+7, 27, 0
};

const unsigned char vdcColorConversion[ 16 ] = {
//    0x00, 0x0f, 0x08, 0x07, 0x0b, 0x04, 0x02, 0x0d, 0x0a, 0x0c, 0x09, 0x06, 0x01, 0x05, 0x03, 0x0e
    0x00, 0x0f, 0x08, 0x07, 0x0a, 0x04, 0x02, 0x0d, 0x0e, 0x0c, 0x09, 0x01, 0x01, 0x05, 0x03, 0x0e
};

const char welcomeString[] = "press \x1e or shift-\x1e for VDC output!";

void initVDCOutput()
{
    unsigned char i;

    // already includes attribute address (8,0) and screen ram (0,0)
    for ( i = 0; i < 37; i++ )
        vdcWrite( i, vdcInitTable[ i ] );

    #if 1
    // clear VDC screen once
    //7x 256 + 208
    vdcWrite( 18, 0 );
    vdcWrite( 19, 0 );
    __asm__ ( "lda #8" );
    __asm__ ( "sta $fb" );

  __asm__ ("outerloop_clr:");
    __asm__ ( "ldx #31" );
    __asm__ ( "lda #32" );
    __asm__ ( "ldy #0" );
    __asm__ ( "cl:" );
    __asm__ ( "stx $d600" );
    __asm__ ("wait_clr:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl wait_clr" );
    __asm__ ( "sta $d601" );
    __asm__ ( "dey" );
    __asm__ ( "bne cl" );
    __asm__ ( "dec $fb" );
    __asm__ ( "lda $fb" );
    __asm__ ( "bne outerloop_clr" );

    __asm__ ( "ldy #208" );
    __asm__ ( "cl2:" );
    __asm__ ( "stx $d600" );
    __asm__ ("wait_clr2:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl wait_clr2" );
    __asm__ ( "sta $d601" );
    __asm__ ( "dey" );
    __asm__ ( "bne cl2" );

    // and clear attributes
    vdcWrite( 18, 8 );
    vdcWrite( 19, 0 );

    __asm__ ( "lda #8" );
    __asm__ ( "sta $fb" );

  __asm__ ("outerloop_clr_attr:");
    __asm__ ( "ldx #31" );
    __asm__ ( "ldy #0" );
    __asm__ ( "cl_attr:" );
    __asm__ ( "stx $d600" );
    __asm__ ( "lda #(7+128)" );
    __asm__ ("wait_clr_attr:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl wait_clr_attr" );
    __asm__ ( "sta $d601" );
    __asm__ ( "dey" );
    __asm__ ( "bne cl_attr" );
    __asm__ ( "dec $fb" );
    __asm__ ( "bne outerloop_clr_attr" );

    __asm__ ( "ldy #208" );
    __asm__ ( "cl2_attr:" );
    __asm__ ( "stx $d600" );
    __asm__ ("wait_clr2_attr:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl wait_clr2_attr" );
    __asm__ ( "sta $d601" );
    __asm__ ( "dey" );
    __asm__ ( "bne cl2_attr" );

#endif

    // set VDC write address
    vdcWrite( 18, 0 );
    vdcWrite( 19, 6 + 48 );
    for ( i = 0; i < 34; i++ ) vdcWrite( 31, welcomeString[ i ] );
}

void copyCharsetVDC()
{
    vdcWrite( 18, 32 );
    vdcWrite( 19, 0 );
    __asm__ ( "lda #$00" );
    __asm__ ( "sta $fb" );
    __asm__ ( "lda #$30" );
    __asm__ ( "sta $fc" );
    __asm__ ( "lda #2" ); // number of charsets
    __asm__ ( "sta $fd" );

    __asm__ ( "lda #0" ); // number of chars to copy (0=256)
    __asm__ ( "sta $fe" );

    __asm__ ( "copy_one_char:" );

    __asm__ ( "ldx #31" );

    __asm__ ( "ldy #0" );
    __asm__ ( "cplf:" );
    __asm__ ( "stx $d600" );
    __asm__ ( "lda ($fb),y" );
    __asm__ ("waitf:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl waitf" );
    __asm__ ( "sta $d601" );
    __asm__ ( "iny" );
    __asm__ ( "cpy #8" );    
    __asm__ ( "bne cplf" );

    __asm__ ( "lda #0" );
    __asm__ ( "ldy #8" );
    __asm__ ( "cplf2:" );
    __asm__ ( "stx $d600" );
    __asm__ ("waitf2:");
    __asm__ ( "bit $d600" );
    __asm__ ( "bpl waitf2" );
    __asm__ ( "sta $d601" );
    __asm__ ( "dey" );
    __asm__ ( "bne cplf2" );

    // increment char pointer
    __asm__ ( "lda $fb" );
    __asm__ ( "ldx $fc" );
    __asm__ ( "clc" );
    __asm__ ( "adc #8" );
    __asm__ ( "sta $fb" );
    __asm__ ( "txa" );
    __asm__ ( "adc #0" );
    __asm__ ( "sta $fc" );

    __asm__ ( "dec $fe" );
    __asm__ ( "bne copy_one_char" );

    __asm__ ( "dec $fd" );
    __asm__ ( "bne copy_one_char" );
}