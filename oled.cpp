/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 oled.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - some simple OLED framebuffer/helpers
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
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/memory.h>
#include "oled.h"
#include "lowlevel_arm64.h"
#include "latch.h"
#include "helpers.h"

u8 oledFrameBuffer[ 128 * 64 / 8 ];

void oledClear()
{
	memset( oledFrameBuffer, 0, 128 * 64 / 8 );
}

static u32 sfb_j, sfb_y, sfb_x;

void sendFramebufferStart()
{
	sfb_j = sfb_y = sfb_x = 0;
}

bool sendFramebufferDone()
{
	return (sfb_y == 8);
}

void sendFramebufferNext( u32 nBytes )
{
	if( sfb_y == 8 ) return;

	if ( sfb_x == 0 && sfb_y == 0 )
	{
		ssd1306_setpos( 0, sfb_y );
		ssd1306_send_data_start();
	}

	for ( u32 i = 0; i < nBytes; i++ )
	{
		ssd1306_send_byte( oledFrameBuffer[ sfb_j ++ ] );
		sfb_x ++;
	}

	if ( sfb_x == 128 )
	{
		sfb_x = 0; sfb_y ++;

		if ( sfb_y == 8 )
			ssd1306_send_data_stop();
	}
}

void sendFramebuffer()
{
	u32 j = 0;
	ssd1306_setpos( 0, 0 );
	ssd1306_send_data_start();
	for ( int y = 0; y < 64 / 8; y++ )
	{
		for ( int x = 0; x < 128; x++ )
			ssd1306_send_byte( oledFrameBuffer[ j++ ] );
	}
	ssd1306_send_data_stop();
}


void oledSetContrast( u8 c )
{
	ssd1306_send_command( 0x81 ); // SSD1306_SETCONTRAST
	ssd1306_send_command( c ); 
}

void splashScreen( const u8 *fb )
{
	ssd1306_init();
	ssd1306_send_command( 0x81 ); // SSD1306_SETCONTRAST
	ssd1306_send_command( 0xfF ); // 0x9F or 0xCF
	ssd1306_send_command( 0x2E ); // SSD1306_DEACTIVATE_SCROLL

	flushI2CBuffer( true );

	for ( int y = 0; y < 64 / 8; y++ )
	{
		ssd1306_setpos( 0, y );
		flushI2CBuffer( true );

		ssd1306_send_data_start();
		flushI2CBuffer( true );

		for ( int x = 0; x < 128; x++ )
		{
			ssd1306_send_byte( *fb++ );
			flushI2CBuffer( true );
		}
		ssd1306_send_data_stop();
		flushI2CBuffer( true );
	}
}

int splashScreenFile( const char *drive, char *fn )
{
	u8 temp[ 65536 ];
	u32 size;
	extern CLogger *logger;
	if ( readFile( logger, (char*)drive, fn, temp, &size ) )
	{
		u8 buf[ 1024 ];
		memset( buf, 0, 1024 );
		for ( int i = 0; i < 8192; i++ )
			if ( temp[ i ] >= 128 )
				buf[ (i&127) + ((i/128)/8) * 128 ] |=  (1 << ((i/128)&7));
		splashScreen( buf );
		return 1;
	}
	return 0;
}