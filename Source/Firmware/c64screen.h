/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 c64screen.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
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
#ifndef _c64screen_h
#define _c64screen_h

extern u8 c64screen[ 40 * 25 + 1024 * 4 ]; 
extern u8 c64color[ 40 * 25 + 1024 * 4 ]; 

extern int subGeoRAM;
extern int subSID;
extern int subHasKernal;
extern int subHasLaunch;
extern u32 updateMenu;


extern void clearC64();
extern void printC64( u32 x, u32 y, const char *t, u8 color, u8 flag = 0, u32 convert = 0, u32 maxL = 40 );
extern void printBrowserScreen();
extern int  checkForScreenFade( int k, char *menuItemStr, u32 *startC128 );
extern void handleC64( int k, u32 *launchKernel, char *FILENAME, char *filenameKernal, char *menuItemStr, u32 *startForC128 );
extern void renderC64();
extern void readSettingsFile();
extern void applySIDSettings();
extern void settingsGetGEORAMInfo( char *filename, u32 *size );


#endif
