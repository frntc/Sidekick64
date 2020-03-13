/*
  _________.__    .___      __   .__        __       ________  ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_____  \/  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /  ____/   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    /       \  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   \_______ \_____  /\____   | 
        \/         \/    \/     \/       \/     \/           \/     \/      |__|  
 
 264screen.h

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - menu/screen code
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
extern void printC64( u32 x, u32 y, const char *t, u8 color, u8 flag = 0, u32 convert = 0, u32 maxL = 1024 );
extern void printBrowserScreen();
extern void handleC64( int k, u32 *launchKernel, char *FILENAME, char *filenameKernal );
extern void renderC64();
extern void readSettingsFile();
extern void applySIDSettings();
extern void settingsGetGEORAMInfo( char *filename, u32 *size );


#endif
