/*
  _________.__    .___      __   .__        __        _____         .__                __  .__               
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   /  _  \   ____ |__| _____ _____ _/  |_|__| ____   ____  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /  /  /_\  \ /    \|  |/     \\__  \\   __\  |/  _ \ /    \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <  /    |    \   |  \  |  Y Y  \/ __ \|  | |  (  <_> )   |  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \ \____|__  /___|  /__|__|_|  (____  /__| |__|\____/|___|  /
        \/         \/    \/     \/       \/     \/         \/     \/         \/     \/                    \/         


 Sidekick64 - Animation Converter
 Copyright (c) 2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RES_X 192
#define RES_Y 147
#define NFRAMES 128
#define BITS_PER_PIXEL 8
#define CNT_AFTER_REPETITION 1

// standard ordered dither matrix
const unsigned char tm[ 4 * 4 ] =
{
	 0, 12,  3, 15,
	 8,  4, 11,  7,
	 2, 14,  1, 13,
	10,  6,  9,  5
};

// line-like dither matrix
const unsigned char tml[ 4 * 4 ] =
{
	 0,  4,  2,  6,
	 8, 12, 10, 14,
	 3,  7,  1,  5,
	11, 15,  9, 13
};

// some memory for loading + dithering
unsigned char rawData[ RES_X * RES_Y * NFRAMES ];
unsigned char bitData[ RES_X * RES_Y * NFRAMES ];

// ... plus generating the output (hopefully large enough)
unsigned char output[ 512 * 1024 ];
unsigned char flags[ 512 * 1024 ];
unsigned char bitflags[ 512 * 1024 / 8 ];

int main( int argc, char **argv )
{
	char path[ 2048 ] = { 0 };
	int flipY = 0;
	int ditherType = 0;
	float gamma = 1.5f, scale = 395.0f / 255.0f;

	if ( argc < 5 )
	{
		printf( "usage: skanim f{l|o} gamma scale path output.zap\n" );
		printf( "       converts a sequence of 128 raw-images (resolution 192x147, 8-bit gray scale) with filename 0000.raw 0001.raw etc.\n\n" );
		printf( "       f       flip images vertically (not shown in preview!)\n" );
		printf( "       o       standard ordered dither matrix\n" );
		printf( "       l       line-like dither matrix\n" );
		printf( "       gamma   gamma-correction value (typically between 0.5 and 2.5)\n" );
		printf( "       scale   brightness scaling (1.0 and 2.0)\n" );
		printf( "       e.g. \"skanim fl 1.5 1.55 ./images animation.zap\" creates a flipped animation with line dithering-style.\n" );
		exit( 1 );
	}

	// options
	if ( strstr( argv[ 1 ], "f" ) != 0 ) flipY = 1;
	if ( strstr( argv[ 1 ], "l" ) != 0 ) ditherType = 1;

	// gamma + scale
	gamma = atof( argv[ 2 ] );
	scale = atof( argv[ 3 ] );

	// patch
	strcpy( path, argv[ 4 ] );
	if ( path[ strlen( path ) - 1 ] != '\\' && path[ strlen( path ) - 1 ] != '/' )
	{
		path[ strlen( path ) ] = '/';
		path[ strlen( path ) + 1 ] = 0;
	}

	for ( int i = 0; i < NFRAMES; i++ )
	{
		char fn[ 4096 ];
		sprintf( fn, "%s%04d.raw", path, i );

		FILE *f = fopen( fn, "rb" );
		fread( &rawData[ i * RES_X * RES_Y ], 1, RES_X * RES_Y, f );
		fclose( f );
	}

	memset( bitData, 0, RES_X * RES_Y * NFRAMES );

	//
	// Gamma-correction + dithering
	//
	for ( int f = 0; f < NFRAMES; f++ )
	{
		for ( int y = 0; y < RES_Y; y++ )
		{
			int yy = y;
			
			if ( flipY )
				yy = RES_Y - 1 - y;

			int b = 0;
			for ( int x = 0; x < RES_X; x ++ )
			{
				int l = rawData[ x + yy * RES_X + f * RES_X * RES_Y ];

				l = (int)( ( powf( l / 255.0f, gamma ) ) * scale * 255.0f );
				if ( l >= 2 ) l -= 2; else l = 0;
				
				b <<= 1;

				if ( ditherType == 1 )
				{
					if ( l > tml[ (x&3) + (y&3) * 4 ] * 16 ) b |= 1;
				} else
				{
					if ( l > tm[ (y&3) + (x&3) * 4 ] * 16 ) b |= 1;
				}

				if ( ( x & 7 ) == 7 )
				{
					bitData[ f * RES_X * RES_Y / 8 + x / 8 + ( y * RES_X / 8 ) ] = b;

					// write back dithered result
					for ( int i = 0; i < 8; i++ )
						rawData[ x + i - 7 + yy * RES_X + f * RES_X * RES_Y ] = ( b & ( 1 << ( 7 - i ) ) ) ? 255 : 0;

					b = 0;
				}
			}
		}

		char fn[ 4096 ];
		sprintf( fn, "out%04d.raw", f );

		FILE *g = fopen( fn, "wb" );
		fwrite( &rawData[ f * RES_X * RES_Y ], 1, RES_X * RES_Y, g );
		fclose( g );
	}

	//
	//
	// RLE compression for "per-pixel" incremental reconstruction
	//
	//
	unsigned char *o = output;
	int compressedSize = 0;

	// during compression stored as bytes, saved as bits 
	unsigned char *flg = flags;
	unsigned char *bitflg = flags;

	memset( flags, 0, 512 * 1024 );

	int pixelStartOfs[ RES_X * RES_Y / 8 ];
	int pixelRLELength[ RES_X * RES_Y / 8 ];

	for ( int j = 0; j < RES_Y; j++ )
		for ( int i = 0; i < RES_X / BITS_PER_PIXEL; i++ )
		{
			unsigned char stream[ 2048 ];

			for ( int f = 0; f < NFRAMES; f++  )
				stream[ f ] = bitData[ f * RES_X * RES_Y / BITS_PER_PIXEL + j * RES_X / BITS_PER_PIXEL + i ];

			pixelStartOfs[ i + j * RES_X / BITS_PER_PIXEL ] = compressedSize;
			int compressedSizeBefore = compressedSize;

			// now we compress 'stream'
			int ofs = 0;

			while ( ofs < NFRAMES )
			{
				int cur = stream[ ofs ++ ];
					
				// store byte
				*(o++) = cur;
				*(flg++) = 1;
				compressedSize ++;

				int rle = 0;
				while ( ofs < NFRAMES && stream[ ofs ] == cur && rle < 255 )
				{
					ofs ++; rle ++;
				}

				if ( rle )
				{
					if ( rle == 1 )
					{
						// don't want to handle this case, makes decoding a little bit simpler
						ofs --;
					} else
					{
						// store count
						*(o++) = rle;
						*(flg++) = 0;
						compressedSize ++;
					}
				} 
			}

			int lengthRLE = compressedSize - compressedSizeBefore;

			// optional optimization: let's see if we find a pixel with the same compressed data
			for ( int k = 0; k < j * RES_X / BITS_PER_PIXEL + i; k++ )
			{
				unsigned char *prevCompressedData = &output[ pixelStartOfs[ k ] ];
				unsigned char *thisCompressedData = &output[ pixelStartOfs[ i + j * RES_X / BITS_PER_PIXEL ] ];

				if ( pixelRLELength[ k ] == lengthRLE )
				{
					for ( int m = 0; m < lengthRLE; m++ )
						if ( prevCompressedData[ m ] != thisCompressedData[ m ] )
							goto notFound;
							
					pixelStartOfs[ i + j * RES_X / BITS_PER_PIXEL ] = pixelStartOfs[ k ];
					compressedSize -= lengthRLE;
					o -= lengthRLE;
					flg -= lengthRLE;
					goto foundMatch;

				notFound:;
				}
			}
			foundMatch:

			pixelRLELength[ i + j * RES_X / BITS_PER_PIXEL ] = lengthRLE;
		}

	// store flags as single bits instead of bytes
	bitflg = bitflags;
	int b = 0;
	for ( int i = 0; i < compressedSize; i++ )
	{
		b >>= 1;
		b |= flags[ i ] ? 128 : 0;

		if ( ( i & 7 ) == 7 )
			*(bitflg++) = b;
	}

	// output the bits which did not fill a byte
	b >>= 8 - ( compressedSize & 7 );
	*(bitflg++) = b;

	memset( bitData, 0, RES_X * RES_Y * NFRAMES );

	FILE *f;
	if ( argc > 5 )
		f = fopen( argv[ 5 ], "wb" ); else
		f = fopen( "animation.zap", "wb" );
	fwrite( &compressedSize, 1, sizeof( int ), f );
	fwrite( output, 1, compressedSize, f );

	int bitFlagSize = (int)( bitflg-bitflags );
	fwrite( &bitFlagSize, 1, sizeof( int ), f );
	fwrite( bitflags, 1, bitFlagSize, f );

	for ( int i = 0; i < RES_X * RES_Y / BITS_PER_PIXEL; i++ )
	{
		unsigned int s = pixelStartOfs[ i ] << 8;
		fwrite( &s, 1, 4, f );
	}
	fclose( f );
}
