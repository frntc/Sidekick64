/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 crt.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - code for reading/parsing CRT files
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
#include "crt.h"

u32 swapBytesU32( u8 *buf )
{
	return buf[ 3 ] | ( buf[ 2 ] << 8 ) | ( buf[ 1 ] << 16 ) | ( buf[ 0 ] << 24 );
}

u16 swapBytesU16( u8 *buf )
{
	return buf[ 1 ] | ( buf[ 0 ] << 8 );
}

#define readCRT( dst, bytes ) memcpy( (dst), crt, bytes ); crt += bytes; 

// .CRT reading - header only!
int readCRTHeader( CLogger *logger, CRT_HEADER *crtHeader, const char *DRIVE, const char *FILENAME )
{
	CRT_HEADER header;

	FATFS m_FileSystem;

	// mount file system
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
	{
		//logger->Write( "RPiFlash-CRTHeader", LogNotice, "Cannot mount drive: %s", DRIVE );
		return -10;
	}

	// get filesize
	FILINFO info;
	u32 result = f_stat( FILENAME, &info );
	u32 filesize = (u32)info.fsize;

	// open file
	FIL file;
	result = f_open( &file, FILENAME, FA_READ | FA_OPEN_EXISTING );
	if ( result != FR_OK )
	{
		f_mount( 0, DRIVE, 0 );
		//logger->Write( "RPiFlash-CRTHeader", LogNotice, "Cannot open file: %s", FILENAME );
		return -12;
	}

	if ( filesize < 64 )
		return -2;

	// read data in one big chunk
	u32 nBytesRead;
	u8 rawCRT[ 64 ];
	result = f_read( &file, rawCRT, 64, &nBytesRead );

	if ( result != FR_OK )
	{
		//logger->Write( "RPiFlash-CRTHeader", LogNotice, "Read error" );
		return -13;
	}

	if ( f_close( &file ) != FR_OK )
	{
		//logger->Write( "RPiFlash-CRTHeader", LogNotice, "Cannot close file" );
		return -14;
	}

	// unmount file system
	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
	{
		//logger->Write( "RPiFlash-CRTHeader", LogNotice, "Cannot unmount drive: %s", DRIVE );
		return -15;
	}

	// now "parse" the file which we already have in memory
	u8 *crt = rawCRT;

	readCRT( &header.signature, 16 );

	if ( memcmp( CRT_HEADER_SIG, header.signature, 16 ) )
	{
		//logger->Write( "RPiFlash-CRTHeader", LogNotice, "no CRT file." );
		return -1;
	}

	readCRT( &header.length, 4 );
	readCRT( &header.version, 2 );
	readCRT( &header.type, 2 );
	readCRT( &header.exrom, 1 );
	readCRT( &header.game, 1 );
	readCRT( &header.reserved, 6 );
	readCRT( &header.name, 32 );
	header.name[ 32 ] = 0;

	header.length = swapBytesU32( (u8*)&header.length );
	header.version = swapBytesU16( (u8*)&header.version );
	header.type = swapBytesU16( (u8*)&header.type );

	memcpy( crtHeader, &header, sizeof( header ) );

	return 0;
}

// .CRT reading
void readCRTFile( CLogger *logger, CRT_HEADER *crtHeader, const char *DRIVE, const char *FILENAME, u8 *flash, volatile u8 *bankswitchType, volatile u32 *ROM_LH, volatile u32 *nBanks, bool getRAW )
{
	CRT_HEADER header;

	FATFS m_FileSystem;

	// mount file system
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot mount drive: %s", DRIVE );

	// get filesize
	FILINFO info;
	u32 result = f_stat( FILENAME, &info );
	u32 filesize = (u32)info.fsize;

	// open file
	FIL file;
	result = f_open( &file, FILENAME, FA_READ | FA_OPEN_EXISTING );
	if ( result != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot open file: %s", FILENAME );

	if ( filesize > 1025 * 1024 )
		filesize = 1025 * 1024;

	// read data in one big chunk
	u32 nBytesRead;
	u8 rawCRT[ 1025 * 1024 ];
	result = f_read( &file, rawCRT, filesize, &nBytesRead );

	if ( result != FR_OK )
		logger->Write( "RaspiFlash", LogError, "Read error" );

	if ( f_close( &file ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot close file" );

	// unmount file system
	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot unmount drive: %s", DRIVE );


	// now "parse" the file which we already have in memory
	u8 *crt = rawCRT;
	u8 *crtEnd = crt + filesize;

	readCRT( &header.signature, 16 );

	if ( memcmp( CRT_HEADER_SIG, header.signature, 16 ) )
	{
		logger->Write( "RaspiFlash", LogPanic, "no CRT file." );
	}

	readCRT( &header.length, 4 );
	readCRT( &header.version, 2 );
	readCRT( &header.type, 2 );
	readCRT( &header.exrom, 1 );
	readCRT( &header.game, 1 );
	readCRT( &header.reserved, 6 );
	readCRT( &header.name, 32 );
	header.name[ 32 ] = 0;

	header.length = swapBytesU32( (u8*)&header.length );
	header.version = swapBytesU16( (u8*)&header.version );
	header.type = swapBytesU16( (u8*)&header.type );

	switch ( header.type ) {
	case 32:
		//logger->Write( "RaspiFlash", LogNotice, "EasyFlash CRT" );
		*bankswitchType = BS_EASYFLASH;
		*ROM_LH = bROML | bROMH;
		break;
	case 19:
		//logger->Write( "RaspiFlash", LogNotice, "MagicDesk CRT" );
		*bankswitchType = BS_MAGICDESK;
		*ROM_LH = bROML;
		break;
	case 3:
		//logger->Write( "RaspiFlash", LogNotice, "Final Cartridge 3 CRT" );
		*bankswitchType = BS_FC3;
		*ROM_LH = bROML | bROMH;
		break;
	case 1:
		//logger->Write( "RaspiFlash", LogNotice, "Action Replay 4.2/5/6/7 CRT" );
		*bankswitchType = BS_AR6;
		*ROM_LH = bROML;
		break;
	case 9:
		//logger->Write( "RaspiFlash", LogNotice, "Action Replay 4.2/5/6/7 CRT" );
		*bankswitchType = BS_ATOMICPOW;
		*ROM_LH = bROML;
		break;
	case 0:
	default:
		*bankswitchType = BS_NONE;
		*ROM_LH = 0;
		break;
	}

	#ifdef CONSOLE_DEBUG
	logger->Write( "RaspiFlash", LogNotice, "length=%d", header.length );
	logger->Write( "RaspiFlash", LogNotice, "version=%d", header.version );
	logger->Write( "RaspiFlash", LogNotice, "type=%d", header.type );
	logger->Write( "RaspiFlash", LogNotice, "exrom=%d", header.exrom );
	logger->Write( "RaspiFlash", LogNotice, "game=%d", header.game );
	logger->Write( "RaspiFlash", LogNotice, "name=%s", header.name );
	#endif

	*nBanks = 0;

	while ( crt < crtEnd )
	{
		CHIP_HEADER chip;

		memset( &chip, 0, sizeof( CHIP_HEADER ) );

		readCRT( &chip.signature, 4 );

		if ( memcmp( CHIP_HEADER_SIG, chip.signature, 4 ) )
		{
			logger->Write( "RaspiFlash", LogPanic, "no valid CHIP section." );
		}

		readCRT( &chip.total_length, 4 );
		readCRT( &chip.type, 2 );
		readCRT( &chip.bank, 2 );
		readCRT( &chip.adr, 2 );
		readCRT( &chip.rom_length, 2 );

		chip.total_length = swapBytesU32( (u8*)&chip.total_length );
		chip.type = swapBytesU16( (u8*)&chip.type );
		chip.bank = swapBytesU16( (u8*)&chip.bank );
		chip.adr = swapBytesU16( (u8*)&chip.adr );
		chip.rom_length = swapBytesU16( (u8*)&chip.rom_length );

		#ifdef CONSOLE_DEBUG
		logger->Write( "RaspiFlash", LogNotice, "total length=%d", chip.total_length );
		logger->Write( "RaspiFlash", LogNotice, "type=%d", chip.type );
		logger->Write( "RaspiFlash", LogNotice, "bank=%d", chip.bank );
		logger->Write( "RaspiFlash", LogNotice, "adr=$%x", chip.adr );
		logger->Write( "RaspiFlash", LogNotice, "rom length=%d", chip.rom_length );
		#endif

		// MagicDesk only uses the low-bank
		if ( (*bankswitchType) == BS_MAGICDESK )
		{
			*ROM_LH = bROML;

			if ( getRAW )
			{
				for ( register u32 i = 0; i < 8192; i++ )
					flash[ chip.bank * 8192 + i ] = crt[ i ];
			} else
			{
				for ( register u32 i = 0; i < 8192; i++ )
				{
					u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
					flash[ chip.bank * 8192 + realAdr ] = crt[ i ];
				}
			}

			crt += 8192;
		} else
		{
			if ( chip.adr == 0x8000 )
			{
				*ROM_LH |= bROML;

				if ( getRAW )
				{
					for ( register u32 i = 0; i < 8192; i++ )
						flash[ ( chip.bank * 8192 + i ) * 2 + 0 ] = crt[ i ];
				} else
				{
					for ( register u32 i = 0; i < 8192; i++ )
					{
						u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
						flash[ ( chip.bank * 8192 + realAdr ) * 2 + 0 ] = crt[ i ];
					}
				}

				crt += 8192;

				if ( chip.rom_length > 8192 )
				{
					*ROM_LH |= bROMH;

					if ( getRAW )
					{
						for ( register u32 i = 0; i < 8192; i++ )
							flash[ ( chip.bank * 8192 + i ) * 2 + 1 ] = crt[ i ];
					} else
					{
						for ( register u32 i = 0; i < 8192; i++ )
						{
							u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
							flash[ ( chip.bank * 8192 + realAdr ) * 2 + 1 ] = crt[ i ];
						}
					}

					crt += chip.rom_length - 8192;
				}
			} else
			{
				*ROM_LH |= bROMH;

				if ( getRAW )
				{
					for ( register u32 i = 0; i < 8192; i++ )
						flash[ ( chip.bank * 8192 + i ) * 2 + 1 ] = crt[ i ];
				} else
				{
					for ( register u32 i = 0; i < 8192; i++ )
					{
						u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
						flash[ ( chip.bank * 8192 + realAdr ) * 2 + 1 ] = crt[ i ];
					}
				}

				crt += 8192;
			}
		}

		if ( chip.bank > *nBanks )
			*nBanks = chip.bank;
	}

	memcpy( crtHeader, &header, sizeof( CRT_HEADER ) );
	(*nBanks) ++;
}

// very lazy implementation of writing changes back to a .CRT file:
// read file, simply go through and replace bank contents
// (only for EasyFlash CRTs!)
void writeChanges2CRTFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *flash, bool isRAW )
{
	u32 nBanks;

	CRT_HEADER header;
	FATFS m_FileSystem;

	logger->Write( "RaspiFlash", LogNotice, "saving modified CRT file", DRIVE );

	// mount file system
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot mount drive: %s", DRIVE );

	// get filesize
	FILINFO info;
	u32 result = f_stat( FILENAME, &info );
	u32 filesize = (u32)info.fsize;

	// open file
	FIL file;
	result = f_open( &file, FILENAME, FA_READ | FA_OPEN_EXISTING );
	if ( result != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot open file: %s", FILENAME );

	if ( filesize > 1025 * 1024 )
		filesize = 1025 * 1024;

	// read data in one big chunk
	u32 nBytesRead;
	u8 rawCRT[ 1025 * 1024 ];
	result = f_read( &file, rawCRT, filesize, &nBytesRead );

	if ( result != FR_OK )
		logger->Write( "RaspiFlash", LogError, "Read error" );

	if ( f_close( &file ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot close file" );

	// now "parse" the file which we already have in memory
	u8 *crt = rawCRT;
	u8 *crtEnd = crt + filesize;

	readCRT( &header.signature, 16 );

	if ( memcmp( CRT_HEADER_SIG, header.signature, 16 ) )
	{
		logger->Write( "RaspiFlash", LogPanic, "no CRT file." );
	}

	readCRT( &header.length, 4 );
	readCRT( &header.version, 2 );
	readCRT( &header.type, 2 );
	readCRT( &header.exrom, 1 );
	readCRT( &header.game, 1 );
	readCRT( &header.reserved, 6 );
	readCRT( &header.name, 32 );
	header.name[ 32 ] = 0;

	header.length = swapBytesU32( (u8*)&header.length );
	header.version = swapBytesU16( (u8*)&header.version );
	header.type = swapBytesU16( (u8*)&header.type );

	if ( header.type != 32 )
	{
		logger->Write( "RaspiFlash", LogNotice, "no EF CRT" );
		// unmount file system
		if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
			logger->Write( "RaspiFlash", LogPanic, "Cannot unmount drive: %s", DRIVE );
		return;
	}

	nBanks = 0;

		logger->Write( "RaspiFlash", LogNotice, "patching" );

	while ( crt < crtEnd )
	{
		CHIP_HEADER chip;

		memset( &chip, 0, sizeof( CHIP_HEADER ) );

		readCRT( &chip.signature, 4 );

		if ( memcmp( CHIP_HEADER_SIG, chip.signature, 4 ) )
		{
			logger->Write( "RaspiFlash", LogPanic, "no valid CHIP section." );
		}

		readCRT( &chip.total_length, 4 );
		readCRT( &chip.type, 2 );
		readCRT( &chip.bank, 2 );
		readCRT( &chip.adr, 2 );
		readCRT( &chip.rom_length, 2 );

		chip.total_length = swapBytesU32( (u8*)&chip.total_length );
		chip.type = swapBytesU16( (u8*)&chip.type );
		chip.bank = swapBytesU16( (u8*)&chip.bank );
		chip.adr = swapBytesU16( (u8*)&chip.adr );
		chip.rom_length = swapBytesU16( (u8*)&chip.rom_length );

		{
			if ( chip.adr == 0x8000 )
			{
				if ( isRAW )
				{
					for ( register u32 i = 0; i < 8192; i++ )
						crt[ i ] = flash[ ( chip.bank * 8192 + i ) * 2 + 0 ];
				} else
				{
					for ( register u32 i = 0; i < 8192; i++ )
					{
						u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
						crt[ i ] = flash[ ( chip.bank * 8192 + realAdr ) * 2 + 0 ];
					}
				}

				crt += 8192;

				if ( chip.rom_length > 8192 )
				{
					if ( isRAW )
					{
						for ( register u32 i = 0; i < 8192; i++ )
							crt[ i ] = flash[ ( chip.bank * 8192 + i ) * 2 + 1 ];
					} else
					{
						for ( register u32 i = 0; i < 8192; i++ )
						{
							u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
							crt[ i ] = flash[ ( chip.bank * 8192 + realAdr ) * 2 + 1 ];
						}
					}

					crt += chip.rom_length - 8192;
				}
			} else
			{
				if ( isRAW )
				{
					for ( register u32 i = 0; i < 8192; i++ )
						crt[ i ] = flash[ ( chip.bank * 8192 + i ) * 2 + 1 ];
				} else
				{
					for ( register u32 i = 0; i < 8192; i++ )
					{
						u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
						crt[ i ] = flash[ ( chip.bank * 8192 + realAdr ) * 2 + 1 ];
					}
				}

				crt += 8192;
			}
		}

		if ( chip.bank > nBanks )
			nBanks = chip.bank;
	}


	// write file
	result = f_open( &file, FILENAME, FA_CREATE_ALWAYS | FA_WRITE );
	if ( result != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot open file: %s", FILENAME );

	// read data in one big chunk
	u32 nBytesWritten;
	result = f_write( &file, rawCRT, nBytesRead, &nBytesWritten );

	if ( result != FR_OK || nBytesWritten != nBytesRead )
		logger->Write( "RaspiFlash", LogError, "Write error" );

	if ( f_close( &file ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot close file" );

	// unmount file system
	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
		logger->Write( "RaspiFlash", LogPanic, "Cannot unmount drive: %s", DRIVE );
}


int checkCRTFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *error )
{
	CRT_HEADER header;

	*error = 0;
	int res = readCRTHeader( logger, &header, DRIVE, FILENAME );

	if ( res == 0 )
	{
		switch ( header.type ) {
		case 32: //	EasyFlash CRT
		case 19: // MagicDesk CRT
		case 0:  // normal cartridge
			return 5;	
		case 3:	 // Final Cartridge 3 CRT
			return 60;
		case 9:
		case 1:  // Action Replay 4.2/5/6/7 or AtomicPower/NordicPower CRT
			return 70;
		default: //unknown CRT-type
			*error = 1;
			return 0;
		}
	} else
	if ( res == -1 )
	{
		*error = 2; // "no .CRT file"
		return 0;
	}

	*error = 3; // "error reading file"
	return 0;
}
