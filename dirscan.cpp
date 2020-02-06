/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 dirscan.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - code for reading/parsing D64 files
 Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

 .d64 reader below adapted from d642prg V2.09, original source (C)Covert Bitops, (C)2003/2009 by iAN CooG/HokutoForce^TWT^HVSC

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
#include "dirscan.h"
#include "linux/kernel.h"

DIRENTRY dir[ MAX_DIR_ENTRIES ];
s32 nDirEntries;

#define DIRSECTS  18

u32 nSectorTable[] =
{
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
	19, 19, 19, 19, 19, 19, 19,
	18, 18, 18, 18, 18, 18,
	17, 17, 17, 17, 17,
	17, 17, 17, 17, 17,
	21, 21, 21, 21, 21, 21, 21, 21, 21,
	21, 21, 21, 21, 21, 21, 21, 21,
	19, 19, 19, 19, 19, 19, 19,
	18, 18, 18, 18, 18, 18,
	17, 17, 17, 17, 17
};

u32 firstSectorTable[ 70 ];

u32 d64GetOffset( u32 track, u32 sector )
{
    return ( firstSectorTable[ track - 1 ] + sector ) << 8;
}

u32 getTracks( u32 d64size )
{
	if ( d64size == 174848 ) return 35; 
	if ( d64size == 175531 ) return 35; 
	if ( d64size == 196608 ) return 40; 
	if ( d64size == 197376 ) return 40; 
	if ( d64size == 205312 ) return 42; 
	if ( d64size == 206114 ) return 42; 
	if ( d64size == 349696 ) return 70; 
	if ( d64size == 351062 ) return 70; 
	if ( d64size == 819200 ) return 80; 
	if ( d64size == 822400 ) return 80; 
	return 0xff;
}

int d64ParseExtract( u8 *d64buf, u32 d64size, u32 job, u8 *dst, s32 *s, u32 parent )
{
	u32 nTracks = getTracks( d64size );

	// unknown format or d81 (not yet supported)
	if ( nTracks > 70 ) return 1;

	for ( u32 t = 0, s = 0; t < nTracks; t++ )
	{
		firstSectorTable[ t ] = s;
		s += nSectorTable[ t ];
	}

	if ( job & D64_GET_HEADER )
	{
		strncpy( (char*)dst, (const char*)&d64buf[ d64GetOffset( 18, 0 ) + 0x90 ], 23 );
		dst[ 16 ] = dst[ 17 ] = '\"';
		dst[ 23 ] = 0;
		return 0;
	}

	u8 dirsectors[ DIRSECTS + 1 ];
	dirsectors[ 0 ] = 0x01;
	dirsectors[ DIRSECTS ] = dirsectors[ 0 ];

	u8 *ptr = &d64buf[ d64GetOffset( 18, 1 ) ];
	u32 ofs = 2;

	u32 fileIndex = 0;

	while ( true )
	{
		fileIndex ++;

		if ( job & D64_GET_DIR )
		{
			char types[][ 6 ] = { " DEL ", " SEQ ", " PRG ", " USR ", " REL ", " ??? " };

			u8 *fileInfo = &ptr[ ofs ];

			char fln2[ 17 + 6 ];
			strncpy( fln2, (const char*)&fileInfo[ 3 ], 16 );
			fln2[ 16 ] = 0;

			u32 nt = min( *fileInfo & 7, 5 );

			if ( !( *fileInfo & 0x80 ) )
				types[ nt ][ 0 ] = '*';
			if ( ( *fileInfo & 0x40 ) )
				types[ nt ][ 4 ] = '<';

			u32 blk = fileInfo[ 28 ] | ( fileInfo[ 29 ] << 8 );

			if ( fileInfo[ 3 ] || blk )
			{
				strcat( fln2, " " );
				strcat( fln2, types[ nt ] );
				DIRENTRY *d = &((DIRENTRY *)dst)[ *s ];
				sprintf( (char*)d->name, "%3d %s", blk, fln2 );

				strcpy( (char*)&d->name[128], fln2 );
				d->f = DIR_FILE_IN_D64 | ( nt << SHIFT_TYPE ) | fileIndex;
				d->parent = parent;
				(*s) ++;
			}
		}

		if ( ( job & D64_GET_FILE ) && fileIndex == ( job & ( ( 1 << SHIFT_TYPE ) - 1 ) ) && ( ptr[ ofs ] & 7 ) < 6 )
		{
			u32 c = 0;
			*s = 0;

			// invalid track or sector?
			if ( ( ptr[ ofs + 1 ] < 1 ) || ( ptr[ ofs + 1 ] > nTracks ) || ( ptr[ ofs + 2 ] >= nSectorTable[ (int)( ptr[ ofs + 1 ] - 1 ) ] ) )
				return 1;

			// found on track ptr[ ofs + 1 ], sector ptr[ ofs + 2 ]
			ptr = &d64buf[ d64GetOffset( ptr[ ofs + 1 ], ptr[ ofs + 2 ] ) ];
			while ( c < d64size )
			{
				if ( ptr[ 0 ] )
				{
					memcpy( &dst[ *s ], &ptr[ 2 ], 254 );
					*s += 254; c += 254;
					ptr = &d64buf[ d64GetOffset( ptr[ 0 ], ptr[ 1 ] ) ];
				} else
				{
					if ( ptr[ 1 ] )
					{
						memcpy( &dst[ *s ], &ptr[ 2 ], ptr[ 1 ] - 1 );
						*s += ptr[ 1 ] - 1; c += ptr[ 1 ] - 1;
					}
					break;
				}
			}
			if ( c < d64size )
				return 0; // file extracted successfully

			return 1;
		}

		ofs += 32;
		if ( ofs >= 256 )
		{
			if ( ptr[ 0 ] == 0x12 )
			{
				for ( u32 c = 0; c < DIRSECTS; c++ )
				{
					if ( dirsectors[ c ] == 0 ) // proceed
					{
						dirsectors[ c ] = dirsectors[ DIRSECTS ] = ptr[ 1 ];
						break;
					}
					if ( dirsectors[ c ] == ptr[ 1 ] ) // recursive directory -> stop
					{
						ptr[ 0 ] = 0;
						break;
					}
				}
			} else
				ptr[ 0 ] = 0; // invalid link 

			if ( ptr[ 0 ] )
			{
				ptr = &d64buf[ d64GetOffset( ptr[ 0 ], ptr[ 1 ] ) ];
				ofs = 2;

				if ( ( ptr[ 0 ] && ptr[ 0 ] > 0x12 ) || ( ptr[ 1 ] > 0x12 && ptr[ 1 ] < 0xff ) )
					return 1; // invalid link 
			} else
				return 1;
		}
	}
}


extern CLogger *logger;

u8 d64buf[ 1024 * 1024 ];

int readD64File( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size )
{
	// get filesize
	FILINFO info;
	u32 res = f_stat( FILENAME, &info );
	u32 filesize = (u32)info.fsize;

	if ( filesize > 822400 )
		return 0;

	// open file
	FIL file;
	res = f_open( &file, FILENAME, FA_READ | FA_OPEN_EXISTING );
	if ( res != FR_OK )
	{
		//logger->Write( "RaspiMenu", LogNotice, "Cannot open file: %s", FILENAME );
		return 0;
	}

	// read data in one big chunk
	u32 nBytesRead;
	res = f_read( &file, data, filesize, &nBytesRead );

	*size = nBytesRead;

	if ( res != FR_OK )
		logger->Write( "RaspiMenu", LogError, "Read error" );

	if ( f_close( &file ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot close file" );

	return 1;
}


void readDirectory( const char *DIRPATH, DIRENTRY *d, s32 *n, u32 parent = 0xffffffff, u32 level = 0 )
{
	char temp[ 4096 ];

	DIR dir;
	FILINFO FileInfo;
	FRESULT res = f_findfirst( &dir, &FileInfo, DIRPATH, "*" );

	for ( u32 i = 0; res == FR_OK && FileInfo.fname[ 0 ]; i++ )
	{
		if ( ( FileInfo.fattrib & ( AM_DIR ) ) )
		{
			strcpy( temp, DIRPATH );
			strcat( temp, "\\" );
			strcat( temp, FileInfo.fname );

			strcpy( (char*)d[ *n ].name, FileInfo.fname );
			d[ *n ].f = DIR_DIRECTORY;
			d[ *n ].parent = parent;
			d[ *n ].level = level;
			u32 curIdx = *n;
			( *n )++;

			readDirectory( temp, d, n, curIdx, level + 1 );

			d[ curIdx ].next = *n;
		} else
		{
			if ( !( FileInfo.fattrib & ( AM_HID | AM_SYS ) ) )
			{
				strcpy( (char*)d[ *n ].name, FileInfo.fname );

				if ( strstr( FileInfo.fname, ".d64" ) > 0 || strstr( FileInfo.fname, ".D64" ) > 0 ||
					 strstr( FileInfo.fname, ".d71" ) > 0 || strstr( FileInfo.fname, ".D71" ) > 0 )
				{
					strcpy( temp, DIRPATH );
					strcat( temp, "\\" );
					strcat( temp, FileInfo.fname );

					u32 imgsize = 0;
					if ( !readD64File( logger, "", temp, d64buf, &imgsize ) )
					{
						logger->Write( "RaspiMenu", LogPanic, "-> error loading file %d", temp );
					}

					u32 parentOfD64Files = *n;

					d[ *n ].f = DIR_D64_FILE | ( 5 << SHIFT_TYPE );
					d[ *n ].parent = parent;
					d[ *n ].level = level;
					( *n )++;

					d[ *n ].f = DIR_FILE_IN_D64 | ( 5 << SHIFT_TYPE );
					d[ *n ].parent = parentOfD64Files;
					d[ *n ].level = level + 1;

					char header[ 32 ] = { 0 };
					if ( d64ParseExtract( d64buf, imgsize, D64_GET_HEADER, (u8*)header ) == 0 )
						strcpy( (char*)d[ *n ].name, header );
					( *n )++;

					u32 curIdx = *n;

					d64ParseExtract( d64buf, imgsize, D64_GET_DIR, (u8*)d, n );

					for ( s32 i = curIdx; i < *n; i++ )
					{
						d[ i ].level = level + 1;
						d[ i ].parent = parentOfD64Files;
					}

					d[ curIdx - 2 ].next = *n;

				} 
				if ( strstr( FileInfo.fname, ".crt" ) > 0 || strstr( FileInfo.fname, ".CRT" ) > 0 )
				{
					d[ *n ].f = DIR_CRT_FILE;
					d[ *n ].parent = parent;
					d[ *n ].level = level;
					( *n )++;
				} 
				if ( strstr( FileInfo.fname, ".prg" ) > 0 || strstr( FileInfo.fname, ".PRG" ) > 0 )
				{
					d[ *n ].f = DIR_PRG_FILE;
					d[ *n ].parent = parent;
					d[ *n ].level = level;
					( *n )++;
				}
				if ( strstr( FileInfo.fname, ".bin" ) > 0 || strstr( FileInfo.fname, ".bin" ) > 0 )
				{
					d[ *n ].f = DIR_PRG_FILE;
					d[ *n ].parent = parent;
					d[ *n ].level = level;
					( *n )++;
				}
			}
		}

		res = f_findnext( &dir, &FileInfo );
	}
}

void scanDirectories( char *DRIVE )
{
	FATFS m_FileSystem;

	// mount file system
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

	u32 head;
	nDirEntries = 0;

	#define APPEND_SUBTREE( NAME, PATH  )					\
		head = nDirEntries ++;								\
		strcpy( (char*)dir[ head ].name, NAME );			\
		dir[ head ].f = DIR_DIRECTORY;						\
		dir[ head ].parent = 0xffffffff;					\
		readDirectory( PATH, dir, &nDirEntries, head, 1 );	\
		dir[ head ].next = nDirEntries;

	APPEND_SUBTREE( "CRT", "SD:CRT" )
	APPEND_SUBTREE( "D64", "SD:D64" )
	APPEND_SUBTREE( "PRG", "SD:PRG" )

	// unmount file system
	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );
}

