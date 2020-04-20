/*
  _________.__    .___      __   .__        __          _________.___________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     /   _____/|   \______ \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     \_____  \ |   ||    |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /        \|   ||    `   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    /_______  /|___/_______  /
        \/         \/    \/     \/       \/     \/            \/             \/ 
 
 oscilloscope_hack.h

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - code for rendering an oscilloscope to the RPi and OLED screen
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
{

//#ifdef USE_HDMI_VIDEO
#if 0

#define COLOR0 COLOR16 (10, 31, 20)
#define COLOR1 COLOR16 (10, 20, 31)
#define COLOR2 COLOR16 (31, 15, 15)

static u32 scopeValue[ 3 ][ 512 ];
static u32 scopeX = 0;
static u32 scopeUpdate = 0;

if ( ( scopeUpdate++ & 3 ) == 0 )
{
	scopeX++;
	scopeX &= 511;
	u32 x = scopeX + 200;

	u32 y = 400 + ( val1 >> 8 );
	screen->SetPixel( x, scopeValue[ 0 ][ scopeX ], 0 );
	screen->SetPixel( x, y, COLOR0 );
	scopeValue[ 0 ][ scopeX ] = y;

#ifndef SID2_DISABLED
	y = 528 + ( val2 >> 8 );
	screen->SetPixel( x, scopeValue[ 1 ][ scopeX ], 0 );
	screen->SetPixel( x, y, COLOR1 );
	scopeValue[ 1 ][ scopeX ] = y;
#endif

#ifdef EMULATE_OPL2
	y = 528 + 128 + ( valOPL >> 8 );
	screen->SetPixel( x, scopeValue[ 2 ][ scopeX ], 0 );
	screen->SetPixel( x, y, COLOR2 );
	scopeValue[ 2 ][ scopeX ] = y;
#endif
}

#endif

#ifdef USE_OLED
static u32 scopeXOLED = 0;
static u32 scopeUpdateOLED = 0;
if ( renderDone == 0 )
{
	if ( ( scopeUpdateOLED++ & 3 ) == 0 )
	{
		if ( scopeXOLED < 8 )
		{
			//memcpy( oledFrameBuffer, raspi_sid_splash, 128 * 64 / 8 );
			memcpy( &oledFrameBuffer[ scopeXOLED * 128 ], &raspi_sid_splash[ scopeXOLED * 128 ], 128 );
		} else
		{
			u32 xpos = scopeXOLED - 8;
			//memset( oledFrameBuffer, 0, 128 * 64 / 8 );

			s32 y = 32 + min( 29, max( -29, ( left + right ) / 2 / 192 ) );

			oledSetPixel( xpos, y );
			//oledClearPixel( xpos, y - 2 );
			oledClearPixel( xpos, y - 1 );
			oledClearPixel( xpos, y + 1 );
			//oledClearPixel( xpos, y + 2 );
		}

		scopeXOLED++;
		if ( scopeXOLED >= 128 + 8 )
		{
			renderDone = 1;
			scopeXOLED = 0;
		}
		//scopeXOLED %= 128 + 8;
	}
}
#endif

}