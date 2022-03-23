/*
  _________.__    .___      __   .__        __      _______________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __  \_____  \   _  \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /  ____/  /_\  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <   /       \  \_/   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \_______ \_____  /
        \/         \/    \/     \/       \/     \/          \/     \/  
 
 vic20screen.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
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
#include "vic20screen.h"
#include "lowlevel_arm64.h"
#include "dirscan.h"
#include "vic20config.h"
#include "crt.h"
#include "kernel_menu20.h"

const int VK_F1 = 133;
const int VK_F2 = 137;
const int VK_F3 = 134;
const int VK_F4 = 138;
const int VK_F5 = 135;
const int VK_F6 = 139;
const int VK_F7 = 136;
const int VK_F8 = 140;

const int VK_m = 109;
const int VK_M = 77;

const int VK_C = 67;

const int VK_ESC = 95;
const int VK_SPACE = 32;
const int VK_DEL = 20;
const int VK_RETURN = 13;
const int VK_SHIFT_RETURN = 141;
//const int VK_MOUNT = 205; // SHIFT-M
//const int VK_MOUNT_START = 77; // M


const int VK_LEFT  = 157;
const int VK_RIGHT = 29;
const int VK_UP    = 145;
const int VK_DOWN  = 17;

extern CLogger *logger;

// todo 
extern u32 prgSizeLaunch;
extern unsigned char prgDataLaunch[ 65536 ];

static const char DRIVE[] = "SD:";
static const char SETTINGS_FILE[] = "SD:VC20/special.cfg";

u8 c64screen[ 40 * 25 + 1024 * 4 ]; 
u8 c64color[ 40 * 25 + 1024 * 4 ]; 

char *errorMsg = NULL;

char errorMessages[7][41] = {
//   1234567890123456789012345678901234567890
	"NO ERROR",
	"ERROR: UNKNOWN .CRT",
	"ERROR: NO .CRT-FILE", 
	"ERROR READING FILE",
	"SETTINGS SAVED",
	"VFLI enabled",
	"VFLI disabled"
};

// is used to signal some special (context-relevant) text on the screen bottom
int extraMsg = 0;

// layout adjustments for wide and narrow charsets
int pos_Settings_left = 4;		// settings screen, x-offset left column 
int pos_Settings_right = 4;		// settings screen, x-offset right column 
int layout_isWidescreen = 1;
int layout_Offset_PRG_Size = 4;

void setLayout( bool wideChars = false )
{
	if ( wideChars )
	{
		pos_Settings_left = 0;		// settings screen, x-offset left column 
		pos_Settings_right = 0;		// settings screen, x-offset right column 
		layout_isWidescreen = 0;
		layout_Offset_PRG_Size = 0;
	} else
	{
		pos_Settings_left = 4;		// settings screen, x-offset left column 
		pos_Settings_right = 4;		// settings screen, x-offset right column 
		layout_isWidescreen = 1;
		layout_Offset_PRG_Size = 4;
	}
}

#define MENU_MAIN	 0x00
#define MENU_BROWSER 0x01
#define MENU_ERROR	 0x02
#define MENU_CONFIG  0x03
#define MENU_CONFIG2 0x04
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
int subMenuItem = 0;
int subHasKernal = -1;
int subHasLaunch = -1;

u8 usesMemorySelection = 0, curMemorySelection = 0;

u8 nSelectedItems = 0;
u16 selItemAddr[ 16 ][ 3 ]; // index, start, end address
char selItemFilenames[ 16 ][ 2048 ];

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



int scanFileTree( u32 cursorPos, u32 scrollPos )
{
	extraMsg = 0;

	u32 lines = 0;
	s32 idx = scrollPos;
	while ( lines < DISPLAY_LINES_VIC20 && idx < nDirEntries ) 
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

bool checkAddressCollision( u16 startAddr, u16 endAddr )
{
	bool collision = false;
	for ( int i = 0; i < nSelectedItems; i++ )
	{
		u16 itemStart = selItemAddr[ i ][ 1 ];
		u16 itemEnd   = selItemAddr[ i ][ 2 ];
		if ( !( endAddr < itemStart || startAddr > itemEnd ) )
			collision = true;
	}
	return collision;
}

bool checkAddressCollisionEntry( u16 c )
{
	u16 startAddr = dir[ c ].vc20 & 65535;
	u16 endAddr   = dir[ c ].vc20 >> 16;
	return checkAddressCollision( startAddr, endAddr );
}


int printFileTree( s32 cursorPos, s32 scrollPos )
{
	s32 lines = 0;
	s32 idx = scrollPos;
	s32 lastVisible = 0;
	while ( lines < DISPLAY_LINES_VIC20 && idx < nDirEntries ) 
	{
		if ( idx >= nDirEntries ) 
			break;

		u32 convert = 0; // was 3
		u8 color = skinValues.SKIN_TEXT_BROWSER;
	
		if ( dir[ idx ].f & DIR_DIRECTORY || dir[ idx ].f & DIR_D64_FILE )
			//color = skinValues.SKIN_TEXT_BROWSER_DIRECTORY;
			color |= 128;

		if ( dir[ idx ].f & DIR_FILE_IN_D64 )
			convert = 1;

		if ( idx == cursorPos )
			//color = skinValues.SKIN_TEXT_BROWSER_CURRENT;
			color |= 64;

		char temp[1024], t2[ 1024 ] = {0};
		memset( t2, 0, 1024 );
		int leading = 0;
		if( dir[ idx ].level > 0 )
		{
			for ( u32 j = 0; j <= dir[ idx ].level - 1; j++ )
			{
				t2[ j ] = ' ';
				leading ++;
			}
		}

		if ( (dir[ idx ].parent != 0xffffffff && dir[ dir[ idx ].parent ].f & DIR_D64_FILE && dir[ idx ].parent == (u32)(idx - 1) ) )
		{
			// headline of D64
			sprintf( &temp[0], "\x7d    %s", dir[ idx ].name );

			for ( int i = 0; i < 35; i++ )
				if ( *(u8*)&temp[ i ] == 160 || *(u8*)&temp[ i ] == 95 ) temp[ i ] = ' ';

			if ( strlen( temp ) > 34 ) temp[ 35 ] = 0;

			if ( idx == cursorPos )
			{
				printC64( 1, lines + 2, temp, color | 16, 0x80, convert ); 
			} else
			{
				printC64( 1, lines + 2, temp, color | 16, 0, convert ); 
			}
		} else
		{
			if ( dir[ idx ].size > 0 )
				sprintf( temp, "%s%s                              ", t2, dir[ idx ].name ); else
				sprintf( temp, "%s%s", t2, dir[ idx ].name );

			for ( int i = 0; i < 35; i++ )
				if ( *(u8*)&temp[ i ] == 160 || *(u8*)&temp[ i ] == 95 ) temp[ i ] = ' ';

			if ( strlen( temp ) > 34 ) temp[ 35 ] = 0;

			for ( int i = 0; i < 35; i++ )
				if ( *(u8*)&temp[ i ] == 160 ) temp[ i ] = ' ';

			if ( !(dir[ cursorPos ].vc20flags & CART20) || !checkAddressCollisionEntry( idx ) )
			{
				printC64( leading, lines + 2, &temp[leading], color, (idx == cursorPos) ? 0x80 : 0, convert );

				if ( dir[ idx ].f & DIR_FILE_IN_D64 )
					printC64( leading - 1, lines + 2, "\x7d", color, (idx == cursorPos) ? 0x80 : 0, convert ); 

			} else
			{
				if ( dir[ idx ].vc20flags & ITEM_SELECTED )
				{
					// selected cartridge PRGs
					printC64( leading, lines + 2, &temp[leading], color | 16, (idx == cursorPos) ? 0x80 : 0, convert ); 
					printC64( 0, lines + 2, "\xbe", color | 16, (idx == cursorPos) ? 0x80 : 0, convert ); 
				} else
				{
					// grayed-out entries
					printC64( 1, lines + 2, &temp[ 1 ], color | 32, (idx == cursorPos) ? 0x80 : 0, convert ); 
				}
			}

			if ( dir[ idx ].size > 0 )
			{
				if ( convert == 1 )
				{
					// file in D64
					sprintf( temp, " %3dK", dir[ idx ].size / 1024 );
				} else
				{
					if ( dir[ idx ].vc20flags & CART20 )
					{
						// for PRGs/CRTs in SD:/CART20 we print start/end addresses (high bytes)
						sprintf( temp, "%X-%X", (dir[ idx ].vc20>>8) & 255, dir[ idx ].vc20 >> 24 );
					} else
					{
						if ( dir[ idx ].size / 1024 > 999 )
							sprintf( temp, " %1.1fm", (float)dir[ idx ].size / (1024 * 1024) ); else
							sprintf( temp, " %3dk", dir[ idx ].size / 1024 );
					}
				} 

				printC64( 31-4+layout_Offset_PRG_Size, lines + 2, temp, color, (idx == cursorPos) ? 0x80 : 0, convert );
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

	//printC64(0,0,  "01234567890123456789012345678901234567 89", 15, 0 );
	if ( layout_isWidescreen )
		printC64( 0,  0, "       .- Sidekick20-Browser -.       ", skinValues.SKIN_BROWSER_TEXT_HEADER, 0 ); else
		printC64( 0,  0, "    .- Sidekick20-Browser -.        ", skinValues.SKIN_BROWSER_TEXT_HEADER, 0 );

	if ( usesMemorySelection )
	{
		//printC64(0,0,  "01234567890123456789012345678901 23456789", 15, 0 );
		int ofsX = 0;
		if  ( layout_isWidescreen )
			ofsX = 3;
		printC64( ofsX, 19, "Memory: auto man 5 8 13 21 29 35", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0 );
		switch ( curMemorySelection )
		{
		default:
		case 0: printC64( 8+ofsX, 19, "auto", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 1: printC64( 13+ofsX, 19, "man", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 2: printC64( 17+ofsX, 19, "5", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 3: printC64( 19+ofsX, 19, "8", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 4: printC64( 21+ofsX, 19, "13", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 5: printC64( 24+ofsX, 19, "21", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 6: printC64( 27+ofsX, 19, "29", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		case 7: printC64( 30+ofsX, 19, "35", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 ); break;
		};
	} else
	//if ( extraMsg == 0 )
	{
		if ( layout_isWidescreen )
		{
		    //printC64(0,0,  "01234567890123456789012345678901234567 89", 15, 0 );
			printC64( 0, 19, " F1/F3 Page Up/Dn, MemConfig, F7 Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0 );
			printC64( 1, 19, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
			printC64( 4, 19, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
			printC64( 22, 19, "C", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
			printC64( 30, 19, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
		} else
		{
			printC64( 0, 19,  "F1/F3 Pg Up/Dn, MemCfg, F7 Menu  ", skinValues.SKIN_BROWSER_TEXT_FOOTER, 0 );
			printC64( 0, 19, "F1", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
			printC64( 3, 19, "F3", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
			printC64( 19, 19, "C", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
			printC64( 24, 19, "F7", skinValues.SKIN_BROWSER_TEXT_FOOTER, 128, 0 );
		}
	}

	lastLine = printFileTree( cursorPos, scrollPos );

	// scroll bar
	int t = scrollPos * DISPLAY_LINES_VIC20 / nDirEntries;
	int b = lastLine * DISPLAY_LINES_VIC20 / nDirEntries;

	// bar on the right
	for ( int i = 0; i < DISPLAY_LINES_VIC20; i++ )
	{
		char c = 95;
		if ( t <= i && i <= b )
			c = 96 + 128 - 64;

		c64screen[ 37 + ( i + 2 ) * 40 ] = c;
		c64color[ 37 + ( i + 2 ) * 40 ] = 1;
	}
}

void resetMenuState( int keep = 0 )
{
	if ( !( keep & 1 ) ) subGeoRAM = 0;
	if ( !( keep & 2 ) ) subMenuItem = 0;
	subHasKernal = -1;
	subHasLaunch = -1;
}

int getMainMenuSelection( int key, char **FILE, char **FILE2, int *addIdx )
{
	*FILE = NULL;

	if ( key >= 'a' && key <= 'z' )
		key = key + 'A' - 'a';


	if ( key == VK_F1 ) { resetMenuState(1); return 3;/* GEORAM */ } else
	if ( key == VK_F3 ) { resetMenuState(0); return 1;/* Exit */ } else
	if ( key == VK_F7 ) { resetMenuState(3); return 2;/* Browser */ } else
	if ( key == VK_F8 ) { resetMenuState(2); return 4;/* SID */ } else
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

#define MAX_SETTINGS_MEM 5
u32 curSettingsLineMem = 0;
s32 rangeSettingsMem[ MAX_SETTINGS_MEM ] = { 2, 2, 2, 2, 2 };
s32 settingsMem[ MAX_SETTINGS_MEM ]      = { 1, 1, 1, 1, 1 };

void writeSettingsFile()
{
	u32 cfgBytes = sizeof( s32 ) * MAX_SETTINGS_MEM;
	writeFile( logger, DRIVE, SETTINGS_FILE, (u8*)settingsMem, cfgBytes );
}


void readSettingsFile()
{
	char cfg[ 16384 ];
	u32 cfgBytes;
    memset( cfg, 0, 16384 );

	memset( settingsMem, 0, sizeof( s32 ) * MAX_SETTINGS_MEM );

	if ( !readFile( logger, DRIVE, SETTINGS_FILE, (u8*)cfg, &cfgBytes ) )
		writeSettingsFile();

	memcpy( settingsMem, cfg, sizeof( s32 ) * MAX_SETTINGS_MEM );

	extern u32 vic20MemoryConfigurationManual;
	vic20MemoryConfigurationManual = 0;
	for ( u32 i = 0; i < MAX_SETTINGS_MEM; i++ )
		vic20MemoryConfigurationManual |= ( settingsMem[ i ] ) << ( i * 2 );

}

void applySIDSettings()
{
}

// ugly, hard-coded handling of UI
void handleC64( int k, u32 *launchKernel, char *FILENAME, char *filenameKernal )
{
	char *filename, *filename2;

	if ( menuScreen == MENU_MAIN )
	{
		usesMemorySelection = 0;
		if ( k == VK_F1 )
		{
			// the help screen is a very special type of "error screen" :-)
			menuScreen = MENU_ERROR;
			*launchKernel = 0x100;
			return;
		}
		if ( k == VK_F2 )
		{
			*launchKernel = 0x101;
			return;
		}
		if ( k == VK_F4 )
		{
			*launchKernel = 0x0;
			extern u8 cfgVIC_Emulation,
					  cfgVIC_VFLI_Support;

			if ( cfgVIC_Emulation )
			{
				cfgVIC_VFLI_Support = 1 - cfgVIC_VFLI_Support;
				extern char *errorMsg;
				extern char errorMessages[7][41];

				if ( cfgVIC_VFLI_Support )
					errorMsg = errorMessages[ 5 ]; else
					errorMsg = errorMessages[ 6 ];
				previousMenuScreen = menuScreen;
				menuScreen = MENU_ERROR;
			}
			return;
		}
		if ( k == VK_F6 )
		{
			*launchKernel = 0x7fffffff;
			return;
		}
		if ( k == VK_F7 )
		{
			menuScreen = MENU_BROWSER;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}
		if ( k == VK_F5 )
		{
			if ( menuScreen == MENU_CONFIG )
				menuScreen = MENU_CONFIG2; else
				menuScreen = MENU_CONFIG;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}

		int temp;
		int r = getMainMenuSelection( k, &filename, &filename2, &temp );

		if ( subMenuItem == 1 )
		{
			if ( k == 27 )
			{
				resetMenuState();
				subHasLaunch = -1;
				subMenuItem = 0;
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
		case 1: // exit to basic with disk emulation
			// todo
			*launchKernel = 7;
			//deactivateCart();
			return;
		case 2: // Browser
			break;
		case 3: // reboot
			subGeoRAM = 1;
			subMenuItem = 0;
			return;
		case 4: // plain basic
			subMenuItem = 1;
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
		if ( k == VK_F7 )
		{
			menuScreen = MENU_MAIN;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}

		if ( k == VK_M && ( dir[ cursorPos ].f & DIR_D64_FILE || dir[ cursorPos ].f & DIR_FILE_IN_D64 ) )
		{
			// mount D64

			// build path
			char path[ 8192 ] = {0};
			char d64file[ 128 ] = {0};
			s32 n = 0, c = cursorPos;
			u32 nodes[ 256 ];

			nodes[ n ++ ] = c;

			while ( dir[ c ].parent != 0xffffffff )
			{
				c = nodes[ n ++ ] = dir[ c ].parent;
			}

			int stopPath = 0;
			if ( dir[ cursorPos ].f & DIR_FILE_IN_D64 )
			{
				strcpy( d64file, (char*)&dir[ cursorPos ].name[128] );
				stopPath = 1;
			}

			strcat( path, "SD:" );
			for ( s32 i = n - 1; i >= stopPath; i -- )
			{
				if ( i != n-1 )
					strcat( path, "\\" );
				strcat( path, (char*)dir[ nodes[i] ].name );
			}

			strcpy( FILENAME, path );
			*launchKernel = 8;
			return;
		}

		if ( k == VK_C )
		{
			if ( usesMemorySelection )
			{
				curMemorySelection ++;
				curMemorySelection %= 8;
			}
			usesMemorySelection = 1;
		}

		int rep = 1;
		if ( k == VK_F3 ) { k = VK_DOWN; rep = DISPLAY_LINES_VIC20 - 1; }
		if ( k == VK_F1 ) { k = VK_UP; rep = DISPLAY_LINES_VIC20 - 1; }

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
	
					//logger->Write( "scan", LogNotice, "path: '%s'", path );

					for ( u32 i = n - 1; i >= 1; i -- )
					{
						if ( i != n - 1 )
							strcat( path, "//" );
						//logger->Write( "scan", LogNotice, "cat: '%s'", (char*)dir[ nodes[i] ].name );
						strcat( path, (char*)dir[ nodes[i] ].name );
						//logger->Write( "scan", LogNotice, "path: '%s'", path );
					}
					strcat( path, "//" );

					//logger->Write( "scan", LogNotice, "path complete: '%s'", path );

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


		if ( k == VK_ESC )
		{
			for ( int i = 0; i < nSelectedItems; i++ )
				dir[ selItemAddr[ i ][ 0 ] ].vc20flags &= ~ITEM_SELECTED;
			nSelectedItems = 0;
		}

		// deselect an item
		if ( k == VK_SPACE && (dir[ cursorPos ].vc20flags & ITEM_SELECTED) )
		{
			dir[ cursorPos ].vc20flags &= ~ITEM_SELECTED;
			for ( int i = 0; i < nSelectedItems; i++ )
			{
				if ( selItemAddr[ i ][ 0 ] == cursorPos )
				{
					selItemAddr[ i ][ 0 ] = selItemAddr[ nSelectedItems - 1 ][ 0 ];
					selItemAddr[ i ][ 1 ] = selItemAddr[ nSelectedItems - 1 ][ 1 ];
					selItemAddr[ i ][ 2 ] = selItemAddr[ nSelectedItems - 1 ][ 2 ];
					nSelectedItems --;
					break;
				}
			}
		} else
		// select multiple items, but only for those where we got the start/end addresses (in vc20) 
		if ( ( k == VK_SPACE && nSelectedItems < 15 && (dir[ cursorPos ].vc20flags & CART20) ) ||
		     ( k == VK_RETURN && nSelectedItems >= 0 && nSelectedItems < 15 && (dir[ cursorPos ].vc20flags & CART20) ) )
		{
			//u8 nSelectedItems = 0;
			//u16 selItemAddr[ 16 ][ 3 ]; // index, start, end address
			// check if there is no collision with previous selections
			u16 startAddr = dir[ cursorPos ].vc20 & 65535;
			u16 endAddr   = dir[ cursorPos ].vc20 >> 16;
			if ( !checkAddressCollision( startAddr, endAddr ) )
			{
				dir[ cursorPos ].vc20flags |= ITEM_SELECTED;
				selItemAddr[ nSelectedItems ][ 0 ] = cursorPos;
				selItemAddr[ nSelectedItems ][ 1 ] = startAddr;
				selItemAddr[ nSelectedItems ][ 2 ] = endAddr;
				nSelectedItems ++;
			}
		}

		if ( k == VK_M )
		{
			s32 c = cursorPos;

			if ( dir[ c ].f & DIR_FILE_IN_D64 || dir[ c ].f == DIR_D64_FILE )
			{
				FILENAME[ 0 ] = 0;
				*launchKernel = 9;
			}
		}

		// launch items
		if ( k == VK_RETURN || k == VK_SHIFT_RETURN )
		{

			// selected some items and just added the current one (see above)
			if ( dir[ cursorPos ].vc20flags & ITEM_SELECTED )
			{
				// creat list of filenames with path...
				for ( int si = 0; si < nSelectedItems; si++ )
				{
					// build path
					char path[ 8192 ] = {0};
					s32 n = 0, c, curC;
					u32 nodes[ 256 ];

					c = nodes[ n ++ ] = curC = selItemAddr[ si ][ 0 ];

//logger->Write( "exec", LogNotice, "node %d: '%s'", dir[c].parent, dir[dir[c].parent].name );
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

					if ( dir[ curC ].f & DIR_PRG_FILE ) 
					{
						strcpy( selItemFilenames[ si ], path );
						continue;
					}

					if ( dir[ curC ].f & DIR_CRT_FILE ) 
					{
						u32 err = 0;

						strcpy( FILENAME, path );

						u32 tempKernel = checkCRTFileVIC20( logger, DRIVE, FILENAME, &err );
						if ( err > 0 || tempKernel != 5 ) // error or no normal VC20-cartridge
						{
							*launchKernel = 0;
							errorMsg = errorMessages[ err ];
							previousMenuScreen = menuScreen;
							menuScreen = MENU_ERROR;
							return;
						} else
						{
							strcpy( selItemFilenames[ si ], path );
							continue;
						} 
					}
				}
				*launchKernel = 6;
				return;
			} else
			{
				for ( int i = 0; i < nSelectedItems; i++ )
					dir[ selItemAddr[ i ][ 0 ] ].vc20flags &= ~ITEM_SELECTED;
				nSelectedItems = 0;

				// build path
				char path[ 8192 ] = {0};
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
					if ( dir[ curC ].f & DIR_FILE_IN_D64 )
					{
						//logger->Write( "exec", LogNotice, "d64file: '%s'", dir[ cursorPos ].name[128] );
						//strcpy( d64file, (char*)&dir[ cursorPos ].name[128] );
						fileIndex = dir[ curC ].f & ((1<<SHIFT_TYPE)-1);
						stopPath = 1;
					}

					strcat( path, "SD:" );

					for ( s32 i = n - 1; i >= stopPath; i -- )
					{
						if ( i != n-1 )
							strcat( path, "\\" );
						strcat( path, (char*)dir[ nodes[i] ].name );
						//logger->Write( "node", LogNotice, "%d -> '%s'", i, (char*)dir[ nodes[i] ].name );
					}
					/*
					if ( stopPath == 1 )
					{
						logger->Write( "node", LogNotice, "%d -> '%s'", 0, (char*)dir[ nodes[0] ].name );
					} 
					*/

					if ( dir[ curC ].f & DIR_FILE_IN_D64 )
					{
						//logger->Write( "exec", LogNotice, "starting PRG from .D64" ); //: '%s'", (char*)dir[ nodes[cursorPos] ].name );

						// we open the D64 to figure out the memory configuration (in case it should be determined automatically)
						// and to get the filename for the load"...",8,1 command
						{
							extern u8 d64buf[ 1024 * 1024 ];
							extern int readD64File( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size );

							u32 imgsize = 0;

							// mount file system
							FATFS m_FileSystem;
							if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
								logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

							logger->Write( "exec", LogNotice, "D64-path '%s'", path );
							if ( !readD64File( logger, "", path, d64buf, &imgsize ) )
								return;

							// unmount file system
							if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
								logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );

							char d64filename[ 20 ];
							if ( d64ParseExtract( d64buf, imgsize, D64_GET_FILE + fileIndex, prgDataLaunch, (s32*)&prgSizeLaunch, 0xffffffff, NULL, d64filename ) == 0 )
							{
								for ( int i = 0; i < (int)strlen( d64filename ); i++ )
									if ( *(u8*)&d64filename[ i ] == 0xa0 ) d64filename[ i ] = 0;

								//logger->Write( "exec", LogNotice, "got PRG-filename: '%s'", (char*)d64filename );
								//logger->Write( "exec", LogNotice, "loaded: %d bytes", prgSizeLaunch );
								strcpy( FILENAME, d64filename );
								strcpy( &FILENAME[ 256 ], path );
								*launchKernel = 9;
								return;
							}
						}
					}

					if ( ( dir[ curC ].f & DIR_PRG_FILE ) /*|| ( dir[ curC ].f & DIR_FILE_IN_D64 )*/ ) 
					{
						logger->Write( "exec", LogNotice, "sending filename '%s'", path );
						strcpy( FILENAME, path );
						*launchKernel = 4;
						return;
					}

					if ( dir[ curC ].f & DIR_CRT_FILE ) 
					{
						u32 err = 0;

						strcpy( FILENAME, path );

						u32 tempKernel = checkCRTFileVIC20( logger, DRIVE, FILENAME, &err );
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
						return;
					}

					if ( dir[ curC ].f & DIR_FILE_IN_D64 )
					{
						return;
					}
				}
			}
		}
	} else
	if ( menuScreen == MENU_CONFIG )
	{
		if ( k == VK_F7 )
		{
			menuScreen = MENU_BROWSER;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}
		if ( k == VK_F5 )
		{
			menuScreen = MENU_MAIN;
			handleC64( 0xffffffff, launchKernel, FILENAME, filenameKernal );
			return;
		}

	#if 1
		// left
		if ( k == 157 )
		{
			settingsMem[ curSettingsLineMem ] --;
			if ( settingsMem[ curSettingsLineMem ] < 0 )
				settingsMem[ curSettingsLineMem ] = rangeSettingsMem[ curSettingsLineMem ] - 1; else
				settingsMem[ curSettingsLineMem ] = max( 0, settingsMem[ curSettingsLineMem ] );

			if ( rangeSettingsMem[ curSettingsLineMem ] < 15 )
			{
				settingsMem[ curSettingsLineMem ] %= rangeSettingsMem[ curSettingsLineMem ]; 
			} else
				settingsMem[ curSettingsLineMem ] = max( 0, settingsMem[ curSettingsLineMem ] );

		} else
		// right 
		if ( k == 29 )
		{
			settingsMem[ curSettingsLineMem ] ++;
			if ( rangeSettingsMem[ curSettingsLineMem ] < 15 )
				settingsMem[ curSettingsLineMem ] %= rangeSettingsMem[ curSettingsLineMem ]; else
				settingsMem[ curSettingsLineMem ] = min( settingsMem[ curSettingsLineMem ], rangeSettingsMem[ curSettingsLineMem ] - 1 );
		} else
		// down
		if ( k == 17 )
		{
			curSettingsLineMem ++;
			curSettingsLineMem %= MAX_SETTINGS_MEM;
		} else
		// up
		if ( k == 145 )
		{
			if ( curSettingsLineMem == 0 )
				curSettingsLineMem = MAX_SETTINGS_MEM - 1; else
				curSettingsLineMem --;
		}
		if( (k == 's' || k == 'S') && typeInName == 0 )
		{
			writeSettingsFile();
			errorMsg = errorMessages[ 4 ];
			previousMenuScreen = menuScreen;
			menuScreen = MENU_ERROR;
		}

		extern u32 vic20MemoryConfigurationManual;
		vic20MemoryConfigurationManual = 0;
		for ( u32 i = 0; i < MAX_SETTINGS_MEM; i++ )
			vic20MemoryConfigurationManual |= ( settingsMem[ i ] ) << ( i * 2 );
		
	#endif
	} else
	{
		menuScreen = previousMenuScreen;
	}

}

extern int subGeoRAM, subMenuItem, subHasKernal;




void printMainMenu()
{
	clearC64();
	//                   "012345678901234567890123456789012345XXXX"
	printC64( layout_isWidescreen ? 2 : 0,  1, "    .- Sidekick20 -.", skinValues.SKIN_MENU_TEXT_HEADER, 0 );

	extern u8 c64screen[ 40 * 25 + 1024 * 4 ]; 

	printC64( layout_isWidescreen ? 4 : 0, 19, " F7 Browser  F8 Exit to Basic", skinValues.SKIN_MENU_TEXT_FOOTER, 0 );

	// menu headers + titels
	for ( int i = 0; i < CATEGORY_NAMES; i++ )
		if ( menuX[ i ] != -1 )
			printC64( menuX[ i ], menuY[ i ], categoryNames[ i ], skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

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
	printC64( menuX[ 0 ], menuY[ 0 ]+2, "F3", skinValues.SKIN_MENU_TEXT_KEY, subMenuItem ? 0x80 : 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+1, "Help", skinValues.SKIN_MENU_TEXT_ITEM, subGeoRAM ? 0x80 : 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+2, "Basic+Disk", skinValues.SKIN_MENU_TEXT_ITEM, subMenuItem ? 0x80 : 0 );

	printC64( menuX[ 0 ], menuY[ 0 ]+3, "F5", skinValues.SKIN_MENU_TEXT_KEY, 0 );
	printC64( menuX[ 0 ]+3, menuY[ 0 ]+3, "Settings", skinValues.SKIN_MENU_TEXT_ITEM, 0 );

	c64screen[ 1000 ] = ((0x6800 >> 8) & 0xFC);
	c64screen[ 1001 ] = skinValues.SKIN_MENU_BACKGROUND_COLOR;
	c64screen[ 1002 ] = skinValues.SKIN_MENU_BORDER_COLOR;
}


void printSettingsScreen()
{
	clearC64();
	//               "012345678901234567890123456789012345XXXX"
	printC64( pos_Settings_left,  0, "   .- Sidekick20 -.          ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );
	printC64( pos_Settings_left, 19, " F5 Main Menu, S Save Settings   ", skinValues.SKIN_MENU_TEXT_HEADER, 0 );

	s32 x = 1, x2 = 13,y1 = -1;
	u32 l = curSettingsLineMem;

	x += pos_Settings_left;

	char memStr[ 4 ][ 12 ] = { "N/A", "RAM", "ROM", "" };
	extern u32 vic20MemoryConfigurationManual;
	u32 m = vic20MemoryConfigurationManual;

	// special menu
	printC64( x+0, y1+3, "Memory Configuration", skinValues.SKIN_MENU_TEXT_CATEGORY, 0 );

	printC64( x+0, y1+5, "RAM123 ($0400-$0fff)", skinValues.SKIN_MENU_TEXT_KEY, (l==0)?0x80:0 );
	printC64( x2+10+pos_Settings_right, y1+5, memStr[ (m >> 0) & 3 ], skinValues.SKIN_MENU_TEXT_ITEM, (l==0)?0x80:0 );

	printC64( x+0, y1+7, "BLK 1 ($2000-$3fff)", skinValues.SKIN_MENU_TEXT_KEY, (l==1)?0x80:0 );
	printC64( x2+10+pos_Settings_right, y1+7, memStr[ (m >> 2) & 3 ], skinValues.SKIN_MENU_TEXT_ITEM, (l==1)?0x80:0 );

	printC64( x+0, y1+9, "BLK 2 ($4000-$5fff)", skinValues.SKIN_MENU_TEXT_KEY, (l==2)?0x80:0 );
	printC64( x2+10+pos_Settings_right, y1+9, memStr[ (m >> 4) & 3 ], skinValues.SKIN_MENU_TEXT_ITEM, (l==2)?0x80:0 );

	printC64( x+0, y1+11, "BLK 3 ($6000-$7fff)", skinValues.SKIN_MENU_TEXT_KEY, (l==3)?0x80:0 );
	printC64( x2+10+pos_Settings_right, y1+11, memStr[ (m >> 6) & 3 ], skinValues.SKIN_MENU_TEXT_ITEM, (l==3)?0x80:0 );

	printC64( x+0, y1+13, "BLK 5 ($a000-$bfff)", skinValues.SKIN_MENU_TEXT_KEY, (l==4)?0x80:0 );
	printC64( x2+10+pos_Settings_right, y1+13, memStr[ (m >> 8) & 3 ], skinValues.SKIN_MENU_TEXT_ITEM, (l==4)?0x80:0 );

	// 8k+ expanded VIC?
	u32 b8K = ((m>>2)&3) == 1 ? 1 : 0;
	// 3k expanded VIC?
	u32 b3K = (((m>>0)&3)==1 && !b8K) ? 1 : 0;
	u32 nUnExp = ( !b3K && !b8K ) ? 1 : 0;

	l = 1;

	u32 totalRAM = 0;

	if ( ((m>>0)&3)==1 )
		totalRAM += 3;

	for ( u32 i = 1; i < 5; i++ )
		totalRAM += ((m>>(i*2))&3) == 1 ? 8 : 0;

	u32 basicRAM = 3583;

	if ( b3K && !b8K )
	{
		basicRAM += 3 * 1024;
	}
	if ( b8K )
	{
		basicRAM += 8 * 1024;
		if ( ((m>>4)&3) == 1 )
		{
			basicRAM += 8 * 1024;
			if ( ((m>>6)&3) == 1 )
			{
				basicRAM += 8 * 1024;
			}
		}
	}

	char b[64];
	int col = skinValues.SKIN_MENU_TEXT_ITEM;
	sprintf( b, "Mem. 5K+%dK, Basic Bytes %d", totalRAM, basicRAM );
	printC64( x+0, y1+15, b, col, (l==0)?0x80:0 );

	// unexpanded VIC?
	if ( nUnExp  )
	{
		printC64( x+0, y1+16, "Basic Start     4097 ($1001)", col, (l==0)?0x80:0 );
		printC64( x+0, y1+17, "Screen Memory   7680 ($1e00)", col, (l==0)?0x80:0 );
		printC64( x+0, y1+18, "Color RAM      38400 ($9600)", col, (l==0)?0x80:0 );
	} else
	if ( b3K )
	{
		printC64( x+0, y1+16, "Basic Start     1025 ($0401)", col, (l==0)?0x80:0 );
		printC64( x+0, y1+17, "Screen Memory   7680 ($1e00)", col, (l==0)?0x80:0 );
		printC64( x+0, y1+18, "Color RAM      38400 ($9600)", col, (l==0)?0x80:0 );
	} else
	if ( b8K )
	{
		printC64( x+0, y1+16, "Basic Start     4609 ($1201)", col, (l==0)?0x80:0 );
		printC64( x+0, y1+17, "Screen Memory   4096 ($1000)", col, (l==0)?0x80:0 );
		printC64( x+0, y1+18, "Color RAM      37888 ($9400)", col, (l==0)?0x80:0 );
	} 

	c64screen[ 1000 ] = ((0x6800 >> 8) & 0xFC);
	c64screen[ 1001 ] = skinValues.SKIN_MENU_BACKGROUND_COLOR;
	c64screen[ 1002 ] = skinValues.SKIN_MENU_BORDER_COLOR;
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
	if ( menuScreen == MENU_ERROR )
	{
		if ( errorMsg != NULL )
		{
			printC64( 0, 10, "\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9", skinValues.SKIN_ERROR_BAR, 0, 1 );
			printC64( 0, 11, "                                        ", skinValues.SKIN_ERROR_TEXT, 0 );

			int p;
			if ( layout_isWidescreen )
				p = 38 - strlen( errorMsg ); else
				p = 32 - strlen( errorMsg ); 
			printC64( 0, 12, "                                      ", skinValues.SKIN_ERROR_TEXT, 0 );
			printC64( p / 2, 12, errorMsg, skinValues.SKIN_ERROR_TEXT, 0 );
			printC64( 0, 13, "                                        ", skinValues.SKIN_ERROR_TEXT, 0 );
			printC64( 0, 14, "\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8", skinValues.SKIN_ERROR_BAR, 0, 1 );
			printC64( 0, 15, "                                        ", skinValues.SKIN_ERROR_BAR, 0, 1 );
		}
	}
}

