/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 config.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - code for reading/parsing the config file
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
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <circle/util.h>
#include "lowlevel_arm64.h"
#include "config.h"
#include "helpers.h"
#include "linux/kernel.h"

//#define DEBUG_OUT

u32 skinFontLoaded;
char skinFontFilename[ 1024 ];
union T_SKIN_VALUES	skinValues;
int screenType;

void setSkinDefaultValues()
{
	skinFontLoaded = 0;
	memset( skinFontFilename, 1024, 0 );

	const u8 d[ SKIN_NAMES ] = { 6, 14, 1, 14, 15, 14, 1, 1, 6, 14, 1, 1, 1, 14, 7, 14, 10, 1 };
	for ( u32 i = 0; i < SKIN_NAMES; i++ )
		skinValues.v[ i ] = d[ i ];
}

extern void clearC64();
extern void printC64( u32 x, u32 y, const char *t, u8 color, u8 flag = 0, u32 convert = 0,  u32 maxL = 1024 );

int atoi( char* str )
{
	int res = 0;
	for ( int i = 0; str[ i ] != '\0' && str[ i ] != 10 && str[ i ] != 13; i ++ )
		if ( str[ i ] >= '0' && str[ i ] <= '9' )
			res = res * 10 + str[ i ] - '0';
	return res;
}

char *cfgPos;
char curLine[ 2048 ];

int getNextLine()
{
	memset( curLine, 0, 2048 );

	int sp = 0, ep = 0;
	while ( cfgPos[ ep ] != 0 && cfgPos[ ep ] != '\n' ) ep++;

	while ( sp < ep && ( cfgPos[ sp ] == ' ' || cfgPos[ sp ] == 9 ) ) sp++;

	strncpy( curLine, &cfgPos[ sp ], ep - sp - 1 );

	cfgPos = &cfgPos[ ep + 1 ];

	return ep - sp;
}

int timingValues[ TIMING_NAMES ] = { 40, 475, 470, 400, 425, 505, 200, 265, 600, 600 };
int menuX[ 5 ], menuY[ 5 ], menuItems[ 5 ];
char menuText[ 5 ][ MAX_ITEMS ][ 32 ], menuFile[ 5 ][ MAX_ITEMS ][ 2048 ];
int menuItemPos[ 5 ][ MAX_ITEMS ][ 2 ];

char cfg[ 16384 ];

int readConfig( CLogger *logger, char *DRIVE, char *FILENAME )
{
	u32 cfgBytes;
	memset( cfg, 0, 16384 );

	if ( !readFile( logger, DRIVE, FILENAME, (u8*)cfg, &cfgBytes ) )
		return 0;

	setSkinDefaultValues();
	memset( menuItems, 0, sizeof( u32 ) * 4 );
	memset( menuText, 0, 4 * 32 );
	memset( menuFile, 0, 4 * 2048 );

	for ( int i = 0; i < CATEGORY_NAMES; i++ )
		menuX[ i ] = -1;

	cfgPos = cfg;

	screenType = 0;

	while ( *cfgPos != 0 )
	{
		if ( getNextLine() && curLine[ 0 ] )
		{
			char *rest = NULL;
			char *ptr = strtok_r( curLine, " \t", &rest );

			if ( ptr )
			{
				if ( strcmp( ptr, "MENU" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					for ( int i = 0; i < CATEGORY_NAMES; i++ )
						if ( strcmp( ptr, categoryNames[ i ] ) == 0 )
						{
							int x = atoi( ptr = strtok_r( NULL, " \t", &rest ) );
							int y = atoi( ptr = strtok_r( NULL, " \t", &rest ) );

							if ( menuX[ i ] == -1 )
							{
								menuX[ i ] = x;
								menuY[ i ] = y;
							}

							for ( int k = menuItems[ i ]; k < MAX_ITEMS; k++ )
							{
								menuItemPos[ i ][ k ][ 0 ] = x;
								menuItemPos[ i ][ k ][ 1 ] = y + k - menuItems[ i ] + 1;
							}

						#ifdef DEBUG_OUT
							logger->Write( "RaspiMenu", LogNotice, "  menu %s x=%d, y=%d", categoryNames[ i ], menuX[ i ], menuY[ i ] );
						#endif
							break;
						}
				}

				for ( int i = 0; i < CATEGORY_NAMES; i++ )
					if ( strcmp( ptr, categoryNames[ i ] ) == 0 && ( ptr = strtok_r( NULL, "\"", &rest ) ) )
					{
						strncpy( menuText[ i ][ menuItems[ i ] ], ptr, 31 );
						ptr = strtok_r( NULL, " \t", &rest );
						strncpy( menuFile[ i ][ menuItems[ i ] ], ptr, 2047 );
						menuItems[ i ] ++;
					#ifdef DEBUG_OUT
						logger->Write( "RaspiMenu", LogNotice, "  %s >%s<, file %s", categoryNames[ i ], menuText[ i ][ menuItems[ i ] - 1 ], menuFile[ i ][ menuItems[ i ] - 1 ] );
					#endif
						break;
					}

				for ( int i = 0; i < TIMING_NAMES; i++ )
					if ( strcmp( ptr, timingNames[ i ] ) == 0 && ( ptr = strtok_r( NULL, "\"", &rest ) ) )
					{
						timingValues[ i ] = atoi( ptr );
						while ( *ptr == '\t' || *ptr == ' ' ) ptr++;
					#ifdef DEBUG_OUT
						logger->Write( "RaspiMenu", LogNotice, "  %s >%d< (%s)", timingNames[ i ], timingValues[ i ], ptr );
					#endif
						break;
					}

				for ( int i = 0; i < SKIN_NAMES; i++ )
					if ( strcmp( ptr, skinNames[ i ] ) == 0 && ( ptr = strtok_r( NULL, "\"", &rest ) ) )
					{
						skinValues.v[ i ] = atoi( ptr );
						while ( *ptr == '\t' || *ptr == ' ' ) ptr++;
					#ifdef DEBUG_OUT
						logger->Write( "RaspiMenu", LogNotice, "  %s >%d< (%s)", skinNames[ i ], skinValues.v[ i ], ptr );
					#endif
						break;
					}		

				if ( strcmp( ptr, "SKIN_FONT" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					strncpy( skinFontFilename, ptr, 1023 );
				#ifdef DEBUG_OUT
					logger->Write( "RaspiMenu", LogNotice, "  >%s<", skinFontFilename );
				#endif
				}

				if ( strcmp( ptr, "DISPLAY" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					if ( strstr( ptr, "OLED1306" ) )
						screenType = 0;
					if ( strstr( ptr, "ST7789" ) )
						screenType = 1;
					strncpy( skinFontFilename, ptr, 1023 );
				#ifdef DEBUG_OUT
					logger->Write( "RaspiMenu", LogNotice, "  >%s<", skinFontFilename );
				#endif
				}

			}
		}
	}

	WAIT_FOR_SIGNALS = timingValues[ 0 ];
	WAIT_CYCLE_READ = timingValues[ 1 ];
	WAIT_CYCLE_WRITEDATA = timingValues[ 2 ];
	WAIT_CYCLE_READ_BADLINE = timingValues[ 3 ];
	WAIT_CYCLE_READ_VIC2 = timingValues[ 4 ];
	WAIT_CYCLE_WRITEDATA_VIC2 = timingValues[ 5 ];
	WAIT_CYCLE_MULTIPLEXER = timingValues[ 6 ];
	WAIT_CYCLE_MULTIPLEXER_VIC2 = timingValues[ 7 ];
	WAIT_TRIGGER_DMA = timingValues[ 8 ];
	WAIT_RELEASE_DMA = timingValues[ 9 ];

	return 1;
}

