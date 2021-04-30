/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 c64screen.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - menu code
 Copyright (c) 2019-2021 Carsten Dachsbacher <frenetic@dachsbacher.de>

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
#include "config.h"
#include "crt.h"
#include "kernel_menu.h"
#include "PSID/psid64/psid64.h"

const int VK_F1 = 133;
const int VK_F2 = 137;
const int VK_F3 = 134;
const int VK_F4 = 138;
const int VK_F5 = 135;
const int VK_F6 = 139;
const int VK_F7 = 136;
const int VK_F8 = 140;

const int VK_ESC = 95;
const int VK_DEL = 20;
const int VK_RETURN = 13;
const int VK_SHIFT_RETURN = 141;
const int VK_MOUNT = 205; // SHIFT-M
const int VK_MOUNT_START = 77; // M


const int VK_LEFT  = 157;
const int VK_RIGHT = 29;
const int VK_UP    = 145;
const int VK_DOWN  = 17;
const int VK_HOME  = 19;
const int VK_S	   = 83;

const int VIRTK_SEARCH_DOWN = 256;
const int VIRTK_SEARCH_UP   = 257;

const int ACT_EXIT			= 1;
const int ACT_BROWSER		= 2;
const int ACT_GEORAM		= 3;
const int ACT_SID			= 4;
const int ACT_LAUNCH_CRT	= 5;
const int ACT_LAUNCH_PRG	= 6;
const int ACT_LAUNCH_KERNAL = 7;
const int ACT_SIDKICK_CFG   = 8;
const int ACT_NOTHING		= 0;

extern CLogger *logger;

extern u32 prgSizeLaunch;
extern unsigned char prgDataLaunch[ 65536 ];

static const char DRIVE[] = "SD:";
static const char SETTINGS_FILE[] = "SD:C64/special.cfg";

u8 c64screen[ 40 * 25 + 1024 * 4 ]; 
u8 c64color[ 40 * 25 + 1024 * 4 ]; 

char *errorMsg = NULL;

char errorMessages[8][41] = {
//   1234567890123456789012345678901234567890
	"                NO ERROR                ",
	"  ERROR: UNKNOWN/UNSUPPORTED .CRT TYPE  ",
	"          ERROR: NO .CRT-FILE           ", 
	"          ERROR READING FILE            ",
	"            SETTINGS SAVED              ",
	"         WRONG SYSTEM, NO C128!         ",
	"         SID-WIRE NOT DETECTED!         ",
	"         DISK2EASYFLASH FAILED!         ",
};

/*char *extraMsg = NULL;
char extraMessages[2][41] = {
//   1234567890123456789012345678901234567890
	"                                        ",
	" L mount&LOAD\"*\"   SHIFT-L mount only ",
};*/
int extraMsg = 0;

#define MENU_MAIN	 0x00
#define MENU_BROWSER 0x01
#define MENU_ERROR	 0x02
#define MENU_CONFIG  0x03
u32 menuScreen = 0, 
	previousMenuScreen = 0;
u32 updateMenu = 1;
u32 typeInName = 0;
u32 typeCurPos = 0;

u8 searchName[ 32 ] = {0};

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

int convChar( char c, u32 convert )
{
	u32 c2 = c;

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
		if ( c == '_' )
			c2 = 100;
	}
	return c2;
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
			if ( c == '_' )
				c2 = 100;
		}

		c64screen[ x + y * 40 + i ] = c2 | flag;
		c64color[ x + y * 40 + i ] = color;
	}
}


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

	scanFileTree( cursorPos, scrollPos );

	//printC64(0,0,  "0123456789012345678901234567890123456789", 15, 0 );
	printC64( 0,  1, "        .- sidekick64-browser -.        ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( 0,  2, "           scanning directory           ", 0, 0 );

	if ( extraMsg == 0 )
	{
		//printC64( 0, 23, "  F1/F3 Page Up/Down  F7 Back to Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		//printC64( 2, 23, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		//printC64( 5, 23, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		//printC64( 22, 23, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
    	//        (0,0,  "0123456789012345678901234567890123456789", 15, 0 );
		printC64( 0, 23, "  F1/F3 Page Up/Dn  S Search  F7 Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 2, 23, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 5, 23, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 20, 23, "S", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 30, 23, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
	} else
	if ( extraMsg == 1 )
	{
		//printC64( 0, 23, "  F1/F3 Page Up/Down  F7 Back to Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		//printC64( 2, 23, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		//printC64( 5, 23, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		//printC64( 22, 23, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 0, 23, "  F1/F3 Page Up/Dn  S Search  F7 Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 2, 23, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 5, 23, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 20, 23, "S", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 30, 23, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );

		//printC64(0,0,  "01234567890123 45 678901234567890123456789", 15, 0 );
//		printC64( 0, 23, " M mount&LOAD\"*\"   SHIFT-M mount only ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 0, 24, " MOUNT&LOAD\"*\",8,1   SHIFT-M mount only ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
	
		printC64( 1, 24, "M", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
		printC64( 21, 24, "SHIFT-M", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
	} 

	if ( modeC128 )
		printC64( 0, 24, "SHIFT+RETURN to launch PRGs in C128-mode", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );

	if ( subGeoRAM )
	{
		printC64(  0, 24, "choose CRT/Dxx or PRG w/ NeoRAM to start", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 18, 24, "PRG w/ NeoRAM", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
		printC64(  7, 24, "CRT/Dxx", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
	} else
	if ( subSID )
	{
		printC64(  0, 24, "choose CRT/Dxx or PRG w/ SID+FM to start", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 18, 24, "PRG w/ SID+FM", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
		printC64(  7, 24, "CRT", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
	}

	if ( typeInName )
	{
		printC64( 0, 24, "       SEARCH: >                <       ", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
		printC64( 16, 24, (const char*)searchName, skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0x00, 3 );
		c64screen[ 16 + typeCurPos + 24 * 40 ] |= 0x80;
	}

	lastLine = printFileTree( cursorPos, scrollPos );

	// scroll bar
	int t = scrollPos * DISPLAY_LINES / nDirEntries;
	int b = lastLine * DISPLAY_LINES / nDirEntries;

	// bar on the right
	for ( int i = 0; i < DISPLAY_LINES; i++ )
	{
		char c = 102;
		if ( t <= i && i <= b )
			c = 96 + 128 - 64;
		c64screen[ 38 + ( i + 3 ) * 40 ] = c;
		c64color[ 38 + ( i + 3 ) * 40 ] = 1;
	}

	startInjectCode();
	injectPOKE( 53280, skinValues.SKIN_BROWSER_BORDER_COLOR );
	injectPOKE( 53281, skinValues.SKIN_BROWSER_BACKGROUND_COLOR );
	if ( skinFontLoaded )
		injectPOKE( 53272, 28 ); else
		injectPOKE( 53272, 21 ); 
}

void resetMenuState( int keep = 0 )
{
	if ( !( keep & 1 ) ) subGeoRAM = 0;
	if ( !( keep & 2 ) ) subSID = 0;
	if ( !( keep & 4 ) ) subHasKernal = -1;
	subHasLaunch = -1;
}

int getMainMenuSelection( int key, char **FILE, int *addIdx, char *menuItemStr )
{
	*FILE = NULL;

	if ( key >= 'a' && key <= 'z' )
		key = key + 'A' - 'a';

	if ( menuItemStr )
		menuItemStr[ 0 ] = 0;

	extern u32 wireKernalAvailable;

	if ( key == VK_F8 ) { resetMenuState(0); return ACT_EXIT;/* Exit */ } else
	if ( key == VK_F7 ) { resetMenuState(3+4); return ACT_BROWSER;/* Browser */ } else
	if ( key == VK_F1 ) { resetMenuState(1); return ACT_GEORAM;/* GEORAM */ } else
	if ( key == VK_F6 ) { return ACT_SIDKICK_CFG; } else
	if ( key == VK_F3 ) { resetMenuState(2); return ACT_SID;/* SID */ } else
	{
		if ( key >= 'A' && key < 'A' + menuItems[ 2 ] ) // CRT
		{
			resetMenuState(); 
			int i = key - 'A';
			*FILE = menuFile[ 2 ][ i ];
			if ( menuItemStr )
				strcpy( menuItemStr, menuText[ 2 ][ i ] );
			//logger->Write( "RaspiMenu", LogNotice, "%s -> %s\n", menuText[ 2 ][ i ], menuFile[ 2 ][ i ] );
			return ACT_LAUNCH_CRT;
		} else
		if ( key >= 'A' + menuItems[ 2 ] && key < 'A' + menuItems[ 2 ] + menuItems[ 3 ] ) // PRG
		{
			int i = key - 'A' - menuItems[ 2 ];
			*FILE = menuFile[ 3 ][ i ];
			if ( menuItemStr )
				strcpy( menuItemStr, menuText[ 3 ][ i ] );
			//logger->Write( "RaspiMenu", LogNotice, "%s -> %s\n", menuText[ 3 ][ i ], menuFile[ 3 ][ i ] );
			return ACT_LAUNCH_PRG;
		} else
		if ( key >= '1' && key < '1' + menuItems[ 1 ] ) // FREEZER
		{
			resetMenuState(); 
			int i = key - '1';
			*FILE = menuFile[ 1 ][ i ];
			//logger->Write( "RaspiMenu", LogNotice, "%s -> %s\n", menuText[ 1 ][ i ], menuFile[ 1 ][ i ] );
			return ACT_LAUNCH_CRT;
		} else
		if ( key >= '1' + menuItems[ 1 ] && key < '1' + menuItems[ 1 ] + menuItems[ 4 ] && wireKernalAvailable ) // KERNAL
		{
			int i = key - '1' - menuItems[ 1 ];
			*FILE = menuFile[ 4 ][ i ];
			if ( subHasKernal == i )
				subHasKernal = -1; else
				subHasKernal = i;
			if ( addIdx != NULL )
				*addIdx = i;
			//logger->Write( "RaspiMenu", LogNotice, "%s -> %s\n", menuText[ 4 ][ i ], menuFile[ 4 ][ i ] );
			return ACT_LAUNCH_KERNAL;
		}
	}

	return ACT_NOTHING;
}

extern void deactivateCart();

#define MAX_SETTINGS 17
#define MAX_SETTINGS_SAVED 64
u32 curSettingsLine = 0;
s32 rangeSettings[ MAX_SETTINGS ] = { 4, 10, 6, 2, 16, 15, 4, 4, 16, 15, 2, 16, 15, 2, 4, 16, 2 };
s32 settings[ MAX_SETTINGS ]      = { 0,  0, 0, 0, 15,  0, 0, 0, 15, 14, 0, 15, 7, 0, 7, 0 };
u8  geoRAM_SlotNames[ 10 ][ 21 ];

void writeSettingsFile()
{
	char cfg[ 16384 ];
	u32 cfgBytes = 0;
    memset( cfg, 0, 16384 );
	cfgBytes = sizeof( s32 ) * MAX_SETTINGS_SAVED;
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

	memset( settings, 0, sizeof( s32 ) * MAX_SETTINGS_SAVED );
	memset( geoRAM_SlotNames, 32, sizeof( u8 ) * 10 * 21 );

	if ( !readFile( logger, DRIVE, SETTINGS_FILE, (u8*)cfg, &cfgBytes ) )
		writeSettingsFile();

	memcpy( settings, cfg, sizeof( s32 ) * MAX_SETTINGS_SAVED );
	memcpy( geoRAM_SlotNames, &cfg[ sizeof( s32 ) * MAX_SETTINGS_SAVED ], sizeof( u8 ) * 10 * 21 );
}

u32 octaSIDMode = 0;

void applySIDSettings()
{
	octaSIDMode = ( settings[ 2 ] > 2 ) ? 1 : 0;
	// how elegant...
	extern void setSIDConfiguration( u32 mode, u32 sid1, u32 sid2, u32 sid2addr, u32 rr, u32 addr, u32 exp, s32 v1, s32 p1, s32 v2, s32 p2, s32 v3, s32 p3, s32 outputPWMHDMI, s32 midi, s32 soundfont, s32 midiVol );
	setSIDConfiguration( 0, settings[2], settings[6], settings[7]-1, settings[3], settings[7], settings[10],
						 settings[4], settings[5], settings[8], settings[9], settings[11], settings[12], settings[ 16 ], settings[13], settings[14], settings[15] );
}

int lastKeyDebug = 0;

static int joyItem[ 4 * 50 ];
static int nJoyItems = 0;
static int joyX = 0, joyY = 0, joyIdx = -1;
#define ADD_JOY_ITEM( x, y, l, k ) { joyItem[nJoyItems*4+0]=x; joyItem[nJoyItems*4+1]=y; joyItem[nJoyItems*4+2]=l; joyItem[nJoyItems*4+3]=k; nJoyItems ++;  }
//#define ADD_JOY_ITEM( x, y, l, k ) {   }

int trySnap( int mx, int my )
{
	int tX = min( 39, max( 0, joyX + mx ) );
	int tY = min( 39, max( 0, joyY + my ) );
	int minD2 = 100000000;
	int minIdx = -1;
	for ( int i = 0; i < nJoyItems; i++ )
	{
		int dx = (tX - joyItem[ i * 4 + 0 ]) / 5;
		int dy = (tY - joyItem[ i * 4 + 1 ]);
		if ( dx * dx + dy * dy < minD2 )
		{
			minD2 = dx * dx + dy * dy;
			minIdx = i;
		}
	}
	return minIdx; 

/*	if ( minIdx != -1 )
	{
		joyX = joyItem[ minIdx * 4 + 0 ];
		joyY = joyItem[ minIdx * 4 + 1 ];
	}
	joyIdx = minIdx;*/
}

void snapJoy( int dir )
{
	// dir: 0 = left, 2 = right, 1 = up, 3 = down

	int dx, dy;
	if ( dir == 1 || dir == 3 )
	{
		dx = 0;
		dy = dir == 1 ? -1 : 1;
	}
	if ( dir == 0 || dir == 2 )
	{
		dx = dir == 0 ? -1 : 1;
		dy = 0;
	}

	int oldJoyIdx = joyIdx;
	int newIdx = -1, attempts = 1;
	while ( newIdx == -1 && attempts < 13 )
	{
		newIdx = trySnap( dx * attempts, dy * attempts );
		if ( newIdx == joyIdx )
			newIdx = -1;
		attempts ++;
	}
	if ( newIdx != -1 )
	{
		joyX = joyItem[ newIdx * 4 + 0 ];
		joyY = joyItem[ newIdx * 4 + 1 ];
		joyIdx = newIdx;
	} else
		joyIdx = oldJoyIdx;
}

// ugly, hard-coded handling of UI
void handleC64( int k, u32 *launchKernel, char *FILENAME, char *filenameKernal, char *menuItemStr, u32 *startC128 = NULL )
{
	char *filename;

	extern u32 wireSIDAvailable;

	lastKeyDebug = k;

	if ( menuScreen == MENU_MAIN )
	{
		if ( k == VK_SHIFT_RETURN )
		{
			*launchKernel = 255; // reboot
			return;
		}

		// virtual key press
		if ( k == 92 && joyIdx != -1 )
		{
			k = joyItem[ joyIdx * 4 + 3 ];
		}

		if ( k == VK_LEFT )
		{
			snapJoy( 0 );
		}
		if ( k == VK_RIGHT )
		{
			snapJoy( 2 );
		}
		if ( k == VK_UP )
		{
			snapJoy( 1 );
		}
		if ( k == VK_DOWN )
		{
			snapJoy( 3 );
		}

		if ( k == VK_F7 )
		{
			menuScreen = MENU_BROWSER;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal, menuItemStr );
			return;
		}
		if ( k == VK_F5 )
		{
			menuScreen = MENU_CONFIG;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal, menuItemStr );
			return;
		}

		int temp;
		int hasKernalAlready = subHasKernal;
		subHasKernal = -1;
		int r = getMainMenuSelection( k, &filename, &temp, menuItemStr );
		
		if ( r != ACT_LAUNCH_KERNAL ) 
			subHasKernal = hasKernalAlready;

		if ( r == ACT_SIDKICK_CFG )
		{
			*launchKernel = 11;
			return;
		}

		if ( r == ACT_LAUNCH_KERNAL && hasKernalAlready != subHasKernal )
		{
			strcpy( filenameKernal, filename );
			return;
		}

		if ( subSID == 1 )
		{
			if ( k == VK_ESC )
			{
				resetMenuState();
				subHasLaunch = -1;
				subSID = 0;
				return;
			}
			// start 
			if ( ( r == ACT_SID ) || ( k == VK_RETURN ) )
			{
				FILENAME[ 0 ] = 0;
				*launchKernel = 8;
				return;
			}
		}

		if ( subGeoRAM == 1 )
		{	
			if ( k == VK_ESC )
			{
				resetMenuState();
				subHasKernal = -1;
				subHasLaunch = -1;
				subGeoRAM = 0;
				return;
			}
			// 
			if ( r == ACT_LAUNCH_KERNAL ) // kernal selected
			{
				strcpy( filenameKernal, filename );
				return;
			}
			// 
			if ( ( ( r == ACT_GEORAM ) || ( k == VK_RETURN ) ) )
			{
				*launchKernel = 3;
				return;
			}
		}

		u32 err = 0, tempKernel = 0;
		switch ( r )
		{
		case ACT_EXIT: // exit to basic
			deactivateCart();
			return;
		case ACT_BROWSER: // Browser
			break;
		case ACT_GEORAM: // GeoRAM
			subGeoRAM = 1;
			subSID = 0;
			return;
		case ACT_SID: // SID+FM
			subSID = 1;
			subGeoRAM = 0;
			return;
		case ACT_LAUNCH_CRT: // .CRT file
			if ( strstr( filename, "georam") != 0 || strstr( filename, "GEORAM") != 0 )
			{
				*launchKernel = 10;
				errorMsg = NULL;
				strcpy( FILENAME, filename );
				return;
			} 
			if ( strstr( filename, "CART128") != 0 )
			{
				if ( modeC128 == 0 )
				{
					*launchKernel = 0;
					errorMsg = errorMessages[ 5 ];
					previousMenuScreen = menuScreen;
					menuScreen = MENU_ERROR;
					return;
				} else
				{
					*launchKernel = 9;
					errorMsg = NULL;
					strcpy( FILENAME, filename );
					return;
				}
			}
			logger->Write( "RaspiFlash", LogNotice, "checking CRT type!" );
			tempKernel = checkCRTFile( logger, DRIVE, filename, &err );
			if ( err > 0 )
			{
				*launchKernel = 0;
				errorMsg = errorMessages[ err ];
				previousMenuScreen = menuScreen;
				menuScreen = MENU_ERROR;
			} else
			{
				*launchKernel = tempKernel;
				errorMsg = NULL;
				strcpy( FILENAME, filename );
			}

			return;
		case ACT_LAUNCH_PRG: // .PRG file
			if ( ( subSID && octaSIDMode && !wireSIDAvailable ) ||
				 ( subSID && !wireSIDAvailable && settings[10] == 0 ) ) // no SID-wire, FM emulation off
			{
				errorMsg = errorMessages[ 6 ];
				previousMenuScreen = menuScreen;
				menuScreen = MENU_ERROR;
			} else
			{
				strcpy( FILENAME, filename );
				*launchKernel = 4;
			}
			return;
		case ACT_LAUNCH_KERNAL: // Kernal
			if ( hasKernalAlready == subHasKernal )
			{
				strcpy( FILENAME, filename );
				*launchKernel = 2;
			}
			return;
		}
	} else
	if ( menuScreen == MENU_BROWSER )
	{
		if ( k == VK_HOME )
		{
			typeInName = 0;
			if ( cursorPos == 0 && scrollPos == 0 )
			{
				for ( int i = 0; i < nDirEntries; i++ )
					dir[ i ].f &= ~DIR_UNROLLED;
			}
			cursorPos = scrollPos = 0;
		}
		// virtual key press
		if ( k == 92 && typeInName == 0 )
		{
			k = VK_RETURN;
		}

		// some remapping
		if ( typeInName == 1 && k == VK_DOWN )
			k = VIRTK_SEARCH_DOWN;
		if ( typeInName == 1 && k == VK_UP )
			k = VIRTK_SEARCH_UP;

		if ( /*k == VK_UP || k == VK_DOWN ||*/ k == VK_RETURN )
			typeInName = 0;

		// browser screen
		if ( k == VK_F7 )
		{
			typeInName = 0;
			menuScreen = MENU_MAIN;
			joyIdx = -1;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal, menuItemStr );
			return;
		}

		int rep = 1;
		if ( k == VK_F3 ) { k = VK_DOWN; rep = DISPLAY_LINES - 1; }
		if ( k == VK_F1 ) { k = VK_UP; rep = DISPLAY_LINES - 1; }

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

		if ( typeInName == 0 && ( k == VK_MOUNT || k == VK_MOUNT_START ) && dir[ cursorPos ].f & DIR_D64_FILE )
		{
			typeInName = 0;
			extern int createD2EF( unsigned char *diskimage, int imageSize, unsigned char *cart, int build, int mode, int autostart );

			unsigned char *cart = new unsigned char[ 1024 * 1025 ];
			unsigned char *diskimage = new unsigned char[ 1024 * 1024 ];
			u32 diSize = 0, crtSize = 0;

			int autostart = (k == VK_MOUNT_START) ? 1 : 0;

			// build path
			char path[ 8192 ] = {0};
			s32 n = 0, c = cursorPos;
			u32 nodes[ 256 ];
			nodes[ n ++ ] = c;

			while ( dir[ c ].parent != 0xffffffff )
			{
				c = nodes[ n ++ ] = dir[ c ].parent;
			}

			strcat( path, "SD:" );
			for ( s32 i = n - 1; i >= 0; i -- )
			{
				if ( i != n-1 )
					strcat( path, "\\" );
				strcat( path, (char*)dir[ nodes[i] ].name );
			}
			
			logger->Write( "d2ef", LogNotice, "'%s'", path );

			if ( readFile( logger, DRIVE, path, diskimage, &diSize ) )
			{
				//logger->Write( "d2ef", LogNotice, "loaded %d bytes D64", diSize );

				crtSize = createD2EF( diskimage, diSize, cart, 2, 0, autostart );

				//logger->Write( "d2ef", LogNotice, "loaded %d bytes D64", diSize );

				strcpy( FILENAME, "SD:C64/temp.crt" );
				writeFile( logger, DRIVE, FILENAME, cart, crtSize );

				u32 err = 0;
				u32 tempKernel = checkCRTFile( logger, DRIVE, FILENAME, &err );
				if ( err > 0 )
				{
					err = 8; // D2EF error
					*launchKernel = 0;
					errorMsg = errorMessages[ err ];
					previousMenuScreen = menuScreen;
					menuScreen = MENU_ERROR;
				} else
				{
					*launchKernel = tempKernel;
					errorMsg = NULL;
				}
			}

			delete [] cart;
			delete [] diskimage;
			return;
		}


		if ( typeInName == 1 )
		{
			int found = -1;
			int searchPos = cursorPos;
			int c, l, ls;
			char name[512], search[32];

			int key = k;
			if ( key >= 'a' && key <= 'z' )
				key = key + 'A' - 'a';
			switch ( k )
			{
			/*case VK_LEFT: 
				if ( typeCurPos > 0 ) 
					typeCurPos --; 
				break;
			case VK_RIGHT: 
				if ( typeCurPos < 18 ) 
					typeCurPos ++; 
				break;*/
			case VK_DEL: 
				if ( typeCurPos > 0 ) 
					typeCurPos --; 
				searchName[ typeCurPos ] = 0; 
				break;
			case VK_ESC:
				typeInName = 0;
				break;
			default:
				if ( k == VIRTK_SEARCH_UP )
				{
					if ( searchPos > 0 )
						searchPos --;
				} else
				if ( k == VIRTK_SEARCH_DOWN )
				{
					if ( searchPos + 1 < nDirEntries )
						searchPos ++;
				} else
				{
					searchName[ typeCurPos ] = k;
					if ( typeCurPos < 15 ) 
						typeCurPos ++; 
				}

				ls = min( 32, strlen( (const char*)searchName ) );
				for ( int i = 0; i < ls; i++ )
					search[ i ] = convChar( searchName[ i ], 3);

				while ( found == -1 && searchPos < nDirEntries && searchPos >= 0)
				{
					// convert name for search
					memset( name, 0, 512 );
					l = min( ls, (int)strlen( (const char*)dir[ searchPos ].name ) );
					for ( c = 0; c < l; c++ )
					{
						if ( search[ c ] != convChar( dir[ searchPos ].name[ c ], 3 ) )
							break;
					}
					if ( c == l )
						found = searchPos; else
						searchPos += (k == VIRTK_SEARCH_UP) ? -1 : 1;
				}
				if ( found != -1 )
				{
					cursorPos = scrollPos = found;

					// unroll all parent entries
					c = cursorPos;
					while ( dir[ c ].parent != 0xffffffff )
					{
						c = dir[ c ].parent;
						dir[ c ].f |= DIR_UNROLLED;
					}
				}
				break;
			}
			return;
		}
		if ( k == VK_S && typeInName == 0 )
		{
			typeInName = 1;
			typeCurPos = 0;
			memset( searchName, 0, 16 );

			int cp = 0;
			while ( cp < nDirEntries )
			{
				if ( (dir[ cp ].f & DIR_DIRECTORY) && !(dir[ cp ].f & DIR_SCANNED) )
				{
					// build path
					char path[ 8192 ] = {0};
					u32 n = 0, c = cp;
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
					insertDirectoryContents( nodes[ 0 ], path, dir[ cp ].f & DIR_LISTALL );
				}

				cp ++;
			}

			return;
		} 

		if ( typeInName == 0 && (k == VK_RETURN || k == VK_SHIFT_RETURN) )
		{
			if ( ( subSID && octaSIDMode && !wireSIDAvailable ) ||
				 ( subSID && !wireSIDAvailable && settings[10] == 0 ) ) // no SID-wire, FM emulation off
			{
				errorMsg = errorMessages[ 6 ];
				previousMenuScreen = menuScreen;
				menuScreen = MENU_ERROR;
			} else
			{
				if ( startC128 && modeC128 )
				{
					if ( k == VK_SHIFT_RETURN )
						*startC128 = 1; else
						*startC128 = 0;
				}

				// build path
				char path[ 8192 ] = {0};
				char d64file[ 128 ] = {0};
				s32 n = 0, c = cursorPos;
				u32 fileIndex = 0xffffffff;
				u32 nodes[ 256 ];

				nodes[ n ++ ] = c;

				s32 curC = c;

				if ( (dir[ c ].f & DIR_FILE_IN_D64 && ((dir[ c ].f>>SHIFT_TYPE)&7) == 2) || dir[ c ].f & DIR_PRG_FILE || dir[ c ].f & DIR_CRT_FILE )
				{
					while ( dir[ c ].parent != 0xffffffff )
					{
						//logger->Write( "exec", LogNotice, "node %d: '%s'", dir[c].parent, dir[dir[c].parent].name );
						c = nodes[ n ++ ] = dir[ c ].parent;
					}

					int stopPath = 0;
					if ( dir[ cursorPos ].f & DIR_FILE_IN_D64 )
					{
						//logger->Write( "exec", LogNotice, "d64file: '%s'", dir[ cursorPos ].name[128] );
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
						if ( strstr( path, "PRG128") != 0 && modeC128 == 0 )
						{
							*launchKernel = 0;
							errorMsg = errorMessages[ 5 ];
							previousMenuScreen = menuScreen;
							menuScreen = MENU_ERROR;
							return;
						}
						strcpy( FILENAME, path );
						*launchKernel = 4;
						return;
					}

					if ( dir[ curC ].f & DIR_CRT_FILE ) 
					{
						u32 err = 0;

						strcpy( FILENAME, path );

						if ( strstr( FILENAME, "CART128") != 0 )
						{
							if ( modeC128 == 0 )
							{
								*launchKernel = 0;
								errorMsg = errorMessages[ 5 ];
								previousMenuScreen = menuScreen;
								menuScreen = MENU_ERROR;
							} else
							{
								*launchKernel = 9;
								errorMsg = NULL;
							}
						} else
						if ( strstr( FILENAME, "georam") != 0 || strstr( FILENAME, "GEORAM") != 0 )
						{
							*launchKernel = 10;
							errorMsg = NULL;
						} else
						{

							u32 tempKernel = checkCRTFile( logger, DRIVE, FILENAME, &err );
							if ( err > 0 )
							{
								*launchKernel = 0;
								errorMsg = errorMessages[ err ];
								previousMenuScreen = menuScreen;
								menuScreen = MENU_ERROR;
							} else
							{
								*launchKernel = tempKernel;
								errorMsg = NULL;
							}
						}
						return;
					}

					if ( dir[ curC ].f & DIR_FILE_IN_D64 )
					{
						if ( ( subSID && octaSIDMode && !wireSIDAvailable ) ||
							 ( subSID && !wireSIDAvailable && settings[10] == 0 ) ) // no SID-wire, FM emulation off
						{
							errorMsg = errorMessages[ 6 ];
							previousMenuScreen = menuScreen;
							menuScreen = MENU_ERROR;
						} else
						{
							extern u8 d64buf[ 1024 * 1024 ];
							extern int readD64File( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size );

							u32 imgsize = 0;

							// mount file system
							FATFS m_FileSystem;
							if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
								logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

							//logger->Write( "exec", LogNotice, "path '%s'", path );
							if ( !readD64File( logger, "", path, d64buf, &imgsize ) )
								return;

							// unmount file system
							if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
								logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );

							if ( d64ParseExtract( d64buf, imgsize, D64_GET_FILE + fileIndex, prgDataLaunch, (s32*)&prgSizeLaunch ) == 0 )
							{
								strcpy( FILENAME, path );
								//logger->Write( "RaspiMenu", LogNotice, "loaded: %d bytes", prgSizeLaunch );
								*launchKernel = 40; 
							}
						}
						return;
					}
				}



				if ( dir[ c ].f & DIR_SID_FILE  )
				{
					while ( dir[ c ].parent != 0xffffffff )
					{
						c = nodes[ n ++ ] = dir[ c ].parent;
					}

					int stopPath = 0;

					strcat( path, "SD:" );
					for ( s32 i = n - 1; i >= stopPath; i -- )
					{
						if ( i != n-1 )
							strcat( path, "\\" );
						strcat( path, (char*)dir[ nodes[i] ].name );
					}
					logger->Write( "exec", LogNotice, "sid file: '%s'", path );

					if ( ( subSID && octaSIDMode && !wireSIDAvailable ) ||
						 ( subSID && !wireSIDAvailable && settings[10] == 0 ) ) // no SID-wire, FM emulation off
					{
						errorMsg = errorMessages[ 6 ];
						previousMenuScreen = menuScreen;
						menuScreen = MENU_ERROR;
					} else
					{
						unsigned char sidData[ 65536 ];
						u32 sidSize = 0;

						if ( readFile( logger, DRIVE, path, sidData, &sidSize ) )
						{
						}

						logger->Write( "exec", LogNotice, "bytes: '%d'", sidSize );

						Psid64 *psid64 = new Psid64();

						psid64->setVerbose(false);
						psid64->setUseGlobalComment(false);
						psid64->setBlankScreen(false);
						psid64->setNoDriver(false);

						if ( !psid64->load( sidData, sidSize ) )
						{
							//return false;
						}
						//logger->Write( "exec", LogNotice, "psid loaded" );

						// convert the PSID file
						if ( !psid64->convert() ) 
						{
							//return false;
						}
						//logger->Write( "exec", LogNotice, "psid converted, prg size %d", psid64->m_programSize );

						memcpy( &prgDataLaunch[0], psid64->m_programData, psid64->m_programSize );
						prgSizeLaunch = psid64->m_programSize;

						delete psid64;

						*launchKernel = 41; 
					}
					return;
				}
			}
		}

	} else
	if ( menuScreen == MENU_CONFIG )
	{
		if ( k == VK_F7 )
		{
			menuScreen = MENU_BROWSER;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal, menuItemStr );
			return;
		}
		if ( k == VK_F5 )
		{
			menuScreen = MENU_MAIN;
			joyIdx = -1;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal, menuItemStr );
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
			case VK_RETURN:
				typeInName = 0; 
				break;
			case VK_LEFT: 
				if ( typeCurPos > 0 ) 
					typeCurPos --; 
				break;
			case VK_DEL: 
				if ( typeCurPos > 0 ) 
					typeCurPos --; 
				geoRAM_SlotNames[ settings[ 1 ] ][ typeCurPos ] = 32; 
				break;
			case VK_RIGHT: 
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
		if ( k == VK_LEFT )
		{
			settings[ curSettingsLine ] --;
			if ( rangeSettings[ curSettingsLine ] < 15 )
			{
				if ( settings[ curSettingsLine ] < 0 )
					settings[ curSettingsLine ] = rangeSettings[ curSettingsLine ] - 1;
			} else
				settings[ curSettingsLine ] = max( 0, settings[ curSettingsLine ] );

		} else
		// right 
		if ( k == VK_RIGHT )
		{
			settings[ curSettingsLine ] ++;
			if ( rangeSettings[ curSettingsLine ] < 15 )
				settings[ curSettingsLine ] %= rangeSettings[ curSettingsLine ]; else
				settings[ curSettingsLine ] = min( settings[ curSettingsLine ], rangeSettings[ curSettingsLine ] - 1 );
		} else
		// down
		if ( k == VK_DOWN )
		{
			curSettingsLine ++;
			if ( settings[2] > 2 )
			{
				if ( curSettingsLine == 13 + 1 )
					curSettingsLine = 0; 
				if ( curSettingsLine == 2 + 1 )
					curSettingsLine = 13; 
				//curSettingsLine %= 3; 
			} else
				curSettingsLine %= MAX_SETTINGS;
		} else
		// up
		if ( k == VK_UP )
		{
			if ( curSettingsLine == 0 )
			{
				if ( settings[2] > 2 )
				{
					curSettingsLine = 13; 
				} else
					curSettingsLine = MAX_SETTINGS - 1; 
			} else
			{
				if ( settings[2] > 2 && curSettingsLine == 13 )
					curSettingsLine = 2; else
					curSettingsLine --;
			}
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
		if ( k != 0 )
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
	nJoyItems = 0;

	clearC64();
	//               "012345678901234567890123456789012345XXXX"
	printC64( 0,  1, "   .- Sidekick64 -- Frenetic -.         ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );

	//char b[20];
	//extern u32 temperature;
	//sprintf( b, "%d", temperature );
	//sprintf( b, "%d", lastKeyDebug );
	//printC64( 0, 0, b, skinValues.SKIN_MENU_TEXT_HEADER, 0 );

	if ( subGeoRAM && subHasKernal )
	{
		//               "012345678901234567890123456789012345XXXX"
		printC64( 0, 23, "    choose KERNAL and/or PRG (w/F7)     ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 0, 24, "        ? back, RETURN/F1 launch        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		//               "012345678901234567890123456789012345XXXX"
		printC64( 32, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );					
		printC64( 8, 24, "\x9f", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );				
		printC64( 16, 24, "RETURN", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );				
		printC64( 23, 24, "F1", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );					
		ADD_JOY_ITEM( 33, 23, 2, VK_F7 );
		ADD_JOY_ITEM( 8, 24, 1, VK_ESC );
		ADD_JOY_ITEM( 16, 24, 6, VK_RETURN );
		ADD_JOY_ITEM( 23, 24, 2, VK_F1 );
	} else
	if ( subGeoRAM && !subHasKernal )
	{
		printC64( 0, 23, "        choose PRG (also via F7)        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 0, 24, "        ? back, RETURN/F1 launch        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		//               "012345678901234567890123456789012345XXXX"
		printC64( 29, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 8, 24, "\x9f", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 16, 24, "RETURN", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 23, 24, "F1", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		ADD_JOY_ITEM( 29, 23, 2, VK_F7 );
		ADD_JOY_ITEM( 8, 24, 1, VK_ESC );
		ADD_JOY_ITEM( 16, 24, 6, VK_RETURN );
		ADD_JOY_ITEM( 23, 24, 2, VK_F1 );
	} else
	if ( subSID )
	{
		printC64( 0, 23, "        choose PRG (also via F7)        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 0, 24, "        ? back, RETURN/F3 launch        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		//               "012345678901234567890123456789012345XXXX"
		printC64( 29, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 8, 24, "\x9f", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 16, 24, "RETURN", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 23, 24, "F3", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		ADD_JOY_ITEM( 29, 23, 2, VK_F7 );
		ADD_JOY_ITEM( 8, 24, 1, VK_ESC );
		ADD_JOY_ITEM( 16, 24, 6, VK_RETURN );
		ADD_JOY_ITEM( 23, 24, 2, VK_F3 );
	} else
	{
		printC64( 0, 23, "     F7 Browser, F8 Exit to Basic", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		//               "012345678901234567890123456789012345XXXX"
		printC64( 5, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 17, 23, "F8", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		ADD_JOY_ITEM( 5, 23, 2, VK_F7 );
		ADD_JOY_ITEM( 17, 23, 2, VK_F8 );
	}

	if ( modeC128 )
		printC64( 36, 23-0*(hasSIDKick?1:0), "C128", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); else
		printC64( 37, 23-0*(hasSIDKick?1:0), "C64", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	if ( modeVIC == 0 ) // old VIC2
	{
		printC64( 36, 24-0*(hasSIDKick?1:0), "6569", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	} else {
		if ( modeC128 )
			printC64( 36, 24-0*(hasSIDKick?1:0), "8564", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); else
			printC64( 36, 24-0*(hasSIDKick?1:0), "8565", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	}

	if ( hasSIDKick )
		printC64( 36, 22, "SIDK", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 

	// menu headers + titels
	for ( int i = 0; i < CATEGORY_NAMES; i++ )
		if ( menuX[ i ] != -1 )
		{
			extern u32 wireKernalAvailable;
			if ( i == 4 && !wireKernalAvailable )
				printC64( menuX[ i ], menuY[ i ], categoryNames[ i ], 15, 0 ); else
				printC64( menuX[ i ], menuY[ i ], categoryNames[ i ], skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );
		}

	for ( int i = 1; i < CATEGORY_NAMES; i++ )
		if ( menuX[ i ] != -1 )
		for ( int j = 0; j < menuItems[ i ]; j++ )
		{
			char key[ 2 ] = { 0, 0 };
			switch ( i )
			{
				case 1: key[ 0 ] = '1' + j; break;
				case 2: key[ 0 ] = 'A' + j; break;
				case 3: key[ 0 ] = 'A' + j + menuItems[ 2 ]; break;
				case 4: key[ 0 ] = '1' + j + menuItems[ 1 ]; break;
			};
			int flag = 0;
			if ( i == 4 && j == subHasKernal )
				flag = 0x80;

			extern u32 wireKernalAvailable;
			if ( i == 4 && !wireKernalAvailable )
			{
				printC64( menuItemPos[ i ][ j ][ 0 ], menuItemPos[ i ][ j ][ 1 ], key, 12, 0 );
				printC64( menuItemPos[ i ][ j ][ 0 ] + 2, menuItemPos[ i ][ j ][ 1 ], menuText[ i ][ j ], 12, 0 );
			} else
			{
				ADD_JOY_ITEM( menuItemPos[ i ][ j ][ 0 ], menuItemPos[ i ][ j ][ 1 ], 2 + strlen( menuText[ i ][ j ] ), key[ 0 ] );
				printC64( menuItemPos[ i ][ j ][ 0 ], menuItemPos[ i ][ j ][ 1 ], key, skinValues.SKIN_MENU_TEXT_KEY, flag );
				printC64( menuItemPos[ i ][ j ][ 0 ] + 2, menuItemPos[ i ][ j ][ 1 ], menuText[ i ][ j ], skinValues.SKIN_MENU_TEXT_ITEM, flag );
			}
		}

	// special menu
	printC64( menuX[ 0 ], menuY[ 0 ]+1, "F1", skinValues.SKIN_MENU_TEXT_KEY, subGeoRAM ? 0x80 : 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+1, "GeoRAM", skinValues.SKIN_MENU_TEXT_ITEM, subGeoRAM ? 0x80 : 0 );
	ADD_JOY_ITEM( menuX[ 0 ], menuY[ 0 ]+1, 2, VK_F1 );

	extern u32 wireSIDAvailable;
	if ( !wireSIDAvailable )
	{
		printC64( menuX[ 0 ], menuY[ 0 ]+2, "F3", skinValues.SKIN_MENU_TEXT_KEY, subSID ? 0x80 : 0 );
		printC64( menuX[ 0 ]+3+4, menuY[ 0 ]+2, "FM Emulation", skinValues.SKIN_MENU_TEXT_ITEM, subSID ? 0x80 : 0 );
		printC64( menuX[ 0 ]+3, menuY[ 0 ]+2, "SID+", 12, 0 );
	} else
	{
		printC64( menuX[ 0 ], menuY[ 0 ]+2, "F3", skinValues.SKIN_MENU_TEXT_KEY, subSID ? 0x80 : 0 );
		printC64( menuX[ 0 ]+3, menuY[ 0 ]+2, "SID+FM Emulation", skinValues.SKIN_MENU_TEXT_ITEM, subSID ? 0x80 : 0 );
	}
	ADD_JOY_ITEM( menuX[ 0 ], menuY[ 0 ]+2, 2, VK_F3 );

	printC64( menuX[ 0 ], menuY[ 0 ]+3, "F5", skinValues.SKIN_MENU_TEXT_KEY, 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+3, "Settings", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
	ADD_JOY_ITEM( menuX[ 0 ], menuY[ 0 ]+3, 2, VK_F5 );

	if ( hasSIDKick )
	{
		printC64( menuX[ 0 ], menuY[ 0 ]+4, "F6", skinValues.SKIN_MENU_TEXT_KEY, 0 );
		printC64( menuX[ 0 ]+3, menuY[ 0 ]+4, "SIDKick Config", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		ADD_JOY_ITEM( menuX[ 0 ], menuY[ 0 ]+4, 2, VK_F6 );
	}

	if ( joyIdx != - 1 )
		printC64( (joyX-1+40)%40, (joyY%25), ">", 1, 0 );

	printSidekickLogo();

	startInjectCode();
	injectPOKE( 53280, skinValues.SKIN_MENU_BORDER_COLOR );
	injectPOKE( 53281, skinValues.SKIN_MENU_BACKGROUND_COLOR );
	if ( skinFontLoaded )
		injectPOKE( 53272, 30 ); else
		injectPOKE( 53272, 23 ); 
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
	printC64( 0,  1, "   .- Sidekick64 -- Frenetic -.         ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( 0, 23, "    F5 Back to Menu, S Save Settings    ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	//               "012345678901234567890123456789012345XXXX"
	printC64( 4, 23, "F5", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
	printC64( 21, 23, "S", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );

	s32 x = 1, x2 = 7,y1 = 1-1, y2 = 1-2;
	s32 l = curSettingsLine;

	extern u32 wireSIDAvailable;

	if ( !wireSIDAvailable ) { y1 --; y2 --; }

	// special menu
	printC64( x+1, y1+3+1, "GeoRAM", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

	printC64( x+1, y1+5, "Memory", skinValues.SKIN_MENU_TEXT_ITEM, (l==0)?0x80:0 );

	char memStr[ 4 ][ 8 ] = { "512 KB", "1 MB", "2 MB", "4 MB" };
	printC64( x2+10, y1+5, memStr[ settings[ 0 ] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==0)?0x80:0 );

	printC64( x+1, y1+6, "File on SD", skinValues.SKIN_MENU_TEXT_ITEM, (l==1)?0x80:0 );
	char t[ 64 ];
	sprintf( t, "SD:GEORAM/slot%02d.ram", settings[ 1 ] );
	printC64( x2+10, y1+6, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==1)?0x80:0 );

	printC64( x2+10, y1+7, (const char*)"\"                  \"", skinValues.SKIN_MENU_TEXT_ITEM, (typeInName==1)?0x80:0 );
	printC64( x2+11, y1+7, (const char*)geoRAM_SlotNames[ settings[ 1 ] ], skinValues.SKIN_MENU_TEXT_ITEM, (typeInName==1)?0x80:0 );
	if ( typeInName )
		c64color[ x2+11+typeCurPos + (y1+7)*40 ] = skinValues.SKIN_MENU_TEXT_CATEGORY;
	printC64( x+1,  y2+10, "SID+FM (reSID/fmopl/TinySoundFont)", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );
	if ( !wireSIDAvailable )
		printC64( x+1,  y2+11, "No SID-wire, only SFX will work!", skinValues.SKIN_ERROR_BAR, 0 );

	if ( !wireSIDAvailable ) { y2 ++; }

	char sidStrS[ 6 ][ 21 ] = { "6581", "8580", "8580 w/ Digiboost", "8x 6581", "8x 8580", "8x 8580 w/ Digiboost" };
	char sidStrO[ 3 ][ 8 ] = { "off", "on" };
	char sidStrS2[ 4 ][ 20 ] = { "6581", "8580", "8580 w/ Digiboost", "none" };
		// MIDI 
		// 0 = off
		// 1 = Datel
		// 2 = SEQUENTIAL CIRCUITS INC.
		// 3 = PASSPORT & SENTECH
	char sidStrM[ 3 ][ 24 ]  = { "off", "Datel/Sequential" };
	char sidStrA[ 4 ][ 8 ] = { "$D400", "$D420", "$D500", "$DE00" };
	char sidStrOutput[ 3 ][ 9 ] = { "PWM", "HDMI", "PWM+HDMI" };
	
	printC64( x+1,  y2+11, "SID #1", skinValues.SKIN_MENU_TEXT_ITEM, (l==2)?0x80:0 );
	printC64( x2+10, y2+11, sidStrS[ settings[2] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==2)?0x80:0 );

	if ( settings[2] < 3 )
	{
		printC64( x+1,  y2+12, "Register Read", skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );
		printC64( x2+10, y2+12, sidStrO[ settings[3] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );

		printC64( x+1,  y2+13, "Volume", skinValues.SKIN_MENU_TEXT_ITEM, (l==4)?0x80:0 );
		printC64( x+1+6,  y2+13, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		printC64( x+1+7,  y2+13, "Panning", skinValues.SKIN_MENU_TEXT_ITEM, (l==5)?0x80:0 );
		sprintf( t, "%2d", settings[ 4 ] );
		printC64( x2+10, y2+13, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==4)?0x80:0 );
		printC64( x2+13, y2+13, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		sprintf( t, "%2d", settings[ 5 ] - 7 );
		printC64( x2+15, y2+13, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==5)?0x80:0 );

		printC64( x+1,  y2+14, "SID #2", skinValues.SKIN_MENU_TEXT_KEY, (l==6)?0x80:0 );
		printC64( x2+10, y2+14, sidStrS2[ settings[6] ], skinValues.SKIN_MENU_TEXT_KEY, (l==6)?0x80:0 );

		printC64( x+1,  y2+15, "Address", skinValues.SKIN_MENU_TEXT_KEY, (l==7)?0x80:0 );
		printC64( x2+10, y2+15, sidStrA[ settings[7] ], skinValues.SKIN_MENU_TEXT_KEY, (l==7)?0x80:0 );

		printC64( x+1,  y2+16, "Volume", skinValues.SKIN_MENU_TEXT_KEY, (l==8)?0x80:0 );
		printC64( x+1+6,  y2+16, "/", skinValues.SKIN_MENU_TEXT_KEY, 0 );
		printC64( x+1+7,  y2+16, "Panning", skinValues.SKIN_MENU_TEXT_KEY, (l==9)?0x80:0 );
		sprintf( t, "%2d", settings[ 8 ] );
		printC64( x2+10, y2+16, t, skinValues.SKIN_MENU_TEXT_KEY, (l==8)?0x80:0 );
		printC64( x2+13, y2+16, "/", skinValues.SKIN_MENU_TEXT_KEY, 0 );
		sprintf( t, "%2d", settings[ 9 ] - 7 );
		printC64( x2+15, y2+16, t, skinValues.SKIN_MENU_TEXT_KEY, (l==9)?0x80:0 );

		printC64( x+1,  y2+17, "SFX Sound Exp.", skinValues.SKIN_MENU_TEXT_ITEM, (l==10)?0x80:0 );
		printC64( x2+10, y2+17, sidStrO[ settings[10] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==10)?0x80:0 );

		printC64( x+1,  y2+18, "Volume", skinValues.SKIN_MENU_TEXT_ITEM, (l==11)?0x80:0 );
		printC64( x+1+6,  y2+18, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		printC64( x+1+7,  y2+18, "Panning", skinValues.SKIN_MENU_TEXT_ITEM, (l==12)?0x80:0 );
		sprintf( t, "%2d", settings[ 11 ] );
		printC64( x2+10, y2+18, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==11)?0x80:0 );
		printC64( x2+13, y2+18, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		sprintf( t, "%2d", settings[ 12 ] - 7 );
		printC64( x2+15, y2+18, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==12)?0x80:0 );

		// MIDI 
		// 0 = off
		// 1 = Datel
		// 2 = PASSPORT & SENTECH
		// 3 = SEQUENTIAL CIRCUITS INC.

		printC64( x+1,  y2+19, "MIDI", skinValues.SKIN_MENU_TEXT_KEY, (l==13)?0x80:0 );
		printC64( x2+10, y2+19, sidStrM[ settings[13] ], skinValues.SKIN_MENU_TEXT_KEY, (l==13)?0x80:0 );
		printC64( x+1,  y2+20, "Sound Font", skinValues.SKIN_MENU_TEXT_KEY, (l==14)?0x80:0 );
		sprintf( t, "instrument%02d.sf2", settings[ 14 ] );
		printC64( x2+10, y2+20, t, skinValues.SKIN_MENU_TEXT_KEY, (l==14)?0x80:0 );
		printC64( x+1,  y2+21, "Volume", skinValues.SKIN_MENU_TEXT_KEY, (l==15)?0x80:0 );
		sprintf( t, "%2d", settings[ 15 ] );
		printC64( x2+10, y2+21, t, skinValues.SKIN_MENU_TEXT_KEY, (l==15)?0x80:0 );


		printC64( x+1,  y2+22, "Output", skinValues.SKIN_MENU_TEXT_ITEM, (l==16)?0x80:0 );
		printC64( x2+10, y2+22, sidStrOutput[ settings[16] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==16)?0x80:0 );

	} else 
	{
		//                     "012345678901234567890123456789012345XXXX"
		printC64( x+1,  y2+13, "This is a special mode with 8 SIDs", skinValues.SKIN_MENU_TEXT_KEY, (l==3)?0x80:0 );
		printC64( x+1,  y2+14, "at the following addresses:", skinValues.SKIN_MENU_TEXT_KEY, (l==3)?0x80:0 );

		printC64( x+1,  y2+16, "Left:  $D400, $D480, $D500, $D580", skinValues.SKIN_MENU_TEXT_KEY, (l==3)?0x80:0 );
		printC64( x+1,  y2+17, "Right: $D420, $D4A0, $D520, $D5A0", skinValues.SKIN_MENU_TEXT_KEY, (l==3)?0x80:0 );

		printC64( x+1,  y2+20, "Output", skinValues.SKIN_MENU_TEXT_ITEM, (l==13)?0x80:0 );
		printC64( x2+10, y2+20, sidStrOutput[ settings[13] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==13)?0x80:0 );
	}

	printSidekickLogo();

	if ( !wireSIDAvailable )
	{
		for ( int i = 12 * 40; i < 20 * 40; i++ )
			c64color[ i ] = 12;
	}

	startInjectCode();
	injectPOKE( 53280, skinValues.SKIN_MENU_BORDER_COLOR );
	injectPOKE( 53281, skinValues.SKIN_MENU_BACKGROUND_COLOR );
	if ( skinFontLoaded )
		injectPOKE( 53272, 30 ); else
		injectPOKE( 53272, 23 ); 
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
