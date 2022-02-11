/*
  _________.__    .___      __   .__        __         ______________________________________  ______  ________ 
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    /   _____/\__    ___/\______  \______  \/  __  \/   __   \
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    \_____  \   |    |       /    /   /    />      <\____    /
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     /        \  |    |      /    /   /    //   --   \  /    / 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   /_______  /  |____|     /____/   /____/ \______  / /____/  
        \/         \/    \/     \/       \/     \/           \/                                     \/           
 
 tft_st7789.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - code for driving the ST7789 TFT and lots of helper function (framebuffer, lazy updates etc)
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

#ifndef _tft7789_h
#define _tft7789_h

#include <circle/bcm2835.h>
#include <circle/types.h>
#include <circle/gpiopin.h>
#include <circle/util.h>
#include <circle/memio.h>
#include <circle/logger.h>
#include "latch.h"
#include "helpers.h"

extern u32 rgb( u32 r, u32 g, u32 b );
extern void tftSendFramebuffer16BitImm();
extern void tftUse12BitColor();
extern void tftUse16BitColor();
extern void setDoubleWPixel12( u32 x, u32 y, u32 c );

extern void tftSplashScreen( const u8 *fb );
extern int tftSplashScreenFile( const char *drive, const char *fn );

extern unsigned char tftBackground[ 240 * 240 * 2 ];
extern unsigned char tftFrameBuffer[ 240 * 240 * 2 ];
extern unsigned char tftFrameBuffer12Bit[ 240 * 240 * 3 / 2 ];

extern void tftSendFramebuffer16BitImm( const u8 *raw );
extern void tftSendFramebuffer12BitImm( const u8 *raw );
extern void tftInit( int rot = -1 );
extern void tftInitImm( int rot = -1 );

extern u32 rgb24to16( u32 r, u32 g, u32 b );
extern int tftLoadTGA( const char *drive, const char *name, unsigned char *dst, int *imgWidth, int *imgHeight, int wantAlpha );
extern int tftLoadBackgroundTGA( const char *drive, const char *name, int dither = 0 );
extern void tftConvertFrameBuffer12Bit();
extern void tftBlendRGBA( unsigned char *rgba, unsigned char *dst, int dither = 0 );
extern void tftBlendRGBA( u32 r_, u32 g_, u32 b_, u32 a, unsigned char *dst, int dither );
extern void tftCopyBackground2Framebuffer( int x, int y, int w, int h );
extern void tftCopyBackground2Framebuffer();
extern void tftLoadCharset( const char *drive, const char *name );
extern void tftPrint( const char *s, int x_, int y_, int color, int condensed = 0 );

extern void tftClearDirty();
extern void setPixelDirty( u32 x, u32 y, u32 c );
extern int  tftUpdateNextDirtyRegions();
extern bool tftIsDirtyRegion();


#endif

 