/*
    $Id$

    psid64 - create a C64 executable from a PSID file
    Copyright (C) 2001-2003  Roland Hermans <rolandh@users.sourceforge.net>

	this code has been modified for integration into the Sidekick64 software
    (the modifications are mostly removing everything that does not compile with vanilla Circle,
    I really feel sorry for having disfigured this code, please refer to the original repository 
    if you want to get the real and decent psid64 version)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////

#ifndef PSID64_H
#define PSID64_H

//#include <iostream>
//#include <string>

//#include "../sidplay/utils/SidDatabase.h"
#include "../sidplay/utils/SidTuneMod.h"

#define size_t int


//////////////////////////////////////////////////////////////////////////////
//                   F O R W A R D   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////

class Screen;
//class SidId;
class STIL;


//////////////////////////////////////////////////////////////////////////////
//                           D E F I N I T I O N S
//////////////////////////////////////////////////////////////////////////////

/**
 * Class to generate a C64 self extracting executable from a PSID file.
 */
class Psid64
{
public:
    enum Theme {
        THEME_DEFAULT,
	THEME_BLUE,
	THEME_C1541_ULTIMATE,
	THEME_COAL,
	THEME_DUTCH,
	THEME_KERNAL,
	THEME_LIGHT,
	THEME_MONDRIAAN,
	THEME_OCEAN,
	THEME_PENCIL,
	THEME_RAINBOW
    };

    /**
     * Constructor.
     */
    Psid64();

    /**
     * Destructor.
     */
    ~Psid64();

    /**
     * Set the path to the HVSC. This path is e.g. used to retrieve the STIL
     * information for a PSID file.
     */
    //bool setHvscRoot(std::string &hvscRoot);

    /**
     * Get the path to the HVSC.
     */
    /*inline const std::string getHvscRoot() const
    {
        return m_hvscRoot;
    }*/

    /**
     * Set the path to the HVSC song length database.
     */
    //bool setDatabaseFileName(std::string &databaseFileName);

    /**
     * Get the path to the HVSC song length database.
     */
    /*inline const std::string getDatabaseFileName() const
    {
        return m_databaseFileName;
    }*/

    /**
     * Set the path to the SID ID player identification configuration file.
     */
    //bool setSidIdConfigFileName(std::string &sidIdConfigFileName);

    /**
     * Get the path to the SID ID player identification configuration file.
     */
    /*inline const std::string getSidIdConfigFileName() const
    {
        return m_sidIdConfigFileName;
    }*/

    /**
     * Set the no driver option. When true, no driver code is added to the C64
     * executable and PSID64 only acts as a .sid to .prg converter.
     */
    inline void setNoDriver(bool noDriver)
    {
        m_noDriver = noDriver;
    }

    /**
     * Get the no driver option.
     */
    inline bool getNoDriver() const
    {
        return m_noDriver;
    }

    /**
     * Set the blank screen option. When true, the screen is forced to be
     * blank, even when there is enough free memory available to display
     * a screen with information about the PSID. A C64 executable that blanks
     * the screen will be several pages smaller as it does not require display
     * data and code.
     */
    inline void setBlankScreen(bool blankScreen)
    {
        m_blankScreen = blankScreen;
    }

    /**
     * Get the blank screen option.
     */
    inline bool getBlankScreen() const
    {
        return m_blankScreen;
    }

    /**
     * Set the compress option. When true, the output file is compressed with
     * Exomizer.
     */
    inline void setCompress(bool compress)
    {
        m_compress = compress;
    }

    /**
     * Get the compress option.
     */
    inline bool getCompress() const
    {
        return m_compress;
    }

    /**
     * Set the initial song number. When 0 or larger than the total number of
     * songs, the initial song as specified in the SID file header is used.
     */
    inline void setInitialSong(int initialSong)
    {
	m_initialSong = initialSong;
    }

    /**
     * Get the initial song.
     */
    inline int getInitialSong()
    {
	return m_initialSong;
    }

    /**
     * Set the use global comment flag. When set, PSID64 tries to extract
     * the global comment field of a PSID from the STIL database.
     */
    inline void setUseGlobalComment(bool useGlobalComment)
    {
        m_useGlobalComment = useGlobalComment;
    }

    /**
     * Get the use global comment flag.
     */
    inline bool getUseGlobalComment() const
    {
        return m_useGlobalComment;
    }

    /**
     * Set the verbose flag. When set, PSID64 will be more verbose about the
     * generation of the C64 executable.
     */
    inline void setVerbose(bool verbose)
    {
        m_verbose = verbose;
    }

    /**
     * Get the verbose flag.
     */
    inline bool getVerbose() const
    {
        return m_verbose;
    }

    /**
     * Set the theme.
     */
    /*inline void setTheme(Theme theme)
    {
	m_theme = theme;
    }*/

    /**
     * Get the theme.
     */
    /*inline Theme getTheme()
    {
	return m_theme;
    }*/

    /**
     * Get the status string. After an error has occurred, the status string
     * contains a description of the error.
     */
    inline const char* getStatus() const
    {
	return m_statusString;
    }

    /**
     * Load a PSID file.
     */
    //bool load(const char* fileName);
    bool load(unsigned char* sidData, int sidLength);

    /**
     * Convert the currently loaded PSID file.
     */
    bool convert();

    /**
     * Save the most recently generated C64 executable.
     */
    //bool save(const char* fileName);

    /**
     * Write the C64 executable to an output stream.
     */
    //bool write(std::ostream& out = std::cout);

    // converted file
    uint_least8_t *m_programData;
    unsigned int m_programSize;

private:
    Psid64(const Psid64&);
    Psid64 operator=(const Psid64&);

    static const unsigned int MAX_BLOCKS = 5;
    static const unsigned int MAX_PAGES = 256; // number of memory pages
    static const unsigned int NUM_MINDRV_PAGES = 2; // driver without screen display
    static const unsigned int NUM_EXTDRV_PAGES = 5; // driver with screen display
    static const unsigned int NUM_SCREEN_PAGES = 4; // size of screen in pages
    static const unsigned int NUM_CHAR_PAGES = 4; // size of charset in pages
    static const unsigned int STIL_EOT_SPACES = 10; // number of spaces before EOT
    static const unsigned int BAR_X = 15;
    static const unsigned int BAR_WIDTH = 19;
    static const unsigned int BAR_SPRITE_SCREEN_OFFSET = 0x300;

    // error and status message strings
    static const char* txt_relocOverlapsImage;
    static const char* txt_notEnoughC64Memory;
    static const char* txt_fileIoError;
    static const char* txt_noSidTuneLoaded;
    static const char* txt_noSidTuneConverted;
    //static const char* txt_sidIdConfigError;

    // configuration options
    bool m_noDriver;
    bool m_blankScreen;
    bool m_compress;
    int m_initialSong;
    bool m_useGlobalComment;
    bool m_verbose;
    //std::string m_hvscRoot;
    //std::string m_databaseFileName;
    //std::string m_sidIdConfigFileName;
    //Theme m_theme;

    // state data
    bool m_status;
    const char* m_statusString;   // error/status message of last operation

    // other internal data
    //std::string m_fileName;
    SidTuneMod m_tune;
    SidTuneInfo m_tuneInfo;
    //SidDatabase m_database;
    //STIL *m_stil;
    //SidId *m_sidId;

    // conversion data
    Screen *m_screen;
    //std::string m_stilText;
    uint_least8_t m_songlengthsData[4 * SIDTUNE_MAX_SONGS];
    size_t m_songlengthsSize;
    uint_least8_t m_driverPage; // startpage of driver, 0 means no driver
    uint_least8_t m_screenPage; // startpage of screen, 0 means no screen
    uint_least8_t m_charPage; // startpage of chars, 0 means no chars
    uint_least8_t m_stilPage; // startpage of stil, 0 means no stil
    uint_least8_t m_songlengthsPage; // startpage of song length data, 0 means no song lengths
    //std::string m_playerId;

    // member functions
    int_least32_t roundDiv(int_least32_t dividend, int_least32_t divisor);
    bool convertNoDriver();
    bool convertBASIC();
    bool formatStilText();
    bool getSongLengths();
    uint_least8_t findSonglengthsSpace(bool* pages, uint_least8_t scr,
				       uint_least8_t chars,
				       uint_least8_t driver,
				       uint_least8_t stil,
				       uint_least8_t stil_pages,
				       uint_least8_t size) const;
    uint_least8_t findStilSpace(bool* pages, uint_least8_t scr,
				uint_least8_t chars,
				uint_least8_t driver,
				uint_least8_t size) const;
    uint_least8_t findDriverSpace(bool* pages, uint_least8_t scr,
			          uint_least8_t chars,
				  uint_least8_t size) const;
    void findFreeSpace();
    uint8_t iomap(uint_least16_t addr);
    void initDriver(uint_least8_t** mem, uint_least8_t** ptr, int* n);
    void addFlag(bool &hasFlags, const char *flagName);
    char *toHexWord(uint_least16_t value) const;
    char *toNumStr(int value) const;
    void drawScreen();
    bool compress();
};


/*inline std::ostream& operator << (std::ostream& out, Psid64 &psid64)
{
    psid64.write(out);
    return out;
}*/

#endif // PSID64_H
