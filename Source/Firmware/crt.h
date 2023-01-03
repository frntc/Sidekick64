/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 crt.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - code for reading/parsing CRT files
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
#ifndef _crt_h
#define _crt_h

//#define CONSOLE_DEBUG

#include <circle/types.h>
#include <circle/util.h>
#include <circle/logger.h>
#include <fatfs/ff.h>
#include "gpio_defs.h"

// which type of cartridge are we emulating?
#define BS_NONE				0x00
#define BS_EASYFLASH		0x01
#define BS_MAGICDESK		0x02
#define BS_FC3      		0x03
#define BS_AR6      		0x04
#define BS_ATOMICPOW   		0x05
#define BS_ZAXXON			0x06
#define BS_COMAL80			0x07
#define BS_EPYXFL			0x08
#define BS_SIMONSBASIC		0x09
#define BS_DINAMIC			0x0A
#define BS_C64GS			0x0B
#define BS_OCEAN			0x0C
#define BS_GMOD2			0x0D
#define BS_FUNPLAY			0x0E
#define BS_PROPHET			0x0F
#define BS_RGCD				0x10
#define BS_HUCKY			0x11
#define BS_FREEZEFRAME		0x12
#define BS_FREEZEMACHINE	0x13
#define BS_WARPSPEED    	0x14

static const char CRT_HEADER_SIG[] = "C64 CARTRIDGE   ";
static const char CRT_HEADER_SIG20[] = "VIC20 CARTRIDGE  ";
static const char CHIP_HEADER_SIG[] = "CHIP";

typedef struct  {
	u8  signature[16];
	u32 length;
    u16 version;
    u16 type;
    u8  exrom;
    u8  game;
	u8  reserved[ 6 ];
    u8  name[32 + 1];
} CRT_HEADER;

typedef struct  {
	u8  signature[4];
	u32 total_length;
    u16 type;
	u16 bank;
	u16 adr;
	u16 rom_length;
	u8  data[ 8192 ];
} CHIP_HEADER;

int  readCRTHeader( CLogger *logger, CRT_HEADER *crtHeader, const char *DRIVE, const char *FILENAME );
void readCRTFile( CLogger *logger, CRT_HEADER *crtHeader, const char *DRIVE, const char *FILENAME, u8 *flash, volatile u8 *bankswitchType, volatile u32 *ROM_LH, volatile u32 *nBanks, bool getRAW = false );
void writeChanges2CRTFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *flash, bool isRAW );
int  checkCRTFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *error, u32 *isFreezer = 0 );
int checkCRTFileVIC20( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *error );
extern int  getVIC20CRTFileStartEndAddr( CLogger *logger, const char *FILENAME, u32 *addr );

extern u8 gmod2EEPROM[ 2048 ];
extern u8 gmod2EEPROM_data;

#endif