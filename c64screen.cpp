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

const int VK_LEFT  = 157;
const int VK_RIGHT = 29;
const int VK_UP    = 145;
const int VK_DOWN  = 17;

const int ACT_EXIT			= 1;
const int ACT_BROWSER		= 2;
const int ACT_GEORAM		= 3;
const int ACT_SID			= 4;
const int ACT_LAUNCH_CRT	= 5;
const int ACT_LAUNCH_PRG	= 6;
const int ACT_LAUNCH_KERNAL = 7;
const int ACT_NOTHING		= 0;

extern CLogger *logger;

extern u32 prgSizeLaunch;
extern unsigned char prgDataLaunch[ 65536 ];

static const char DRIVE[] = "SD:";
static const char SETTINGS_FILE[] = "SD:C64/special.cfg";

u8 c64screen[ 40 * 25 + 1024 * 4 ]; 
u8 c64color[ 40 * 25 + 1024 * 4 ]; 

char *errorMsg = NULL;

char errorMessages[7][41] = {
//   1234567890123456789012345678901234567890
	"                NO ERROR                ",
	"  ERROR: UNKNOWN/UNSUPPORTED .CRT TYPE  ",
	"          ERROR: NO .CRT-FILE           ", 
	"          ERROR READING FILE            ",
	"            SETTINGS SAVED              ",
	"         WRONG SYSTEM, NO C128!         ",
	"         SID-WIRE NOT DETECTED!         ",
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

static const u8 scrtab[256] = {
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, // 0x00
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, // 0x08
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, // 0x10
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, // 0x18
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, // 0x20  !"#$%&'
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, // 0x28 ()*+,-./
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, // 0x30 01234567
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, // 0x38 89:;<=>?
	0x00, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, // 0x40 @ABCDEFG
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, // 0x48 HIJKLMNO
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, // 0x50 PQRSTUVW
	0x58, 0x59, 0x5a, 0x1b, 0xbf, 0x1d, 0x1e, 0x64, // 0x58 XYZ[\]^_
	0x27, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // 0x60 `abcdefg
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, // 0x68 hijklmno
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, // 0x70 pqrstuvw
	0x18, 0x19, 0x1a, 0x1b, 0x5d, 0x1d, 0x1f, 0x20, // 0x78 xyz{|}~ 
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 0x80
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 0x88
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 0x90
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // 0x98
	0x20, 0x21, 0x03, 0x1c, 0xbf, 0x59, 0x5d, 0xbf, // 0xa0 ��������
	0x22, 0x43, 0x01, 0x3c, 0xbf, 0x2d, 0x52, 0x63, // 0xa8 ��������
	0x0f, 0xbf, 0x32, 0x33, 0x27, 0x15, 0xbf, 0xbf, // 0xb0 ��������
	0x2c, 0x31, 0x0f, 0x3e, 0xbf, 0xbf, 0xbf, 0x3f, // 0xb8 ��������
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x43, // 0xc0 ��������
	0x45, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49, // 0xc8 ��������
	0xbf, 0x4e, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x18, // 0xd0 ��������
	0x4f, 0x55, 0x55, 0x55, 0x55, 0x59, 0xbf, 0xbf, // 0xd8 ��������
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, // 0xe0 ��������
	0x05, 0x05, 0x05, 0x05, 0x09, 0x09, 0x09, 0x09, // 0xe8 ��������
	0xbf, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xbf, // 0xf0 ��������
	0x0f, 0x15, 0x15, 0x15, 0x15, 0x19, 0xbf, 0x19  // 0xf8 ��������
};

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
	u32 lines = 0;
	s32 idx = scrollPos;
	while ( lines < DISPLAY_LINES && idx < nDirEntries ) 
	{
		if ( idx >= nDirEntries ) 
			break;

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
				if ( dir[ idx ].size / 1024 > 999 )
					sprintf( temp, " %1.1fm", (float)dir[ idx ].size / (1024 * 1024) ); else
					sprintf( temp, " %3dk", dir[ idx ].size / 1024 );
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
	printC64( 0,  1, "        .- sidekick64-browser -.        ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( 0,  2, "           scanning directory           ", 0, 0 );
	printC64( 0, 23, "  F1/F3 Page Up/Down  F7 Back to Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
	printC64( 2, 23, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
	printC64( 5, 23, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
	printC64( 22, 23, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 3 );
	if ( modeC128 )
		printC64( 0, 24, "SHIFT+RETURN to launch PRGs in C128-mode", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );

	if ( subGeoRAM )
	{
		printC64(  0, 24, "  choose CRT or PRG w/ NeoRAM to start  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 16, 24, "PRG w/ NeoRAM", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
		printC64(  9, 24, "CRT", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
	} else
	if ( subSID )
	{
		printC64(  0, 24, "  choose CRT or PRG w/ SID+FM to start  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0, 3 );
		printC64( 16, 24, "PRG w/ SID+FM", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
		printC64(  9, 24, "CRT", skinValues.SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED, 0, 3 );
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
	subHasKernal = -1;
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
	if ( key == VK_F7 ) { resetMenuState(3); return ACT_BROWSER;/* Browser */ } else
	if ( key == VK_F1 ) { resetMenuState(1); return ACT_GEORAM;/* GEORAM */ } else
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

#define MAX_SETTINGS 13
u32 curSettingsLine = 0;
s32 rangeSettings[ MAX_SETTINGS ] = { 4, 10, 6, 2, 16, 15, 4, 4, 16, 15, 2, 16, 15 };
s32 settings[ MAX_SETTINGS ]      = { 0,  0, 0, 0, 15,  0, 0, 0, 15, 14, 0, 15, 7 };
u8  geoRAM_SlotNames[ 10 ][ 21 ];

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

u32 octaSIDMode = 0;

void applySIDSettings()
{
	octaSIDMode = ( settings[ 2 ] > 2 ) ? 1 : 0;
	// how elegant...
	extern void setSIDConfiguration( u32 mode, u32 sid1, u32 sid2, u32 sid2addr, u32 rr, u32 addr, u32 exp, s32 v1, s32 p1, s32 v2, s32 p2, s32 v3, s32 p3 );
	setSIDConfiguration( 0, settings[2], settings[6], settings[7]-1, settings[3], settings[7], settings[10],
						 settings[4], settings[5], settings[8], settings[9], settings[11], settings[12] );
}

// ugly, hard-coded handling of UI
void handleC64( int k, u32 *launchKernel, char *FILENAME, char *filenameKernal, char *menuItemStr, u32 *startC128 = NULL )
{
	char *filename;

	extern u32 wireSIDAvailable;

	if ( menuScreen == MENU_MAIN )
	{
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
		int r = getMainMenuSelection( k, &filename, &temp, menuItemStr );

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
			strcpy( FILENAME, filename );
			*launchKernel = 2;
			return;
		}
	} else
	if ( menuScreen == MENU_BROWSER )
	{
		// browser screen
		if ( k == VK_F7 )
		{
			menuScreen = MENU_MAIN;
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
					insertDirectoryContents( nodes[ i ], path, dir[ cursorPos ].f & DIR_LISTALL );
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

		if ( k == VK_RETURN || k == VK_SHIFT_RETURN )
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
				curSettingsLine %= 3; else
				curSettingsLine %= MAX_SETTINGS;
		} else
		// up
		if ( k == VK_UP )
		{
			if ( curSettingsLine == 0 )
			{
				if ( settings[2] > 2 )
					curSettingsLine = 2; else
					curSettingsLine = MAX_SETTINGS - 1; 
			} else
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
	printC64( 0,  1, "   .- Sidekick64 -- Frenetic -.         ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );

	/*char b[20];
	extern u32 temperature;
	sprintf( b, "%d", temperature );
	//sprintf( b, "%d", lastKeyDebug );
	printC64( 0, 0, b, skinValues.SKIN_MENU_TEXT_HEADER, 0 );*/

	if ( subGeoRAM && subHasKernal )
	{
		//               "012345678901234567890123456789012345XXXX"
		printC64( 0, 23, "    choose KERNAL and/or PRG (w/ F7)    ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		printC64( 0, 24, "        ? back, RETURN/F1 launch        ", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		//               "012345678901234567890123456789012345XXXX"
		printC64( 33, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 8, 24, "\x9f", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 16, 24, "RETURN", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 23, 24, "F1", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
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
	} else
	{
		printC64( 0, 23, "     F7 Browser, F8 Exit to Basic", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );
		//               "012345678901234567890123456789012345XXXX"
		printC64( 5, 23, "F7", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
		printC64( 17, 23, "F8", skinValues.SKIN_MENU_TEXT_FOOTER, 128, 0 );
	}

	if ( modeC128 )
		printC64( 36, 23, "C128", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); else
		printC64( 37, 23, "C64", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	if ( modeVIC == 0 ) // old VIC2
	{
		printC64( 36, 24, "6569", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	} else {
		if ( modeC128 )
			printC64( 36, 24, "8564", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); else
			printC64( 36, 24, "8565", skinValues.SKIN_MENU_TEXT_SYSINFO, 0 ); 
	}

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
				printC64( menuItemPos[ i ][ j ][ 0 ], menuItemPos[ i ][ j ][ 1 ], key, skinValues.SKIN_MENU_TEXT_KEY, flag );
				printC64( menuItemPos[ i ][ j ][ 0 ] + 2, menuItemPos[ i ][ j ][ 1 ], menuText[ i ][ j ], skinValues.SKIN_MENU_TEXT_ITEM, flag );
			}
		}

	// special menu
	printC64( menuX[ 0 ], menuY[ 0 ]+1, "F1", skinValues.SKIN_MENU_TEXT_KEY, subGeoRAM ? 0x80 : 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+1, "GeoRAM", skinValues.SKIN_MENU_TEXT_ITEM, subGeoRAM ? 0x80 : 0 );

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

	printC64( menuX[ 0 ], menuY[ 0 ]+3, "F5", skinValues.SKIN_MENU_TEXT_KEY, 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+3, "Settings", skinValues.SKIN_MENU_TEXT_ITEM, 0 );

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

	u32 x = 1, x2 = 7,y1 = 1, y2 = 1;
	u32 l = curSettingsLine;

	extern u32 wireSIDAvailable;

	if ( !wireSIDAvailable ) { y1 --; y2 --; }

	// special menu
	printC64( x+1, y1+3, "GeoRAM", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

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

	printC64( x+1,  y2+9, "SID+FM (using reSID and fmopl)", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );
	if ( !wireSIDAvailable )
		printC64( x+1,  y2+10, "No SID-wire, only SFX will work!", skinValues.SKIN_ERROR_BAR, 0 );

	if ( !wireSIDAvailable ) { y2 ++; }

	char sidStrS[ 6 ][ 21 ] = { "6581", "8580", "8580 w/ Digiboost", "8x 6581", "8x 8580", "8x 8580 w/ Digiboost" };
	char sidStrO[ 3 ][ 8 ] = { "off", "on" };
	char sidStrS2[ 4 ][ 20 ] = { "6581", "8580", "8580 w/ Digiboost", "none" };
	char sidStrA[ 4 ][ 8 ] = { "$D400", "$D420", "$D500", "$DE00" };
	
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

		printC64( x+1,  y2+15, "SID #2", skinValues.SKIN_MENU_TEXT_ITEM, (l==6)?0x80:0 );
		printC64( x2+10, y2+15, sidStrS2[ settings[6] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==6)?0x80:0 );

		printC64( x+1,  y2+16, "Address", skinValues.SKIN_MENU_TEXT_ITEM, (l==7)?0x80:0 );
		printC64( x2+10, y2+16, sidStrA[ settings[7] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==7)?0x80:0 );

		printC64( x+1,  y2+17, "Volume", skinValues.SKIN_MENU_TEXT_ITEM, (l==8)?0x80:0 );
		printC64( x+1+6,  y2+17, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		printC64( x+1+7,  y2+17, "Panning", skinValues.SKIN_MENU_TEXT_ITEM, (l==9)?0x80:0 );
		sprintf( t, "%2d", settings[ 8 ] );
		printC64( x2+10, y2+17, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==8)?0x80:0 );
		printC64( x2+13, y2+17, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		sprintf( t, "%2d", settings[ 9 ] - 7 );
		printC64( x2+15, y2+17, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==9)?0x80:0 );

		printC64( x+1,  y2+19, "SFX Sound Exp.", skinValues.SKIN_MENU_TEXT_ITEM, (l==10)?0x80:0 );
		printC64( x2+10, y2+19, sidStrO[ settings[10] ], skinValues.SKIN_MENU_TEXT_ITEM, (l==10)?0x80:0 );

		printC64( x+1,  y2+20, "Volume", skinValues.SKIN_MENU_TEXT_ITEM, (l==11)?0x80:0 );
		printC64( x+1+6,  y2+20, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		printC64( x+1+7,  y2+20, "Panning", skinValues.SKIN_MENU_TEXT_ITEM, (l==12)?0x80:0 );
		sprintf( t, "%2d", settings[ 11 ] );
		printC64( x2+10, y2+20, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==11)?0x80:0 );
		printC64( x2+13, y2+20, "/", skinValues.SKIN_MENU_TEXT_ITEM, 0 );
		sprintf( t, "%2d", settings[ 12 ] - 7 );
		printC64( x2+15, y2+20, t, skinValues.SKIN_MENU_TEXT_ITEM, (l==12)?0x80:0 );
	} else 
	{
		//                     "012345678901234567890123456789012345XXXX"
		printC64( x+1,  y2+13, "This is a special mode with 8 SIDs", skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );
		printC64( x+1,  y2+14, "at the following addresses:", skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );

		printC64( x+1,  y2+16, "Left:  $D400, $D480, $D500, $D580", skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );
		printC64( x+1,  y2+17, "Right: $D420, $D4A0, $D520, $D5A0", skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );
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
