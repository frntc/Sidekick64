/*
  _________.__    .___      __   .__        __            _____                       
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __       /     \   ____   ____  __ __ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      /  \ /  \_/ __ \ /    \|  |  \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /    Y    \  ___/|   |  \  |  /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \____|__  /\___  >___|  /____/ 
        \/         \/    \/     \/       \/     \/             \/     \/     \/       
 
 render_sprite_animation.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - code for rendering and decompressing the background animation
 Copyright (c) 2021-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

//
// some macros for fancy forward and backward RLE decompression
//
#define FLAG( o ) ( bitflagsRLE[ (o) / 8 ] & ( 1 << ( (o) & 7 ) ) )

#define FORWARD( state )								\
	{ ofs = state >> 8; rle = state & 255;				\
	if ( rle )											\
	{													\
		retVal = animationRLE[ ofs - 2 ];				\
		rle --;											\
	} else {											\
		if ( FLAG( ofs ) ) {							\
			retVal = animationRLE[ ++ofs - 1 ];			\
		} else {										\
			rle = animationRLE[ ofs ] - 1;				\
			retVal = animationRLE[ ++ofs - 2 ];			\
	}	}												\
	state = ( ofs << 8 ) | rle; }
		
#define BACKWARD( state )								\
	{ ofs = state >> 8; rle = state & 255;				\
	if ( !FLAG( ofs - 1 ) )								\
	{													\
		if ( rle ++ >= animationRLE[ ofs - 1 ] ) 		\
		{												\
			ofs -= 2;									\
			rle = 0; retVal = animationRLE[ ofs ];		\
		} else	{										\
			retVal = animationRLE[ ofs - 2 ];}			\
	} else {											\
		retVal = animationRLE[ --ofs ];					\
		rle = 0;										\
	}													\
	state = ( ofs << 8 ) | rle; }

//
// here comes the animation handling
//
static int animDir = 1;
static int curFrame = 0;

static int firstD = 1;

if ( firstD )
	memset( bitmap, 0, 64 * 64 );
firstD = 0;

for ( int y = 0; y < 147; y++ )
{
	for ( int x = 0; x < 192 / 8; x++ )
	{
		unsigned int pixelState = animationState[ x + y * 192 / 8 ];
		int ofs, rle, retVal;

		if ( animDir > 0 )
			FORWARD( pixelState ) else
			BACKWARD( pixelState )

		animationState[ x + y * 192 / 8 ] = pixelState;

		retVal &= ~framebuffer[ x + 8 + min( 199, ( y + 6*8 + 1 )) * 320/8 ];

		int bx = (x * 8) / 24;
		int byteX = ( (x * 8) % 24 ) / 8;
		bitmap[ (y / 21) * 8 * 64 + bx * 64 + (y % 21) * 3 + byteX ] = retVal & 255;
	}
}

if ( skinValues.SKIN_BACKGROUND_GFX_LOOP == 1 )
{
	curFrame ++;
	if ( curFrame == 127 ) 
	{
		curFrame = 0;
		memcpy( animationState, animationStateInitial, (192 * 147 / 8)*4 );
	}
} else
{
	if ( animDir > 0 )
	{
		if ( ++curFrame == 127 )
			animDir = -animDir;
	} else
	{
		if ( --curFrame == 0 )
			animDir = -animDir;
	} 
}
