/*
  _________.__    .___      __   .__        __        _________   ________   _____
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  |
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   |
        \/         \/    \/     \/       \/     \/            \/       \/      |__|

 disk_emulation.h

 Sidekick64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
			- code for emulating a 1541 disk drive
 Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

 adapted from Kung Fu Flash, original source (C) 2019-2022 Kim Jørgensen

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


#ifndef _disk_emulation_h
#define _disk_emulation_h

#include "helpers.h"

typedef enum
{
	FILE_DEL = 0,
	FILE_SEQ,
	FILE_PRG,
	FILE_USR,
	FILE_REL,
	FILE_CBM,
	FILE_DIR
} FILE_TYPE;

typedef struct
{
	const char *name;
	bool wildcard;
	bool overwrite;
	uint8_t drive;
	FILE_TYPE type;
	char mode;
} PARSED_FILENAME;

void initDiskEmulation( const char *FILENAME );
bool loadPrgFile( CLogger *logger, const char *DRIVE, char *FILENAME, char *vic20Filename, u8 *data, u32 *size );
bool savePrgFile( CLogger *logger, const char *DRIVE, char *FILENAME, char *vic20Filename, u8 *data, u32 size );

#endif
