/*
  _________.__    .___      __   .__        __       ________  ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_____  \/  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /  ____/   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    /       \  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   \_______ \_____  /\____   | 
        \/         \/    \/     \/       \/     \/           \/     \/      |__|  
 
 264screen.cpp

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - menu/screen code
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

// this file contains unelegant code for generating the menu screens 
// and handling key presses forwarded from the C64

#include "linux/kernel.h"
#include "c64screen.h"
#include "lowlevel_arm64.h"
#include "dirscan.h"
#include "264config.h"
#include "crt.h"
#include "kernel_menu264.h"

#define KEY_F1	133
#define KEY_F4	138
#define KEY_F2	137
#define KEY_F5	135
#define KEY_F3	134
#define KEY_F6	139
#define KEY_HELP	140
#define KEY_F7	136

const int VK_LEFT  = 157;
const int VK_RIGHT = 29;
const int VK_UP    = 145;
const int VK_DOWN  = 17;
const int VK_HOME  = 19;
const int VK_S	   = 83;

const int VK_ESC = 95;
const int VK_DEL = 20;
const int VK_RETURN = 13;
const int VK_SHIFT_RETURN = 141;
const int VK_MOUNT = 205; // SHIFT-M
const int VK_MOUNT_START = 77; // M

extern CLogger *logger;

// todo 
extern u32 prgSizeLaunch;
extern unsigned char prgDataLaunch[ 65536 ];

static const char DRIVE[] = "SD:";
static const char SETTINGS_FILE[] = "SD:C16/special.cfg";

u8 c64screen[ 40 * 25 + 1024 * 4 ]; 
u8 c64color[ 40 * 25 + 1024 * 4 ]; 

char *errorMsg = NULL;

char errorMessages[5][41] = {
//   1234567890123456789012345678901234567890
	"                NO ERROR                ",
	"  ERROR: UNKNOWN/UNSUPPORTED .CRT TYPE  ",
	"          ERROR: NO .CRT-FILE           ", 
	"          ERROR READING FILE            ",
	"            SETTINGS SAVED              "
};


#define MENU_MAIN	 0x00
#define MENU_BROWSER 0x01
#define MENU_ERROR	 0x02
#define MENU_CONFIG  0x03
u32 menuScreen = 0, 
	previousMenuScreen = 0;
u32 updateMenu = 1;
u32 typeInName = 0;
u32 typeCurPos = 0;

int cursorPos = 0;
int scrollPos = 0;
int lastLine;
int lastRolled = -1;
int lastScrolled = -1;
int lastSubIndex = -1;
int subGeoRAM = 0;
int subSID = 0;
int subHasKernal = -1;
int subHasLaunch = -1;

void clearC64()
{
	memset( c64screen, ' ', 40 * 25 );
	memset( c64color, 0, 40 * 25 );
}

u8 PETSCII2ScreenCode( u8 c )
{
	if ( c < 32 ) return c + 128;
	if ( c < 64 ) return c;
	if ( c < 96 ) return c - 64;
	if ( c < 128 ) return c - 32;
	if ( c < 160 ) return c + 64;
	if ( c < 192 ) return c - 64;
	return c - 128;
}

void printC64( u32 x, u32 y, const char *t, u8 color, u8 flag, u32 convert, u32 maxL )
{
	u32 l = min( strlen( t ), maxL );

	for ( u32 i = 0; i < l; i++ )
	{
		u32 c = t[ i ], c2 = c;

		// screen code conversion 
		if ( convert == 1 )
		{
			c2 = PETSCII2ScreenCode( c );
		} else
		{
			if ( convert == 2 && c >= 'a' && c <= 'z' )
				c = c + 'A' - 'a';
			if ( convert == 3 && c >= 'A' && c <= 'Z' )
				c = c + 'a' - 'A';
			if ( ( c >= 'a' ) && ( c <= 'z' ) )
				c2 = c + 1 - 'a';
		}

		c64screen[ x + y * 40 + i ] = c2 | flag;
		c64color[ x + y * 40 + i ] = color;
	}
}

u32 extraMsg = 0;

int scanFileTree( u32 cursorPos, u32 scrollPos )
{
	extraMsg = 0;

	u32 lines = 0;
	s32 idx = scrollPos;
	while ( lines < DISPLAY_LINES && idx < nDirEntries ) 
	{
		if ( idx >= nDirEntries ) 
			break;

		if ( (u32)idx == cursorPos && dir[ idx ].f & DIR_D64_FILE )
			extraMsg = 1;

		if ( dir[ idx ].f & DIR_DIRECTORY || dir[ idx ].f & DIR_D64_FILE )
		{
			if ( !(dir[ idx ].f & DIR_UNROLLED) )
				idx = dir[ idx ].next; else
				idx ++;
		} else
			idx ++;

		lines ++;
	}
	return idx;
}


int printFileTree( s32 cursorPos, s32 scrollPos )
{
	s32 lines = 0;
	s32 idx = scrollPos;
	s32 lastVisible = 0;
	while ( lines < DISPLAY_LINES && idx < nDirEntries ) 
	{
		if ( idx >= nDirEntries ) 
			break;

		u32 convert = 3;
		u8 color = skinValues.SKIN_TEXT_BROWSER;
	
		if ( dir[ idx ].f & DIR_DIRECTORY || dir[ idx ].f & DIR_D64_FILE )
			color = skinValues.SKIN_TEXT_BROWSER_DIRECTORY;

		if ( dir[ idx ].f & DIR_FILE_IN_D64 )
			convert = 1;

		if ( idx == cursorPos )
			color = skinValues.SKIN_TEXT_BROWSER_CURRENT;

		char temp[1024], t2[ 1024 ] = {0};
		memset( t2, 0, 1024 );
		int leading = 0;
		if( dir[ idx ].level > 0 )
		{
			for ( u32 j = 0; j < dir[ idx ].level - 1; j++ )
			{
				strcat( t2, " " );
				leading ++;
			}
			if ( convert != 3 )
				t2[ dir[ idx ].level - 1 ] = 93+32; else
				t2[ dir[ idx ].level - 1 ] = 93;
			leading ++;
		}

		if ( (dir[ idx ].parent != 0xffffffff && dir[ dir[ idx ].parent ].f & DIR_D64_FILE && dir[ idx ].parent == (u32)(idx - 1) ) )
		{
			if ( dir[ idx ].size > 0 )
				sprintf( temp, "%s%s                              ", t2, dir[ idx ].name ); else
				sprintf( temp, "%s%s", t2, dir[ idx ].name );
			
			if ( strlen( temp ) > 34 ) temp[ 35 ] = 0;

			if ( idx == cursorPos )
			{
				printC64( 2, lines + 3, temp, color, 0x80, convert ); 
			} else
			{
				printC64( 2, lines + 3, t2, color, 0x00, convert ); 
				printC64( 2 + leading, lines + 3, (char*)dir[ idx ].name, color, 0x80, convert ); 
			}
		} else
		{
			if ( dir[ idx ].size > 0 )
				sprintf( temp, "%s%s                              ", t2, dir[ idx ].name ); else
				sprintf( temp, "%s%s", t2, dir[ idx ].name );
			if ( strlen( temp ) > 34 ) temp[ 35 ] = 0;

			printC64( 2, lines + 3, temp, color, (idx == cursorPos) ? 0x80 : 0, convert );

			if ( dir[ idx ].size > 0 )
			{
				if ( convert == 1 )
				{
					// file in D64
					sprintf( temp, " %3dK", dir[ idx ].size / 1024 );
				} else
				{
					if ( dir[ idx ].size / 1024 > 999 )
						sprintf( temp, " %1.1fm", (float)dir[ idx ].size / (1024 * 1024) ); else
						sprintf( temp, " %3dk", dir[ idx ].size / 1024 );
				} 

				printC64( 32, lines + 3, temp, color, (idx == cursorPos) ? 0x80 : 0, convert );
			}
		}
		lastVisible = idx;

		if ( dir[ idx ].f & DIR_DIRECTORY || dir[ idx ].f & DIR_D64_FILE )
		{
			if ( !(dir[ idx ].f & DIR_UNROLLED) )
				idx = dir[ idx ].next; else
				idx ++;
		} else
			idx ++;

		lines ++;
	}

	return lastVisible;
}


void printBrowserScreen()
{
	clearC64();

	//printC64(0,0,  "0123456789012345678901234567890123456789", 15, 0 );
	printC64( 0,  1, "       .- sidekick264 browser -.        ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( 0, 23, "  F1/F2 Page Up/Down, HELP Back to Menu ", skinValues.SKIN_BROWSER_TEXT_HEADER, 0, 3 );
	printC64( 2, 23, "F1", skinValues.SKIN_BROWSER_TEXT_HEADER, 128, 3 );
	printC64( 5, 23, "F2", skinValues.SKIN_BROWSER_TEXT_HEADER, 128, 3 );
	printC64( 22, 23, "HELP", skinValues.SKIN_BROWSER_TEXT_HEADER, 128, 3 );

	if ( subGeoRAM )
	{
		printC64(  0, 24, "      choose PRG w/ NeoRAM to start     ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 13, 24, "PRG w/ NeoRAM", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
	} else
	if ( subSID )
	{
		printC64(  0, 24, "      choose PRG w/ SID+FM to start     ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 13, 24, "PRG w/ SID+FM", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
	}

	lastLine = printFileTree( cursorPos, scrollPos );

	// scroll bar
	int t = scrollPos * DISPLAY_LINES / nDirEntries;
	int b = lastLine * DISPLAY_LINES / nDirEntries;

	// bar on the right
	for ( int i = 0; i < DISPLAY_LINES; i++ )
	{
		char c = 94;
		if ( t <= i && i <= b )
			c = 96 + 128 - 64;
		c64screen[ 38 + ( i + 3 ) * 40 ] = c;
		c64color[ 38 + ( i + 3 ) * 40 ] = 1;
	}

	c64screen[ 1000 ] = ((0x6000 >> 8) & 0xFC);
	c64screen[ 1001 ] = skinValues.SKIN_BROWSER_BACKGROUND_COLOR;
	c64screen[ 1002 ] = skinValues.SKIN_BROWSER_BORDER_COLOR;

	/*startInjectCode();
	// charset address
	injectPOKE( 0xff13, ((0x6000 >> 8) & 0xFC) );
	injectPOKE( 0xff15, skinValues.SKIN_BROWSER_BACKGROUND_COLOR );
	injectPOKE( 0xff19, skinValues.SKIN_BROWSER_BORDER_COLOR );*/
}

void resetMenuState( int keep = 0 )
{
	if ( !( keep & 1 ) ) subGeoRAM = 0;
	if ( !( keep & 2 ) ) subSID = 0;
	subHasKernal = -1;
	subHasLaunch = -1;
}

int getMainMenuSelection( int key, char **FILE, char **FILE2, int *addIdx )
{
	*FILE = NULL;

	if ( key >= 'a' && key <= 'z' )
		key = key + 'A' - 'a';


	if ( key == KEY_F7 ) { resetMenuState(0); return 1;/* Exit */ } else
	if ( key == KEY_HELP ) { resetMenuState(3); return 2;/* Browser */ } else
	if ( key == KEY_F1 ) { resetMenuState(1); return 3;/* GEORAM */ } else
	if ( key == KEY_F2 ) { resetMenuState(2); return 4;/* SID */ } else
	{
		if ( key >= 'A' && key < 'A' + menuItems[ 1 ] ) // PRG
		{
			int i = key - 'A';
			*FILE = menuFile[ 1 ][ i ];
			//logger->Write( "RaspiMenu", LogNotice, "getselection: %s -> %s\n", menuText[ 1 ][ i ], menuFile[ 1 ][ i ] );
			return 6;
		} else
		if ( key >= '1' && key < '1' + menuItems[ 2 ] ) // CRT
		{
			resetMenuState(); 
			int i = key - '1';
			*FILE = menuFile[ 2 ][ i ];
			*FILE2 = menuFile2[ 2 ][ i ];
			//logger->Write( "RaspiMenu", LogNotice, "%s -> %s + %s\n", menuText[ 2 ][ i ], menuFile[ 2 ][ i ], menuFile2[ 2 ][ i ] );
			return 5;
		} 
	}

	return 0;
}

extern void deactivateCart();

#define MAX_SETTINGS 18
u32 curSettingsLine = 0;
s32 rangeSettings[ MAX_SETTINGS ] = { 4, 10, 3, 2, 2, 16, 15, 4, 2, 16, 15, 2, 2, 16, 15, 16, 16, 2 };
s32 settings[ MAX_SETTINGS ]      = { 0,  0, 0, 0, 0, 15,  0, 0, 0, 15, 14, 0, 0, 15, 7, 15, 15, 0 };
u8  geoRAM_SlotNames[ 10 ][ 21 ];

// 0
// 1
// 2 SID #1 type
// 3 SID #1 addr
// 4 register read
// 5 vol
// 6 panning
// 7 SID #2 type
// 8 SID #2 addr
// 9 vol
// 10 panning
// 11 clock
// 12 SFX
// 13 vol
// 14 pan
// 15 Digiblaster vol
// 16 TED vol
// 17 output

void writeSettingsFile()
{
	char cfg[ 16384 ];
	u32 cfgBytes = 0;
    memset( cfg, 0, 16384 );
	cfgBytes = sizeof( s32 ) * MAX_SETTINGS;
	memcpy( cfg, settings, cfgBytes );
	memcpy( &cfg[ cfgBytes ], geoRAM_SlotNames, sizeof( u8 ) * 10 * 21 );
	cfgBytes += sizeof( u8 ) * 10 * 21;

	writeFile( logger, DRIVE, SETTINGS_FILE, (u8*)cfg, cfgBytes );
}


void readSettingsFile()
{
	char cfg[ 16384 ];
	u32 cfgBytes;
    memset( cfg, 0, 16384 );

	memset( settings, 0, sizeof( s32 ) * MAX_SETTINGS );
	memset( geoRAM_SlotNames, 32, sizeof( u8 ) * 10 * 21 );

	if ( !readFile( logger, DRIVE, SETTINGS_FILE, (u8*)cfg, &cfgBytes ) )
		writeSettingsFile();

	memcpy( settings, cfg, sizeof( s32 ) * MAX_SETTINGS );
	memcpy( geoRAM_SlotNames, &cfg[ sizeof( s32 ) * MAX_SETTINGS ], sizeof( u8 ) * 10 * 21 );
}

void applySIDSettings()
{
	// how elegant...
	extern void setSIDConfiguration( u32 mode, u32 sid1, u32 sid1addr, u32 sid2, u32 sid2addr, u32 rr, u32 addr, u32 exp, s32 v1, s32 p1, s32 v2, s32 p2, s32 v3, s32 p3, s32 digiblasterVol, s32 sidfreq, s32 tedVol, u8 output );
	u8 useHDMI = settings[ 17 ];
	extern u8 hdmiSoundAvailable;
	if ( !hdmiSoundAvailable )
		useHDMI = 0;
	setSIDConfiguration( 0, settings[2], settings[ 3 ], settings[7], settings[8]-1, settings[4], settings[8], settings[12],
						 settings[5], settings[6], settings[9], settings[10], settings[13], settings[14], settings[15], settings[11], settings[16], useHDMI );
}


// ugly, hard-coded handling of UI
void handleC64( int k, u32 *launchKernel, char *FILENAME, char *filenameKernal )
{
	char *filename, *filename2;

	if ( menuScreen == MENU_MAIN )
	{
		/*if ( k == VK_SHIFT_RETURN )
		{
			*launchKernel = 255; // reboot
			return;
		}*/

		if ( k == KEY_HELP )
		{
			menuScreen = MENU_BROWSER;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}
		if ( k == KEY_F3 )
		{
			menuScreen = MENU_CONFIG;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}

		int temp;
		int r = getMainMenuSelection( k, &filename, &filename2, &temp );

		if ( subSID == 1 )
		{
			if ( k == 27 )
			{
				resetMenuState();
				subHasLaunch = -1;
				subSID = 0;
				return;
			}
			// start 
			if ( ( r == 4 ) || ( k == 13 ) )
			{
				FILENAME[ 0 ] = 0;
				*launchKernel = 8;
				return;
			}
		}

		if ( subGeoRAM == 1 )
		{	
			if ( k == 27 )
			{
				resetMenuState();
				subHasKernal = -1;
				subHasLaunch = -1;
				subGeoRAM = 0;
				return;
			}
			// 
			if ( r == 7 ) // kernal selected
			{
				strcpy( filenameKernal, filename );
				return;
			}
			// start 
			if ( ( ( r == 3 ) || ( k == 13 ) ) )
			{
				*launchKernel = 3;
				return;
			}
		}

		switch ( r )
		{
		case 1: // exit to basic
			deactivateCart();
			return;
		case 2: // Browser
			break;
		case 3: // GeoRAM
			subGeoRAM = 1;
			subSID = 0;
			return;
		case 4: // SID+FM
			subSID = 1;
			subGeoRAM = 0;
			return;
		case 5: // .CRT file
			*launchKernel = 5;
			errorMsg = NULL;
			strncpy( &FILENAME[0], filename, 2047 );
			if ( strcmp( filename2, "none" ) == 0 )
				FILENAME[2048] = 0; else
				strncpy( &FILENAME[2048], filename2, 2047 );
			return;
		case 6: // .PRG file
			strcpy( FILENAME, filename );
			*launchKernel = 4;
			return;
		}
	} else
	if ( menuScreen == MENU_BROWSER )
	{
		// browser screen
		if ( k == KEY_HELP )
		{
			menuScreen = MENU_MAIN;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}

		int rep = 1;
		if ( k == KEY_F2 ) { k = 17; rep = DISPLAY_LINES - 1; }
		if ( k == KEY_F1 ) { k = 145; rep = DISPLAY_LINES - 1; }

		lastLine = scanFileTree( cursorPos, scrollPos );

		for ( int i = 0; i < rep; i++ )
		{
			// left
			if ( k == VK_LEFT || 
				 ( ( dir[ cursorPos ].f & DIR_DIRECTORY || dir[ cursorPos ].f & DIR_D64_FILE ) && k == 13 && (dir[ cursorPos ].f & DIR_UNROLLED ) ) )
			{
				typeInName = 0;
				if ( dir[ cursorPos ].parent != 0xffffffff )
				{
					lastSubIndex = cursorPos; 
					lastRolled = dir[ cursorPos ].parent;
					// fix
					if (! ( ( dir[ cursorPos ].f & DIR_DIRECTORY || dir[ cursorPos ].f & DIR_D64_FILE ) &&
							( dir[ cursorPos ].f & DIR_UNROLLED ) ) )
						cursorPos = dir[ cursorPos ].parent;
					// fix 2
					lastScrolled = scrollPos;
				}
				if ( !( dir[ cursorPos ].f & DIR_UNROLLED ) )
					k = VK_UP;
				if ( dir[ cursorPos ].f & DIR_DIRECTORY || dir[ cursorPos ].f & DIR_D64_FILE )
					dir[ cursorPos ].f &= ~DIR_UNROLLED;
				k = 0;
			} else
			// right 
			if ( k == VK_RIGHT || 
				 ( ( dir[ cursorPos ].f & DIR_DIRECTORY || dir[ cursorPos ].f & DIR_D64_FILE ) && k == 13 && !(dir[ cursorPos ].f & DIR_UNROLLED ) ) )
			{
				typeInName = 0;
				if ( (dir[ cursorPos ].f & DIR_DIRECTORY) && !(dir[ cursorPos ].f & DIR_SCANNED) )
				{
					// build path
					char path[ 8192 ] = {0};
					u32 n = 0, c = cursorPos;
					u32 nodes[ 256 ];

					nodes[ n ++ ] = c;

					while ( dir[ c ].parent != 0xffffffff )
					{
						c = nodes[ n ++ ] = dir[ c ].parent;
					}

					sprintf( path, "SD:" );

					for ( u32 i = n - 1; i >= 1; i -- )
					{
						if ( i != n - 1 )
							strcat( path, "//" );
						strcat( path, (char*)dir[ nodes[i] ].name );
					}
					strcat( path, "//" );

					extern void insertDirectoryContents( int node, char *basePath, int takeAll );
					insertDirectoryContents( nodes[ 0 ], path, dir[ cursorPos ].f & DIR_LISTALL );
				}

				if ( dir[ cursorPos ].f & DIR_DIRECTORY || dir[ cursorPos ].f & DIR_D64_FILE )
					dir[ cursorPos ].f |= DIR_UNROLLED;

				if ( cursorPos == lastRolled )
				{
					scrollPos = lastScrolled;
					cursorPos = lastSubIndex; 
				} else
				if ( cursorPos < nDirEntries - 1 )
				{
					cursorPos ++;
					lastLine = scanFileTree( cursorPos, scrollPos );
				}
				lastRolled = -1;

				k = 0;
			} else
			// down
			if ( k == VK_DOWN )
			{
				typeInName = 0;
				int oldPos = cursorPos;
				if ( dir[ cursorPos ].f & DIR_DIRECTORY || dir[ cursorPos ].f & DIR_D64_FILE )
				{
					if ( dir[ cursorPos ].f & DIR_UNROLLED )
						cursorPos ++; else
						cursorPos = dir[ cursorPos ].next;
				} else
					cursorPos ++;
				if ( cursorPos >= nDirEntries )
					cursorPos = oldPos;
			} else
			// up
			if ( k == VK_UP )
			{
				typeInName = 0;
				cursorPos --;
				if ( cursorPos < 0 ) cursorPos = 0;

				// current item has parent(s) -> check if they are unrolled
				int c = cursorPos;
				while ( c >= 0 && dir[ c ].parent != 0xffffffff )
				{
					c = dir[ c ].parent;
					if ( c >= 0 && c < nDirEntries && ( dir[ c ].f & DIR_DIRECTORY || dir[ c ].f & DIR_D64_FILE ) && !(dir[ c ].f & DIR_UNROLLED) )
						cursorPos = c;
				}
			}

			//if ( cursorPos < 0 ) cursorPos = 0;
			//if ( cursorPos >= nDirEntries ) cursorPos = nDirEntries - 1;

			// scrollPos decrease
			if ( cursorPos < scrollPos )
			{
				scrollPos = cursorPos;

				if ( dir[ scrollPos ].parent != 0xffffffff && !(dir[ dir[ scrollPos ].parent ].f & DIR_UNROLLED) )
					scrollPos = dir[ scrollPos ].parent;
			}

			// scrollPos increase
			if ( cursorPos >= lastLine )
			{
				if ( dir[ scrollPos ].f & DIR_DIRECTORY || dir[ scrollPos ].f & DIR_D64_FILE )
				{
					if ( dir[ scrollPos ].f & DIR_UNROLLED )
						scrollPos ++; else
						scrollPos = dir[ scrollPos ].next;
				} else
					scrollPos ++;

				lastLine = scanFileTree( cursorPos, scrollPos );
			}

			if ( scrollPos < 0 ) scrollPos = 0;
			if ( scrollPos >= nDirEntries ) scrollPos = nDirEntries - 1;
			if ( cursorPos < 0 ) cursorPos = 0;
			if ( cursorPos >= nDirEntries ) cursorPos = nDirEntries - 1;
		}

		if ( k == 13 )
		{
			// build path
			char path[ 8192 ] = {0};
			char d64file[ 32 ] = {0};
			s32 n = 0, c = cursorPos;
			u32 fileIndex = 0xffffffff;
			u32 nodes[ 256 ];

			nodes[ n ++ ] = c;

			s32 curC = c;

			if ( (dir[ c ].f & DIR_FILE_IN_D64 && ((dir[ c ].f>>SHIFT_TYPE)&7) == 2) || dir[ c ].f & DIR_PRG_FILE || dir[ c ].f & DIR_CRT_FILE )
			{
				while ( dir[ c ].parent != 0xffffffff )
				{
					c = nodes[ n ++ ] = dir[ c ].parent;
				}

				int stopPath = 0;
				if ( dir[ cursorPos ].f & DIR_FILE_IN_D64 )
				{
					strcpy( d64file, (char*)&dir[ cursorPos ].name[128] );
					fileIndex = dir[ cursorPos ].f & ((1<<SHIFT_TYPE)-1);
					stopPath = 1;
				}

				strcat( path, "SD:" );
				for ( s32 i = n - 1; i >= stopPath; i -- )
				{
					if ( i != n-1 )
						strcat( path, "\\" );
					strcat( path, (char*)dir[ nodes[i] ].name );
				}


				if ( dir[ curC ].f & DIR_PRG_FILE ) 
				{
					strcpy( FILENAME, path );
					*launchKernel = 4;
					return;
				}

				if ( dir[ curC ].f & DIR_FILE_IN_D64 )
				{
					extern u8 d64buf[ 1024 * 1024 ];
					extern int readD64File( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size );

					u32 imgsize = 0;

					// mount file system
					FATFS m_FileSystem;
					if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
						logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

					if ( !readD64File( logger, "", path, d64buf, &imgsize ) )
						return;

					// unmount file system
					if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
						logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );

					if ( d64ParseExtract( d64buf, imgsize, D64_GET_FILE + fileIndex, prgDataLaunch, (s32*)&prgSizeLaunch ) == 0 )
					{
						strcpy( FILENAME, path );
						logger->Write( "RaspiMenu", LogNotice, "loaded: %d bytes", prgSizeLaunch );
						*launchKernel = 40; 
					}
					return;
				}
			}
		}
	} else
	if ( menuScreen == MENU_CONFIG )
	{
		if ( k == KEY_HELP )
		{
			menuScreen = MENU_BROWSER;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}
		if ( k == KEY_F3 )
		{
			menuScreen = MENU_MAIN;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}

		if ( ( ( k == 'n' || k == 'N' ) || ( k == 13 && curSettingsLine == 1 ) )&& typeInName == 0 )
		{
			typeInName = 1;
			// currently selected georam slot is 'settings[ 1 ]'
			typeCurPos = 0;
			while ( typeCurPos < 19 && geoRAM_SlotNames[ settings[ 1 ] ][ typeCurPos ] != 0 ) typeCurPos ++;
		} else
		if ( typeInName == 1 )
		{
			int key = k;
			if ( key >= 'a' && key <= 'z' )
				key = key + 'A' - 'a';
			switch ( k )
			{
			case 13:
				typeInName = 0; 
				break;
			case 157: 
				if ( typeCurPos > 0 ) 
					typeCurPos --; 
				break;
			case 20: 
				if ( typeCurPos > 0 ) 
					typeCurPos --; 
				geoRAM_SlotNames[ settings[ 1 ] ][ typeCurPos ] = 32; 
				break;
			case 29: 
				if ( typeCurPos < 18 ) 
					typeCurPos ++; 
				break;
			default:
				// normal character
				geoRAM_SlotNames[ settings[ 1 ] ][ typeCurPos ] = k;
				if ( typeCurPos < 17 ) 
					typeCurPos ++; 
				break;
			}
		} else
		// left
		if ( k == 157 )
		{
			if ( rangeSettings[ curSettingsLine ] == 17 ) // a volume variable
			{
				settings[ curSettingsLine ] = max( 0, settings[ curSettingsLine ] - 1 );
			} else
			{
				settings[ curSettingsLine ] --;
				if ( rangeSettings[ curSettingsLine ] < 15 )
				{
					if ( settings[ curSettingsLine ] < 0 )
						settings[ curSettingsLine ] = rangeSettings[ curSettingsLine ] - 1;
				} else
					settings[ curSettingsLine ] = max( 0, settings[ curSettingsLine ] );

			}
			if ( curSettingsLine == 17 ) // PWM or HDMI -> force to PWM if HDMI not available
			{
				extern u8 hdmiSoundAvailable;
				if ( !hdmiSoundAvailable )
					settings[ 17 ] = 0;
			}
		} else
		// right 
		if ( k == 29 )
		{
			settings[ curSettingsLine ] ++;
			if ( rangeSettings[ curSettingsLine ] < 15 )
				settings[ curSettingsLine ] %= rangeSettings[ curSettingsLine ]; else
				settings[ curSettingsLine ] = min( settings[ curSettingsLine ], rangeSettings[ curSettingsLine ] - 1 );

			if ( curSettingsLine == 17 ) // PWM or HDMI -> force to PWM if HDMI not available
			{
				extern u8 hdmiSoundAvailable;
				if ( !hdmiSoundAvailable )
					settings[ 17 ] = 0;
			}
		} else
		// down
		if ( k == 17 )
		{
			curSettingsLine ++;
			curSettingsLine %= MAX_SETTINGS;
		} else
		// up
		if ( k == 145 )
		{
			if ( curSettingsLine == 0 )
				curSettingsLine = MAX_SETTINGS - 1; else
				curSettingsLine --;
		}
		if( (k == 's' || k == 'S') && typeInName == 0 )
		{
			writeSettingsFile();
			errorMsg = errorMessages[ 4 ];
			previousMenuScreen = menuScreen;
			menuScreen = MENU_ERROR;
		}

		applySIDSettings();
	} else
	{
		menuScreen = previousMenuScreen;
	}

}

extern int subGeoRAM, subSID, subHasKernal;

void printSidekickLogo()
{
	if ( skinFontLoaded )
	{
		u32 a = 91;

		for ( u32 j = 0; j < 4; j++ )
			for ( u32 i = 0; i < 7; i++ )
			{
				c64screen[ i + 33 + j * 40 ] = (a++);
				c64color[ i + 33 + j * 40 ] = j < 2 ? skinValues.SKIN_MENU_TEXT_ITEM : skinValues.SKIN_MENU_TEXT_KEY;
			}
	}
}


void printMainMenu()
{
	clearC64();
	//               "012345678901234567890123456789012345XXXX"
	printC64( 0,  1, "   .- Sidekick264 - Frenetic -.         ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );

	extern u8 c64screen[ 40 * 25 + 1024 * 4 ]; 

	if ( subGeoRAM )
	{
		printC64( 0, 23, "       choose PRG (also via HELP)       ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 0, 24, "       ESC back, RETURN/F1 launch       ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 28, 23, "HELP", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 7, 24, "ESC", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 17, 24, "RETURN", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 24, 24, "F1", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
	} else
	if ( subSID )
	{
		printC64( 0, 23, "       choose PRG (also via HELP)        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 0, 24, "       ESC back, RETURN/F2 launch       ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 28, 23, "HELP", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 7, 24, "ESC", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 17, 24, "RETURN", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 24, 24, "F2", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
	} else
	{
		printC64( 0, 23, "   HELP Browser, F7 Exit to Basic", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 3, 23, "HELP", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 17, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
	}

	if ( machine264 & 1 )
		printC64( 36, 23, "64KB", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); else
		printC64( 36, 23, "16KB", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	if ( machine264 & 2 )
		printC64( 36, 24, "NTSC", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); else
		printC64( 37, 24, "PAL", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 

	// menu headers + titels
	for ( int i = 0; i < CATEGORY_NAMES; i++ )
		if ( menuX[ i ] != -1 )
			printC64( menuX[ i ], menuY[ i ], categoryNamesPrint[ i ], skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

	for ( int i = 1; i < CATEGORY_NAMES; i++ )
		if ( menuX[ i ] != -1 )
		for ( int j = 0; j < menuItems[ i ]; j++ )
		{
			char key[ 2 ] = { 0, 0 };
			switch ( i )
			{
				case 1: key[ 0 ] = 'A' + j; break;
				case 2: key[ 0 ] = '1' + j; break;
				//case 3: key[ 0 ] = 'A' + j + menuItems[ 2 ]; break;
				//case 4: key[ 0 ] = '1' + j + menuItems[ 1 ]; break;
			};
			int flag = 0;
			if ( i == 4 && j == subHasKernal )
				flag = 0x80;

			printC64( menuItemPos[ i ][ j ][ 0 ], menuItemPos[ i ][ j ][ 1 ], key, skinValues.SKIN_MENU_TEXT_KEY, flag );
			printC64( menuItemPos[ i ][ j ][ 0 ] + 2, menuItemPos[ i ][ j ][ 1 ], menuText[ i ][ j ], skinValues.SKIN_MENU_TEXT_ITEM, flag );
		}

	// special menu
	printC64( menuX[ 0 ], menuY[ 0 ]+1, "F1", skinValues.SKIN_MENU_TEXT_KEY, subGeoRAM ? 0x80 : 0 );
	printC64( menuX[ 0 ], menuY[ 0 ]+2, "F2", skinValues.SKIN_MENU_TEXT_KEY, subSID ? 0x80 : 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+1, "NeoRAM264", skinValues.SKIN_MENU_TEXT_ITEM, subGeoRAM ? 0x80 : 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+2, "SID Emulation", skinValues.SKIN_MENU_TEXT_ITEM, subSID ? 0x80 : 0 );

	printC64( menuX[ 0 ], menuY[ 0 ]+3, "F3", skinValues.SKIN_MENU_TEXT_KEY, 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+3, "Settings", skinValues.SKIN_MENU_TEXT_ITEM, 0 );

	printSidekickLogo();

	c64screen[ 1000 ] = ((0x6800 >> 8) & 0xFC);
	c64screen[ 1001 ] = skinValues.SKIN_MENU_BACKGROUND_COLOR;
	c64screen[ 1002 ] = skinValues.SKIN_MENU_BORDER_COLOR;
/*	startInjectCode();
	// charset address
	injectPOKE( 0xff13, ((0x6800 >> 8) & 0xFC) );
	injectPOKE( 0xff15, skinValues.SKIN_MENU_BACKGROUND_COLOR );
	injectPOKE( 0xff19, skinValues.SKIN_MENU_BORDER_COLOR );*/
}


void settingsGetGEORAMInfo( char *filename, u32 *size )
{
	*size = 512 * ( 1 << settings[ 0 ] );
	sprintf( filename, "SD:GEORAM/slot%02d.ram", settings[ 1 ] );
}



void printSettingsScreen()
{
	clearC64();
	//               "012345678901234567890123456789012345XXXX"
	printC64( 0,  1, "   .- Sidekick264 - Frenetic -.         ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( 0, 24, "    F3 Back to Menu, S Save Settings    ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( 4, 24, "F3", skinValues.SKIN_MENU_TEXT_HEADER, 128, 0 );
	printC64( 21, 24, "S", skinValues.SKIN_MENU_TEXT_HEADER, 128, 0 );

	u32 x = 1, x2 = 8,y1 = 1, y2 = 1;
	u32 l = curSettingsLine;

	// special menu
	printC64( x+1, y1+3, "NeoRAM", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

	printC64( x+1, y1+4, "Memory", skinValues.SKIN_MENU_TEXT_ITEM, (l==0)?0x80:0 );

	char memStr[ 4 ][ 8 ] = { "512 KB", "1 MB", "2 MB", "4 MB" };
	printC64( x2+10, y1+4, memStr[ settings[ 0 ] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==0)?0x80:0 );

	printC64( x+1, y1+5, "File on SD", skinValues.SKIN_MENU_TEXT_ITEM, (l==1)?0x80:0 );
	char t[ 64 ];
	sprintf( t, "SD:GEORAM/slot%02d.ram", settings[ 1 ] );
	printC64( x2+10, y1+5, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==1)?0x80:0 );

	printC64( x2+10, y1+6, (const char*)"\"                  \"", skinValues.SKIN_MENU_TEXT_ITEM, (typeInName==1)?0x80:0 );
	printC64( x2+11, y1+6, (const char*)geoRAM_SlotNames[ settings[ 1 ] ], skinValues.SKIN_MENU_TEXT_ITEM, (typeInName==1)?0x80:0 );
	if ( typeInName )
		c64color[ x2+11+typeCurPos + (y1+6)*40 ] = skinValues.SKIN_MENU_TEXT_CATEGORY;

	y2 -=1;
	printC64( x+1,  y2+9, "Sound Emulation (w/ reSID, fmopl)", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

	u32 textCol2 = skinValues.SKIN_MENU_TEXT_ITEM - 32;
	y2 --;

	printC64( x+1,  y2+11, "SID #1", skinValues.SKIN_MENU_TEXT_ITEM, (l==2)?0x80:0 );
	char sidStrS[ 3 ][ 20 ] = { "6581", "8580", "8580 w/ Digiboost" };
	printC64( x2+10, y2+11, sidStrS[ settings[2] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==2)?0x80:0 );

	char sidStrA1[ 2 ][ 8 ] = { "$FD40", "$D400" };
	printC64( x+1,  y2+12, "Address", skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );
	printC64( x2+10, y2+12, sidStrA1[ settings[3] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );

	y2 ++;
	printC64( x+1,  y2+12, "Register Read", skinValues.SKIN_MENU_TEXT_ITEM, (l==4)?0x80:0 );
	char sidStrO[ 3 ][ 8 ] = { "off", "on" };
	printC64( x2+10, y2+12, sidStrO[ settings[4] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==4)?0x80:0 );

	printC64( x+1,  y2+13, "Volume", skinValues.SKIN_MENU_TEXT_ITEM, (l==5)?0x80:0 );
	printC64( x+1+6,  y2+13, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
	printC64( x+1+7,  y2+13, "Panning", skinValues.SKIN_MENU_TEXT_ITEM, (l==6)?0x80:0 );
	sprintf( t, "%2d", settings[ 5 ] );
	printC64( x2+10, y2+13, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==5)?0x80:0 );
	printC64( x2+13, y2+13, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
	sprintf( t, "%2d", settings[ 6 ] - 7 );
	printC64( x2+15, y2+13, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==6)?0x80:0 );

	printC64( x+1,  y2+14, "SID #2", textCol2, (l==7)?0x80:0 );
	char sidStrS2[ 4 ][ 20 ] = { "6581", "8580", "8580 w/ Digiboost", "none" };
	printC64( x2+10, y2+14, sidStrS2[ settings[7] ], textCol2, (l==7)?0x80:0 );

	printC64( x+1,  y2+15, "Address", textCol2, (l==8)?0x80:0 );
	char sidStrA[ 2 ][ 8 ] = { "$FD40", "$FE80" };
	printC64( x2+10, y2+15, sidStrA[ settings[8] ], textCol2, (l==8)?0x80:0 );

	printC64( x+1,  y2+16, "Volume", textCol2, (l==9)?0x80:0 );
	printC64( x+1+6,  y2+16, "/", textCol2, 0 );
	printC64( x+1+7,  y2+16, "Panning", textCol2, (l==10)?0x80:0 );
	sprintf( t, "%2d", settings[ 9 ] );
	printC64( x2+10, y2+16, t, textCol2, (l==9)?0x80:0 );
	printC64( x2+13, y2+16, "/", textCol2, 0 );
	sprintf( t, "%2d", settings[ 10 ] - 7 );
	printC64( x2+15, y2+16, t, textCol2, (l==10)?0x80:0 );

	printC64( x+1,  y2+17, "SID Frequency", skinValues.SKIN_MENU_TEXT_ITEM, (l==11)?0x80:0 );
//	char sidStrFreq[ 2 ][ 12 ] = { "886 KHz", "985 KHz" };
	char sidStrFreq[ 2 ][ 13 ] = { "TED clock", "VIC-II clock" };
	printC64( x2+10, y2+17, sidStrFreq[ settings[11] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==11)?0x80:0 );

	y2+=1;
	printC64( x+1,  y2+17, "SFX Sound Exp.", textCol2, (l==12)?0x80:0 );
	char sidStrO2[ 3 ][ 12 ] = { "off", "on ($FDE2)" };
	printC64( x2+10, y2+17, sidStrO2[ settings[12] ], textCol2, (l==12)?0x80:0 );

	printC64( x+1,  y2+18, "Volume", textCol2, (l==13)?0x80:0 );
	printC64( x+1+6,  y2+18, "/", textCol2, 0 );
	printC64( x+1+7,  y2+18, "Panning", textCol2, (l==14)?0x80:0 );
	sprintf( t, "%2d", settings[ 13 ] );
	printC64( x2+10, y2+18, t, textCol2, (l==13)?0x80:0 );
	printC64( x2+13, y2+18, "/", textCol2, 0 );
	sprintf( t, "%2d", settings[ 14 ] - 7 );
	printC64( x2+15, y2+18, t, textCol2, (l==14)?0x80:0 );
	
	y2--;
	printC64( x+1,  y2+20, "Digiblaster Vol", skinValues.SKIN_MENU_TEXT_ITEM, (l==15)?0x80:0 );
	sprintf( t, "%2d", settings[ 15 ] );
	printC64( x2+10, y2+20, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==15)?0x80:0 );
	printC64( x2+13, y2+20, "($FDE5)", skinValues.SKIN_MENU_TEXT_ITEM, (l==15)?0x80:0 );


	printC64( x+1,  y2+21, "EmulaTED Volume", textCol2, (l==16)?0x80:0 );
	sprintf( t, "%2d", settings[ 16 ] );
	printC64( x2+10, y2+21, t, textCol2, (l==16)?0x80:0 );
	
	y2 --;
	printC64( x+1,  y2+23, "Output", skinValues.SKIN_MENU_TEXT_ITEM, (l==17)?0x80:0 );
	char sidStrOutput[ 2 ][ 17 ] = { "PWM (audio jack)", "HDMI" };
	printC64( x2+10, y2+23, sidStrOutput[ settings[17] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==17)?0x80:0 );

	printSidekickLogo();

	c64screen[ 1000 ] = ((0x6800 >> 8) & 0xFC);
	c64screen[ 1001 ] = skinValues.SKIN_MENU_BACKGROUND_COLOR;
	c64screen[ 1002 ] = skinValues.SKIN_MENU_BORDER_COLOR;

/*	startInjectCode();
	// charset address
	injectPOKE( 0xff13, ((0x6800 >> 8) & 0xFC) );
	injectPOKE( 0xff15, skinValues.SKIN_MENU_BACKGROUND_COLOR );
	injectPOKE( 0xff19, skinValues.SKIN_MENU_BORDER_COLOR );*/
}


void renderC64()
{
	if ( menuScreen == MENU_MAIN )
	{
		printMainMenu();
	} else 
	if ( menuScreen == MENU_BROWSER )
	{
		printBrowserScreen();
	} else
	if ( menuScreen == MENU_CONFIG )
	{
		printSettingsScreen();
	} else
	//if ( menuScreen == MENU_ERROR )
	{
		if ( errorMsg != NULL )
		{
			int convert = 0;
			if ( previousMenuScreen == MENU_BROWSER )
				convert = 3;

			printC64( 0, 10, "\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9", skinValues.SKIN_ERROR_BAR, 0, 1 );
			printC64( 0, 11, "                                        ", skinValues.SKIN_ERROR_TEXT, 0 );
			printC64( 0, 12, errorMsg, skinValues.SKIN_ERROR_TEXT, 0, convert );
			printC64( 0, 13, "                                        ", skinValues.SKIN_ERROR_TEXT, 0 );
			printC64( 0, 14, "\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8", skinValues.SKIN_ERROR_BAR, 0, 1 );
		}
	}
}

