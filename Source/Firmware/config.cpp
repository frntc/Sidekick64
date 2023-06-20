/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 config.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - code for reading/parsing the config file
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
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <circle/util.h>
#include <circle/machineinfo.h>
#include "lowlevel_arm64.h"
#include "config.h"
#include "helpers.h"
#include "linux/kernel.h"

//#define DEBUG_OUT

#ifdef SIDEKICK20
u8 	cfgVIC_Emulation = 0,
	cfgVIC_VFLI_Support = 0,
	cfgVIC_Audio_Filter = 0;
u16	cfgVIC_ScanlineIntensity = 256;
#endif

u32 skinFontLoaded;
char skinFontFilename[ 1024 ];
char skinAnimationFilename[ 1024 ];
union T_SKIN_VALUES	skinValues;
int screenType, screenRotation, vdcSupport;

#define N_PROFILES 9
static u32 currentProfile = 0;

u8 colorfulProfiles[ N_PROFILES ][ SKIN_NAMES ] = 
{
	{ 0, 11, 1, 1, 1, 15, 3, 13, 0, 11, 1, 1, 1, 13, 3, 12, 10, 1, 11, 12, 15, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 7, 10, 0, 11, 1, 1, 1, 10, 7, 12, 10, 1,  9,  8,  7, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 7, 13, 0, 11, 1, 1, 1, 13, 7, 12, 10, 1,  3, 13,  7, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 7, 13, 0, 11, 1, 1, 1, 13, 7, 12, 10, 1, 11,  5, 13, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 3, 13, 0, 11, 1, 1, 1, 13, 3, 12, 10, 1, 11, 12, 15, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 7, 10, 0, 11, 1, 1, 1, 10, 7, 12, 10, 1,  4, 10,  7, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 4, 10, 0, 11, 1, 1, 1, 10, 4, 12, 10, 1,  6,  4, 10, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 10, 8, 0, 11, 1, 1, 1, 8, 10, 12, 10, 1,  9,  2, 10, 11, 0, 1, 1, 1, 0, 0 },
	{ 0, 11, 1, 1, 1, 15, 12,15, 0, 11, 1, 1, 1, 15,12, 12, 10, 1, 11, 12, 15, 11, 0, 1, 1, 1, 0, 0 },
};



void setSkinDefaultValues()
{
	skinFontLoaded = 0;
	memset( skinFontFilename, 1024, 0 );
	memset( skinAnimationFilename, 1024, 0 );

	for ( u32 i = 0; i < SKIN_NAMES; i++ )
		skinValues.v[ i ] = colorfulProfiles[ 0 ][ i ];
}

void backupUserProfile()
{
	currentProfile = 0;
	for ( u32 i = 0; i < SKIN_NAMES; i++ )
		colorfulProfiles[ 0 ][ i ] = skinValues.v[ i ];

	// special handling: overwrite scrollspeed and VDC settings in all default profiles
	for ( u32 j = 1; j < N_PROFILES; j++ )
	{
		colorfulProfiles[ j ][ SKIN_NAMES - 2 ] = colorfulProfiles[ 0 ][ SKIN_NAMES - 2 ];
		colorfulProfiles[ j ][ SKIN_NAMES - 1 ] = colorfulProfiles[ 0 ][ SKIN_NAMES - 1 ];
	}
}

void nextSkinProfile()
{
	currentProfile = ( currentProfile + 1 ) % N_PROFILES;
	for ( u32 i = 0; i < SKIN_NAMES; i++ )
		skinValues.v[ i ] = colorfulProfiles[ currentProfile ][ i ];
	//memcpy( &skinValues.v[ 0 ], &colorfulProfiles[ currentProfile ][ 0 ], SKIN_NAMES * sizeof( u32 ) );
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

//int timingValues[ TIMING_NAMES ] = { 40, 475, 470, 400, 425, 505, 200, 265, 600, 600 };
int timingValues[ TIMING_NAMES ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int menuX[ 5 ], menuY[ 5 ], menuItems[ 5 ];
char menuText[ 5 ][ MAX_ITEMS ][ 32 ], menuFile[ 5 ][ MAX_ITEMS ][ 2048 ];
int menuItemPos[ 5 ][ MAX_ITEMS ][ 2 ];

char cfg[ 65536 ];

int readConfig( CLogger *logger, char *DRIVE, char *FILENAME )
{
	u32 cfgBytes;
	memset( cfg, 0, 65536 );

	if ( !readFile( logger, DRIVE, FILENAME, (u8*)cfg, &cfgBytes ) )
		return 0;

	setSkinDefaultValues();
	memset( menuItems, 0, sizeof( u32 ) * CATEGORY_NAMES );
	memset( menuText, 0, CATEGORY_NAMES * 32 );
	memset( menuFile, 0, CATEGORY_NAMES * 2048 );

	for ( int i = 0; i < CATEGORY_NAMES; i++ )
		menuX[ i ] = -1;

	cfgPos = cfg;

	screenType = 0;
	screenRotation = 0;

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

				#ifdef SIDEKICK20 
				if ( strcmp( ptr, "VIC_EMULATION" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					if ( strstr( ptr, "NONE" ) ) cfgVIC_Emulation = 0;
					if ( strstr( ptr, "PAL" ) ) cfgVIC_Emulation = 1;
					if ( strstr( ptr, "NTSC" ) ) cfgVIC_Emulation = 2;
				}

				if ( strcmp( ptr, "VIC_VFLI_SUPPORT" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					if ( strstr( ptr, "YES" ) ) cfgVIC_VFLI_Support = 1;
					if ( strstr( ptr, "NO" ) ) cfgVIC_VFLI_Support = 0;
				}
				if ( strcmp( ptr, "VIC_AUDIO_FILTER" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					if ( strstr( ptr, "YES" ) ) cfgVIC_Audio_Filter = 1;
					if ( strstr( ptr, "NO" ) ) cfgVIC_Audio_Filter = 0;
				}
				if ( strcmp( ptr, "VIC_SCANLINE_INTENSITY" ) == 0 && ( ptr = strtok_r( NULL, "\"", &rest ) ) )
				{
					cfgVIC_ScanlineIntensity = atoi( ptr );
					if ( cfgVIC_ScanlineIntensity < 0 ) cfgVIC_ScanlineIntensity = 0;
					if ( cfgVIC_ScanlineIntensity > 256 ) cfgVIC_ScanlineIntensity = 256;
					ptr = strtok_r( NULL, " \t", &rest );
					if ( strstr( ptr, "YES" ) ) cfgVIC_Audio_Filter = 1;
					if ( strstr( ptr, "NO" ) ) cfgVIC_Audio_Filter = 0;
				}
				#endif

				if ( strcmp( ptr, "SKIN_BACKGROUND_ANIMATION" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					strncpy( skinAnimationFilename, ptr, 1023 );
					logger->Write( "RaspiMenu", LogNotice, "  >%s<", skinAnimationFilename );
				#ifdef DEBUG_OUT
					logger->Write( "RaspiMenu", LogNotice, "  >%s<", skinAnimationFilename );
				#endif
				}

				if ( strcmp( ptr, "DISPLAY" ) == 0 )
				{
					ptr = strtok_r( NULL, " \t", &rest );
					if ( strstr( ptr, "OLED1306" ) )
						screenType = 0;
					if ( strstr( ptr, "ST7789" ) )
						screenType = 1;
					if ( strstr( ptr, "ST7789_ROTATE" ) )
					{
						screenType = 1;
						screenRotation = 1;
					}
					if ( strstr( ptr, "ST7789_AUTOROTATE" ) )
					{
						screenType = 1;
						CMachineInfo *m_pMachineInfo;
						if ( m_pMachineInfo->Get()->GetMachineModel() == MachineModelZero2W )
							screenRotation = 1;
					}
				}
			}
		}
	}

	if ( timingValues[ 0 ] ) WAIT_FOR_SIGNALS = timingValues[ 0 ];
	if ( timingValues[ 1 ] ) WAIT_CYCLE_READ = timingValues[ 1 ];
	if ( timingValues[ 2 ] ) WAIT_CYCLE_WRITEDATA = timingValues[ 2 ];
	if ( timingValues[ 3 ] ) WAIT_CYCLE_READ_BADLINE = timingValues[ 3 ];
	if ( timingValues[ 4 ] ) WAIT_CYCLE_READ_VIC2 = timingValues[ 4 ];
	if ( timingValues[ 5 ] ) WAIT_CYCLE_WRITEDATA_VIC2 = timingValues[ 5 ];
	if ( timingValues[ 6 ] ) WAIT_CYCLE_MULTIPLEXER = timingValues[ 6 ];
	if ( timingValues[ 7 ] ) WAIT_CYCLE_MULTIPLEXER_VIC2 = timingValues[ 7 ];
	if ( timingValues[ 8 ] ) WAIT_TRIGGER_DMA = timingValues[ 8 ];
	if ( timingValues[ 9 ] ) WAIT_RELEASE_DMA = timingValues[ 9 ];

	if ( timingValues[ 10 ] ) POLL_FOR_SIGNALS_VIC = timingValues[ 10 ];
	if ( timingValues[ 11 ] ) POLL_FOR_SIGNALS_CPU = timingValues[ 11 ];
	if ( timingValues[ 12 ] ) POLL_CYCLE_MULTIPLEXER_VIC = timingValues[ 12 ];
	if ( timingValues[ 13 ] ) POLL_CYCLE_MULTIPLEXER_CPU = timingValues[ 13 ];
	if ( timingValues[ 14 ] ) POLL_READ = timingValues[ 14 ];
	if ( timingValues[ 15 ] ) POLL_READ_VIC2 = timingValues[ 15 ];
	if ( timingValues[ 16 ] ) POLL_WAIT_CYCLE_WRITEDATA = timingValues[ 16 ];
	if ( timingValues[ 17 ] ) POLL_TRIGGER_DMA = timingValues[ 17 ];
	if ( timingValues[ 18 ] ) POLL_RELEASE_DMA = timingValues[ 18 ];

	#ifndef SIDEKICK20 

	backupUserProfile();

	vdcSupport = skinValues.SKIN_ENABLE_VDC_OUTPUT > 0 ? 1 : 0;

	if ( skinValues.SKIN_USE_FAVORITES )
		readFavorites( logger, DRIVE );

	#endif

	return 1;
}

static int sortstring( const void *str1, const void *str2 )
{
    const char *pp1 = *(const char**)str1;
    const char *pp2 = *(const char**)str2;
    return strcasecmp( pp1, pp2 );
}

void quicksortFavs( char **begin, char **end )
{
	char **ptr = begin, **split = begin + 1;
	if ( end - begin < 1 ) return;
	while ( ++ptr <= end ) {
		if ( sortstring( ptr, begin ) < 0 ) {
			char *tmp = *ptr; *ptr = *split; *split = tmp;
			++split;
		}
	}
	char *tmp = *begin; *begin = *( split - 1 ); *( split - 1 ) = tmp;
	quicksortFavs( begin, split - 1 );
	quicksortFavs( split, end );
}

static char strupr_buffer[ 2048 ];

char *strupr( char *s )
{
	char *d = strupr_buffer;
	while ( *s != 0 )
	{
		*d = *s;
		if ( *d >= 'a' && *d <= 'z' ) *d -= 32;
		s++; d++;
	}
	*d = 0;
	return strupr_buffer;
}

int readFavorites( CLogger *logger, char *DRIVE )
{
	#define MAX_FILES	256
	char pool[ 2048 * MAX_FILES ];
	char menuTextRaw[ 128 * MAX_FILES ];
	unsigned char type[ MAX_FILES ];
	int nFiles = 0;
	int nPRG = 0, nCRT = 0, nFreezer = 0, nKernal = 0;


	char path[ 2048 ];
	sprintf( path, "SD:/FAVORITES" );

	// mount file system
	FATFS m_FileSystem;
	if ( f_mount( &m_FileSystem, "SD:", 1 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: SD:" );

	DIR dir;
	FILINFO FileInfo;
	FRESULT res = f_findfirst( &dir, &FileInfo, path, "*" );

	if ( res != FR_OK )
		logger->Write( "read directory", LogNotice, "error opening favorites directory" );

	for ( u32 i = 0, nF = 0; res == FR_OK && FileInfo.fname[ 0 ] && nF < MAX_FILES; i++, nF ++ )
	{
		if ( !( FileInfo.fattrib & ( AM_DIR ) ) )
		{
			sprintf( &pool[ (nFiles++) * 2048 ], "%s/%s", path, FileInfo.fname );
			/*
			char temp[ 4096 ];
			strcpy( temp, path );
			strcat( temp, "/" );
			strcat( temp, FileInfo.fname );

			logger->Write( "RaspiMenu", LogNotice, "%s", temp );*/
		}
		res = f_findnext( &dir, &FileInfo );
	}

	f_closedir( &dir );


	// unmount file system
	if ( f_mount( 0, "SD:", 0 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: SD:" );


	//
	// now let's figure out some decent layout and create the menu
	//
	char *list[ MAX_FILES ];
	for ( int i = 0; i < nFiles; i++ )
		list[ i ] = &pool[ i * 2048 ];

	quicksortFavs( list, &list[ nFiles - 1 ] );

	//for ( int i = 0; i < nFiles; i++ )
		//logger->Write( "RaspiMenu", LogNotice, "%s", strupr( list[ i ] ) );

	for ( int i = 0; i < nFiles; i++ )
	{
		// extract menu entry
		char m[ 2048 ];

		strncpy( m, &list[ i ][ 14 ], 2047 );
		m[ 2047 ] = 0;
		type[ i ] = 0;


		u32 error = 0, isFreezer = 0;

		extern int checkCRTFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *error, u32 *isFreezer );
		if ( strstr( strupr( m ), ".CRT" ) != NULL )
			checkCRTFile( logger, DRIVE, list[ i ], &error, &isFreezer );

		if ( strstr( strupr( m ), ".GEORAM" ) != NULL )
		{
			nCRT ++; type[ i ] = 0;
		} else
		if ( strstr( strupr( m ), ".PRG" ) != NULL ) 
		{
			nPRG ++; type[ i ] = 1;
		} else
		if ( strstr( strupr( m ), ".CRT" ) != NULL && error == 0 )
		{
			if ( isFreezer )
			{
				nFreezer ++; type[ i ] = 2;
			} else
			{ 
				nCRT ++; type[ i ] = 0; 
			};
		} else
		if ( strstr( strupr( m ), ".BIN" ) != NULL ||
			 strstr( strupr( m ), ".ROM" ) != NULL ) 
		{
			nKernal ++; type[ i ] = 3;
		} else
		{
			for ( int j = i; j < nFiles - 1; j++ )
				list[ j ] = list[ j + 1 ];
			nFiles --; i --;
			if ( i < nFiles )
				continue;
		}

		// strip extension
		for ( int j = strlen( m ) - 1; j > 0; j -- ) 
			if ( m[ j ] == '.' ) 
			{
				m[ j ] = 0;
				break;
			}
		// strip leading sorting index (2 digits)
		if ( strlen( m ) > 3 && m[ 0 ] >= '0' && m[ 0 ] <= '9' && m[ 1 ] >= '0' && m[ 1 ] <= '9' && m[ 2 ] == '_' )
		{
			memcpy( &m[ 0 ], &m[ 3 ], strlen( m ) - 3 + 1 );
		} else
		// strip leading sorting index (2 digits)
		if ( strlen( m ) > 2 && m[ 0 ] >= '0' && m[ 0 ] <= '9' && m[ 1 ] == '_' )
		{
			memcpy( &m[ 0 ], &m[ 2 ], strlen( m ) - 2 + 1 );
		}

		strncpy( &menuTextRaw[ i * 128 ], m, 17 );
		menuTextRaw[ i * 128 + 17 ] = 0;
	}

	// lines per category: 1 + count + 1 = count + 2 (for header and space below)
	int pos[ 4 ][ 2 ];

	// I don't want to implement testing all possible permutations and scoring of layouts,
	// the following is testing the most meaningful default settings, and if it doesn't work
	// then it doesn't work :)

	int nRows = 18, nRowsS = 18 - 6;

	// negative indicates: not available (and counters the space of header + empty space
	if ( nCRT == 0 ) nCRT = -2;
	if ( nPRG == 0 ) nPRG = -2;
	if ( nFreezer == 0 ) nFreezer = -2;
	if ( nKernal == 0 ) nKernal = -2;

	int nElements[ 4 ] = { nCRT, nPRG, nFreezer, nKernal };

	// permutations
	const int nPermutations = 14;
	#define LAYOUT22 0
	#define LAYOUT31 1
	int perm[nPermutations][ 5 ] = {
		// good 2+2 layouts
		{ 0, 1, 2, 3, LAYOUT22 },
		{ 2, 3, 0, 1, LAYOUT22 },
		{ 0, 2, 1, 3, LAYOUT22 },
		{ 1, 3, 0, 2, LAYOUT22 },

		// good 3+1 layouts
		{ 0, 1, 2, 3, LAYOUT31 },
		{ 0, 1, 3, 2, LAYOUT31 },
		{ 0, 2, 3, 1, LAYOUT31 },
		{ 1, 2, 3, 0, LAYOUT31 },

		{ 2, 3, 0, 1, LAYOUT31 },
		{ 2, 3, 1, 0, LAYOUT31 },
		{ 2, 0, 1, 3, LAYOUT31 },
		{ 3, 0, 1, 2, LAYOUT31 },

		// other 2+2 layouts
		{ 0, 3, 1, 2, LAYOUT22 },
		{ 1, 2, 0, 3, LAYOUT22 },
	};

	bool success = false;
	for ( int p = 0; p < nPermutations; p++ )
	{
		int p0 = perm[ p ][ 0 ];
		int p1 = perm[ p ][ 1 ];
		int p2 = perm[ p ][ 2 ];
		int p3 = perm[ p ][ 3 ];

		// first test layouts with 2 categories in each column
		if ( perm[ p ][ 4 ] == LAYOUT22 )
		{
			int nGroup1 = nElements[ p0 ] +  nElements[ p1 ] + 4;
			int nGroup2 = nElements[ p2 ] +  nElements[ p3 ] + 4;

			int sizeL = max( nGroup1, nGroup2 );
			int sizeS = min( nGroup1, nGroup2 );

			if ( sizeL <= nRows && sizeS <= nRowsS )
			{
				pos[ p0 ][ 0 ] = ( sizeL == nGroup1 ) ? 1 : 21; pos[ p0 ][ 1 ] = 4; 
				pos[ p1 ][ 0 ] = ( sizeL == nGroup1 ) ? 1 : 21; pos[ p1 ][ 1 ] = pos[ p0 ][ 1 ] + nElements[ p0 ] + 2; 

				pos[ p2 ][ 0 ] = ( sizeL == nGroup1 ) ? 21 : 1; pos[ p2 ][ 1 ] = 4; 
				pos[ p3 ][ 0 ] = ( sizeL == nGroup1 ) ? 21 : 1; pos[ p3 ][ 1 ] = pos[ p2 ][ 1 ] + nElements[ p2 ] + 2; 

				success = true;
				break;
			} 
		} else
		// layouts with 3 categories in one column
		if ( perm[ p ][ 4 ] == LAYOUT31 )
		{
			int nGroup1 = nElements[ p0 ] +  nElements[ p1 ] + nElements[ p2 ] + 6;
			int nGroup2 = nElements[ p3 ] + 2;

			int sizeL = max( nGroup1, nGroup2 );
			int sizeS = min( nGroup1, nGroup2 );

			if ( sizeL <= nRows && sizeS <= nRowsS )
			{
				pos[ p0 ][ 0 ] = ( sizeL == nGroup1 ) ? 1 : 21; pos[ p0 ][ 1 ] = 4; 
				pos[ p1 ][ 0 ] = ( sizeL == nGroup1 ) ? 1 : 21; pos[ p1 ][ 1 ] = pos[ p0 ][ 1 ] + nElements[ p0 ] + 2; 

				pos[ p2 ][ 0 ] = ( sizeL == nGroup1 ) ? 1 : 21; pos[ p2 ][ 1 ] = pos[ p1 ][ 1 ] + nElements[ p1 ] + 2; 
				
				pos[ p3 ][ 0 ] = ( sizeL == nGroup1 ) ? 21 : 1; pos[ p3 ][ 1 ] = 4; 

				success = true;
				break;
			} 
		}
	}

	if ( !success )
	{
 		// layout entries one after another (including break to next column?)
		// try all permutations, if none works take the preferred order of
		// categories and place on screen what fits
		for ( int p = 0; p < nPermutations + 1; p++ )
		{
			int p0 = perm[ p % nPermutations ][ 0 ];
			int p1 = perm[ p % nPermutations ][ 1 ];
			int p2 = perm[ p % nPermutations ][ 2 ];
			int p3 = perm[ p % nPermutations ][ 3 ];
			int pg[] = { p0, p1, p2, p3 };

			int nElementsSplit = -1;
			int x = 1, y = 0; // +4 added afterwards

			for ( int g = 0; g < 4; g++ )
			if ( nElements[ pg[ g ] ] > 0 )
			{
				pos[ pg[ g ] ][ 0 ] = x; 
				pos[ pg[ g ] ][ 1 ] = y;
				y += nElements[ pg[ g ] ] + 2;
				
				if ( y > nRows )
				{
					nElementsSplit = y - nRows;
					x = 21;
					y = nElementsSplit + 1;
				}
			}
			if ( nElementsSplit >= 2 || nElementsSplit < 0 ) // layout ok
			{
				// do we need to truncate a categorie in the right column?
				for ( int g = 0; g < 4; g++ )
				if ( nElements[ pg[ g ] ] > 0 && pos[ pg[ g ] ][ 0 ] > 1 )
				{
					while ( pos[ pg[ g ] ][ 1 ] + nElements[ pg[ g ] ] >= nRowsS - 1 ) 
						nElements[ pg[ g ] ] --;
					if ( nElements[ pg[ g ] ] < 0 ) nElements[ pg[ g ] ] = 0;
					/*{ 
						nElements[ pg[ g ] ] = max( 0, nElements[ pg[ g ] ] - (y - nRowsS) );
					}*/
				}

				for ( int g = 0; g < 4; g++ ) pos[ g ][ 1 ] += 4;
				nCRT = nElements[ 0 ];
				nPRG = nElements[ 1 ];
				nFreezer = nElements[ 2 ];
				nKernal = nElements[ 3 ];
				success = true;
				break;
			}
		}
	}

	if ( nCRT <= 0 ) pos[ 0 ][ 1 ] = 25;
	if ( nPRG <= 0 ) pos[ 1 ][ 1 ] = 25;
	if ( nFreezer <= 0 ) pos[ 2 ][ 1 ] = 25;
	if ( nKernal <= 0 ) pos[ 3 ][ 1 ] = 25;

	// output menu definition
	memset( menuItems, 0, sizeof( u32 ) * CATEGORY_NAMES );
	memset( menuText, 0, CATEGORY_NAMES * 32 );
	memset( menuFile, 0, CATEGORY_NAMES * 2048 );

	for ( int i = 0; i < CATEGORY_NAMES; i++ )
		menuX[ i ] = -1;


	#define UPDATE_ITEM_POS( x, y, i ) \
		for ( int k = menuItems[ i ]; k < MAX_ITEMS; k++ ) {			\
			menuItemPos[ i ][ k ][ 0 ] = x;								\
			menuItemPos[ i ][ k ][ 1 ] = y + k - menuItems[ i ] + 1; }

	const u8 rm[] = { 2, 3, 1, 4, 0 };

	//printf( "MENU SPECIAL 20 16\n" );
	int x = 21, y = 16;
	menuX[ 0 ] = x; menuY[ 0 ] = y; UPDATE_ITEM_POS( x, y, 0 )

	for ( int i = 0; i < 4; i++ )
	{
		int x = pos[ i ][ 0 ], y = pos[ i ][ 1 ];

		const char *catName[4] = { "CRT", "PRG", "FREEZER", "KERNAL" };

		if ( menuX[ rm[ i ] ] == -1 )
		{
			menuX[ rm[ i ] ] = x; menuY[ rm[ i ] ] = y; 
			UPDATE_ITEM_POS( x, y, rm[ i ] )
		}

		if ( y >= 25 ) 
			continue;
		//printf( "\nMENU %s %d %d\n", catName[ i ], x, y );
		//logger->Write( "RaspiMenu", LogNotice, "MENU %s %d %d", catName[ i ], x, y );
		y ++;

		int nElementsInCategory = nElements[ i ];

		for ( int j = 0; j < nFiles; j++ )
		if ( type[ j ] == i )
		{
			char *m = &menuTextRaw[ j * 128 ];
			int t = rm[ i ];

			logger->Write( "RaspiMenu", LogNotice, "%s %s (%d: %d)", catName[ i ], m, t, menuItems[ t ] );
			memset( menuText[ t ][ menuItems[ t ] ], 0, 18 );
			strncpy( menuText[ t ][ menuItems[ t ] ], m, 17 );
			menuText[ t ][ menuItems[ t ] ][ 17 ] = 0;
			strncpy( menuFile[ t ][ menuItems[ t ] ], list[ j ], 2047 );
			menuItems[ t ] ++;

			y ++;
			if ( y >= nRows + 4 -1)
			{
				x = 21; y = 4 - 1; 
				if ( nElementsInCategory - 1 > 0 )
				{
					//logger->Write( "RaspiMenu", LogNotice, "MENU %s %d %d", catName[ i ], x, y - 1 );
					/*menuX[ t ] = x; menuY[ t ] = y - 1; */ UPDATE_ITEM_POS( x, y, t )
				}
			}
			if ( --nElementsInCategory == 0 ) break;
		}
	}

	return 0;

}

