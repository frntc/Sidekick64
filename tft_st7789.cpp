/*
  _________.__    .___      __   .__        __         ______________________________________  ______  ________ 
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    /   _____/\__    ___/\______  \______  \/  __  \/   __   \
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    \_____  \   |    |       /    /   /    />      <\____    /
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     /        \  |    |      /    /   /    //   --   \  /    / 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   /_______  /  |____|     /____/   /____/ \______  / /____/  
        \/         \/    \/     \/       \/     \/           \/                                     \/           

 tft_st7789.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - code for driving the ST7789 TFT and lots of helper function (framebuffer, lazy updates etc)
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

#include "tft_st7789.h"

#define OLED_DC		LATCH_LED2
#define OLED_RES	LATCH_LED3

const static u32 ysize = 240, xsize = 240, invert = 1, rotate = 0;

const u32 CASET = 0x2A; // column address
const u32 RASET = 0x2B; // row address
const u32 RAMWR = 0x2C; // write to display RAM

#define CMD_SCL	(0<<2)
#define CMD_SDA	(1<<2)
#define CMD_RES	(2<<2)
#define CMD_DC 	(3<<2)

#define TFT_SCK_LOW		put4BitCommand( CMD_SCL + 0 );
#define TFT_SCK_HIGH	put4BitCommand( CMD_SCL + 1 );
#define TFT_SDA_LOW		put4BitCommand( CMD_SDA + 0 );
#define TFT_SDA_HIGH	put4BitCommand( CMD_SDA + 1 );
#define TFT_RESET_LOW	put4BitCommand( CMD_RES + 0 );
#define TFT_RESET_HIGH	put4BitCommand( CMD_RES + 1 );
#define TFT_DC_LOW		put4BitCommand( CMD_DC  + 0 );
#define TFT_DC_HIGH		put4BitCommand( CMD_DC  + 1 );

#define TFTimm_SCK_LOW		latchSetClearImm( 0, LATCH_SCL );
#define TFTimm_SCK_HIGH		latchSetClearImm( LATCH_SCL, 0 );
#define TFTimm_SDA_LOW		latchSetClearImm( 0, LATCH_SDA );
#define TFTimm_SDA_HIGH		latchSetClearImm( LATCH_SDA, 0 );
#define TFTimm_RESET_LOW	latchSetClearImm( 0, OLED_RES );
#define TFTimm_RESET_HIGH	latchSetClearImm( OLED_RES, 0 );
#define TFTimm_DC_LOW		latchSetClearImm( 0, OLED_DC );
#define TFTimm_DC_HIGH		latchSetClearImm( OLED_DC, 0 );

void delay( u32 d )
{
	DELAY( 1 << 20 );
}

u32 lastBit = 0;

// send a byte to the display
void tftSendData( u8 d )
{
	for ( u8 bit = 0x80; bit; bit >>= 1 )
	{
		TFT_SCK_LOW

		if ( ( d & bit ) && lastBit == 0 )
		{
			TFT_SDA_HIGH
			lastBit = 1;
		} else
		if ( !( d & bit ) && lastBit == 1 )
		{
			TFT_SDA_LOW
			lastBit = 0;
		}

		TFT_SCK_HIGH
	}
}

void tftSendDataImm( u8 d )
{
	for ( u8 bit = 0x80; bit; bit >>= 1 )
	{
		TFTimm_SCK_LOW

		if ( ( d & bit ) && lastBit == 0 )
		{
			TFTimm_SDA_HIGH
			lastBit = 1;
		} else
		if ( !( d & bit ) && lastBit == 1 )
		{
			TFTimm_SDA_LOW
			lastBit = 0;
		}

		TFTimm_SCK_HIGH
	}
}

// send command to tft
void tftCommand( u8 c )
{
	TFT_DC_LOW
	tftSendData( c );
	TFT_DC_HIGH
}

void tftCommand( u8 c, u8 d )
{
	TFT_DC_LOW
	tftSendData( c );
	TFT_DC_HIGH
	tftSendData( d ); 
}

void tftCommand( u8 c, u8 d1, u8 d2 )
{
	TFT_DC_LOW
	tftSendData( c );
	TFT_DC_HIGH
	tftSendData( d1 ); 
	tftSendData( d2 ); 
}

void tftCommand( u8 c, u8 d1, u8 d2, u8 d3 )
{
	TFT_DC_LOW
	tftSendData( c );
	TFT_DC_HIGH
	tftSendData( d1 ); 
	tftSendData( d2 ); 
	tftSendData( d3 ); 
}

void tftCommandImm( u8 c )
{
	TFTimm_DC_LOW
	tftSendDataImm( c );
	TFTimm_DC_HIGH
}

void tftCommandImm( u8 c, u8 d )
{
	TFTimm_DC_LOW
	tftSendDataImm( c );
	TFTimm_DC_HIGH
	tftSendDataImm( d );
}

void tftCommand2x( u8 c, u16 d1, u16 d2 )
{
	tftCommand( c );
	tftSendData( d1 >> 8 ); tftSendData( d1 ); 
	tftSendData( d2 >> 8 ); tftSendData( d2 );
}

void tftCommand2xImm( u8 c, u16 d1, u16 d2 )
{
	tftCommandImm( c );
	tftSendDataImm( d1 >> 8 ); tftSendDataImm( d1 ); 
	tftSendDataImm( d2 >> 8 ); tftSendDataImm( d2 );
}
  
void tftInitDisplay() 
{
	TFT_SDA_LOW
	lastBit = 0;

	TFT_SCK_HIGH
	TFT_DC_HIGH

	TFT_RESET_LOW	
	flush4BitBuffer( true );	delay( 20 );
	TFT_RESET_HIGH	
	flush4BitBuffer( true );	delay( 30 );

	tftCommand( 0x01 );								// software reset
	flush4BitBuffer( true );	delay( 150 );
	tftCommand( 0x11 );								// quit sleep mode
	flush4BitBuffer( true );	delay( 500 );
	tftCommand( 0x3A, 0x05 );						// 16-bit color mode 
	tftCommand( 0x20 + invert );					// invert
	tftCommand( 0x36, rotate << 5 );				// orientation
	tftCommand( 0x29 );								// display on
	flush4BitBuffer( true );	delay( 100 );
}

void tftInitDisplayImm() 
{
	TFTimm_SDA_LOW
	TFT_SDA_LOW
	lastBit = 0;
	TFTimm_SCK_HIGH
	TFTimm_DC_HIGH

	TFTimm_RESET_LOW
	delay( 20 );
	TFTimm_RESET_HIGH
	delay( 30 );

	tftCommandImm( 0x01 );								// software reset
	delay( 150 );
	tftCommandImm( 0x11 );								// quit sleep mode
	delay( 500 );
	tftCommandImm( 0x3A, 0x05 );						// 16-bit color mode 
	tftCommandImm( 0x20 + invert );						// invert
	tftCommandImm( 0x36, rotate << 5 );					// orientation
	tftCommandImm( 0x29 );								// display on
	delay( 100 );
}

void tftUse12BitColor()
{
	tftCommand( 0x3A, 0x03 );
}

void tftUse16BitColor()
{
	tftCommand( 0x3A, 0x05 );
}

u32 rgb24to16( u32 r, u32 g, u32 b ) 
{
	return ( r & 0xf8 ) << 8 | ( g & 0xfc ) << 3 | b >> 3;
}

unsigned short rgb16to12( unsigned short c )
{
	// let the compiler optimize this... (it's conversion to 24-bit RGB and then to 12-bit)
	unsigned short r;
	r  = ( ( (c >> 8) & 0xf8 ) & 0xf0 ) << 4;
	r |= ( ( (c >> 3) & 0xfc ) & 0xf0 ) << 0;
	r |= ( ( (c & 31) << 3 ) & 0xf0 ) >> 4;
	return r;
}

void setPixel( u32 x, u32 y, u32 c )
{
	tftCommand2x( CASET, y, y );
	tftCommand2x( RASET, x, x );
	tftCommand( RAMWR, c >> 8, c & 0xff );
}

void setDoubleWPixel12( u32 x, u32 y, u32 c )
{
	tftCommand2x( CASET, y, y+1 );
	tftCommand2x( RASET, x, x );
	tftCommand( RAMWR, c & 0xff, (c >> 8) & 0xff, c >> 16 );
}

void setDoubleVPixel12( u32 x, u32 y, u32 c )
{
	tftCommand2x( CASET, y, y );
	tftCommand2x( RASET, x, x+1 );
	tftCommand( RAMWR, c & 0xff, (c >> 8) & 0xff, c >> 16 ); 
}

void setMultiplePixels( u32 x, u32 y, u32 nx, u32 ny, u16 *c )
{
	tftCommand2x( CASET, y, y + ny );
	tftCommand2x( RASET, x, x + nx );
	tftCommand( RAMWR ); 
	for ( u32 i = 0; i < (nx+1)*(ny+1); i++ )
	{
		tftSendData( *c >> 8 ); 
		tftSendData( *c & 0xff );
		c++;
	}
}

void setMultiplePixels12( u32 x, u32 y, u32 nx, u32 ny, u16 *c )
{
	tftCommand2x( CASET, y, y + ny );
	tftCommand2x( RASET, x, x + nx );
	tftCommand( RAMWR ); 
	for ( u32 i = 0; i < (nx+1)*(ny+1); i+=2 )
	{
		unsigned int s = ( rgb16to12( c[0] ) << 12 ) | ( ((u32)rgb16to12( c[1] )) << 0 );
		tftSendData( (s >> 16 ) & 255 ); 
		tftSendData( (s >>  8 ) & 255 ); 
		tftSendData( (s >>  0 ) & 255 ); 
		c+=2;
	}
}

void setMultiplePixelsImm( u32 x, u32 y, u32 nx, u32 ny, u16 *c )
{
	tftCommand2xImm( CASET, y, y + ny );
	tftCommand2xImm( RASET, x, x + nx );
	tftCommandImm( RAMWR ); 
	for ( u32 i = 0; i < (nx+1)*(ny+1); i++ )
	{
		tftSendDataImm( *c >> 8 ); 
		tftSendDataImm( *c & 0xff );
		c++;
	}
}

void tftCopy2Framebuffer16BitImm( u32 x, u32 y, u32 w, u32 h, const u8 *raw )
{
	tftCommand2x( CASET, x, x + w - 1 );
	tftCommand2x( RASET, y, y + h - 1 );
	tftUse16BitColor();
	tftCommand( RAMWR );

	for ( u32 j = 0; j < h; j++ )
	{
		for ( u32 i = 0; i < w; i++ )
		{
			tftSendData( raw[ (i + j * 240)*2+1 ] );
			tftSendData( raw[ (i + j * 240)*2 ] );
		}
		flush4BitBuffer( true );
	}
}

void tftSendFramebuffer16BitImm( const u8 *raw )
{
	tftCommand2xImm( CASET, 0, ysize - 1 );
	tftCommand2xImm( RASET, 0, xsize - 1 );
	tftCommandImm( 0x3A ); tftSendDataImm( 0x05 );
	tftCommandImm( RAMWR );

	for ( u32 j = 0; j < ysize; j++ )
	{
		for ( u32 i = 0; i < xsize; i++ )
		{
			tftSendDataImm( raw[ (i + j * 240)*2+1 ] );
			tftSendDataImm( raw[ (i + j * 240)*2 ] );
		}
	}
	TFTimm_DC_LOW
}

void tftSendFramebuffer12BitImm( const u8 *raw )
{
	tftCommand2x( CASET, 0, ysize - 1 );
	tftCommand2x( RASET, 0, xsize - 1 );
	tftUse12BitColor();
	tftCommand( RAMWR );

	const u8 *p = raw;
	for ( u32 j = 0; j < ysize; j++ )
	{
		for ( u32 i = 0; i < xsize; i += 2 )
		{
			tftSendData( p[0] );
			tftSendData( p[1] );
			tftSendData( p[2] );
			p += 3;
			//tftSendData( raw[ (i + j * 240)*3/2+1 ] );
			//tftSendData( raw[ (i + j * 240)*2 ] );
		}
		flush4BitBuffer( true );
	}
	TFTimm_DC_LOW
}

void tftInit()
{
	TFT_SDA_LOW
	lastBit = 0;
	tftInitDisplay();
}

void tftInitImm()
{
	TFTimm_SDA_LOW
	lastBit = 0;
	tftInitDisplayImm();
}

void tftSplashScreen( const u8 *fb )
{
	tftInit();
	tftSendFramebuffer16BitImm( fb );
}

int tftSplashScreenFile( const char *drive, const char *fn )
{
	tftInitDisplay();
	u8 temp[ 256 * 256 * 3 ];
	u32 size;
	extern CLogger *logger;
	if ( readFile( logger, (char*)drive, fn, temp, &size ) )
	{
		tftCommand2x( CASET, 0, ysize - 1 );
		tftCommand2x( RASET, 0, xsize - 1 );
		tftCommand( 0x3A ); tftSendData( 0x05 );               
		tftCommand( RAMWR );
	
		// convert RGB24 to RGB16 on the fly
		for ( u32 y = 0; y < ysize; y++ )
		{
			for ( u32 x = 0; x < xsize; x++ )
			{
				u32 a = ( x + y * xsize ) * 3;
				u32 r = temp[ a + 0 ];
				u32 g = temp[ a + 1 ];
				u32 b = temp[ a + 2 ];

				u32 col = rgb24to16( r, g, b );
				
				tftSendData( col >> 8 );
				tftSendData( col & 255 );
				flush4BitBuffer( true );
			}
		}
		//tftCommand( 0x3A ); tftSendData( 0x05 );
		flush4BitBuffer( true );
		return 1;
	}
	flush4BitBuffer(true);
	return 0;
}



// loads a 24/32-bit, uncompressed Targa file
int tftLoadTGA( const char *drive, const char *name, unsigned char *dst, int *imgWidth, int *imgHeight, int wantAlpha )
{
	u8 tga[ 256 * 256 * 4 ];
	u32 size;
	extern CLogger *logger;
	if ( readFile( logger, (char*)drive, name, tga, &size ) )
	{
		unsigned char *type = &tga[ 0 ];
		if ( type[ 1 ] != 0 || ( type[ 2 ] != 2 && type[ 2 ] != 3 ) )
			return 0;

		unsigned char *info = &tga[ 12 ];
		*imgWidth   = info[ 0 ] + info[ 1 ] * 256;
		*imgHeight  = info[ 2 ] + info[ 3 ] * 256;
		int imgBits = info[ 4 ];

		if ( imgBits != 32 && imgBits != 24 )
			return -1;

		int bytesPerPixel = imgBits / 8;
		int bytesPerPixelTarget;

		if ( wantAlpha )
			bytesPerPixelTarget = 4; else
			bytesPerPixelTarget = 3;

		size = *imgWidth * *imgHeight;

		for ( int j = 0; j < *imgHeight; j++ )
		{
			unsigned char *p = &dst[ bytesPerPixelTarget * *imgWidth * ( *imgHeight - 1 - j ) ];
			for ( int i = 0; i < *imgWidth * bytesPerPixel; i += bytesPerPixel )
			{
				*( p++ ) = tga[ i + 2 + 18 + j * *imgWidth * bytesPerPixel ];
				*( p++ ) = tga[ i + 1 + 18 + j * *imgWidth * bytesPerPixel ];
				*( p++ ) = tga[ i + 0 + 18 + j * *imgWidth * bytesPerPixel ];
				if ( wantAlpha && imgBits == 32 )
					*( p++ ) = tga[ i + 3 + 18 + j * *imgWidth * bytesPerPixel ]; else
				if ( wantAlpha )
					*( p++ ) = 255;
			}
		}
		return 1;
	}
	return 0;
}

unsigned char charset[ 4096 ];
unsigned char rgbChars[ 256 * 16 * 16 ];

unsigned char tftBackground[ 240 * 240 * 2 ];
unsigned char tftFrameBuffer[ 240 * 240 * 2 ];
unsigned char tftFrameBuffer12Bit[ 240 * 240 * 3 / 2 ];
unsigned char tempTGA[ 256 * 256 * 4 ];

#define DIRTY_SIZE 4
unsigned char tftDirty[ (240/DIRTY_SIZE) * (240/DIRTY_SIZE) ];

u32 nDirtyRegions, curDirtyRegion;

void tftClearDirty()
{
	for ( u32 j = 0; j < 240/DIRTY_SIZE; j++ )
		for ( u32 i = 0; i < 240/DIRTY_SIZE; i++ )
			tftDirty[ i + j * (240/DIRTY_SIZE) ] = 0;
	nDirtyRegions = 0;
	curDirtyRegion = 0;
}

void tftPrepareDirtyUpdates()
{
	u16 *dst = (u16*)tftFrameBuffer;
	u16 *src = (u16*)tftBackground;
	for ( int i = 0; i < 240; i++ )
		for ( int j = 0; j < 240; j++ )
			dst[i+j*240] = src[ j + i * 240 ];
			
}

void setPixelDirty( u32 x, u32 y, u32 c )
{
	if ( c == *(u16*)&tftFrameBuffer[ (x + y * 240) * 2 ] )
		return;

	*(u16*)&tftFrameBuffer[ (x + y * 240) * 2 ] = c;
	if ( tftDirty[ (x/DIRTY_SIZE) + (y/DIRTY_SIZE) * (240/DIRTY_SIZE) ] == 0 )
	{
		nDirtyRegions ++;
		tftDirty[ (x/DIRTY_SIZE) + (y/DIRTY_SIZE) * (240/DIRTY_SIZE) ] = 1;
	}
}

bool tftIsDirtyRegion()
{
	return nDirtyRegions > 0;
}


int tftUpdateNextDirtyRegions()
{
	if ( nDirtyRegions == 0 )
		return 0;

	while ( tftDirty[ curDirtyRegion ] == 0 && curDirtyRegion < ((240/DIRTY_SIZE)*(240/DIRTY_SIZE)) )
		curDirtyRegion ++;
	
	u32 x = ( curDirtyRegion % (240/DIRTY_SIZE) ) * DIRTY_SIZE;
	u32 y = ( curDirtyRegion / (240/DIRTY_SIZE) ) * DIRTY_SIZE;

	u16 c[ DIRTY_SIZE * DIRTY_SIZE ];
	u16 *dst = &c[ 0 ];

	for ( u32 j = 0; j < DIRTY_SIZE; j++ )
		for ( u32 i = 0; i < DIRTY_SIZE; i++ )
		{
			*(dst++) = *(u16*)&tftFrameBuffer[ ((x+j) + (y+i) * 240) * 2 ];
		}

	setMultiplePixels12( x, y, DIRTY_SIZE-1, DIRTY_SIZE-1, c );

	tftDirty[ curDirtyRegion++ ] = 0;
	nDirtyRegions --;
	return 1;
}

int tftUpdateNextDirtyRegionsImm()
{
	if ( nDirtyRegions == 0 )
		return 0;

	while ( tftDirty[ curDirtyRegion ] == 0 && curDirtyRegion < ((240/DIRTY_SIZE)*(240/DIRTY_SIZE)) )
		curDirtyRegion ++;
	
	u32 x = ( curDirtyRegion % (240/DIRTY_SIZE) ) * DIRTY_SIZE;
	u32 y = ( curDirtyRegion / (240/DIRTY_SIZE) ) * DIRTY_SIZE;

	u16 c[ DIRTY_SIZE * DIRTY_SIZE ];
	u16 *dst = &c[ 0 ];

	for ( u32 j = 0; j < DIRTY_SIZE; j++ )
		for ( u32 i = 0; i < DIRTY_SIZE; i++ )
		{
			*(dst++) = *(u16*)&tftFrameBuffer[ ((x+j) + (y+i) * 240) * 2 ];
		}

	setMultiplePixelsImm( x, y, DIRTY_SIZE-1, DIRTY_SIZE-1, c );

	tftDirty[ curDirtyRegion++ ] = 0;
	nDirtyRegions --;
	return 1;
}

void tftCopyBackground2Framebuffer()
{
	memcpy( tftFrameBuffer, tftBackground, 240 * 240 * 2 );
}

void tftCopyBackground2Framebuffer( int x, int y, int w, int h )
{
	for ( int b = y; b < y + h; b ++ )
		memcpy( &tftFrameBuffer[ (x+b*240)*2 ], &tftBackground[ (x+b*240)*2 ], w * 2 );
}

int ditherColor( int v, int x, int y, int d )
{
	const int tm[ 4 * 4 ] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
	return min( 255, max( 0, (int)( (float)v + (float)d * ( tm[ ( x & 3 ) + ( y & 3 ) * 4 ] / 16.0f - 0.5f ) ) ) );
}

void tftBlendRGBA( u32 r_, u32 g_, u32 b_, u32 a, unsigned char *dst, int dither )
{
	for ( int i = 0; i < 240*240; i++ )
	{
		unsigned short c = *(short*)&dst[ i * 2 ];

		int r = (c >> 8) & 0xf8;
		int g = (c >> 3) & 0xfc;
		int b = (c & 31) << 3;

		r = ( r * ( 255 - a ) + r_ * a ) / 255;
		g = ( g * ( 255 - a ) + g_ * a ) / 255;
		b = ( b * ( 255 - a ) + b_ * a ) / 255;

		if ( dither )
		{
			int x = i % 240, y = i / 240;
			r = ditherColor( r, x, y, dither );
			g = ditherColor( g, x, y, dither );
			b = ditherColor( b, x, y, dither );
		}

		*(short*)&dst[ i * 2 ] = rgb24to16( r, g, b );
	}
}

void tftBlendRGBA( unsigned char *rgba, unsigned char *dst, int dither )
{
	for ( int i = 0; i < 240*240; i++ )
	{
		unsigned short c = *(short*)&dst[ i * 2 ];

		int r = (c >> 8) & 0xf8;
		int g = (c >> 3) & 0xfc;
		int b = (c & 31) << 3;

		int a = rgba[ i * 4 + 3 ];
		r = ( r * ( 255 - a ) + rgba[ i * 4 + 0 ] * a ) / 255;
		g = ( g * ( 255 - a ) + rgba[ i * 4 + 1 ] * a ) / 255;
		b = ( b * ( 255 - a ) + rgba[ i * 4 + 2 ] * a ) / 255;

		if ( dither )
		{
			int x = i % 240, y = i / 240;
			r = ditherColor( r, x, y, dither );
			g = ditherColor( g, x, y, dither );
			b = ditherColor( b, x, y, dither );
		}

		*(short*)&dst[ i * 2 ] = rgb24to16( r, g, b );
	}
}

void tftConvertFrameBuffer12Bit()
{
	unsigned char *p = tftFrameBuffer12Bit;
	unsigned short *s = (unsigned short *)tftFrameBuffer;
	for ( int i = 0; i < 240 * 240; i += 2 )
	{
		unsigned int c = ( rgb16to12( s[ i ] ) << 12 ) | ( ((u32)rgb16to12( s[ i + 1 ] )) << 0 );
		*(p++) = (c >> 16 ) & 255; 
		*(p++) = (c >> 8 ) & 255; 
		*(p++) = (c >> 0 ) & 255; 
	}
}



int tftLoadBackgroundTGA( const char *drive, const char *name, int dither )
{
	int w, h;

	int r = tftLoadTGA( drive, name, tempTGA, &w, &h, false );

	if ( r == 0 )
		return 0;

	int bytesPerPixel = 3; 
	for ( int y = 0; y < min( 240, h ); y++ )
		for ( int x = 0; x < min( 240, w ); x++ )
		{
			unsigned char *p = &tempTGA[ bytesPerPixel * ( x + y * w ) ];

			if ( dither )
			{
				for ( int i = 0; i < 3; i++ )
					p[ i ] = ditherColor( p[ i ], x, y, dither );
			}

			*(unsigned short*)&tftBackground[ ( x + y * 240 ) * 2 + 0 ] = rgb24to16( p[0], p[1], p[2] );
		}

	tftCopyBackground2Framebuffer();
	return 1;
}


void tftLoadCharset( const char *drive, const char *name )
{
	u32 size;
	extern CLogger *logger;
	if ( readFile( logger, (char*)drive, name, charset, &size ) )
	{
		memset( rgbChars, 255, 256 * 16 * 16 );

		for ( int c = 0; c < 256; c++ )
			for ( int j = 0; j < 16; j++ )
				for ( int i = 0; i < 16; i++ )
				{
					unsigned int a = i / 2, b = j / 2;
					unsigned char v = charset[ c * 8 + b + 2048 ];

					if ( (v>>(7-a)) & 1 )
						rgbChars[ c * 16 * 16 + j * 16 + i ] = 1;
				}

		for ( int c = 0; c < 256; c++ )
		{
			unsigned int col = 1;
			for ( int j = 0; j < 16; j++ )
				for ( int i = 0; i < 16; i++ )
				{
					int set = 0;
					const int dilate = 2;
					for ( int a = -dilate; a <= dilate; a++ )
						for ( int b = -dilate; b <= dilate; b++ )
							if ( a * a + b * b != 2 * dilate * dilate )
							if ( rgbChars[ c * 16 * 16 + (max(0,min(15,j+b))) * 16 + (max(0,min(15,i+a))) ] == col )
								set ++;

					if ( set && rgbChars[ c * 16 * 16 + j * 16 + i ] == 255 )
						rgbChars[ c * 16 * 16 + j * 16 + i ] = 0;
				}
		}
	}
}

static unsigned char PETSCII2ScreenCode( unsigned char c )
{
	if ( c < 32 ) return c + 128;
	if ( c < 64 ) return c;
	if ( c < 96 ) return c - 64;
	if ( c < 128 ) return c - 32;
	if ( c < 160 ) return c + 64;
	if ( c < 192 ) return c - 64;
	return c - 128;
}

void tftPrint( const char *s, int x_, int y_, int color, int condensed )
{
	int l = strlen( s );

	for ( int a = 0; a < l; a++ )
	{
		int c = PETSCII2ScreenCode( s[ a ] );

		for ( int j = 0; j < 16; j++ )
		{
			for ( int i = 0; i < 16; i++ )
			{
				unsigned int x = x_ + i + a * ( 16 + condensed );
				unsigned int y = y_ + j;

				unsigned int col = rgbChars[ c * 16 * 16 + j * 16 + i ];

				if ( col != 255 )
				{
					tftFrameBuffer[ (x + y * 240) * 2 + 0 ] = (color * col) & 255;
					tftFrameBuffer[ (x + y * 240) * 2 + 1 ] = (color * col) >> 8;
				}
			}
		}
	}
}
