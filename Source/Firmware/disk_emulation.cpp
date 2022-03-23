/*
  _________.__    .___      __   .__        __        _________   ________   _____
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  |
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   |
        \/         \/    \/     \/       \/     \/            \/       \/      |__|

 disk_emulation.cpp

 Sidekick64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
			- code for rudimentary emulation of a floppy disk drive

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

#include <stddef.h>
#include <stdio.h>
#include "dirscan.h"
#include "disk_emulation.h"

extern int cursorPos;
extern u8 d64buf[ 1024 * 1024 ];

extern void insertDirectoryContents( int node, char *basePath, int takeAll );
extern int readD64File( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size );

static s32 dirPos = 0;
static s32 dirCurrent = -1;
static u32 dirParent = 0xffffffff;
static bool menuFile = false;

static char FILENAME_TEMP[24];

static u32 getParent( s32 pos )
{
	if ( pos >= 0 && pos < nDirEntries )
	{
		return dir[ pos ].parent;
	}

	return 0xffffffff;
}

void initDiskEmulation( const char *FILENAME )
{
	dirCurrent = cursorPos;
	dirParent = getParent( cursorPos );
	menuFile = FILENAME[ 0 ] != 0;
}

static void rewindDir( void )
{
	dirPos = 0;
}

static bool readDir( DIRENTRY **entry )
{
	while ( dirPos >= 0 && dirPos < nDirEntries )
	{
		*entry = &dir[ dirPos++ ];
		if ( (*entry)->parent == dirParent )
		{
			return true;
		}
	}

	return false;
}

static void buildPath( u32 c, char *path, s32 stopPath )
{
	s32 n = 0;
	u32 nodes[ 256 ];

	nodes[ n ++ ] = c;
	while ( dir[ c ].parent != 0xffffffff )
	{
		c = nodes[ n ++ ] = dir[ c ].parent;
	}

	strcpy( path, "SD:" );
	for ( s32 i = n - 1; i >= stopPath; i -- )
	{
		if ( i != n - 1 )
		{
			strcat( path, "\\" );
		}

		strcat( path, (char*)dir[ nodes[ i ] ].name );
	}
}

static char asciiToPetscii( char c )
{
	if ( c >= 'a' && c <= 'z' )
	{
		c -= 0x20;
	}
	else if ( c == '_' )
	{
		c = (char)0xa4;
	}

	return c;
}

static const char * getFilename( DIRENTRY *entry )
{
	char *str = (char *)entry->name;
	bool convert = true;

	if ( entry->f & DIR_FILE_IN_D64 )
	{
		str = (char *)&entry->name[128];
		convert = false;
	}

	for ( u8 i = 0; i < 16; i++ )
	{
		char c = *str;
		if ( c )
		{
			str++;
		}
		else
		{
			c = (char)0xa0;
		}

		if ( convert )
		{
			if (c == '.' && strcasecmp(str, "PRG") == 0)
			{
				str += 3;	// skip PRG extension
				c = (char)0xa0;
			}

			c = asciiToPetscii( c );
		}

		FILENAME_TEMP[ i ] = c;
	}

	FILENAME_TEMP[ 16 ] = 0;
	return FILENAME_TEMP;
}

static inline void putU8( u8 **ptr, u8 value )
{
	*(*ptr)++ = value;
}

static inline void putU16( u8 **ptr, u16 value )
{
	*(*ptr)++ = (u8)value;
	*(*ptr)++ = (u8)(value >> 8);
}

static void putU8Times( u8 **ptr, u8 value, size_t size )
{
	while ( size-- )
	{
		putU8( ptr, value );
	}
}

static void putString( u8 **ptr, const char* str )
{
	while ( *str )
	{
		putU8( ptr, *str++ );
	}
}

static void putDiskname( u8 **ptr, const char* str )
{
	putU8( ptr, '"' );

	for ( u8 i = 0; i < 23; i++ )
	{
		char c = *str++;
		if( i == 16 )
		{
			c = '"';
		}
		else if( i == 17 )
		{
			c = ' ';
		}
		else if ( c == (char)0xa0 )
		{
			c = ( i != 22 ) ? ' ' : '1';
		}

		putU8( ptr, c );
	}
}

static void putFilename( u8 **ptr, const char* str )
{
	putU8( ptr, '"' );
	char endChar = '"';

	for ( u8 i = 0; i < 16; i++ )
	{
		char c = *str++;
		if ( endChar != '"' )
		{
			// allow characters after the end-quote, like the 1541
			c &= 0x7f;
		}
		else if ( c == (char)0xa0 || c == '"' )
		{
			endChar = ' ';
			c = '"';
		}

		putU8( ptr, c );
	}

	putU8( ptr, endChar );
}

static void putDirEntry( u8 **ptr, DIRENTRY *entry )
{
	u16 blocks;
	if ( entry->f & (DIR_FILE_IN_D64|DIR_D64_FILE|DIR_DIRECTORY) )
	{
		blocks = entry->size / 254;
	}
	else
	{
		blocks = (entry->size / 254) + 1;
	}

	putU16( ptr, blocks );

	u8 pad = 0;
	if ( blocks < 10 )
	{
		pad = 3;
	}
	else if ( blocks < 100 )
	{
		pad = 2;
	}
	else if ( blocks < 1000 )
	{
		pad = 1;
	}
	putU8Times( ptr, ' ', pad );

	const char *filename = getFilename( entry );
	putFilename( ptr, filename );

/*  todo: set the file type
	char c = ( entry->type & 0x80 ) ? ' ' : '*';
	putU8( ptr, c );
	putString( ptr, types[ entry->type & 7 ] );
	c = (entry->type & 0x40) ? '<' : ' ';
	putU8( ptr, c );
*/
	const char types[][ 6 ] = { " DEL ", " SEQ ", " PRG ", " USR ", " REL ", " CBM ", " DIR ", "???" };

	if ( entry->f & (DIR_D64_FILE|DIR_DIRECTORY) )
	{
		putString( ptr, types[ FILE_DIR ] );
	}
	else if ( entry->f & DIR_FILE_IN_D64 )
	{
		u8 type = (entry->f >> SHIFT_TYPE) & 7;
		putString( ptr, types[ type ] );
	}
	else
	{
		putString( ptr, types[ FILE_PRG ] );
	}

	putU8Times( ptr, ' ', 4 - pad );
}

static bool filenameMatch( DIRENTRY *entry, const char *pattern )
{
	const char *filename = getFilename( entry );

	bool found = true;
	for ( u8 i = 0; i < 16; i++ )
	{
		char f = filename[i];
		char p = pattern[i];

		if ( p == '*' )
		{
			break;
		}
		else if ( p == '?' && f )
		{
			continue;
		}
		else if ( !p && f == (char)0xa0 )
		{
			break;
		}
		else if ( p == f )
		{
			if ( !p )
			{
				break;
			}
		}
		else
		{
			found = false;
			break;
		}
	}

	return found;
}

static bool typeMatch( DIRENTRY *entry, FILE_TYPE type )
{
	if ( !type )
	{
		return true;
	}

	if ( entry->f & (DIR_D64_FILE|DIR_DIRECTORY) )
	{
		return false;
	}

	if ( entry->f & DIR_FILE_IN_D64 )
	{
		u8 d64_type = (entry->f >> SHIFT_TYPE) & 7;
		return d64_type == type;
	}

	return type == FILE_PRG;
}

static bool loadDirectory( char *pattern, u8 *data, u32 *size )
{
	if ( pattern[0] == 0 )      // no pattern
	{
		pattern[0] = '*';       // do a wildcard search
		pattern[1] = 0;
	}
	else if ( pattern[0] >= '0' && pattern[0] <= '9' )  // drive number
	{
		if ( pattern[0] != '0' )
		{
			return false;       // only support drive 0
		}

		if ( pattern[1] == 0 )  // no pattern
		{
			pattern[0] = '*';   // do a wildcard search
		}
	}

	char *p_ptr = pattern;
	char c;
	while ( ( c = *p_ptr++ ) )	// scan for filename
	{
		if (c == ':')
		{
			pattern = p_ptr;
			break;
		}
	}

	FILE_TYPE type = FILE_DEL;
	p_ptr = pattern;
	while ( ( c = *p_ptr ) )	// scan for type
	{
		if (c == '=')
		{
			*p_ptr++ = 0;		// null terminate filename
			switch ( *p_ptr )
			{
				case 'S':
					type = FILE_SEQ;
					break;

				case 'P':
					type = FILE_PRG;
					break;

				case 'U':
					type = FILE_USR;
					break;

				case 'R':
					type = FILE_REL;
					break;
			}
			break;
		}

		p_ptr++;
	}

	u8 *ptr = data;
	putU16( &ptr, 0x0401 );            // start address

	// header
	const u16 linkAddr = 0x0101;
	putU16( &ptr, linkAddr );
	putU16( &ptr, 0 );                 // drive number
	putU8( &ptr, 0x12 );               // reverse on

	strcpy( FILENAME_TEMP, "SIDEKICK64        00 2A" );

	rewindDir();
	DIRENTRY *entry;
	if ( readDir( &entry ) && entry->f & DIR_FILE_IN_D64 &&
	     ( entry->f & ( ( 1 << SHIFT_TYPE ) - 1 ) ) == 0 )
	{
		memcpy( FILENAME_TEMP, entry->name, sizeof(FILENAME_TEMP)-1 );
	}
	else
	{
		rewindDir();
		if ( dirParent < (u32)nDirEntries )
		{
			getFilename( &dir[ dirParent ] );
		}
	}

	putDiskname( &ptr, FILENAME_TEMP );
	putU8( &ptr, 0 );                  // end of line

	// files
	for ( s32 i = 0; i < 100 && readDir( &entry ); )
	{
		if ( filenameMatch( entry, pattern ) && typeMatch( entry, type ) )
		{
			putU16( &ptr, linkAddr );
			putDirEntry( &ptr, entry );
			putU8( &ptr, 0 );          // end of line
			i++;
		}
	}

	// footer
	putU16( &ptr, linkAddr );
	u16 blocksFree = 0;
	putU16( &ptr, blocksFree );
	putString( &ptr, "BLOCKS FREE." );
	putU8Times( &ptr, ' ', 13 );
	putU8( &ptr, 0 );                  // end of line
	putU16( &ptr, 0 );                 // end of program

	*size = ptr - ( data + 2 );

	return true;
}

static void parseFilename( char *filename, PARSED_FILENAME *parsed )
{
	char *ptr = filename;
	char c, last = 0;
	parsed->overwrite = false;
	parsed->drive = 0;

	while ( ( c = *ptr++ ) )    // scan for drive number and overwrite flag
	{
		if ( c == ':' )
		{
			parsed->overwrite = ( *filename == '@' );
			if ( last >= '0' && last <= '9' )
			{
				parsed->drive = last - '0';
			}

			filename = ptr;
			break;
		}

		last = c;
	}

	ptr = filename;
	parsed->name = filename;
	parsed->wildcard = false;
	parsed->type = FILE_DEL;
	parsed->mode = 0;
	bool endOfFilename = false;

	while ( *ptr )  // scan for wildcard, type and mode
	{
		if ( *ptr == ',' )
		{
			*ptr++ = 0; // null terminate filename
			endOfFilename = true;

			if ( !parsed->type )
			{
				switch ( *ptr )
				{
					case 'S':
						parsed->type = FILE_SEQ;
						break;

					case 'U':
						parsed->type = FILE_USR;
						break;

					case 'L':
						parsed->type = FILE_REL;
						break;

					default:
						parsed->type = FILE_PRG;
						break;
				}
			}
			else if ( !parsed->mode )
			{
				parsed->mode = *ptr;
				break;
			}
		}
		else
		{
			if ( !endOfFilename && ( *ptr == '*' || *ptr == '?' ) )
			{
				parsed->wildcard = true;
			}

			ptr++;
		}
	}
}

static s32 findFile( const char *filename )
{
	DIRENTRY *entry;
	while ( readDir( &entry ) )
	{
		if ( (entry->f & DIR_FILE_IN_D64 && ((entry->f>>SHIFT_TYPE)&7) == FILE_PRG) ||
			 !(entry->f & DIR_FILE_IN_D64) )
		{
			if ( filenameMatch( entry, filename ) )
			{
				return dirPos - 1;
			}
		}
	}

	return -1;
}

static bool loadNewDirectory( char *vic20Filename, u8 *data, u32 *size )
{
	dirCurrent = -1;
	menuFile = false;
	vic20Filename[ 0 ] = 0;
	return loadDirectory( vic20Filename, data, size );
}

static bool readFileFromD64( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 fileIndex, u8 *data, u32 *size )
{
	FATFS m_FileSystem;
	if ( f_mount( &m_FileSystem, DRIVE, 1 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot mount drive: %s", DRIVE );

	u32 imgsize = 0;
	if ( !readD64File( logger, "", FILENAME, d64buf, &imgsize ) )
		return false;

	if ( f_mount( 0, DRIVE, 0 ) != FR_OK )
		logger->Write( "RaspiMenu", LogPanic, "Cannot unmount drive: %s", DRIVE );

	if ( d64ParseExtract( d64buf, imgsize, D64_GET_FILE + fileIndex, data, (s32*)size ) != 0 )
	{
		return false;
	}

	return true;
}

bool loadPrgFile( CLogger *logger, const char *DRIVE, char *FILENAME, char *vic20Filename, u8 *data, u32 *size )
{
	if ( strcmp( vic20Filename, "//" ) == 0 ) // root directory
	{
		dirParent = 0xffffffff;
		return loadNewDirectory( vic20Filename, data, size );
	}

	if ( strcmp( vic20Filename, "_" ) == 0 ) // parent directory
	{
		dirParent = getParent(dirParent);
		return loadNewDirectory( vic20Filename, data, size );
	}

	if ( vic20Filename[ 0 ] == '$' ) // directory
	{
		return loadDirectory( vic20Filename + 1, data, size );
	}

	PARSED_FILENAME parsed;
	parseFilename( vic20Filename, &parsed );
	if ( parsed.drive )
	{
		return false;   // only support drive 0
	}

	if ( parsed.type && parsed.type != FILE_PRG )
	{
		return false;   // only support PRG files
	}

	if ( parsed.name[ 0 ] != '*' || !menuFile )
	{
		menuFile = false;
		if ( parsed.name[ 0 ] != '*' || dirCurrent < 0 || dirCurrent >= nDirEntries )
		{
			rewindDir();
			dirCurrent = findFile( parsed.name );
		}

		if ( dirCurrent < 0 || dirCurrent >= nDirEntries )
		{
			return false;
		}

		u32 f = dir[ dirCurrent ].f;

		// change directory
		if ( f & DIR_DIRECTORY || f & DIR_D64_FILE )
		{
			logger->Write( "menu", LogNotice, "change dir to '%s'", vic20Filename );
			if ( f & DIR_DIRECTORY && !(f & DIR_SCANNED) )
			{
				buildPath( dirCurrent, FILENAME, 1 );
				strcat( FILENAME, "\\" );
				insertDirectoryContents( dirCurrent, FILENAME, f & DIR_LISTALL );
			}

			dirParent = dirCurrent;
			return loadNewDirectory( vic20Filename, data, size );
		}

		if ( f & DIR_FILE_IN_D64 )
		{
			buildPath( dirCurrent, FILENAME, 1 );
			u32 fileIndex = f & ((1<<SHIFT_TYPE)-1);
			return readFileFromD64( logger, DRIVE, FILENAME, fileIndex, data, size );
		}

		buildPath( dirCurrent, FILENAME, 0 );
	}

	logger->Write( "menu", LogNotice, "Loading %s (%s)", vic20Filename, FILENAME );
	return readFile( logger, DRIVE, FILENAME, data, size );
}

static s32 insertFile( u32 parent, const char *name, u32 f, u32 size )
{
	if ( parent >= (u32)nDirEntries )
	{
		return -1;
	}

	s32 prev = parent;
	s32 next = -1;

	for ( s32 i = prev + 1; i < nDirEntries; i++ )
	{
		if ( dir[ i ].parent == parent )
		{
			if ( !(dir[ i ].f & (DIR_D64_FILE|DIR_DIRECTORY)) &&
				 strcasecmp( (char*)dir[ i ].name, name ) > 0 )
			{
				next = i;
				break;
			}
		}
		else if ( dir[ i ].level <= (dir[ parent ].level + 1) )
		{
			break;
		}

		prev = i;
	}

	s32 c = prev + 1;
	s32 count = nDirEntries - prev;
	memmove( &dir[ c ], &dir[ prev ], count * sizeof( DIRENTRY ) );
	nDirEntries ++;

	for ( s32 i = 0; i < nDirEntries; i++ )
	{
		if ( dir[ i ].parent > (u32)prev && dir[ i ].parent < (u32)nDirEntries )
		{
			dir[ i ].parent ++;
		}

		if ( dir[ i ].next > (u32)prev )
		{
			dir[ i ].next ++;
		}
	}

	if ( next < 0 )
	{
		dir[ c ].next = 0;
	}
	else
	{
		dir[ c ].next = c + 1;
	}

	if ( cursorPos > prev && cursorPos < nDirEntries )
	{
		cursorPos ++;
	}

	strcpy( (char*)dir[ c ].name, name );
	dir[ c ].f = f;
	dir[ c ].parent = parent;
	dir[ c ].level = dir[ parent ].level + 1;
	dir[ c ].size = size;

	return c;
}

bool savePrgFile( CLogger *logger, const char *DRIVE, char *FILENAME, char *vic20Filename, u8 *data, u32 size )
{
	PARSED_FILENAME parsed;
	parseFilename( vic20Filename, &parsed );
	if ( parsed.drive )
	{
		return false;   // only support drive 0
	}

	if ( parsed.type && parsed.type != FILE_PRG )
	{
		return false;   // only support PRG files
	}

	rewindDir();
	dirCurrent = findFile( parsed.name );

	char *newName = NULL;
	if ( dirCurrent < 0 )	// file not found
	{
		if ( parsed.wildcard )
		{
			return false;	// no matching file found
		}

		if ( dirParent >= (u32)nDirEntries )
		{
			return false;	// no parent
		}

		u32 f = dir[ dirParent ].f;
		if ( !(f & DIR_DIRECTORY) )
		{
			return false;	// not supported
		}

		buildPath( dirParent, FILENAME, 0 );
		u32 pathLen = strlen( FILENAME );
		sprintf( FILENAME + pathLen, "\\%s.prg", parsed.name );
		newName = FILENAME + pathLen + 1;
	}
	else	// file found
	{
		if ( !parsed.overwrite )
		{
			return false;	// file already exists
		}

		u32 f = dir[ dirCurrent ].f;
		if ( !(f & DIR_PRG_FILE) )
		{
			return false;	// not supported
		}

		buildPath( dirCurrent, FILENAME, 0 );
	}

	logger->Write( "menu", LogNotice, "Saving %s (%s)", vic20Filename, FILENAME );
	if ( writeFile( logger, DRIVE, FILENAME, data, size ) )
	{
		if ( dirCurrent < 0 )
		{
			dirCurrent = insertFile( dirParent, newName, DIR_PRG_FILE, size );
		}
		else
		{
			dir[ dirCurrent ].size = size;
		}

		return true;
	}

	return false;
}
