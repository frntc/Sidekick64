/*
  _________.__    .___      __   .__        __      _______________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __  \_____  \   _  \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /  ____/  /_\  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <   /       \  \_/   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \_______ \_____  /
        \/         \/    \/     \/       \/     \/          \/     \/  
 
 vic656x_inline.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick VIC: VIC6560/6561 emulation code for drop-in replacement and parallel operation
			- Based on many insights, and partly on adapted code, from the VICE source code (https://vice-emu.sourceforge.io)
 Own contributions/developments Copyright (c) 2019-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

#include <arm_neon.h>

#define STRETCH_X
#define SCANLINE

#define INTERLEAVED_SOUND_EMULATION

//#define NO_FILTER
#define OFFSET_X	5

#define VFLI_COLORRAM_4BIT

#define RGBA565(R, G, B) (((R>>3) & 0x1F) << 11 | ((G>>2) & 0x3F) << 5 | ((B>>3) & 0x1F))

u8 VIC20_palettePAL_Even[] = 
{
	0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF,
	0x94, 0x27, 0x41,
	0x6A, 0xED, 0xB8,
	0x98, 0x39, 0xCC,
	0x6E, 0xE1, 0x35,
	0x2C, 0x3A, 0xC0,
	0xE1, 0xE6, 0x31,
	0xC7, 0x6C, 0x39,
	0xD3, 0xC1, 0x80,
	0xCF, 0xA9, 0x8C,
	0xC2, 0xEC, 0xFF,
	0xDE, 0xA3, 0xCE,
	0xAD, 0xF2, 0xC0,
	0xB1, 0x9B, 0xEC,
	0xE6, 0xFF, 0xBB
};

u8 VIC20_palettePAL_Odd[] = 
{
	0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF,
	0x98, 0x2D, 0x18,
	0x7A, 0xD7, 0xFF,
	0xB1, 0x36, 0xA2,
	0x5A, 0xDD, 0x82,
	0x50, 0x2C, 0xA7,
	0xD0, 0xE6, 0x5C,
	0xBC, 0x77, 0x1F,
	0xE6, 0xAE, 0xAF,
	0xD9, 0x9A, 0xBE,
	0xB3, 0xFB, 0xDD,
	0xD2, 0xA2, 0xF5,
	0xBA, 0xF2, 0xA1,
	0xA2, 0xA6, 0xDA,
	0xFF, 0xF1, 0xBF
};


u8 VIC20_palettePAL[] = 
{
	0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF,
	0xB6, 0x1F, 0x21,
	0x4D, 0xF0, 0xFF,
	0xB4, 0x3F, 0xFF,
	0x44, 0xE2, 0x37,
	0x1A, 0x34, 0xFF,
	0xDC, 0xD7, 0x1B,
	0xCA, 0x54, 0x00,
	0xE9, 0xB0, 0x72,
	0xE7, 0x92, 0x93,
	0x9A, 0xF7, 0xFD,
	0xE0, 0x9F, 0xFF,
	0x8F, 0xE4, 0x93,
	0x82, 0x90, 0xFF,
	0xE5, 0xDE, 0x85
};

u8 VIC20_paletteNTSC[] = 
{
/*	0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF,
	0xF9, 0x11, 0x37,
	0x35, 0xF9, 0xF6,
	0xFF, 0x3C, 0xC6,
	0x3C, 0xED, 0xA9,
	0x0F, 0x57, 0xF7,
	0xFE, 0xE9, 0x63,
	0xFB, 0x62, 0x44,
	0xFB, 0xBF, 0xDE,
	0xF3, 0xAC, 0xE5,
	0xA8, 0xEA, 0xDD,
	0xE6, 0xB8, 0xF7,
	0xAB, 0xDD, 0xA4,
	0x6A, 0xB3, 0xE7,
	0xF7, 0xDA, 0xA5*/
	
	// https://www.forum64.de/index.php?thread/116431-vc20-farbpalette-als-rgb-werte/&postID=1811974#post1811974
	/*0x01, 0x00, 0x01,
	0xff, 0xff, 0xfd,
	0x99, 0x17, 0x00,
	0x5c, 0xd1, 0xff,
	0xc5, 0x18, 0xbb,
	0x1f, 0xf1, 0x55,
	0x21, 0x17, 0xe9,
	0xfb, 0xeb, 0x00,
	0xdc, 0x55, 0x00,
	0xff, 0x9c, 0x99,
	0xf6, 0x89, 0xac,
	0x9e, 0xfe, 0xfe,
	0xeb, 0x8e, 0xff,
	0x98, 0xff, 0x78,
	0x7b, 0x9f, 0xff,
	0xff, 0xf3, 0x77*/

	// https://www.forum64.de/index.php?thread/116431-vc20-farbpalette-als-rgb-werte/&postID=1812077#post1812077
	0x00, 0x00, 0x00,
	0xfd, 0xfe, 0xfd,
	0x7c, 0x27, 0x00,
	0x6f, 0xb5, 0xff,
	0xc4, 0x1d, 0x79,
	0x12, 0xde, 0x9a,
	0x3a, 0x08, 0xd2,
	0xc0, 0xfb, 0x00,
	0xb2, 0x68, 0x00,
	0xf9, 0xa0, 0x6e,
	0xe5, 0x8b, 0x84,
	0x9e, 0xeb, 0xff,
	0xec, 0x86, 0xfa,
	0x83, 0xfc, 0x92,
	0x86, 0x8d, 0xff,
	0xff, 0xf7, 0x5a	
};

typedef struct
{
	// PAL/NTSC model specific settings
	u32 clock;
	s16 hCycles,
		vLines;
	s16 vOffset,
		maxTextCols;

	// registers and VIC states
	u8  regs[ 16 ];
	u8	busLastData, busLastDataHigh;
	s16 hCount, vCount;

	u8  curArea, 
		fetchState,
		nTextRows, 
		nTextCols,
		curTextCols,
		curTextRow, 
		charHeight;

	u8	lineWasBlank,
		rasterBlankThisLine;

	u16 memPtr, memPtrInc;
	u8  bufOffset;
	u16 lastChar;
	
	// raster position related states
	s16 startX, stopX,
		prevCoordX, prevStopX;
	u16 rasterCounterY;
	u8  prevArea;

	// color states (cur + new due to delayed propagation), LUT for faster rendering
	u8  color, pixelShift, colorInvert, colorInvertNew;
	u32 colorBGNew, colorBG, colorBRDNew, colorBRD, colorAUXNew, colorAUX;
	u32 colorLUT[ 12 ];

	// borders
	u16 leftBorder, rightBorder;
	
	// VIC audio
	float filterHighPassBuf,
		  filterLowPassBuf,
		  filterHighPassFactor,
		  filterLowPassFactor;

	u16 noise_LFSR;
	u8	noise_LFSR0_old, 
		ch_shift[ 4 ],
		ch_out[ 4 ];
	int ch_ctr[ 4 ];

	u8  updChannel, accChannel;
	u16 sampleAcc, sampleNAcc;

    int cyclesToNextSample,
		cyclesSampleCounter;
    s16 curSampleOutput;
	u8	hasSampleOutput;
} __attribute__((packed)) VICSTATE;


// PAL
#define VIC20_PAL_CLOCK				1108405
#define VIC20_PAL_H_CYCLES			71
#define VIC20_PAL_V_LINES			312
#define VIC20_PAL_MAX_TEXT_COLS		32
#define VIC20_PAL_FIRST_LINE        28
#define VIC20_PAL_LAST_LINE         311

#define VIC20_NTSC_CLOCK			1022727
#define VIC20_NTSC_H_CYCLES			65
#define VIC20_NTSC_V_LINES			261
#define VIC20_NTSC_MAX_TEXT_COLS	31
#define VIC20_NTSC_FIRST_LINE       28
#define VIC20_NTSC_LAST_LINE        261

#define VIC20_BUS_OFFSET			4
#define VIC20_PIXELS_PER_TICK		4

#define VIC20_FIXEDPOINT_SCALE		256

volatile VICSTATE vic656x AAA;

__attribute__( ( always_inline ) ) inline void initVIC656x( bool VIC20PAL = true, bool debugBorders = false )
{
	memset( (void*)&vic656x, 0, sizeof( vic656x ) );

	if ( VIC20PAL )
	{
		vic656x.clock = VIC20_PAL_CLOCK;
		vic656x.hCycles = VIC20_PAL_H_CYCLES;
		vic656x.vLines = VIC20_PAL_V_LINES;
		vic656x.maxTextCols = VIC20_PAL_MAX_TEXT_COLS;
		vic656x.vOffset = -46;

		if ( debugBorders )
		{
			vic656x.leftBorder = 0;
			vic656x.rightBorder = 255;
		} else
		{
			vic656x.leftBorder = 11; // actually 2 more
			vic656x.rightBorder = 61 + OFFSET_X;
		}
	} else
	{
		vic656x.clock = VIC20_NTSC_CLOCK;
		vic656x.hCycles = VIC20_NTSC_H_CYCLES;
		vic656x.vLines = VIC20_NTSC_V_LINES;
		vic656x.maxTextCols = VIC20_NTSC_MAX_TEXT_COLS;
		vic656x.vOffset = -VIC20_NTSC_FIRST_LINE;

		if ( debugBorders )
		{
			vic656x.leftBorder = 0;
			vic656x.rightBorder = 255;
		} else
		{
			// todo: need to figure out correct values
			vic656x.leftBorder = 7;
			vic656x.rightBorder = 50 + OFFSET_X;
		}
	}

	vic656x.charHeight = 8;

	vic656x.filterHighPassBuf =
	vic656x.filterLowPassBuf  = 0.0f;
	
	#define dt ( 1.0f / 32000.0f )
	vic656x.filterHighPassFactor = dt / ( dt + 1e-3f );
	vic656x.filterLowPassFactor  = dt / ( dt + 1e-4f );

	vic656x.updChannel = 0;
	vic656x.accChannel = 0;
	vic656x.cyclesToNextSample  = ( vic656x.clock * VIC20_FIXEDPOINT_SCALE ) / 32000;
	vic656x.cyclesSampleCounter = vic656x.cyclesToNextSample;
}


#define VIC_AREA_IDLE		0
#define VIC_AREA_PENDING	1
#define VIC_AREA_DONE		2
#define VIC_AREA_DISPLAY	3

#define VIC_FETCH_IDLE		0
#define VIC_FETCH_START		1
#define VIC_FETCH_DONE		2
#define VIC_FETCH_MATRIX	3
#define VIC_FETCH_CHARGEN	4

__attribute__( ( always_inline ) ) inline void updateOnRegisterChangesVIC656x()
{
	vic656x.charHeight = ( vic656x.regs[ 3 ] & 1 ) ? 16 : 8;
	vic656x.colorInvertNew = ( vic656x.regs[ 15 ] & 8 ) == 0;

	#ifdef STRETCH_X
		vic656x.colorBGNew = ( vic656x.regs[ 15 ] >> 4 ) & 0xF;
		vic656x.colorBGNew |= vic656x.colorBGNew << 8;
		vic656x.colorBGNew |= vic656x.colorBGNew << 16;
		vic656x.colorBG = ( vic656x.colorBG & 0xffff ) | ( vic656x.colorBGNew & 0xffff0000 );

		vic656x.colorBRDNew = vic656x.regs[ 15 ] & 7;
		vic656x.colorBRDNew |= vic656x.colorBRDNew << 8;
		vic656x.colorBRDNew |= vic656x.colorBRDNew << 16;
		vic656x.colorBRD = ( vic656x.colorBRD & 0xffff ) | ( vic656x.colorBRDNew & 0xffff0000 );

		vic656x.colorAUXNew = ( vic656x.regs[ 14 ] >> 4 ) & 0xF;
		vic656x.colorAUXNew |= vic656x.colorAUXNew << 8;
		vic656x.colorAUXNew |= vic656x.colorAUXNew << 16;
		vic656x.colorAUX = ( vic656x.colorAUX & 0xffff ) | ( vic656x.colorAUXNew & 0xffff0000 );

		vic656x.colorLUT[ 0 ] = vic656x.colorBG;
		vic656x.colorLUT[ 1 ] = vic656x.colorBRD;
		vic656x.colorLUT[ 2 ] = 0;
		vic656x.colorLUT[ 3 ] = vic656x.colorAUX;

		vic656x.colorLUT[ 4 ] = vic656x.colorBGNew;
		vic656x.colorLUT[ 5 ] = vic656x.colorBRDNew;
		vic656x.colorLUT[ 6 ] = 0;
		vic656x.colorLUT[ 7 ] = vic656x.colorAUXNew;
	#else
		vic656x.colorBGNew = ( vic656x.regs[ 15 ] >> 4 ) & 0xF;
		vic656x.colorBGNew |= vic656x.colorBGNew << 8;
		vic656x.colorBG = ( vic656x.colorBG & 0xff ) | ( vic656x.colorBGNew & 0xff00 );

		vic656x.colorBRDNew = vic656x.regs[ 15 ] & 7;
		vic656x.colorBRDNew |= vic656x.colorBRDNew << 8;
		vic656x.colorBRD = ( vic656x.colorBRD & 0xff ) | ( vic656x.colorBRDNew & 0xff00 );

		vic656x.colorAUXNew = ( vic656x.regs[ 14 ] >> 4 ) & 0xF;
		vic656x.colorAUXNew |= vic656x.colorAUXNew << 8;
		vic656x.colorAUX = ( vic656x.colorAUX & 0xff ) | ( vic656x.colorAUXNew & 0xff00 );

		vic656x.colorLUT[ 0 ] = vic656x.colorBG;
		vic656x.colorLUT[ 1 ] = vic656x.colorBRD;
		vic656x.colorLUT[ 2 ] = 0;
		vic656x.colorLUT[ 3 ] = vic656x.colorAUX;

		vic656x.colorLUT[ 4 ] = vic656x.colorBGNew;
		vic656x.colorLUT[ 5 ] = vic656x.colorBRDNew;
		vic656x.colorLUT[ 6 ] = 0;
		vic656x.colorLUT[ 7 ] = vic656x.colorAUXNew;
	#endif
}

__attribute__( ( always_inline ) ) inline void writeRegisterVIC656x( u8 addr, u8 D )
{
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	vic656x.regs[ addr ] = D;
	updateOnRegisterChangesVIC656x();
	#pragma GCC diagnostic pop
}

extern u8 cart_ram[ 1024 * ( 52 + 16 + 4 ) ];
extern u8 *charROM;

#ifdef VFLI_COLORRAM_4BIT
#define VFLI_RAM_ACCESS( caddr ) ( ( vfli_ram[ caddr >> 1 ] >> ( ( caddr & 1 ) << 2) ) & 0x0f )
#else
#define VFLI_RAM_ACCESS( caddr ) ( vfli_ram[ color_addr2 ] )
#endif

static u16 color_addr, color_addr2, msb, addr;

__attribute__( ( always_inline ) ) inline void pre_fetchVIC656x( u16 _addr )
{
	extern u8 *vfli_ram;
	extern u8 bankVFLI, activateVFLIHack;

	color_addr = 0x9400 + ( _addr & 0x3ff );
	color_addr2 = (_addr & 0x03ff) | (bankVFLI << 10);

	msb = ~( ( _addr & 0x2000 ) << 2 ) & 0x8000;
	addr = ( _addr & 0x1fff ) | msb;

	CACHE_PRELOADL2KEEP( &charROM[ addr & 0x0fff ] );
	CACHE_PRELOADL2KEEP( &cart_ram[ color_addr ] ); 
	CACHE_PRELOADL2KEEP( &cart_ram[ addr ] );

	if ( activateVFLIHack )
	{
		CACHE_PRELOADL2KEEP( &vfli_ram[ color_addr2 >> 1 ] );
		CACHE_PRELOADL2KEEP( &vfli_ram[ (color_addr2 >> 1) + 64 ] );
	}
}

__attribute__( ( always_inline ) ) inline u16 post_fetchVIC656x( u16 ___addr )
{
	u16 D;

	if ( ( addr & 0x9000 ) == 0x8000 )
	{
		// chargen 
		D = charROM[ addr & 0x0fff ];
		D |= activateVFLIHack ? VFLI_RAM_ACCESS( color_addr2 ) << 8 : cart_ram[ color_addr ] << 8;
	} else
	if ( ( addr < 0x0400 ) || ( ( addr >= 0x1000 ) && ( addr < 0x2000 ) ) )
	{
		// RAM 
		D = cart_ram[ addr ];
		D |= activateVFLIHack ? VFLI_RAM_ACCESS( color_addr2 ) << 8 : cart_ram[ color_addr ] << 8;
	} else
	if ( activateVFLIHack && ( addr >= 0x0400 ) && ( addr < 0x1000 ) ) // VFLI
	{
		D = cart_ram[ addr ];
		D |= VFLI_RAM_ACCESS( color_addr2 ) << 8;
	} else
	if ( addr >= 0x9400 && addr < 0x9800 )
	{
		// color RAM 
		D = cart_ram[ color_addr ];
		D |= D << 8;
	} else 
	{
		// unconnected 
		D = vic656x.busLastData & ( 0xf0 | vic656x.busLastDataHigh );
		D |= cart_ram[ color_addr ] << 8;
	}
	vic656x.busLastDataHigh = D >> 8;
	vic656x.busLastData = D & 255;

	return D;
}


__attribute__( ( always_inline ) ) inline u16 fetchVIC656x( u16 addr )
{
	u16 D;

	extern u8 *vfli_ram;
	extern u8 bankVFLI, activateVFLIHack;

	u16 color_addr = 0x9400 + ( addr & 0x3ff );
	u16 color_addr2 = (addr & 0x03ff) | (bankVFLI << 10);

	u16 msb = ~( ( addr & 0x2000 ) << 2 ) & 0x8000;
	addr = ( addr & 0x1fff ) | msb;

	if ( ( addr & 0x9000 ) == 0x8000 )
	{
		// chargen 
		D = charROM[ addr & 0x0fff ];
		D |= activateVFLIHack ? VFLI_RAM_ACCESS( color_addr2 ) << 8 : cart_ram[ color_addr ] << 8;
	} else
	if ( ( addr < 0x0400 ) || ( ( addr >= 0x1000 ) && ( addr < 0x2000 ) ) )
	{
		// RAM 
		D = cart_ram[ addr ];
		D |= activateVFLIHack ? VFLI_RAM_ACCESS( color_addr2 ) << 8 : cart_ram[ color_addr ] << 8;
	} else
	if ( activateVFLIHack && ( addr >= 0x0400 ) && ( addr < 0x1000 ) ) // VFLI
	{
		D = cart_ram[ addr ];
		D |= VFLI_RAM_ACCESS( color_addr2 ) << 8;
	} else
	if ( addr >= 0x9400 && addr < 0x9800 )
	{
		// color RAM 
		D = cart_ram[ color_addr ];
		D |= D << 8;
	} else 
	{
		// unconnected 
		D = vic656x.busLastData & ( 0xf0 | vic656x.busLastDataHigh );
		D |= cart_ram[ color_addr ] << 8;
	}
	vic656x.busLastDataHigh = D >> 8;
	vic656x.busLastData = D & 255;

	return D;
}

__attribute__( ( always_inline ) ) inline void tickVideoVIC656x() 
{
	// perform some ahead address calculation and prefetch
	// otherwise a cache miss might stall execution and the emulation is out of sync 
	// (plus bus reads/writes might be missed)
	u16 addr;

	if ( vic656x.fetchState == VIC_FETCH_MATRIX )
		addr = ( ( ( vic656x.regs[ 5 ] & 0xf0 ) << 6 ) | ( ( vic656x.regs[ 2 ] & 0x80 ) << 2 ) ) + ( ( vic656x.memPtr + vic656x.bufOffset ) ); else
		addr = ( ( vic656x.regs[ 5 ] & 0xf ) << 10 ) + ( ( ( vic656x.lastChar & 255 ) * vic656x.charHeight + ( vic656x.rasterCounterY  & ( ( vic656x.charHeight >> 1 ) | 7 ) ) ) );

	// cache preloading, a single cache miss will result in a crash/too late bus communication
	pre_fetchVIC656x( addr );

	//
	// here the emulation (and instantaneous write to the frame buffer) starts
	//

	// for whatever (to me unknown) reason the VIC has an bus offset of 4 cycles
	// this requires an ugly hack. We can't handle this elegantly as every tick 
	// (= call of this function) must have constant work load. 
	u8 rightBorderHack = 0;

 	s16 x = vic656x.hCount - ( VIC20_BUS_OFFSET + 1 );

	#ifdef STRETCH_X
    s16 y = vic656x.vCount + vic656x.vOffset;
	#else
	s16 y = vic656x.vCount + vic656x.vOffset / 2;
	#endif

	// depending on where we are we need to fill some remaining pixels on the right border
	if ( x < 0 )
	{
		s16 prev_border_right = vic656x.prevStopX;

		if ( x + VIC20_BUS_OFFSET + 1 + vic656x.prevCoordX < ( prev_border_right - 1 ) )
		{
			rightBorderHack = 1;
			x = vic656x.prevCoordX + VIC20_BUS_OFFSET + 2 + x;
			y = ( y + vic656x.vLines - 1 ) % vic656x.vLines;
		} else
		if ( x + VIC20_BUS_OFFSET + 1 < 2 + prev_border_right - ( vic656x.hCycles - 3 ) && prev_border_right >= ( vic656x.hCycles - 3 ) )
		{
			// more cycles with background and border color
			rightBorderHack = 2;
			x = vic656x.prevCoordX + VIC20_BUS_OFFSET + 2 + x;
			y = ( y + vic656x.vLines - 1 ) % vic656x.vLines;
		} else
		{
			// simply fill with border color
			x = vic656x.prevCoordX + VIC20_BUS_OFFSET + 2 + x;
			y = ( y + vic656x.vLines - 1 ) % vic656x.vLines;
			rightBorderHack = 3;
		}
	}

	extern u16 *pScreen;
	extern u32 pitch;
	u8 *pScreen8 = (u8*)pScreen;

	#ifdef STRETCH_X
	x += OFFSET_X;
	u8* dst = (u8*)&pScreen8[ 2 * ( x * VIC20_PIXELS_PER_TICK + y * pitch * 2 ) ];

	if ( vic656x.hCycles == VIC20_NTSC_H_CYCLES )
	{
		if ( y <= 0 || y >= VIC20_NTSC_LAST_LINE - VIC20_NTSC_FIRST_LINE )
			rightBorderHack = 4;
		dst += 80;
	} else
	{
		if ( y <= 0 || y >= VIC20_PAL_LAST_LINE - VIC20_PAL_FIRST_LINE )
			rightBorderHack = 4;
		dst += 16;
	}
		

	if ( x < vic656x.leftBorder || x > vic656x.rightBorder )
		rightBorderHack = 4; // = render nothing

	CACHE_PRELOADL2STRMW( dst + pitch * 2 );
	#else
	u8* dst = (u8*)&pScreen8[ x * VIC20_PIXELS_PER_TICK + ( y + 20 ) * pitch * 2 ];
	#endif
	CACHE_PRELOADL2STRMW( dst );

	if ( rightBorderHack == 4 )
	{
	} else
	if ( rightBorderHack == 1 && vic656x.prevArea != VIC_AREA_DISPLAY )
	{
		#ifdef STRETCH_X
		  #ifndef SCANLINE
			*(u32*)dst =
			*(u32*)( dst + 4 ) =
			*(u32*)( dst + pitch * 2 ) =
			*(u32*)( dst + 4 + pitch * 2 ) = vic656x.colorLUT[ 9 ];
		  #else
			register u64 px = vic656x.colorLUT[ 9 ];
			px |= px << 32;
			*(u64*)dst = px;
			*(u64*)( dst + pitch * 2 ) = px | 0x1010101010101010;
		  #endif
		#else
		for ( int i = 0; i < 4; i++ )
			*dst++ = vic656x.colorLUT[ 9 ];
		#endif
	} else
	if ( rightBorderHack == 2 )
	{
		if ( vic656x.prevArea != VIC_AREA_DISPLAY )
		{
			#ifdef STRETCH_X
			  #ifndef SCANLINE
				*(u32*)dst =
				*(u32*)( dst + 4 ) =
				*(u32*)( dst + pitch * 2 ) =
				*(u32*)( dst + 4 + pitch * 2 ) = vic656x.colorLUT[ 9 ];
			  #else
				register u64 px = vic656x.colorLUT[ 9 ];
				px |= px << 32;
				*(u64*)dst = px;
				*(u64*)( dst + pitch * 2 ) = px | 0x1010101010101010;
			  #endif
			#else
			for ( int i = 0; i < 4; i++ )
				*dst++ = vic656x.colorLUT[ 9 ];
			#endif
		} else
		{
			#ifdef STRETCH_X
			  #ifndef SCANLINE
				*(u32*)dst =
				*(u32*)( dst + 4 ) =
				*(u32*)( dst + pitch * 2 ) =
				*(u32*)( dst + 4 + pitch * 2 ) = vic656x.colorLUT[ 8 ];
			  #else
				register u64 p = vic656x.colorLUT[ 8 ];
				p |= p << 32;
				*(u64*)dst = p;
				*(u64*)( dst + pitch * 2 ) = p | 0x1010101010101010;
			  #endif
			#else
			for ( int i = 0; i < 4; i++ )
				*dst++ = vic656x.colorLUT[ 9 ];
			#endif
		}
	} else
	if ( rightBorderHack == 3 )
	{
		#ifdef STRETCH_X
		  #ifndef SCANLINE
			*(u32*)dst =
			*(u32*)( dst + 4 ) =
			*(u32*)( dst + pitch * 2 ) =
			*(u32*)( dst + 4 + pitch * 2 ) = vic656x.colorLUT[ 9 ];
		  #else
			register u64 px = vic656x.colorLUT[ 9 ];
			px |= px << 32;
			*(u64*)dst = px;
			*(u64*)( dst + pitch * 2 ) = px | 0x1010101010101010;
		  #endif
		#else
	    for (int i = 0; i < 4; i++) 
			*dst++ = vic656x.colorLUT[ 9 ];
		#endif
	} else
	if ( rightBorderHack == 0 &&  
		( vic656x.curArea != VIC_AREA_DISPLAY ||
		( vic656x.hCount < vic656x.startX + VIC20_BUS_OFFSET + 1 || vic656x.hCount >= vic656x.stopX + VIC20_BUS_OFFSET + 1 ) ) )
	{
		#ifdef STRETCH_X
		#ifdef SCANLINE
			register u64 px = (u64)vic656x.colorLUT[ 1 ] | ( ( (u64)vic656x.colorLUT[ 5 ] ) << 32 );
			*(u64*)dst = px;
			*(u64*)( dst + pitch * 2 ) = px | 0x1010101010101010;
		#else
			*(u32*)dst = vic656x.colorLUT[ 1 ];
			*(u32*)( dst + 4 ) = vic656x.colorLUT[ 5 ];
			*(u32*)( dst + pitch * 2 ) = vic656x.colorLUT[ 1 ];
			*(u32*)( dst + 4 + pitch * 2 ) = vic656x.colorLUT[ 5 ];
		#endif
		#else
		*dst = vic656x.colorBRD;
		*( dst + 1 ) = vic656x.colorBRDNew;
		*( dst + 2 ) = vic656x.colorBRDNew;
		*( dst + 3 ) = vic656x.colorBRDNew;
		#endif
		
		if ( vic656x.hCount >= vic656x.stopX + VIC20_BUS_OFFSET + 1 )
			vic656x.pixelShift <<= 4;
	} else 
	{
        u8 p = vic656x.pixelShift;
		if ( vic656x.color & 8 ) // multi-color mode 
		{
            u32 fg_color = vic656x.color & 7;
			fg_color |= fg_color << 8;
			fg_color |= fg_color << 16;
			vic656x.colorLUT[ 2 ] = 
			vic656x.colorLUT[ 4 + 2 ] = fg_color;

			#ifdef STRETCH_X
			  #ifdef SCANLINE
				register u64 px = (u64)vic656x.colorLUT[ p >> 6 ] | ( ( (u64)vic656x.colorLUT[ 4 + ( ( p >> 4 ) & 3 ) ] ) << 32 );
				*(u64*)dst = px;
				*(u64*)( dst + pitch * 2 ) = px | 0x1010101010101010;

			  #else
				*(u32*)dst = vic656x.colorLUT[ p >> 6 ];
				*(u32*)( dst + pitch * 2 ) = vic656x.colorLUT[ ( p >> 6 ) ];
				*(u32*)( dst + 4 ) = vic656x.colorLUT[ 4 + ( ( p >> 4 ) & 3 ) ];
				*(u32*)( dst + 4 + pitch * 2 ) = vic656x.colorLUT[ 4 + ( ( p >> 4 ) & 3 ) ];
			  #endif
			#else
			  *(u16*)dst = vic656x.colorLUT[ p >> 6 ];
			  *(u16*)( dst + 2 ) = vic656x.colorLUT[ 4 + ( ( p >> 4 ) & 3 ) ];
			#endif
        } else 
		{	// hires mode 
			#ifdef STRETCH_X
				u32 bg, fg;
				uint64_t bg64, fg64;
				if ( vic656x.colorInvert )
				{
					bg = vic656x.color & 7;
					fg = vic656x.colorBG;
					bg64 = bg * 0x0101010101010101;
					fg64 = fg * 0x0000000100000001;
				} else {
					bg = vic656x.colorBG;
					fg = vic656x.color & 7;
					bg64 = bg * 0x0000000100000001;
					fg64 = fg * 0x0101010101010101;
				}

				uint64_t mask =
					( ( p >> 7 ) & 1 ) * ( 65535LL ) +
					( ( p >> 6 ) & 1 ) * ( 65535LL << 16 ) +
					( ( p >> 5 ) & 1 ) * ( 65535LL << 32 ) +
					( ( p >> 4 ) & 1 ) * ( 65535LL << 48 );

				*(uint64_t*)dst = ( fg64 & mask ) | ( bg64 & ~mask );

				#ifdef SCANLINE
					*(uint64_t*)( dst + pitch * 2 ) = ( fg64 & mask ) | ( bg64 & ~mask ) | 0x1010101010101010;
				#else
					*(uint64_t*)( dst + pitch * 2 ) = ( fg64 & mask ) | ( bg64 & ~mask );
				#endif	
			#else
				u32 bg, fg;
				u32 bg12, fg12;
				u32 bg3, fg3;

				if ( vic656x.colorInvert )
				{
					bg = vic656x.color & 7;
					fg = vic656x.colorBG;
				} else
				{
					bg = vic656x.colorBG;
					fg = vic656x.color & 7;
				}

				if ( vic656x.colorInvert )
				{
					bg12 = vic656x.color & 7;
					fg12 = vic656x.colorBGNew;
				} else
				{
					bg12 = vic656x.colorBGNew;
					fg12 = vic656x.color & 7;
				}

				if ( vic656x.colorInvertNew )
				{
					bg3 = vic656x.color & 7;
					fg3 = vic656x.colorBGNew;
				} else
				{
					bg3 = vic656x.colorBGNew;
					fg3 = vic656x.color & 7;
				}

				dst[ 0 ] = ( p & ( 1 << 7 ) ) ? fg : bg;
				dst[ 1 ] = ( p & ( 1 << 6 ) ) ? fg12 : bg12;
				dst[ 2 ] = ( p & ( 1 << 5 ) ) ? fg12 : bg12;
				dst[ 3 ] = ( p & ( 1 << 4 ) ) ? fg3 : bg3;
			#endif
		}

		vic656x.pixelShift = p << 4;
    }

	// update colors to be output
	vic656x.colorInvert = vic656x.colorInvertNew;
	vic656x.colorLUT[ 0 ] = vic656x.colorLUT[ 4 ] = vic656x.colorBG = vic656x.colorBGNew;
	vic656x.colorLUT[ 1 ] = vic656x.colorLUT[ 5 ] = vic656x.colorBRD = vic656x.colorBRDNew;
	vic656x.colorLUT[ 3 ] = vic656x.colorLUT[ 7 ] = vic656x.colorAUX = vic656x.colorAUXNew;

	if ( vic656x.curArea == VIC_AREA_IDLE && vic656x.regs[ 1 ] == ( vic656x.vCount >> 1 ) )
	{
		vic656x.curArea = VIC_AREA_PENDING;

		if ( vic656x.nTextRows == 0 )
			vic656x.curArea = VIC_AREA_DONE;
	}

    // next cycle 
	vic656x.hCount ++;

	if ( vic656x.hCount == vic656x.hCycles - VIC20_BUS_OFFSET - 1 )
	{
		vic656x.colorLUT[ 8 ] = vic656x.colorBG;
		vic656x.colorLUT[ 9 ] = vic656x.colorBRD;
		vic656x.colorLUT[ 10 ] = vic656x.colorAUX;

		vic656x.prevArea = vic656x.curArea;
		vic656x.prevStopX = vic656x.stopX;
	}
	
	if ( vic656x.hCount == vic656x.hCycles )
	{
		vic656x.prevCoordX = x - OFFSET_X;

		vic656x.hCount = 0;
		vic656x.vCount ++;

		if ( vic656x.curArea == VIC_AREA_DISPLAY )
			vic656x.rasterCounterY ++;

		vic656x.lineWasBlank = vic656x.rasterBlankThisLine;
		vic656x.rasterBlankThisLine = 1;

		vic656x.fetchState = VIC_FETCH_IDLE;

		if ( vic656x.vCount == vic656x.vLines )
		{
			vic656x.curArea = VIC_AREA_IDLE;
			vic656x.vCount = 0;

			vic656x.curTextRow = 0;
			vic656x.vCount = 0;
			vic656x.rasterCounterY = 0;
			vic656x.memPtr = 0;
			vic656x.memPtrInc = 0;
		}
	}

	if ( vic656x.curArea == VIC_AREA_DISPLAY || vic656x.curArea == VIC_AREA_PENDING )
	{
        // horizontal beginning of visible screen area
		if ( vic656x.fetchState == VIC_FETCH_IDLE && (int)( vic656x.regs[ 0 ] & 0x7fu ) == (int)( vic656x.hCount ) )
		{
			vic656x.startX = vic656x.regs[ 0 ] & 0x7f;

			vic656x.fetchState = VIC_FETCH_START;
			vic656x.bufOffset = VIC20_BUS_OFFSET;

			if ( vic656x.curArea == VIC_AREA_PENDING )
				vic656x.curArea = VIC_AREA_DISPLAY;

			vic656x.memPtrInc = 0;
			vic656x.curTextCols = vic656x.nTextCols;
		}

        // update of memory pointers
		if ( vic656x.curArea == VIC_AREA_DISPLAY && vic656x.hCount == 0 )
		{
			if ( vic656x.charHeight == (unsigned int)vic656x.rasterCounterY
				 || 2 * vic656x.charHeight == (unsigned int)vic656x.rasterCounterY )
			{
				vic656x.rasterCounterY = 0;
				vic656x.memPtrInc = vic656x.lineWasBlank ? 0 : vic656x.curTextCols;

				vic656x.curTextRow++;
				if ( vic656x.curTextRow == vic656x.nTextRows )
					vic656x.curArea = VIC_AREA_DONE;
			}

			vic656x.memPtr += vic656x.memPtrInc;
			vic656x.memPtrInc = 0;
		}
    }

	// keep number of text rows
	if ( vic656x.vCount == 0 && vic656x.hCount == 2 )
		vic656x.nTextRows = ( vic656x.regs[ 3 ] & 0x7e ) >> 1;

	// keep number of columns
	if ( vic656x.hCount == 1 )
		vic656x.nTextCols = min( vic656x.regs[ 2 ] & 0x7f, (int)vic656x.maxTextCols );

	switch ( vic656x.fetchState )
	{
        //case VIC_FETCH_IDLE:
        //case VIC_FETCH_DONE:
        default:
            break;

		case VIC_FETCH_START:
			if ( ( --vic656x.bufOffset ) == 0 )
			{
				vic656x.curTextCols = vic656x.nTextCols;

				vic656x.stopX = vic656x.startX + 2 * vic656x.curTextCols;

				if ( vic656x.curTextCols > 0 ) {
					vic656x.rasterBlankThisLine = 0;
					vic656x.fetchState = VIC_FETCH_MATRIX;
				} else
				{
					vic656x.fetchState = VIC_FETCH_DONE;
				}
			}
			break;

        case VIC_FETCH_MATRIX:	// fetch screen/color memory
			addr = ( ( ( vic656x.regs[ 5 ] & 0xf0 ) << 6 ) | ( ( vic656x.regs[ 2 ] & 0x80 ) << 2 ) ) + ( ( vic656x.memPtr + vic656x.bufOffset ) );
			pre_fetchVIC656x( addr );
			vic656x.fetchState = VIC_FETCH_CHARGEN;
			vic656x.lastChar = post_fetchVIC656x( addr );
			break;

        case VIC_FETCH_CHARGEN:	// fetch character ROM
			u32 b = vic656x.lastChar & 255;
			addr = ( ( vic656x.regs[ 5 ] & 0xf ) << 10 ) + ( ( b * vic656x.charHeight + ( vic656x.rasterCounterY  & ( ( vic656x.charHeight >> 1 ) | 7 ) ) ) );
			pre_fetchVIC656x( addr );

			vic656x.bufOffset++;

			vic656x.color = ( vic656x.lastChar >> 8 ) & 0xF;

			if ( vic656x.rasterCounterY == ( vic656x.charHeight - 1 ) )
				vic656x.memPtrInc ++;

			if ( vic656x.bufOffset >= vic656x.curTextCols )
				vic656x.fetchState = VIC_FETCH_DONE; else
				vic656x.fetchState = VIC_FETCH_MATRIX;

			vic656x.pixelShift = (u8)post_fetchVIC656x( addr );
			break;
    }
}



static s16 voltageLUT[ 32 ] = {
	 1477,  4159,  6463,  8847, 10840, 12968, 15369, 18058,
	20623, 22212, 23313, 24634, 25837, 26898, 27768, 27908,
	28043, 28178, 28313, 28448, 28583, 28718, 28853, 28988,
	29123, 29259, 29394, 29488, 29491, 29491, 29491, 29491
};

__attribute__( ( always_inline ) ) inline void sampleUpdateVIC656x() 
{
    // output new sample
	if ( vic656x.cyclesSampleCounter <= 0 )
	{
		vic656x.cyclesSampleCounter += vic656x.cyclesToNextSample;

		int smi = ( ( ( vic656x.sampleAcc * 7 ) / vic656x.sampleNAcc ) + 1 ) * ( vic656x.regs[ 14 ] & 0xF );

		if ( !cfgVIC_Audio_Filter )
		{
			vic656x.curSampleOutput = smi * 75;
		} else
		{
			float o = vic656x.filterLowPassBuf - vic656x.filterHighPassBuf;
			vic656x.filterHighPassBuf += vic656x.filterHighPassFactor * ( vic656x.filterLowPassBuf - vic656x.filterHighPassBuf );
			int v1 = voltageLUT[ smi >> 4 ];
			int v2 = voltageLUT[ ( smi >> 4 ) + 1 ];
			int lerp = ( ( 15 - ( smi & 15 ) ) * v1 + ( smi & 15 ) * v2 ) >> ( 4 );
			vic656x.filterLowPassBuf += vic656x.filterLowPassFactor * ( (float)lerp - vic656x.filterLowPassBuf );

			int t = (int)o;
			if ( t < -32768 ) t = -32768; else
			if ( t > 32767 ) t = 32767;
			vic656x.curSampleOutput = t;
		}

		vic656x.sampleAcc = 0;
		vic656x.sampleNAcc = 0;

		vic656x.hasSampleOutput = 1;
	}
}

__attribute__( ( always_inline ) ) inline void tickAudioVIC656x() 
{
	#ifdef INTERLEAVED_SOUND_EMULATION
	if ( vic656x.updChannel < 3 )
	{
		register u8 i = vic656x.updChannel;
		vic656x.ch_ctr[ i ] -= 4;
		if ( vic656x.ch_ctr[ i ] <= 0 )
		{
			u8 enabled = ( vic656x.regs[ 10 + i ] & 128 ) >> 7;
			u8 a = ( ~vic656x.regs[ 10 + i ] ) & 127;
			a = a ? a : 128;
			vic656x.ch_ctr[ i ] += a << ( 4 - i );

			u8 shift = vic656x.ch_shift[ i ];
			vic656x.ch_shift[ i ] = ( shift << 1 ) | ( ( ( ( ( shift & 128 ) >> 7 ) ) ^ 1 ) & enabled );

			vic656x.ch_out[ i ] = vic656x.ch_shift[ i ] & 1;
		}
		vic656x.accChannel += vic656x.ch_out[ i ];
	} else
	{
		// noise 
		vic656x.ch_ctr[ 3 ] -= 4;
		if ( vic656x.ch_ctr[ 3 ] <= 0 )
		{
			u8 a = ( ~vic656x.regs[ 13 ] ) & 127;
			a = a ? a : 128;
			vic656x.ch_ctr[ 3 ] += a << 1;
			u8 enabled = ( vic656x.regs[ 13 ] & 128 ) >> 7;

			if ( ( vic656x.noise_LFSR & 1 ) & !vic656x.noise_LFSR0_old )
			{
				u8 shift = vic656x.ch_shift[ 3 ];
				vic656x.ch_shift[ 3 ] = ( shift << 1 ) | ( ( ( ( ( shift & 128 ) >> 7 ) ) ^ 1 ) & enabled );
			}

			u8 bit3 = ( vic656x.noise_LFSR >> 3 ) & 1;
			u8 bit12 = ( vic656x.noise_LFSR >> 12 ) & 1;
			u8 bit14 = ( vic656x.noise_LFSR >> 14 ) & 1;
			u8 bit15 = ( vic656x.noise_LFSR >> 15 ) & 1;
			u8 gate1 = bit3 ^ bit12;
			u8 gate2 = bit14 ^ bit15;
			u8 gate3 = ( gate1 ^ gate2 ) ^ 1;
			u8 gate4 = ( gate3 & enabled ) ^ 1;
			vic656x.noise_LFSR0_old = vic656x.noise_LFSR & 1;
			vic656x.noise_LFSR = ( vic656x.noise_LFSR << 1 ) | gate4;

			vic656x.ch_out[ 3 ] = vic656x.ch_shift[ 3 ] & enabled;
		}
		vic656x.accChannel += vic656x.ch_out[ 3 ];
	}

	if( ++ vic656x.updChannel == 4 )
	{
		vic656x.sampleAcc += vic656x.accChannel;
		vic656x.sampleNAcc ++;
		vic656x.accChannel = vic656x.updChannel = 0;
	}
	
	#else
	for ( u8 i = 0; i < 3; i++ )
	{
		if ( --vic656x.ch_ctr[ i ] <= 0 )
		{
			u8 enabled = ( vic656x.regs[ 10 + i ] & 128 ) >> 7;
			u8 a = ( ~vic656x.regs[ 10 + i ] ) & 127;
			a = a ? a : 128;
			vic656x.ch_ctr[ i ] += a << ( 4 - i );

			u8 shift = vic656x.ch_shift[ i ];
			vic656x.ch_shift[ i ] = ( shift << 1 ) | ( ( ( ( ( shift & 128 ) >> 7 ) ) ^ 1 ) & enabled );

			vic656x.ch_out[ i ] = vic656x.ch_shift[ i ] & 1;
		}
	}

    // noise 
	if ( --vic656x.ch_ctr[ 3 ] <= 0 )
	{
		u8 a = ( ~vic656x.regs[ 13 ] ) & 127;
		a = a ? a : 128;
		vic656x.ch_ctr[ 3 ] += a << 1;
		u8 enabled = ( vic656x.regs[ 13 ] & 128 ) >> 7;

		if ( ( vic656x.noise_LFSR & 1 ) & !vic656x.noise_LFSR0_old )
		{
			u8 shift = vic656x.ch_shift[ 3 ];
			vic656x.ch_shift[ 3 ] = ( shift << 1 ) | ( ( ( ( ( shift & 128 ) >> 7 ) ) ^ 1 ) & enabled );
		}

		u8 bit3 = ( vic656x.noise_LFSR >> 3 ) & 1;
		u8 bit12 = ( vic656x.noise_LFSR >> 12 ) & 1;
		u8 bit14 = ( vic656x.noise_LFSR >> 14 ) & 1;
		u8 bit15 = ( vic656x.noise_LFSR >> 15 ) & 1;
		u8 gate1 = bit3 ^ bit12;
		u8 gate2 = bit14 ^ bit15;
		u8 gate3 = ( gate1 ^ gate2 ) ^ 1;
		u8 gate4 = ( gate3 & enabled ) ^ 1;
		vic656x.noise_LFSR0_old = vic656x.noise_LFSR & 1;
		vic656x.noise_LFSR = ( vic656x.noise_LFSR << 1 ) | gate4;

		vic656x.ch_out[ 3 ] = vic656x.ch_shift[ 3 ] & enabled;
	}

	vic656x.sampleAcc += vic656x.ch_out[ 0 ];
	vic656x.sampleAcc += vic656x.ch_out[ 1 ];
	vic656x.sampleAcc += vic656x.ch_out[ 2 ];
	vic656x.sampleAcc += vic656x.ch_out[ 3 ];

	vic656x.sampleNAcc++;
	#endif

    // update cycle counter
    vic656x.cyclesSampleCounter -= VIC20_FIXEDPOINT_SCALE;

	// if out-commented: do this asynchronously
	//sampleUpdateVIC656x();
}

__attribute__( ( always_inline ) ) inline void tickVIC656x() 
{
    tickVideoVIC656x();
	if ( !activateVFLIHack )
    	tickAudioVIC656x();
}



 