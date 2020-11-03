//
// STIL Common header
//

//
// Contains some definitions common to STIL
//

#ifndef _STILCOMM_H
#define _STILCOMM_H

#include "stildefs.h"

// convertSlashes()
//
// FUNCTION: Converts slashes to the one the OS uses to access files.
// ARGUMENTS:
//      str - what to convert
// RETURNS:
//      NONE
//
extern void convertSlashes(char *str);

// convertToSlashes()
//
// FUNCTION: Converts OS specific dir separators to slashes.
// ARGUMENTS:
//      str - what to convert
// RETURNS:
//      NONE
//
extern void convertToSlashes(char *str);

#endif // _STILCOMM_H
