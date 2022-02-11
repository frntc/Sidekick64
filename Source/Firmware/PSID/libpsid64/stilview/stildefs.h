//
// STIL - Common defines
//

#ifndef _STILDEFS_H
#define _STILDEFS_H

#if defined(__linux__) || defined(__FreeBSD__) || defined(solaris2) || defined(sun) || defined(sparc) || defined(sgi) || defined(__APPLE__)
#define UNIX
#endif

#if defined(__MACOS__)
#define MAC
#endif

#if defined(__amigaos__)
#define AMIGA
#endif

//
// Here you should define:
// - what the pathname separator is on your system (attempted to be defined
//   automatically),
// - what function compares strings case-insensitively,
// - what function compares portions of strings case-insensitively.
//

#ifdef UNIX
    #define SLASH '/'
#elif MAC
    #define SLASH ':'
#elif AMIGA
    #define SLASH '/'
#else // WinDoze
    #define SLASH '\\'
#endif

// Define which one of the following two is supported on your system.
//#define HAVE_STRCASECMP 1
 #define HAVE_STRICMP 1

// Define which one of the following two is supported on your system.
//#define HAVE_STRNCASECMP 1
 #define HAVE_STRNICMP 1

// Now let's do the actual definitions.

#ifdef HAVE_STRCASECMP
#define MYSTRICMP strcasecmp
#elif HAVE_STRICMP
#define MYSTRICMP stricmp
#else
#error Neither strcasecmp nor stricmp is available.
#endif

#ifdef HAVE_STRNCASECMP
#define MYSTRNICMP strncasecmp
#elif HAVE_STRNICMP
#define MYSTRNICMP strnicmp
#else
#error Neither strncasecmp nor strnicmp is available.
#endif

// These are the hardcoded STIL/BUG field names.
const char    _NAME_STR[]="   NAME: ";
const char  _AUTHOR_STR[]=" AUTHOR: ";
const char   _TITLE_STR[]="  TITLE: ";
const char  _ARTIST_STR[]=" ARTIST: ";
const char _COMMENT_STR[]="COMMENT: ";
const char     _BUG_STR[]="BUG: ";

// Maximum size of a single line in STIL - also accounts for some extra
// padding, just in case.
#define STIL_MAX_LINE_SIZE 91

// Maximum size of a single STIL entry (in bytes).
#define STIL_MAX_ENTRY_SIZE STIL_MAX_LINE_SIZE*100

// HVSC path to STIL.
const char PATH_TO_STIL[]="/DOCUMENTS/STIL.txt";

// HVSC path to BUGlist.
const char PATH_TO_BUGLIST[]="/DOCUMENTS/BUGlist.txt";

#endif // _STILDEFS_H
