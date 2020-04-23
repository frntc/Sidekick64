/*
  _________.__    .___      __   .__        __       .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/           \/    \/           \/     \/     \/ 

 launch.h

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
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

// we will read this .PRG file
static const char DRIVE[] = "SD:";
#ifndef COMPILE_MENU
static const char FILENAME[] = "SD:C64/test.prg";		// .PRG to start
#endif
static const char FILENAME_CBM80_ULTIMAX[] = "SD:C64/launch_ultimax.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM80[] = "SD:C64/launch.cbm80";	// launch code (CBM80 8k cart)

// setting EXROM and GAME (low = 0, high = 1)
#define EXROM_ACTIVE	1
#define GAME_ACTIVE		0

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

static u32	configGAMEEXROMSet, configGAMEEXROMClr;
static u32	disableCart, ultimaxDisabled, transferStarted, currentOfs;

static u32 prgSize;
static unsigned char prgData[ 65536 ] AAA;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static unsigned char launchCode[ 65536 ] AAA;

static int launchGetProgram( const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 )
{
	if ( !hasData )
	{
		if ( readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData, &prgSize ) )
			return 1; 
		return 0;
	} else
	{
		prgSize = prgSizeExt;
		memcpy( prgData, prgDataExt, prgSize );
		return 1;
	}
}

static void launchInitLoader( bool ultimax )
{
	u32 size;
	if ( ultimax )
	{
		configGAMEEXROMSet = bNMI | bDMA;
		configGAMEEXROMClr = bGAME | bEXROM | bCTRL257; 
		SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80_ULTIMAX, launchCode, &size );
	} else
	{
		configGAMEEXROMSet = bGAME | bNMI | bDMA;
		configGAMEEXROMClr = bEXROM | bCTRL257; 
		SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, launchCode, &size );
	}
}

static void launchPrepareAndWarmCache()
{
	disableCart = transferStarted = currentOfs = 0;

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], 65536, CACHE_PRELOADL2STRM )
	FORCE_READ_LINEARa( prgData, prgSize, 65536 * 8 );

	// launch code / CBM80
	CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 8192, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEARa( launchCode, 8192, 65536 * 8 );
}

//	if ( resetCounter > 3  ) {													\
		disableCart = transferStarted = ultimaxDisabled = 0;					\
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );			\
		FINISH_BUS_HANDLING														\
		return;																	\
	}																			\


#define LAUNCH_FIQ( resetCounter )												\
																				\
	if ( !disableCart )	{														\
		if ( IO1_ACCESS ) {														\
			if ( CPU_WRITES_TO_BUS ) {											\
				/* any write to IO1 will (re)start the PRG transfer */			\
				currentOfs = 0;													\
				transferStarted = 1;											\
				FINISH_BUS_HANDLING												\
				return;															\
			} else 																\
			/* if ( CPU_READS_FROM_BUS ) */										\
			{																	\
				if ( GET_IO12_ADDRESS == 1 )									\
					/* $DE01 -> get number of 256-byte pages */					\
					D = ( prgSize + 255 ) >> 8; else							\
					/* $DE00 -> get next byte */								\
					D = prgData[ currentOfs++ ];								\
																				\
				WRITE_D0to7_TO_BUS( D )											\
				FINISH_BUS_HANDLING												\
				return;															\
			}																	\
		}																		\
																				\
		/* writing #123 to $df00 (IO2) will disable the cartridge */			\
		if ( CPU_WRITES_TO_BUS && IO2_ACCESS ) {								\
			READ_D0to7_FROM_BUS( D )											\
			if ( GET_IO12_ADDRESS == 0 && D == 1 ) {							\
				SET_GPIO( bGAME | bEXROM | bNMI );								\
				ultimaxDisabled = 1;											\
				FINISH_BUS_HANDLING												\
				return;															\
			}																	\
			if ( GET_IO12_ADDRESS == 0 && D == 123 ) {							\
				LED_ON( 1 );													\
				disableCart = 1;												\
				SET_GPIO( bGAME | bEXROM | bNMI );								\
				FINISH_BUS_HANDLING												\
				return;															\
			}																	\
		}																		\
																				\
		/* access to CBM80 ROM (launch code) */									\
		if ( CPU_READS_FROM_BUS && ROML_ACCESS )								\
			WRITE_D0to7_TO_BUS( launchCode[ GET_ADDRESS + LAUNCH_BYTES_TO_SKIP ] );	\
																				\
		FINISH_BUS_HANDLING														\
		return;																	\
	}

/*
		if ( !ultimaxDisabled )	{												
			if ( CPU_READS_FROM_BUS && ROMH_ACCESS )							
				WRITE_D0to7_TO_BUS( kernalROM[ GET_ADDRESS ] );					
		}																		
																				
*/