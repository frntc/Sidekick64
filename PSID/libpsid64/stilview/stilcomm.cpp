//
// STIL - Common stuff
//

//
// Common functions used for STIL handling.
// See stilcomm.h for prologs.
//

#ifndef _STILCOMM
#define _STILCOMM

#include "stildefs.h"
#include "stil.h"

const char *STIL::STIL_ERROR_STR[] = {
    "No error.",
    "Failed to open BUGlist.txt.",
    "Base dir path is not the HVSC base dir path.",
    "The entry was not found in STIL.txt.",
    "The entry was not found in BUGlist.txt.",
    "A section-global comment was asked for in the wrong way.",
    "",
    "",
    "",
    "",
    "CRITICAL ERROR",
    "Incorrect HVSC base dir length!",
    "Failed to open STIL.txt!",
    "Failed to determine EOL from STIL.txt!",
    "No STIL sections were found in STIL.txt!",
    "No STIL sections were found in BUGlist.txt!"
};

void convertSlashes(char *str);
void convertToSlashes(char *str);

void convertSlashes(char *str)
{
    while (*str) {
        if (*str == '/') {
            *str = SLASH;
        }
        str++;
    }
}

void convertToSlashes(char *str)
{
    while (*str) {
        if (*str == SLASH) {
            *str = '/' ;
        }
        str++;
    }
}


#endif //_STILCOMM
