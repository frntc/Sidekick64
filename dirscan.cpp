/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 dirscan.cpp

 Sidekick64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
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
#include <circle/util.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

int d64ParseExtract( u8 *d64buf, u32 d64size, u32 job, u8 *dst, s32 *s, u32 parent, u32 *nFiles )
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

	if ( job & D64_COUNT_FILES && nFiles )
		*nFiles = 0;

	while ( true )
	{
		fileIndex ++;

		if ( job & D64_COUNT_FILES )
		{
			u8 *fileInfo = &ptr[ ofs ];
			u32 blk = fileInfo[ 28 ] | ( fileInfo[ 29 ] << 8 );

			if ( fileInfo[ 3 ] || blk )
			{
				if ( nFiles )
					(*nFiles) ++;
			}
		}

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
				d->size = blk * 254;
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

int compareEntries( const void *e1, const void *e2 )
{
	DIRENTRY *a = (DIRENTRY*)e1;
	DIRENTRY *b = (DIRENTRY*)e2;

	if ( a->f && !b->f ) return -1;
	if ( b->f && !a->f ) return 1;

	return strcasecmp( (char*)a->name, (char*)b->name );
}

void quicksort( DIRENTRY *begin, DIRENTRY *end )
{
	DIRENTRY *ptr = begin, *split = begin + 1;
	if ( end - begin < 1 ) return;
	while ( ++ptr <= end ) {
		if ( compareEntries( ptr, begin ) < 0 ) {
			DIRENTRY tmp = *ptr; *ptr = *split; *split = tmp;
			++split;
		}
	}
	DIRENTRY tmp = *begin; *begin = *( split - 1 ); *( split - 1 ) = tmp;
	quicksort( begin, split - 1 );
	quicksort( split, end );
}

void readDirectory( int mode, const char *DIRPATH, DIRENTRY *d, s32 *n, u32 parent = 0xffffffff, u32 level = 0, u32 takeAll = 0, u32 *nAdded = NULL )
{
	char temp[ 4096 ];

	DIRENTRY sort[ 2048 ];
	u32 sortCur = 0;

	if ( parent != 0xffffffff )
		d[ parent ].f |= DIR_SCANNED;

	if ( mode > 0 && parent != 0xffffffff ) // insert
	{
		DIR dir;
		FILINFO FileInfo;
		FRESULT res = f_findfirst( &dir, &FileInfo, DIRPATH, "*" );

		if ( res != FR_OK )
			logger->Write( "read directory", LogNotice, "error opening dir" );

		int nAdditionalEntries = 0;

		for ( u32 i = 0; res == FR_OK && FileInfo.fname[ 0 ]; i++ )
		{
			{
				//sprintf( sPath, "%s\\%s", sDir, FileInfo.fname );
				//logger->Write( "insert", LogNotice, "file '%s'", FileInfo.fname );

				// folder? 
				if ( ( FileInfo.fattrib & ( AM_DIR ) ) )
				{
					strcpy( (char*)sort[sortCur].name, FileInfo.fname );
					sort[ sortCur ].level = FileInfo.fattrib;
					sort[ sortCur ].size = 0;
					sort[ sortCur++ ].f = 1;
					nAdditionalEntries ++;
				} else
				{
					if ( strstr( FileInfo.fname, ".crt" ) > 0 || strstr( FileInfo.fname, ".CRT" ) > 0 ||
						 strstr( FileInfo.fname, ".georam" ) > 0 || strstr( FileInfo.fname, ".GEORAM" ) > 0 || 
						 strstr( FileInfo.fname, ".prg" ) > 0 || strstr( FileInfo.fname, ".PRG" ) > 0 || 
						 strstr( FileInfo.fname, ".sid" ) > 0 || strstr( FileInfo.fname, ".SID" ) > 0 || 
						 strstr( FileInfo.fname, ".bin" ) > 0 || strstr( FileInfo.fname, ".BIN" ) > 0 ||
						 strstr( FileInfo.fname, ".rom" ) > 0 || strstr( FileInfo.fname, ".ROM" ) > 0 )
					{
						strcpy( (char*)sort[sortCur].name, FileInfo.fname );
						sort[ sortCur ].level = FileInfo.fattrib;
						sort[ sortCur ].size = FileInfo.fsize;
						sort[ sortCur++ ].f = 0;
						nAdditionalEntries ++;
					}

					if ( strstr( FileInfo.fname, ".d64" ) > 0 || strstr( FileInfo.fname, ".D64" ) > 0 || 
						 strstr( FileInfo.fname, ".d71" ) > 0 || strstr( FileInfo.fname, ".D71" ) > 0 )
					{
						strcpy( (char*)sort[sortCur].name, FileInfo.fname );
						sort[ sortCur ].level = FileInfo.fattrib;
						sort[ sortCur ].size = 0; //FileInfo.fsize;
						sort[ sortCur++ ].f = 1;

						strcpy( temp, DIRPATH );
						strcat( temp, "\\" );
						strcat( temp, FileInfo.fname );

						u32 imgsize = 0;
						if ( !readD64File( logger, "", temp, d64buf, &imgsize ) )
						{
							logger->Write( "RaspiMenu", LogPanic, "-> error loading file %d", temp );
						}

						u32 nFiles;

						d64ParseExtract( d64buf, imgsize, D64_COUNT_FILES, (u8*)d, n, 0xffffffff, &nFiles );
						nAdditionalEntries += nFiles + 2; // .d64 filename + disk header
					} 
				}
			}
			res = f_findnext( &dir, &FileInfo );
		}

		f_closedir( &dir );


		if ( !nAdditionalEntries )
			return;

		//qsort( &sort[ 0 ], sortCur, sizeof( DIRENTRY ), compareEntries );
		quicksort( &sort[ 0 ], &sort[ sortCur - 1 ] );

		//logger->Write( "insert", LogNotice, "additional entries %d", nAdditionalEntries );

		for ( u32 i = nDirEntries - 1; i >= parent + 1; i-- )
		{
			if ( d[ i ].parent != 0xffffffff && d[ i ].parent > parent )
				d[ i ].parent += nAdditionalEntries;
			if ( d[ i ].next != 0 )
				d[ i ].next += nAdditionalEntries;
			d[ i + nAdditionalEntries ] = d[ i ];
		}

		// traverse all parents of node given by "parent" and increase their next-indices
		u32 p = d[ parent ].parent;
		while ( p != 0xffffffff )
		{
			d[ p ].next += nAdditionalEntries;
			p = d[ p ].parent;
		}

		if ( nAdded )
			*nAdded = nAdditionalEntries;
	}


/*	DIR dir;
	FILINFO FileInfo;
	FRESULT res = f_findfirst( &dir, &FileInfo, DIRPATH, "*" );

	for ( u32 i = 0; res == FR_OK && FileInfo.fname[ 0 ]; i++ )
	{*/
	u32 pos = 0;

	do {
		d[*n].size = sort[ pos ].size;

		// file or folder?
		if ( sort[ pos ].level & AM_DIR )
		{
			strcpy( (char*)d[*n].name, (char*)sort[ pos ].name );
			d[*n].f = DIR_DIRECTORY;
			if ( takeAll )
				d[ *n ].f |= DIR_LISTALL;
			d[*n].parent = parent;
			d[*n].level = level;
			d[*n].next = 1 + *n; (*n) ++;
		} else
		{
			strcpy( (char*)d[*n].name, (char*)sort[ pos ].name );

			if ( strstr( (char*)sort[ pos ].name, ".d64" ) > 0 || strstr( (char*)sort[ pos ].name, ".D64" ) > 0 ||
			 	 strstr( (char*)sort[ pos ].name, ".d71" ) > 0 || strstr( (char*)sort[ pos ].name, ".D71" ) > 0 )
			{
				strcpy( temp, DIRPATH );
				strcat( temp, "\\" );
				strcat( temp, (char*)sort[ pos ].name );

				u32 imgsize = 0;
				if ( !readD64File( logger, "", temp, d64buf, &imgsize ) )
				{
					logger->Write( "RaspiMenu", LogPanic, "-> error loading file %d", temp );
				}

				u32 parentOfD64Files = *n;
				d[*n].f = DIR_D64_FILE  | ( 5 << SHIFT_TYPE );
				d[*n].parent = parent;
				d[*n].level = level;
				(*n) ++;

				d[*n].f = DIR_FILE_IN_D64  | ( 5 << SHIFT_TYPE );
				d[*n].parent = parentOfD64Files;
				d[*n].level = level + 1;

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

			} else
			if ( strstr( (char*)sort[ pos ].name, ".crt" ) > 0 || strstr( (char*)sort[ pos ].name, ".CRT" ) > 0 )
			{
				d[ *n ].f = DIR_CRT_FILE;
				d[ *n ].parent = parent;
				d[ *n ].level = level;
				( *n )++;
			} else 
			if ( strstr( (char*)sort[ pos ].name, ".georam" ) > 0 || strstr( (char*)sort[ pos ].name, ".GEORAM" ) > 0 )
			{
				d[ *n ].f = DIR_CRT_FILE;
				d[ *n ].parent = parent;
				d[ *n ].level = level;
				( *n )++;
			} else
			if ( strstr( (char*)sort[ pos ].name, ".prg" ) > 0 || strstr( (char*)sort[ pos ].name, ".PRG" ) > 0 )
			{
				d[ *n ].f = DIR_PRG_FILE;
				d[ *n ].parent = parent;
				d[ *n ].level = level;
				( *n )++;
			} else
			if ( strstr( (char*)sort[ pos ].name, ".sid" ) > 0 || strstr( (char*)sort[ pos ].name, ".SID" ) > 0 )
			{
				d[ *n ].f = DIR_SID_FILE;
				d[ *n ].parent = parent;
				d[ *n ].level = level;
				( *n )++;
			} else
			if ( strstr( (char*)sort[ pos ].name, ".bin" ) > 0 || strstr( (char*)sort[ pos ].name, ".bin" ) > 0 )
			{
				d[ *n ].f = DIR_PRG_FILE;
				d[ *n ].parent = parent;
				d[ *n ].level = level;
				( *n )++;
			} else
			if ( strstr( (char*)sort[ pos ].name, ".rom" ) > 0 || strstr( (char*)sort[ pos ].name, ".ROM" ) > 0 || takeAll == 1 || (d[ parent ].f & DIR_LISTALL) )
			{
				d[ *n ].f = DIR_CRT_FILE;
				d[ *n ].parent = parent;
				d[ *n ].level = level;
				( *n )++;
			} 
		}

	} while ( ++pos < sortCur );
}

void insertDirectoryContents( int node, char *basePath, int listAll )
{
	s32 tempEntries = dir[ node ].next;
	u32 nAdded = 0;
	char path[ 2048 ];
	sprintf( path, "%s%s", basePath, dir[ node ].name );

	// mount file system
	FATFS m_FileSystem;
	if ( f_mount( &m_FileSystem, "SD:", 1 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: SD:" );

	readDirectory( 1, path, dir, &tempEntries, node, dir[ node ].level + 1, listAll, &nAdded );	

	// unmount file system
	if ( f_mount( 0, "SD:", 0 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: SD:" );

	nDirEntries += tempEntries - dir[ node ].next;
	dir[ node ].next += nAdded;
}

void scanDirectories( char *DRIVE )
{
	FATFS m_FileSystem;

	// mount file system
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

	u32 head = 0;
	nDirEntries = 0;

	#define APPEND_SUBTREE( NAME, PATH, ALL )						\
		head = nDirEntries ++;										\
		strcpy( (char*)dir[ head ].name, NAME );					\
		dir[ head ].f = DIR_DIRECTORY | (ALL?DIR_LISTALL:0);		\
		dir[ head ].parent = 0xffffffff;							\
		readDirectory( 0, PATH, dir, &nDirEntries, head, 1, ALL );	\
		if ( nDirEntries == (s32)head + 1 ) nDirEntries --; else	\
		dir[ head ].next = nDirEntries;

	#define APPEND_SUBTREE_UNSCANNED( NAME, PATH, ALL )				\
		head = nDirEntries ++;										\
		strcpy( (char*)dir[ head ].name, NAME );					\
		dir[ head ].f = DIR_DIRECTORY | (ALL?DIR_LISTALL:0);		\
		dir[ head ].parent = 0xffffffff;							\
		dir[ head ].next = nDirEntries;

	APPEND_SUBTREE_UNSCANNED( "CRT", "SD:CRT", 0 )
	APPEND_SUBTREE_UNSCANNED( "D64", "SD:D64", 0 )
	APPEND_SUBTREE_UNSCANNED( "PRG", "SD:PRG", 0 )
	APPEND_SUBTREE_UNSCANNED( "SID", "SD:SID", 0 )
	APPEND_SUBTREE_UNSCANNED( "PRG128", "SD:PRG128", 0 )
	APPEND_SUBTREE_UNSCANNED( "CART128", "SD:CART128", 0 )

	//insertDirectoryContents( 0, "SD:" );

	// unmount file system
	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );
}

void scanDirectories264( char *DRIVE )
{
	FATFS m_FileSystem;

	// mount file system
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

	u32 head = 0;
	nDirEntries = 0;

	APPEND_SUBTREE( "D264", "SD:D264", 0 )
	APPEND_SUBTREE( "PRG264", "SD:PRG264", 0 )

	// unmount file system
	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );
}

