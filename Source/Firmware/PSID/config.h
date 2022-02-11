/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
#undef AC_APPLE_UNIVERSAL_BUILD

/* Define if the C++ compiler supports BOOL */
#undef HAVE_BOOL

/* Define if your C++ compiler implements exception-handling. */
#undef HAVE_EXCEPTIONS

/* Define to 1 if you have the <getopt.h> header file. */
#undef HAVE_GETOPT_H

/* Define to 1 if you have the `getopt_long' function. */
#undef HAVE_GETOPT_LONG

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if standard member ``ios::binary'' is called ``ios::bin''. */
#undef HAVE_IOS_BIN

/* Define if ``ios::openmode'' is supported. */
#undef HAVE_IOS_OPENMODE

/* Define to 1 if you have the <limits.h> header file. */
#undef HAVE_LIMITS_H

/* Define to 1 if you have the `lstat' function. */
#undef HAVE_LSTAT

/* Define to 1 if you have the `memmove' function. */
#undef HAVE_MEMMOVE

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have the `memset' function. */
#undef HAVE_MEMSET

/* Define to 1 if you have the `mkdir' function. */
#undef HAVE_MKDIR

/* Define to 1 if you have the `snprintf' function. */
#undef HAVE_SNPRINTF

/* Define if you have the <sstream> header file. */
#undef HAVE_SSTREAM

/* Define to 1 if you have the <stdbool.h> header file. */
#undef HAVE_STDBOOL_H

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the `strcasecmp' function. */
//#undef HAVE_STRCASECMP

/* Define to 1 if you have the `strchr' function. */
#undef HAVE_STRCHR

/* Define to 1 if you have the `strdup' function. */
#undef HAVE_STRDUP

/* Define to 1 if you have the `strerror' function. */
#undef HAVE_STRERROR

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if you have the `strncasecmp' function. */
//#undef HAVE_STRNCASECMP

/* Define to 1 if you have the `strrchr' function. */
#undef HAVE_STRRCHR

/* Define to 1 if you have the `strstr' function. */
#undef HAVE_STRSTR

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to 1 if the system has the type `_Bool'. */
#undef HAVE__BOOL

/* Define to 1 if you have the `_mkdir' function. */
#undef HAVE__MKDIR

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
#undef LSTAT_FOLLOWS_SLASHED_SYMLINK

/* Define if mkdir takes only one argument. */
#undef MKDIR_TAKES_ONE_ARG

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* The size of `char', as computed by sizeof. */
#undef SIZEOF_CHAR

/* The size of `int', as computed by sizeof. */
#undef SIZEOF_INT

/* The number of bytes in type long */
#undef SIZEOF_LONG

/* The size of `long int', as computed by sizeof. */
#undef SIZEOF_LONG_INT

/* The number of bytes in type short */
#undef SIZEOF_SHORT

/* The size of `short int', as computed by sizeof. */
#undef SIZEOF_SHORT_INT

/* The number of bytes in type void* */
#undef SIZEOF_VOIDP

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Version number of package */
#undef VERSION

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* Define for Solaris 2.5.1 so the uint8_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
#undef _UINT8_T

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t

/* Define to the type of an unsigned integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
#undef uint16_t

/* Define to the type of an unsigned integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
#undef uint8_t
