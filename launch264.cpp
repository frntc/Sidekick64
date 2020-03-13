/*
  _________.__    .___      __   .__        __       .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/           \/    \/           \/     \/     \/ 

 launch264.cpp

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - launch/.PRG dropper routines to be inlined in some other kernel
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
#include <circle/startup.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/memory.h>
#include <circle/koptions.h>
#include <circle/types.h>
#include <circle/util.h>
#include "lowlevel_arm64.h"
#include "helpers.h"
#include "launch264.h"

extern CLogger	*logger;

// we will read this .PRG file
static const char DRIVE[] = "SD:";
#ifndef COMPILE_MENU
static const char FILENAME[] = "SD:C16/test.prg";		// .PRG to start
#endif
static const char FILENAME_CBM80[] = "SD:C16/launch16.cbm80";	// launch code (8k cart)

u32	disableCart_l264, transferStarted_l264, currentOfs_l264;

u32 prgSize_l264, launchPrg_l264;
unsigned char prgData_l264[ 65536 + 64 ] AAA;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
unsigned char launchCode_l264[ 65536 ] AAA;
unsigned char *curTransfer_l264;

int launchGetProgram( const char *FILENAME, bool hasData, u8 *prgDataExt, u32 prgSizeExt )
{
	if ( !hasData )
	{
		if ( readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData_l264, &prgSize_l264 ) )
			return 1; 
		return 0;
	} else
	{
		prgSize_l264 = prgSizeExt;
		memcpy( prgData_l264, prgDataExt, prgSize_l264 );
		return 1;
	}

	// first byte of prgData will be used for transmitting ceil(prgSize/256)
	for ( u32 i = prgSize_l264; i > 0; i-- )
		prgData_l264[ i ] = prgData_l264[ i - 1 ];
	prgData_l264[0] = ( ( prgSize_l264 + 255 ) >> 8 );
}

void launchInitLoader( bool ultimax )
{
	u32 size;
	readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, launchCode_l264, &size );
}

void launchPrepareAndWarmCache()
{
	disableCart_l264 = transferStarted_l264 = currentOfs_l264 = 0;

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &prgData_l264[ 0 ], 65536, CACHE_PRELOADL2STRM )
	FORCE_READ_LINEARa( prgData_l264, prgSize_l264, 65536 * 8 );

	// launch code / CBM80
	CACHE_PRELOAD_DATA_CACHE( &launchCode_l264[ 0 ], 8192, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEARa( launchCode_l264, 8192, 65536 * 8 );
}
