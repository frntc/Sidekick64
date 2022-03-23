/*
  _________.__    .___      __   .__        __      _______________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __  \_____  \   _  \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /  ____/  /_\  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <   /       \  \_/   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \_______ \_____  /
        \/         \/    \/     \/       \/     \/          \/     \/ 
		
 vic20config.h

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
#ifndef _config_h
#define _config_h

#define MAX_ITEMS	20
extern int menuX[ 5 ], menuY[ 5 ], menuItems[ 5 ];
extern char menuText[ 5 ][ MAX_ITEMS ][ 32 ], menuFile[ 5 ][ MAX_ITEMS ][ 1024 ], menuFile2[ 5 ][ MAX_ITEMS ][ 1024 ];
extern int menuItemPos[ 5 ][ MAX_ITEMS ][ 2 ];

extern int screenType, screenRotation;

extern u8 cfgVIC_Emulation,
		  cfgVIC_VFLI_Support,
		  cfgVIC_Audio_Filter;
extern u8  cfgDeviceNumber;
extern u16 cfgVIC_ScanlineIntensity;

#define TIMING_NAMES 4
const char timingNames[TIMING_NAMES][32] = {
	"WAIT_FOR_SIGNALS", 
	"WAIT_CYCLE_READ", 
	"WAIT_CYCLE_WRITEDATA",
	"WAIT_CYCLE_MULTIPLEXER", 
};

#define CATEGORY_NAMES 3
const char categoryNames[CATEGORY_NAMES][32] = {
	"SPECIAL",
	"PRG", 
	"CRT", 
};

#define SKIN_NAMES 18
const char skinNames[SKIN_NAMES][40] = {
	"SKIN_BROWSER_BACKGROUND_COLOR",
	"SKIN_BROWSER_BORDER_COLOR",
	"SKIN_BROWSER_TEXT_HEADER",
	"SKIN_BROWSER_TEXT_FOOTER",
	"SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED",
	"SKIN_TEXT_BROWSER",
	"SKIN_TEXT_BROWSER_DIRECTORY",
	"SKIN_TEXT_BROWSER_CURRENT",
	"SKIN_MENU_BACKGROUND_COLOR",
	"SKIN_MENU_BORDER_COLOR",
	"SKIN_MENU_TEXT_HEADER",
	"SKIN_MENU_TEXT_FOOTER",
	"SKIN_MENU_TEXT_CATEGORY",
	"SKIN_MENU_TEXT_ITEM",
	"SKIN_MENU_TEXT_KEY",
	"SKIN_MENU_TEXT_SYSINFO",
	"SKIN_ERROR_BAR",
	"SKIN_ERROR_TEXT"
};

extern int readConfig( CLogger *logger, char *DRIVE, char *FILENAME );
extern int getMainMenuSelection( int key, char **FILE, int *addIdx = 0 );
extern void printMainMenu();

#pragma pack(push)
#pragma pack(1)
union  __attribute__((packed)) T_SKIN_VALUES {
	struct __attribute__((packed)) {
		// browser colors
		u32 SKIN_BROWSER_BACKGROUND_COLOR;
		u32 SKIN_BROWSER_BORDER_COLOR;
		// text in menu header, footer, and highlighted text
		u32 SKIN_BROWSER_TEXT_HEADER;
		u32 SKIN_BROWSER_TEXT_FOOTER;
		u32 SKIN_BROWSER_TEXT_FOOTER_HIGHLIGHTED;

		// standard text (files, crts), for directories in tree view, and item below cursore
		u32 SKIN_TEXT_BROWSER;
		u32 SKIN_TEXT_BROWSER_DIRECTORY;
		u32 SKIN_TEXT_BROWSER_CURRENT;

		// menu colors
		u32 SKIN_MENU_BACKGROUND_COLOR;
		u32 SKIN_MENU_BORDER_COLOR;
		// text in menu header and footer
		u32 SKIN_MENU_TEXT_HEADER;
		u32 SKIN_MENU_TEXT_FOOTER;

		// menu items: heading (category), items, and the key that needs to be pressed
		u32 SKIN_MENU_TEXT_CATEGORY;
		u32 SKIN_MENU_TEXT_ITEM;
		u32 SKIN_MENU_TEXT_KEY;

		// sysinfo on the bottom right
		u32 SKIN_MENU_TEXT_SYSINFO;

		// error message colors (horizontal bar and text)
		u32 SKIN_ERROR_BAR;
		u32 SKIN_ERROR_TEXT;
	};

	u32 v[ SKIN_NAMES ];
};
#pragma pack(pop)

extern u32 skinFontLoaded;
extern char skinFontFilename[ 1024 ];
extern union T_SKIN_VALUES	skinValues;

#endif
