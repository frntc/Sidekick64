/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 crt.cpp

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
#include "crt.h"

#define CONSOLE_DEBUG

u8 tempRAWCRTBuffer[ 1032 * 1024 ];

static u8 *rawCRT = tempRAWCRTBuffer;

u32 swapBytesU32( u8 *buf )
{
	return buf[ 3 ] | ( buf[ 2 ] << 8 ) | ( buf[ 1 ] << 16 ) | ( buf[ 0 ] << 24 );
}

u16 swapBytesU16( u8 *buf )
{
	return buf[ 1 ] | ( buf[ 0 ] << 8 );
}

#define min( a, b ) ((a)<(b)?(a):(b))

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
	memset( rawCRT, 0, filesize );
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

	bool isC64Cartridge   = memcmp( CRT_HEADER_SIG, header.signature, 16 ) == 0;
	bool isVIC20Cartridge = memcmp( CRT_HEADER_SIG20, header.signature, 16 ) == 0;

	if ( !isC64Cartridge && !isVIC20Cartridge )
	//if ( memcmp( CRT_HEADER_SIG, header.signature, 16 ) )
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

	if ( filesize > 1032 * 1024 )
		filesize = 1032 * 1024;

	// read data in one big chunk
	u32 nBytesRead;
	memset( rawCRT, 0, filesize );
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

	bool isC64Cartridge   = memcmp( CRT_HEADER_SIG, header.signature, 16 ) == 0;
	bool isVIC20Cartridge = memcmp( CRT_HEADER_SIG20, header.signature, 16 ) == 0;

	if ( !isC64Cartridge && !isVIC20Cartridge )
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

	if ( isC64Cartridge )
	{
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
		case 7:
			*bankswitchType = BS_FUNPLAY;
			*ROM_LH = bROML;
			break;
		case 43:
			*bankswitchType = BS_PROPHET;
			*ROM_LH = bROML;
			break;
		case 57:
			if ( header.reserved[ 0 ] == 0 )
				*bankswitchType = BS_RGCD; else
				*bankswitchType = BS_HUCKY; 
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
			*bankswitchType = BS_ATOMICPOW;
			*ROM_LH = bROML;
			break;
		case 15:
			*bankswitchType = BS_C64GS;
			*ROM_LH = bROML;
			break;
		case 60:
			*bankswitchType = BS_GMOD2;
			*ROM_LH = bROML;
			break;
		case 18:
			*bankswitchType = BS_ZAXXON;
			*ROM_LH = bROML;
			break;
		case 5:
			*bankswitchType = BS_OCEAN;
			*ROM_LH = bROML;
			break;
		case 21:
			*bankswitchType = BS_COMAL80;
			*ROM_LH = bROML | bROMH;
			break;
		case 10:
			*bankswitchType = BS_EPYXFL;
			*ROM_LH = bROML;
			break;
		case 4:
			*bankswitchType = BS_SIMONSBASIC;
			*ROM_LH = bROML;
			break;
		case 17:
			*bankswitchType = BS_DINAMIC;
			*ROM_LH = bROML;
			break;
		case 45:
		case 77:
			*bankswitchType = BS_FREEZEFRAME;
			*ROM_LH = bROML;
			break;
		case 16:
			*bankswitchType = BS_WARPSPEED;
			*ROM_LH = bROML;
			break;
		case 46:
			*bankswitchType = BS_FREEZEMACHINE;
			*ROM_LH = bROML;
			break;
		case 0:
		default:
			*bankswitchType = BS_NONE;
			*ROM_LH = 0;
			break;
		}
	} else
	if ( isVIC20Cartridge )
	{
		*bankswitchType = BS_NONE;
		*ROM_LH = 0;
		*nBanks = 0;
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

		if ( isVIC20Cartridge )
		{
			if ( header.type == 0 ) // standard 8/16kb cart: we will write the ROM data directly at the VIC20-RAM/ROM position
			{
				u32 nBytes = chip.rom_length;

				#define VIC20_READ_WRITE		(1)
				#define VIC20_READ_ONLY 		(2)
				#define VIC20_RAMx_DISABLED		(0<<0)
				#define VIC20_RAMx_READ_WRITE	(1<<0)
				#define VIC20_RAMx_READ_ONLY	(2<<0)
				#define VIC20_BLK1(m)			((m)<<2)
				#define VIC20_BLK1_DISABLED		(0<<2)
				#define VIC20_BLK1_READ_WRITE	(1<<2)
				#define VIC20_BLK1_READ_ONLY	(2<<2)
				#define VIC20_BLK2(m)			((m)<<4)
				#define VIC20_BLK2_DISABLED		(0<<4)
				#define VIC20_BLK2_READ_WRITE	(1<<4)
				#define VIC20_BLK2_READ_ONLY	(2<<4)
				#define VIC20_BLK3(m)			((m)<<6)
				#define VIC20_BLK3_DISABLED		(0<<6)
				#define VIC20_BLK3_READ_WRITE	(1<<6)
				#define VIC20_BLK3_READ_ONLY	(2<<6)
				#define VIC20_BLK5(m)			((m)<<8)
				#define VIC20_BLK5_DISABLED		(0<<8)
				#define VIC20_BLK5_READ_WRITE	(1<<8)
				#define VIC20_BLK5_READ_ONLY	(2<<8)

				u32 blkShift;
				switch ( chip.adr )
				{
				default:		blkShift = 0; break;
				case 0x2000:	blkShift = 2; break;
				case 0x4000:	blkShift = 4; break;
				case 0x6000:	blkShift = 6; break;
				case 0xA000:	blkShift = 8; break;
				}

				if ( chip.type == 0 )
					*ROM_LH |= VIC20_READ_ONLY << blkShift; else	// ROM
					*ROM_LH |= VIC20_BLK1_READ_WRITE << blkShift;	// RAM or flash 					

				(*nBanks) ++;

				for ( register u32 i = 0; i < nBytes; i++ )
					flash[ chip.adr + i ] = crt[ i ];

				crt += nBytes;
			}
		} else
		if ( isC64Cartridge )
		{
			// MagicDesk and some others only use the low-bank
			if ( (*bankswitchType) == BS_MAGICDESK || 
				 (*bankswitchType) == BS_C64GS || 
				 (*bankswitchType) == BS_FUNPLAY || 
				 (*bankswitchType) == BS_PROPHET || 
				 (*bankswitchType) == BS_OCEAN || 
				 (*bankswitchType) == BS_GMOD2 || 
				 (*bankswitchType) == BS_HUCKY || 
				 (*bankswitchType) == BS_RGCD || 
				 (*bankswitchType) == BS_FREEZEFRAME || 
				 (*bankswitchType) == BS_FREEZEMACHINE || 
				 (*bankswitchType) == BS_WARPSPEED || 
				 header.type == 36 /* Retro Replay */ )
			{
				*ROM_LH = bROML;

				u32 nBytes = 8192;
				if ( chip.rom_length > 8192 )
				{
					nBytes = chip.rom_length;

					//if ( getRAW )
					{
						for ( register u32 i = 0; i < nBytes; i++ )
							flash[ chip.bank * chip.rom_length + i ] = crt[ i ];
					} 
				} else
				{
					nBytes = min( 8192, chip.rom_length );

					if ( getRAW )
					{
						for ( register u32 i = 0; i < nBytes; i++ )
							flash[ chip.bank * 8192 + i ] = crt[ i ];
					} else
					{
						for ( register u32 i = 0; i < nBytes; i++ )
						{
							u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
							flash[ chip.bank * 8192 + realAdr ] = crt[ i ];
						}
					}
				}

				crt += nBytes;
			} else
			{
				if ( chip.adr == 0x8000 )
				{
					*ROM_LH |= bROML;

					u32 nBytes = min( 8192, chip.rom_length );

					if ( getRAW )
					{
						//logger->Write( "RaspiFlash", LogNotice, "bank=%d, bytes=%d", chip.bank, chip.rom_length );
						for ( register u32 i = 0; i < nBytes; i++ )
							flash[ ( chip.bank * 8192 + i ) * 2 + 0 ] = crt[ i ];
					} else
					{
						for ( register u32 i = 0; i < nBytes; i++ )
						{
							u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
							flash[ ( chip.bank * 8192 + realAdr ) * 2 + 0 ] = crt[ i ];
						}
					}

					crt += nBytes;

					if ( chip.rom_length > 8192 )
					{
						*ROM_LH |= bROMH;

						nBytes = min( 8192, chip.rom_length - 8192 );

						if ( getRAW )
						{
							for ( register u32 i = 0; i < nBytes; i++ )
								flash[ ( chip.bank * 8192 + i ) * 2 + 1 ] = crt[ i ];
						} else
						{
							for ( register u32 i = 0; i < nBytes; i++ )
							{
								u32 realAdr = ( ( i & 255 ) << 5 ) | ( ( i >> 8 ) & 31 );
								flash[ ( chip.bank * 8192 + realAdr ) * 2 + 1 ] = crt[ i ];
							}
						}

						crt += nBytes;
					}
				} else
				{
					u32 ofs = 0;
					// todo: calculate offset correctly!
					if ( chip.adr == 0xf000 || chip.adr == 0xb000 )
						ofs = 4096;

					*ROM_LH |= bROMH;

					if ( getRAW )
					{
						for ( register u32 i = 0; i < 8192 - ofs; i++ )
							flash[ ( chip.bank * 8192 + i + ofs ) * 2 + 1 ] = crt[ i ];
					} else
					{
						for ( register u32 i = 0; i < 8192 - ofs; i++ )
						{
							u32 realAdr = ( ( (i+ofs) & 255 ) << 5 ) | ( ( (i+ofs) >> 8 ) & 31 );
							flash[ ( chip.bank * 8192 + realAdr ) * 2 + 1 ] = crt[ i ];
						}
					}

					crt += 8192 - ofs;
				}
			}
		}

		if ( chip.bank > *nBanks )
			*nBanks = chip.bank;
	}

	memcpy( crtHeader, &header, sizeof( CRT_HEADER ) );
	(*nBanks) ++;
}

// .CRT reading
int  getVIC20CRTFileStartEndAddr( CLogger *logger, const char *FILENAME, u32 *addr )
{
	CRT_HEADER header;

	// get filesize
	FILINFO info;
	u32 result = f_stat( FILENAME, &info );
	u32 filesize = (u32)info.fsize;

	// open file
	FIL file;
	result = f_open( &file, FILENAME, FA_READ | FA_OPEN_EXISTING );
	if ( result != FR_OK )
	{
		logger->Write( "RaspiFlash", LogPanic, "Cannot open file: %s", FILENAME );
		return 0;
	}

	if ( filesize > 1032 * 1024 )
		filesize = 1032 * 1024;

	// read data in one big chunk
	u32 nBytesRead;
	memset( rawCRT, 0, filesize );
	result = f_read( &file, rawCRT, filesize, &nBytesRead );

	if ( result != FR_OK )
	{
		logger->Write( "RaspiFlash", LogError, "Read error" );
		return 0;
	}

	if ( f_close( &file ) != FR_OK )
	{
		logger->Write( "RaspiFlash", LogPanic, "Cannot close file" );
		return 0;
	}

	// now "parse" the file which we already have in memory
	u8 *crt = rawCRT;
	u8 *crtEnd = crt + filesize;

	readCRT( &header.signature, 16 );

	//bool isC64Cartridge   = memcmp( CRT_HEADER_SIG, header.signature, 16 ) == 0;
	bool isVIC20Cartridge = memcmp( CRT_HEADER_SIG20, header.signature, 16 ) == 0;

	if ( !isVIC20Cartridge )
	{
		return 0;
		//logger->Write( "RaspiFlash", LogPanic, "no CRT file." );
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

	/*if ( isVIC20Cartridge )
	{
		*bankswitchType = BS_NONE;
		*ROM_LH = 0;
		*nBanks = 0;
	}

	*nBanks = 0;*/

	u32 minAddr = 65536;
	u32 maxAddr = 0;

	while ( crt < crtEnd )
	{
		CHIP_HEADER chip;

		memset( &chip, 0, sizeof( CHIP_HEADER ) );

		readCRT( &chip.signature, 4 );

		if ( memcmp( CHIP_HEADER_SIG, chip.signature, 4 ) )
		{
			return 0;
			//logger->Write( "RaspiFlash", LogPanic, "no valid CHIP section." );
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

		//if ( isVIC20Cartridge )
		{
			u32 nBytes = chip.rom_length;
			if ( header.type == 0 ) // standard 8/16kb cart: we will write the ROM data directly at the VIC20-RAM/ROM position
			{
				/*u32 blkShift;
				switch ( chip.adr )
				{
				default:		blkShift = 0; break;
				case 0x2000:	blkShift = 2; break;
				case 0x4000:	blkShift = 4; break;
				case 0x6000:	blkShift = 6; break;
				case 0xA000:	blkShift = 8; break;
				}*/

				//logger->Write( "RaspiFlash", LogNotice, "blk=%x", chip.adr );
				if ( chip.adr < minAddr ) minAddr = chip.adr;
				if ( chip.adr + nBytes > maxAddr ) maxAddr = chip.adr + nBytes;

				/*if ( chip.type == 0 )
					*ROM_LH |= VIC20_READ_ONLY << blkShift; else	// ROM
					*ROM_LH |= VIC20_BLK1_READ_WRITE << blkShift;	// RAM or flash

				(*nBanks) ++;*/

			}
			crt += nBytes;
		} 

		//if ( chip.bank > *nBanks )
		//	*nBanks = chip.bank;
	}

	maxAddr --;
	*addr = minAddr + ( maxAddr << 16 );

	return 1;
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
	memset( rawCRT, 0, filesize );
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
		//logger->Write( "RaspiFlash", LogNotice, "no EF CRT" );
		// unmount file system
		if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
			logger->Write( "RaspiFlash", LogPanic, "Cannot unmount drive: %s", DRIVE );
		return;
	}

	nBanks = 0;

	//logger->Write( "RaspiFlash", LogNotice, "patching" );

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

int checkCRTFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *error, u32 *isFreezer )
{
	CRT_HEADER header;

	if ( isFreezer ) *isFreezer = 0;

	*error = 0;
	int res = readCRTHeader( logger, &header, DRIVE, FILENAME );

	if ( res == 0 )
	{
		//logger->Write( "RaspiFlash", LogNotice, "CRT type %d", header.type );
		switch ( header.type ) {
		case 32: //	EasyFlash CRT
		case 19: // MagicDesk CRT
		case 0:  // normal cartridge
		case 15: // C64 Game System
		case 60: // C64 Game System
		case 18: // Zaxxon
		case 43: // Prophet
		case 5: // Ocean
		case 21: // Comal80
		case 10: // Epyx Fastload
		case 4: // Simon's Basic
		case 17: // Dinamic
		case 57: // RGCD / Hucky
		case 7: // Funplay
			return 5;	
		case 3:	 // Final Cartridge 3 CRT
			if ( isFreezer ) *isFreezer = 1;
			return 60;
		case 2:  // KCS Power Cartridge
			if ( isFreezer ) *isFreezer = 8;
			return 61;
		case 20:  // Super Snapshot V5
			if ( isFreezer ) *isFreezer = 4;
			return 62;
		case 9:
		case 1:  // Action Replay 4.2/5/6/7 or AtomicPower/NordicPower CRT
			if ( isFreezer ) *isFreezer = 2;
			return 70;
		case 36:	// Retro Replay or Nordic Replay CRT
			if ( isFreezer ) *isFreezer = 32;
			return 71;
		case 45:	// Freeze Frame
		case 77:	// Freeze Frame
		case 46:	// Freeze Machine
			if ( isFreezer ) *isFreezer = 16;
			return 63;
			break;
		case 16:	// Warp Speed
			return 64;
			break;


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

	if ( res == -10 || res == -12 )
		*error = 8; else // "file not found"
		*error = 3; // "error reading file"
	return 0;
}

int checkCRTFileVIC20( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *error )
{
	CRT_HEADER header;

	*error = 0;
	int res = readCRTHeader( logger, &header, DRIVE, FILENAME );

	if ( res == 0 )
	{
		bool isVIC20Cartridge = memcmp( CRT_HEADER_SIG20, header.signature, 16 ) == 0;

		if ( !isVIC20Cartridge )
		{
			*error = 2;
			return 0;
		}

		if ( header.type != 0 )
		{
			*error = 1;
			return 0;
		}

		return 5;
	} else
	if ( res == -1 )
	{
		*error = 2; // "no .CRT file"
		return 0;
	}

	*error = 3; // "error reading file"
	return 0;
}
