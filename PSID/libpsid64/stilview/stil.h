//
// STIL class
//
// AUTHOR: LaLa
// Email : LaLa@C64.org
// Copyright (C) 1998, 2002 by LaLa
//

//
// Given the location of HVSC this class can extract STIL information for a
// given tune of a given SID file. (Sounds simple, huh?)
//
// PLEASE, READ THE ACCOMPANYING README.TXT FILE BEFORE PROCEEDING!!!!
//

#ifndef _STIL_H
#define _STIL_H

#include <fstream>
#include <cstdlib>     // For atof() and size_t
#include "stilcomm.h"

class STIL {

    public:

        // Enum to use for asking for specific fields.
        enum STILField {
            all,
            name,
            author,
            title,
            artist,
            comment
        };

        // Enum that describes the possible errors this class may encounter.
        enum STILerror {
            NO_STIL_ERROR = 0,
            BUG_OPEN,           // INFO ONLY: failed to open BUGlist.txt.
            WRONG_DIR,          // INFO ONLY: path was not within HVSC base dir.
            NOT_IN_STIL,        // INFO ONLY: requested entry was not found in STIL.txt.
            NOT_IN_BUG,         // INFO ONLY: requested entry was not found in BUGlist.txt.
            WRONG_ENTRY,        // INFO ONLY: section-global comment was asked for with get*Entry().
            CRITICAL_STIL_ERROR = 10,
            BASE_DIR_LENGTH,    // The length of the HVSC base dir was wrong (empty string?)
            STIL_OPEN,          // Failed to open STIL.txt.
            NO_EOL,             // Failed to determine EOL char(s).
            NO_STIL_DIRS,       // Failed to get sections (subdirs) when parsing STIL.txt.
            NO_BUG_DIRS         // Failed to get sections (subdirs) when parsing BUGlist.txt.
        };

        // To turn debug output on
        bool STIL_DEBUG;

        //----//

        // CONSTRUCTOR
        //      Allocates necessary memory.
        STIL();

        // DESTRUCTOR
        //      Deallocates the necessary memory.
        ~STIL();

        //
        // getVersion()
        //
        // FUNCTION: Returns a formatted string telling what the version
        //           number is for the STIL class and other info.
        //           If it is called after setBaseDir(), the string also
        //           has the STIL.txt file's version number in it.
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      char * - printable formatted string with version and copyright
        //               info
        //               (It's kinda dangerous to return a pointer that points
        //               to an internal structure, but I trust you. :)
        //
        char *getVersion();

        //
        // getVersionNo()
        //
        // FUNCTION: Returns a floating number telling what the version
        //           number is of this STIL class.
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      float - version number
        float getVersionNo();

        //
        // setBaseDir()
        //
        // FUNCTION: Tell the object where the HVSC base directory is - it
        //           figures that the STIL should be in /DOCUMENTS/STIL.txt
        //           and that the BUGlist should be in /DOCUMENTS/BUGlist.txt.
        //           It should not matter whether the path is given in UNIX,
        //           WinDOS, or Mac format (ie. '\' vs. '/' vs. ':')
        // ARGUMENTS:
        //      pathToHVSC = HVSC base directory in your machine's format
        // RETURNS:
        //      false - Problem opening or parsing STIL/BUGlist
        //      true  - All okay
        bool setBaseDir(const char *pathToHVSC);

        //
        // getSTILVersionNo()
        //
        // FUNCTION: Returns a floating number telling what the version
        //           number is of the STIL.txt file.
        //           To be called only after setBaseDir()!
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      float - version number (0.0 if setBaseDir() was not called, yet)
        //
        float getSTILVersionNo();

        //
        // getEntry()
        //
        // FUNCTION: Given an HVSC pathname, a tune number and a
        //           field designation, it returns a formatted string that
        //           contains the STIL field for the tune number (if exists).
        //           If it doesn't exist, returns a NULL.
        // ARGUMENTS:
        //      relPathToEntry = relative to the HVSC base dir, starting with
        //                       a slash
        //      tuneNo         = song number within the song (default=0).
        //      field          = which field to retrieve (default=all).
        //
        //      What the possible combinations of tuneNo and field represent:
        //
        //      - tuneNo = 0, field = all : all of the STIL entry is returned.
        //      - tuneNo = 0, field = comment : the file-global comment is returned.
        //        (For single-tune entries, this returns nothing!)
        //      - tuneNo = 0, field = <other> : INVALID! (NULL is returned)
        //      - tuneNo != 0, field = all : all fields of the STIL entry for the
        //        given tune number are returned. (For single-tune entries, this is
        //        equivalent to saying tuneNo = 0, field = all.)
        //        However, the file-global comment is *NOT* returned with it any
        //        more! (Unlike in versions before v2.00.) It led to confusions:
        //        eg. when a comment was asked for tune #3, it returned the
        //        file-global comment even if there was no specific entry for tune #3!
        //      - tuneNo != 0, field = <other> : the specific field of the specific
        //        tune number is returned. If the tune number doesn't exist (eg. if
        //        tuneNo=2 for single-tune entries, or if tuneNo=2 when there's no
        //        STIL entry for tune #2 in a multitune entry), returns NULL.
        //
        //      NOTE: For older versions of STIL (older than v2.59) the tuneNo and
        //      field parameters are ignored and are assumed to be tuneNo=0 and
        //      field=all to maintain backwards compatibility.
        //
        // RETURNS:
        //      NULL - if there's absolutely no STIL entry for the tune
        //      char * - pointer to a printable formatted string containing
        //               the STIL entry
        //               (It's kinda dangerous to return a pointer that points
        //               to an internal structure, but I trust you. :)
        //
        char *getEntry(const char *relPathToEntry, int tuneNo=0, STILField field=all);

        // Same as above, but with an absolute path given
        // given in your machine's format.
        //
        char *getAbsEntry(const char *absPathToEntry, int tuneNo=0, STILField field=all);

        //
        // getGlobalComment()
        //
        // FUNCTION: Given an HVSC pathname and tune number it returns a
        //           formatted string that contains the section-global
        //           comment for the tune number (if it exists). If it
        //           doesn't exist, returns a NULL.
        // ARGUMENTS:
        //      relPathToEntry = relative to the HVSC base dir starting with
        //                       a slash
        // RETURNS:
        //      NULL - if there's absolutely no section-global comment
        //             for the tune
        //      char * - pointer to a printable formatted string containing
        //               the section-global comment
        //               (It's kinda dangerous to return a pointer that points
        //               to an internal structure, but I trust you. :)
        //
        char *getGlobalComment(const char *relPathToEntry);

        // Same as above, but with an absolute path
        // given in your machine's format.
        //
        char *getAbsGlobalComment(const char *absPathToEntry);

        //
        // getBug()
        //
        // FUNCTION: Given an HVSC pathname and tune number it returns a
        //           formatted string that contains the BUG entry for the
        //           tune number (if exists). If it doesn't exist, returns
        //           a NULL.
        // ARGUMENTS:
        //      relPathToEntry = relative to the HVSC base dir starting with
        //                       a slash
        //      tuneNo         = song number within the song (default=0)
        //                       If tuneNo=0, returns all of the BUG entry.
        //
        //      NOTE: For older versions of STIL (older than v2.59) tuneNo is
        //      ignored and is assumed to be 0 to maintain backwards
        //      compatibility.
        //
        // RETURNS:
        //      NULL - if there's absolutely no BUG entry for the tune
        //      char * - pointer to a printable formatted string containing
        //               the BUG entry
        //               (It's kinda dangerous to return a pointer that points
        //               to an internal structure, but I trust you. :)
        //
        char *getBug(const char *relPathToEntry, int tuneNo=0);

        // Same as above, but with an absolute path
        // given in your machine's format.
        //
        char *getAbsBug(const char *absPathToEntry, int tuneNo=0);

        //
        // getError()
        //
        // FUNCTION: Returns a specific error number identifying the problem
        //           that happened at the last invoked public method.
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      STILerror - an enumerated error value
        //
        inline STILerror getError() {return (lastError);}

        //
        // hasCriticalError()
        //
        // FUNCTION: Returns true if the last error encountered was critical
        //           (ie. not one that the STIL class can recover from).
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      bool - true if the last error encountered was critical
        //
        inline bool hasCriticalError() {
            return ( (lastError >= CRITICAL_STIL_ERROR) ? true : false);
        }

        //
        // getErrorStr()
        //
        // FUNCTION: Returns an ASCII error string containing the
        //           description of the error that happened at the last
        //           invoked public method.
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      char * - pointer to string with the error description
        //
        inline const char *getErrorStr() {return (STIL_ERROR_STR[lastError]);}

    private:

        // Version number/copyright string
        char versionString[2*STIL_MAX_LINE_SIZE];

        // STIL.txt's version number
        float STILVersion;

        // Base dir
        char *baseDir;
        size_t baseDirLength;

        // File handles
        std::ifstream stilFile;
        std::ifstream bugFile;

        // Linked list of sections (subdirs) for easier positioning.
        // NOTE: There's always at least one (empty) member on these
        //  lists! (The pointers will never be NULL.)
        struct dirList {
            char *dirName;
            std::streampos position;
            struct dirList *next;
        } *stilDirs, *bugDirs;

        // This tells us what the line delimiter is in STIL.txt.
        // (It may be two chars!)
        char STIL_EOL;
        char STIL_EOL2;

        // Error number of the last error that happened.
        STILerror lastError;

        // Error strings containing the description of the possible errors in STIL.
        static const char *STIL_ERROR_STR[];

        ////////////////

        // The last retrieved entry
        char entrybuf[STIL_MAX_ENTRY_SIZE];

        // The last retrieved section-global comment
        char globalbuf[STIL_MAX_ENTRY_SIZE];

        // The last retrieved BUGentry
        char bugbuf[STIL_MAX_ENTRY_SIZE];

        // Buffers to hold the resulting strings
        char resultEntry[STIL_MAX_ENTRY_SIZE];
        char resultBug[STIL_MAX_ENTRY_SIZE];

        ////////////////

        //
        // determineEOL()
        //
        // FUNCTION: Determines what the EOL char is (or are) from STIL.txt.
        //           It is assumed that BUGlist.txt will use the same EOL.
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      false - something went wrong
        //      true  - everything is okay
        //
        bool determineEOL();

        //
        // getDirs()
        //
        // FUNCTION: Populates the given dirList array with the directories
        //           obtained from 'inFile' for faster positioning within
        //           'inFile'.
        // ARGUMENTS:
        //      inFile - where to read the directories from
        //      dirs   - the dirList array that should be populated with the
        //               directory list
        //      isSTILFile - is this the STIL or the BUGlist we are parsing
        // RETURNS:
        //      false - No entries were found or otherwise failed to process
        //              inFile
        //      true  - everything is okay
        //
        bool getDirs(std::ifstream& inFile, dirList *dirs, bool isSTILFile);

        //
        // positionToEntry()
        //
        // FUNCTION: Positions the file pointer to the given entry in 'inFile'
        //           using the 'dirs' dirList for faster positioning.
        // ARGUMENTS:
        //      entryStr - the entry to position to
        //      inFile   - position the file pointer in this file
        //      dirs     - the list of dirs in inFile for easier positioning
        // RETURNS:
        //      true - if successful
        //      false - otherwise
        bool positionToEntry(const char *entryStr, std::ifstream& inFile, dirList *dirs);

        //
        // readEntry()
        //
        // FUNCTION: Reads the entry from 'inFile' into 'buffer'. 'inFile' should
        //           already be positioned to the entry to be read.
        // ARGUMENTS:
        //      inFile   - filehandle of file to read from
        //      entryStr - the entry needed to be read
        //      buffer   - where to put the result to
        // RETURNS:
        //      NONE
        void readEntry(std::ifstream& inFile, char *buffer);

        //
        // getField()
        //
        // FUNCTION: Given a STIL formatted entry in 'buffer', a tune number,
        //           and a field designation, it returns the requested
        //           STIL field into 'result'.
        //           If field=all, it also puts the file-global comment (if it exists)
        //           as the first field into 'result'.
        // ARGUMENTS:
        //      result - where to put the resulting string to (if any)
        //      buffer - pointer to the first char of what to search for
        //               the field. Should be a buffer in standard STIL
        //               format.
        //      tuneNo - song number within the song (default=0)
        //      field  - which field to retrieve (default=all).
        // RETURNS:
        //      false - if nothing was put into 'result'
        //      true  - 'result' has the resulting field
        bool getField(char *result, char *buffer, int tuneNo=0, STILField field=all);

        //
        // getOneField()
        //
        // FUNCTION:
        // ARGUMENTS:
        //      result - where to put the resulting string to (if any)
        //      start  - pointer to the first char of what to search for
        //               the field. Should be a buffer in standard STIL
        //               format.
        //      end    - pointer to the last+1 char of what to search for
        //               the field. ('end-1' should be a '\n'!)
        //      field  - which specific field to retrieve
        // RETURNS:
        //      false - if nothing was put into 'result'
        //      true  - 'result' has the resulting field
        bool getOneField(char *result, char *start, char *end, STILField field);

        //
        // getStilLine()
        //
        // FUNCTION: Extracts one line from 'infile' to 'line[]'. The end of
        //           the line is marked by endOfLineChar. Also eats up
        //           additional EOL-like chars.
        // ARGUMENTS:
        //      infile - filehandle (streampos should already be positioned
        //               to the start of the desired line)
        //      line   - char array to put the line into
        // RETURNS:
        //      NONE
        void getStilLine(std::ifstream& infile, char *line);

        //
        // deleteDirList()
        //
        // FUNCTION: Deletes *all* of the linked list of dirnames. Assumes that
        //           there is at least one element on the linked list!
        // ARGUMENTS:
        //      dirs - pointer to the head of the linked list to be deleted.
        // RETURNS:
        //      NONE (Maybe it should return a bool for error-checking purposes?)
        //
        void deleteDirList(dirList *dirs);

        //
        // copyDirList()
        //
        // FUNCTION: Copies the linked list of dirnames from one linked list
        //           to another. It creates new dirlist entries in the
        //           destination list as needed. Assumes that there is at least
        //           one element on the source *and* destination linked lists!
        // ARGUMENTS:
        //      toPtr - pointer to the head of the destination linked list
        //      fromPtr - pointer to the head of the source linked list
        // RETURNS:
        //      NONE (Maybe it should return a bool for error-checking purposes?)
        //
        void copyDirList(dirList *toPtr, dirList *fromPtr);

        //
        // createOneDir()
        //
        // FUNCTION: Creates a new dirlist entry (allocates memory for it), and
        //           returns a pointer to this new entry.
        // ARGUMENTS:
        //      NONE
        // RETURNS:
        //      dirList * - pointer to the newly created entry
        //
        inline dirList *createOneDir(void);
};

#endif // _STIL_H
