/*
    $Id$

    psid64 - create a C64 executable from a PSID file
    Copyright (C) 2001-2014  Roland Hermans <rolandh@users.sourceforge.net>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../psid64/psid64.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include <algorithm>
//#include <cctype>
//#include <iomanip>
//#include <iostream>
//#include <ostream>
//#include <sstream>
//#include <vector>

#include "reloc65.h"
#include "screen.h"
//#include "sidid.h"
//#include "theme.h"
//#include "stilview/stil.h"
#include "exomizer/exomizer.h"

//using std::cerr;
//using std::dec;
//using std::endl;
//using std::hex;
//using std::ofstream;
//using std::ostream;
//using std::ostringstream;
//using std::replace;
//using std::setfill;
//using std::setw;
//using std::string;
//using std::uppercase;
//using std::vector;

//////////////////////////////////////////////////////////////////////////////
//                     L O C A L   D E F I N I T I O N S
//////////////////////////////////////////////////////////////////////////////
static char* emptyString = "";

#if defined(HAVE_IOS_OPENMODE)
    typedef std::ios::openmode openmode;
#else
    typedef int openmode;
#endif

/**
 * Structure to describe a memory block in the C64's memory map.
 */
struct block_t
{
    uint_least16_t load; /**< start address */
    uint_least16_t size; /**< size of the memory block in bytes */
    const uint_least8_t* data; /**< data to be stored */
    //string description; /**< a short description */
};

static inline unsigned int min(unsigned int a, unsigned int b);
static bool block_cmp(const block_t& a, const block_t& b);
//static void setThemeGlobals(globals_t& globals, Psid64::Theme theme);


//////////////////////////////////////////////////////////////////////////////
//                       L O C A L   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////

static inline unsigned int
min(unsigned int a, unsigned int b)
{
    if (a <= b)
    {
	return a;
    }
    return b;
}


static bool
block_cmp(const block_t& a, const block_t& b)
{
    return a.load < b.load;
}


/*static void
setThemeGlobals(globals_t& globals, Psid64::Theme theme)
{
    DriverTheme *driverTheme;
    switch (theme)
    {
    case Psid64::THEME_BLUE:
        driverTheme = DriverTheme::createBlueTheme();
	break;
    case Psid64::THEME_C1541_ULTIMATE:
        driverTheme = DriverTheme::createC1541UltimateTheme();
	break;
    case Psid64::THEME_COAL:
        driverTheme = DriverTheme::createCoalTheme();
	break;
    case Psid64::THEME_DUTCH:
        driverTheme = DriverTheme::createDutchTheme();
	break;
    case Psid64::THEME_KERNAL:
        driverTheme = DriverTheme::createKernalTheme();
	break;
    case Psid64::THEME_LIGHT:
        driverTheme = DriverTheme::createLightTheme();
	break;
    case Psid64::THEME_MONDRIAAN:
        driverTheme = DriverTheme::createMondriaanTheme();
	break;
    case Psid64::THEME_OCEAN:
        driverTheme = DriverTheme::createOceanTheme();
	break;
    case Psid64::THEME_PENCIL:
        driverTheme = DriverTheme::createPencilTheme();
	break;
    case Psid64::THEME_RAINBOW:
        driverTheme = DriverTheme::createRainbowTheme();
	break;
    case Psid64::THEME_DEFAULT:
    default:
        driverTheme = DriverTheme::createSidekickTheme();
        //driverTheme = DriverTheme::createDefaultTheme();
	break;
    }

    globals["COL_BORDER"] = driverTheme->getBorderColor();
    globals["COL_BACKGROUND"] = driverTheme->getBackgroundColor();
    globals["COL_RASTER_TIME"] = driverTheme->getRasterTimeColor();
    globals["COL_TITLE"] = driverTheme->getTitleColor();
    const int *lineColors = driverTheme->getLineColors();
    for (int i = 0; i < NUM_LINE_COLORS; ++i)
    {
	ostringstream oss;
	oss << "COL_LINE_" << i;
	globals[oss.str()] = lineColors[i];
    }
    globals["COL_PARAMETER"] = driverTheme->getFieldNameColor();
    globals["COL_COLON"] = driverTheme->getFieldColonColor();
    globals["COL_VALUE"] = driverTheme->getFieldValueColor();
    globals["COL_LEGEND"] = driverTheme->getLegendColor();
    globals["COL_BAR_FG"] = driverTheme->getProgressBarFillColor();
    globals["COL_BAR_BG"] = driverTheme->getProgressBarBackgroundColor();
    globals["COL_SCROLLER"] = driverTheme->getScrollerColor();
    const int *scrollerColors = driverTheme->getScrollerColors();
    for (int i = 0; i < NUM_SCROLLER_COLORS; ++i)
    {
	ostringstream oss;
	oss << "COL_SCROLLER_" << i;
	globals[oss.str()] = scrollerColors[i];
    }
    const int *footerColors = driverTheme->getFooterColors();
    for (int i = 0; i < NUM_FOOTER_COLORS; ++i)
    {
	ostringstream oss;
	oss << "COL_FOOTER_" << i;
	globals[oss.str()] = footerColors[i];
    }

    delete driverTheme;
}*/


//////////////////////////////////////////////////////////////////////////////
//                   P R I V A T E   M E M B E R   D A T A
//////////////////////////////////////////////////////////////////////////////

const char* Psid64::txt_relocOverlapsImage = "PSID64: relocation information overlaps the load image";
const char* Psid64::txt_notEnoughC64Memory = "PSID64: C64 memory has no space for driver code";
const char* Psid64::txt_fileIoError = "PSID64: File I/O error";
const char* Psid64::txt_noSidTuneLoaded = "PSID64: No SID tune loaded";
const char* Psid64::txt_noSidTuneConverted = "PSID64: No SID tune converted";
//const char* Psid64::txt_sidIdConfigError = "PSID64: Cannot read SID ID configuration file";


//////////////////////////////////////////////////////////////////////////////
//               P U B L I C   M E M B E R   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////

// constructor

Psid64::Psid64() :
    m_noDriver(false),
    m_blankScreen(false),
    m_compress(false),
    m_initialSong(0),
    m_useGlobalComment(false),
    m_verbose(false),
    //m_hvscRoot(),
//    m_databaseFileName(),
    //m_sidIdConfigFileName(),
    //m_theme(THEME_DEFAULT),
    m_status(false),
    m_statusString(NULL),
    //m_fileName(),
    m_tune(0),
    m_tuneInfo(),
//    m_database(),
    //m_stil(new STIL),
    //m_sidId(new SidId),
    m_screen(new Screen),
    //m_stilText(),
    m_songlengthsData(),
    m_songlengthsSize(0),
    m_driverPage(0),
    m_screenPage(0),
    m_charPage(0),
    m_stilPage(0),
    m_songlengthsPage(0),
    //m_playerId(),
    m_programData(NULL),
    m_programSize(0)
{
}

// destructor

Psid64::~Psid64()
{
    //delete m_stil;
    //delete m_sidId;
    //delete m_screen;
    //delete[] m_programData;
}


/*bool Psid64::setDatabaseFileName(string &databaseFileName)
{
    m_databaseFileName = databaseFileName;
    if (!m_databaseFileName.empty())
    {
        if (m_database.open(m_databaseFileName.c_str()) < 0)
        {
	    m_statusString = m_database.error();
	    return false;
        }
    }

    return true;
}*/


/*bool Psid64::setSidIdConfigFileName(string &sidIdConfigFileName)
{
    m_sidIdConfigFileName = sidIdConfigFileName;
    if (!m_sidIdConfigFileName.empty())
    {
	if (!m_sidId->readConfigFile(m_sidIdConfigFileName))
        {
            m_statusString = txt_sidIdConfigError;
            return false;
        }
    }

    return true;
}*/



bool Psid64::load( unsigned char* sidData, int sidLength )
{
	if ( !m_tune.load( sidData, sidLength ) )
	{
		m_statusString = m_tune.getInfo().statusString;
		return false;
	}

	m_tune.getInfo( m_tuneInfo );

	return true;
}

/*
bool
Psid64::load(const char* fileName)
{
    if (!m_tune.load(fileName))
    {
        //m_fileName.clear();
	m_statusString = m_tune.getInfo().statusString;
	return false;
    }

    m_tune.getInfo(m_tuneInfo);

    //m_fileName = fileName;

    return true;
}
*/

bool
Psid64::convert()
{
    static const uint_least8_t psid_boot_obj[] = {
#include "psidboot.h"
    };
    static const uint_least8_t psid_extboot_obj[] = {
#include "psidextboot.h"
    };
    uint_least8_t* psid_mem;
    uint_least8_t* psid_driver;
    int driver_size;
    uint_least16_t size;

    // ensure valid sidtune object
    if (!m_tune)
    {
	m_statusString = txt_noSidTuneLoaded;
	return false;
    }

    // handle special treatment of conversion without driver code
    if (m_noDriver)
    {
	return convertNoDriver();
    }

    // handle special treatment of tunes programmed in BASIC
    if (m_tuneInfo.compatibility == SIDTUNE_COMPATIBILITY_BASIC) {
	return convertBASIC();
    }

    // retrieve STIL entry for this SID tune
    if (!formatStilText())
    {
	return false;
    }

    // retrieve song length data for this SID tune
    if (!getSongLengths())
    {
	return false;
    }

    // find space for driver and screen (optional)
    findFreeSpace();
    if (m_driverPage == 0x00)
    {
	m_statusString = txt_notEnoughC64Memory;
	return false;
    }

    // use minimal driver if screen blanking is enabled
    if (m_blankScreen)
    {
	m_screenPage = (uint_least8_t) 0x00;
	m_charPage = (uint_least8_t) 0x00;
	m_stilPage = (uint_least8_t) 0x00;
    }

    // relocate and initialize the driver
    initDriver (&psid_mem, &psid_driver, &driver_size);

    // copy SID data
    uint_least8_t c64buf[65536];
    m_tune.placeSidTuneInC64mem(c64buf);

    // identify player routine
    const uint_least8_t* p_start = c64buf + m_tuneInfo.loadAddr;
    const uint_least8_t* p_end = p_start + m_tuneInfo.c64dataLen;
    //vector<uint_least8_t> buffer(p_start, p_end);
    //m_playerId = m_sidId->identify(buffer);

    // fill the blocks structure
    //vector<block_t> blocks;
	block_t blocks[ MAX_BLOCKS ];
	int nBlocks = 0;
    //blocks.reserve(MAX_BLOCKS);
    block_t driver_block;
    driver_block.load = m_driverPage << 8;
    driver_block.size = driver_size;
    driver_block.data = psid_driver;
    //driver_block.description = "Driver code";
    //blocks.push_back(driver_block);
	blocks[ nBlocks++ ] = driver_block;

    block_t music_data_block;
    music_data_block.load = m_tuneInfo.loadAddr;
    music_data_block.size = m_tuneInfo.c64dataLen;
    music_data_block.data = &(c64buf[m_tuneInfo.loadAddr]);
    //music_data_block.description = "Music data";
    //blocks.push_back(music_data_block);
	blocks[ nBlocks++ ] = music_data_block;

    if (m_screenPage != 0x00)
    {
	drawScreen();
	block_t screen_block;
	screen_block.load = m_screenPage << 8;
	screen_block.size = m_screen->getDataSize();
	screen_block.data = m_screen->getData();
	//screen_block.description = "Screen";
	//blocks.push_back(screen_block);
	blocks[ nBlocks++ ] = screen_block;
    }

    if (m_stilPage != 0x00)
    {
	block_t stil_text_block;
	stil_text_block.load = m_stilPage << 8;
	stil_text_block.size = 0;
	stil_text_block.data = (uint_least8_t*)emptyString;//(uint_least8_t*) m_stilText.c_str();
	//stil_text_block.description = "STIL text";
	//blocks.push_back(stil_text_block);
	blocks[ nBlocks++ ] = stil_text_block;
    }

    if (m_songlengthsPage != 0x00)
    {
	block_t song_length_data_block;
	song_length_data_block.load = m_songlengthsPage << 8;
	song_length_data_block.size = m_songlengthsSize;
	song_length_data_block.data = m_songlengthsData;
	//song_length_data_block.description = "Song length data";
	//blocks.push_back(song_length_data_block);
	blocks[ nBlocks++ ] = song_length_data_block;
    }

    //std::sort(blocks.begin(), blocks.end(), block_cmp);

    // print memory map
    if (m_verbose)
    {
	uint_least16_t charset = m_charPage << 8;

	//cerr << "C64 memory map:" << endl;
	/*for (vector<block_t>::const_iterator block_iter = blocks.begin();
	     block_iter != blocks.end();
	     ++block_iter)*/
	for ( int i = 0; i < nBlocks; i++ )
	{
		block_t *block_iter = &blocks[ i ];

	    if ((charset != 0) && (block_iter->load > charset))
	    {
		/*cerr << "  $" << toHexWord(charset) << "-$"
		     << toHexWord(charset + (256 * NUM_CHAR_PAGES))
		     << "  Character set" << endl;*/
		charset = 0;
	    }
	    /*cerr << "  $" << toHexWord(block_iter->load) << "-$"
	         << toHexWord(block_iter->load + block_iter->size) << "  "
		 << block_iter->description << endl;*/
	}
	if (charset != 0)
	{
	    /*cerr << "  $" << toHexWord(charset) << "-$"
		 << toHexWord(charset + (256 * NUM_CHAR_PAGES))
		 << "  Character set" << endl;*/
	}
    }

    // calculate total size of the blocks
    size = 0;
/*    for (vector<block_t>::const_iterator block_iter = blocks.begin();
         block_iter != blocks.end();
         ++block_iter)*/
	for ( int i = 0; i < nBlocks; i++ )
	{
		block_t *block_iter = &blocks[ i ];
		size = size + block_iter->size;
    }

    // determine initial song number (passed in at boot time)
    int initialSong;
    if ((1 <= m_initialSong) && (m_initialSong <= m_tuneInfo.songs))
    {
	initialSong = m_initialSong;
    }
    else
    {
	initialSong = m_tuneInfo.startSong;
    }

    // little gimmick: BASIC line number will be set to the preferred SID model
    int lineNumber;
    switch (m_tuneInfo.sidModel)
    {
    case SIDTUNE_SIDMODEL_6581:
	lineNumber = 6581;
	break;
    case SIDTUNE_SIDMODEL_8580:
	lineNumber = 8580;
	break;
    default:
	lineNumber = 1103;
        break;
    }

    // select boot code object
    const uint_least8_t* boot_obj;
    int boot_size;
    if (m_screenPage == 0x00)
    {
        boot_obj = psid_boot_obj;
        boot_size = sizeof(psid_boot_obj);
    }
    else
    {
        boot_obj = psid_extboot_obj;
        boot_size = sizeof(psid_extboot_obj);
    }

    // relocate boot code
    uint_least8_t* boot_mem;
    uint_least8_t* boot_reloc;
    boot_mem = boot_reloc = new uint_least8_t[boot_size];
    if (boot_mem == NULL)
    {
	return false;
    }
    memcpy(boot_reloc, boot_obj, boot_size);

	GLOBALS_BLOCK globals;

    //setThemeGlobals(globals, m_theme);
    globals.song = (initialSong - 1) & 0xff;
    uint_least16_t jmpAddr = m_driverPage << 8;
    // start address of driver
    globals.player = jmpAddr;
    // address of new stop vector for tunes that call $a7ae during init
    globals.stopvec = jmpAddr+3;
    const uint_least16_t load_addr = 0x0801;
    int screen = m_screenPage << 8;
    globals.screen = screen;
    globals.barsprptr = ((screen + BAR_SPRITE_SCREEN_OFFSET) & 0x3fc0) >> 6;
    globals.dd00 = ((((m_screenPage & 0xc0) >> 6) ^ 3) | 0x04);
    uint_least8_t vsa;	// video screen address
    uint_least8_t cba;	// character memory base address
    vsa = (uint_least8_t) ((m_screenPage & 0x3c) << 2);
    cba = (uint_least8_t) (m_charPage ? (m_charPage >> 2) & 0x0e : 0x06);
    globals.d018 = vsa | cba;

    // the additional BASIC starter code is not needed when compressing file
    uint_least16_t basic_size = (m_compress ? 0 : 12);
    uint_least16_t boot_addr = load_addr + basic_size;
    if (!reloc65 ((char **) &boot_reloc, &boot_size, boot_addr, &globals))
    {
	//cerr << ": Relocation error." << endl;
	return false;
    }

    uint_least16_t file_size = basic_size + boot_size + size;
    m_programSize = 2 + file_size;
    delete[] m_programData;
    m_programData = new uint_least8_t[m_programSize];
    uint_least8_t *dest = m_programData;
    *(dest++) = (uint_least8_t) (load_addr & 0xff);
    *(dest++) = (uint_least8_t) (load_addr >> 8);
    if (basic_size > 0)
    {
	// pointer to next BASIC line
	*(dest++) = (uint_least8_t) ((load_addr + 10) & 0xff);
	*(dest++) = (uint_least8_t) ((load_addr + 10) >> 8);
	// line number
	*(dest++) = (uint_least8_t) (lineNumber & 0xff);
	*(dest++) = (uint_least8_t) (lineNumber >> 8);
	// SYS token
	*(dest++) = (uint_least8_t) 0x9e;
	// "2061"
	*(dest++) = (uint_least8_t) 0x32;
	*(dest++) = (uint_least8_t) 0x30;
	*(dest++) = (uint_least8_t) 0x36;
	*(dest++) = (uint_least8_t) 0x31;
	// end of BASIC line
	*(dest++) = (uint_least8_t) 0x00;
	// pointer to next BASIC line
	*(dest++) = (uint_least8_t) 0x00;
	*(dest++) = (uint_least8_t) 0x00;
    }
    memcpy(dest, boot_reloc, boot_size);

    // free memory of relocated boot code
    delete[] boot_mem;

    uint_least16_t addr = 5; // parameter offset in psidboot.a65
    uint_least16_t eof = load_addr + file_size;
    if (m_screenPage != 0x00)
    {
        dest[addr++] = (uint_least8_t) (m_charPage); // page for character set, or 0
    }
    dest[addr++] = (uint_least8_t) (eof & 0xff); // end of C64 file
    dest[addr++] = (uint_least8_t) (eof >> 8);
    dest[addr++] = (uint_least8_t) (0x10000 & 0xff); // end of high memory
    dest[addr++] = (uint_least8_t) (0x10000 >> 8);
    dest[addr++] = (uint_least8_t) ((size + 0xff) >> 8); // number of pages to copy
    dest[addr++] = (uint_least8_t) ((0x10000 - size) & 0xff); // start of blocks after moving
    dest[addr++] = (uint_least8_t) ((0x10000 - size) >> 8);
    dest[addr++] = (uint_least8_t) (nBlocks - 1); // number of blocks - 1

    // copy block data to psidboot.a65 parameters
	for ( int i = 0; i < nBlocks; i++ )
	{
		block_t *block_iter = &blocks[ i ];
/*    for (vector<block_t>::const_iterator block_iter = blocks.begin();
         block_iter != blocks.end();
         ++block_iter)
    {*/
		int block_index = i;//block_iter - blocks.begin();
		uint_least16_t offs = addr + nBlocks - 1 - block_index;
		dest[offs] = (uint_least8_t) (block_iter->load & 0xff);
		dest[offs + MAX_BLOCKS] = (uint_least8_t) (block_iter->load >> 8);
		dest[offs + 2 * MAX_BLOCKS] = (uint_least8_t) (block_iter->size & 0xff);
		dest[offs + 3 * MAX_BLOCKS] = (uint_least8_t) (block_iter->size >> 8);
    }
    // addr = addr + 4 * MAX_BLOCKS;
    dest += boot_size;

    // copy blocks to c64 program file
/*    for (vector<block_t>::const_iterator block_iter = blocks.begin();
         block_iter != blocks.end();
         ++block_iter)
    {*/
	for ( int i = 0; i < nBlocks; i++ )
	{
		block_t *block_iter = &blocks[ i ];
		memcpy(dest, block_iter->data, block_iter->size);
		dest += block_iter->size;
    }

    // free memory of relocated driver
    delete[] psid_mem;

	/*if ( m_programSize > 37 * 1024)
    {
	// Use Exomizer to compress the program data. The first two bytes
	// of m_programData are skipped as these contain the load address.
	uint_least8_t* compressedData = new uint_least8_t[0x10000];
	m_programSize = exomizer(m_programData + 2, m_programSize - 2,
	                         load_addr, boot_addr, compressedData);
	// set BASIC line number
	compressedData[4] = (uint_least8_t) (lineNumber & 0xff);
	compressedData[5] = (uint_least8_t) (lineNumber >> 8);
	delete[] m_programData;
	m_programData = compressedData;
    }*/

    return true;
}


/*bool
Psid64::save(const char* fileName)
{
    // Open binary output file stream.
    openmode createAttr = std::ios::out;
#if defined(HAVE_IOS_BIN)
    createAttr |= std::ios::bin;
#else
    createAttr |= std::ios::binary;
#endif

    ofstream outfile(fileName, createAttr);
    return write(outfile);
}


bool
Psid64::write(ostream& out)
{
    if (!m_programData)
    {
	m_statusString = txt_noSidTuneConverted;
	return false;
    }

    out.write((const char*) m_programData, m_programSize);
    if (!out)
    {
	m_statusString = txt_fileIoError;
	return false;
    }

    return true;
}*/


//////////////////////////////////////////////////////////////////////////////
//            P R O T E C T E D   M E M B E R   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//              P R I V A T E   M E M B E R   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////

int_least32_t
Psid64::roundDiv(int_least32_t dividend, int_least32_t divisor)
{
    return (dividend + (divisor / 2)) / divisor;
}


bool
Psid64::convertNoDriver()
{
    const uint_least16_t load = m_tuneInfo.loadAddr;
    const uint_least16_t end = load + m_tuneInfo.c64dataLen;

    // allocate space for C64 program
    m_programSize = 2 + m_tuneInfo.c64dataLen;
    delete[] m_programData;
    m_programData = new uint_least8_t[m_programSize];

    // first the load address
    m_programData[0] = (uint_least8_t) (load & 0xff);
    m_programData[1] = (uint_least8_t) (load >> 8);

    // then copy the music data
    uint_least8_t c64buf[65536];
    m_tune.placeSidTuneInC64mem(c64buf);
    memcpy(m_programData + 2, &(c64buf[load]), m_tuneInfo.c64dataLen);

    // print memory map
    if (m_verbose)
    {
	/*cerr << "C64 memory map:" << endl;
	cerr << "  $" << toHexWord(load) << "-$" << toHexWord(end)
	     << "  Music data" << endl;*/
    }

    return true;
}


bool
Psid64::convertBASIC()
{
    const uint_least16_t load = m_tuneInfo.loadAddr;
    const uint_least16_t end = load + m_tuneInfo.c64dataLen;
    uint_least16_t bootCodeSize = m_compress ? 27 : 0;

    // allocate space for BASIC program and boot code (optional)
    m_programSize = 2 + m_tuneInfo.c64dataLen + bootCodeSize;
    delete[] m_programData;
    m_programData = new uint_least8_t[m_programSize];

    // first the load address
    m_programData[0] = (uint_least8_t) (load & 0xff);
    m_programData[1] = (uint_least8_t) (load >> 8);

    // then copy the BASIC program
    uint_least8_t c64buf[65536];
    m_tune.placeSidTuneInC64mem(c64buf);
    memcpy(m_programData + 2, &(c64buf[load]), m_tuneInfo.c64dataLen);

    /*if (m_compress)
    {
	uint_least16_t offs = 2 + m_tuneInfo.c64dataLen;
	// lda #0
	m_programData[offs++] = 0xa9;
	m_programData[offs++] = 0x00;
	// sta load-1
	m_programData[offs++] = 0x8d;
	m_programData[offs++] = (uint_least8_t) ((load - 1) & 0xff);
	m_programData[offs++] = (uint_least8_t) ((load - 1) >> 8);
	// lda #<load
	m_programData[offs++] = 0xa9;
	m_programData[offs++] = (uint_least8_t) (load & 0xff);
	// sta $2b
	m_programData[offs++] = 0x85;
	m_programData[offs++] = 0x2b;
	// lda #>load
	m_programData[offs++] = 0xa9;
	m_programData[offs++] = (uint_least8_t) (load >> 8);
	// sta $2c
	m_programData[offs++] = 0x85;
	m_programData[offs++] = 0x2c;
	// lda #<end
	m_programData[offs++] = 0xa9;
	m_programData[offs++] = (uint_least8_t) (end & 0xff);
	// sta $2d
	m_programData[offs++] = 0x85;
	m_programData[offs++] = 0x2d;
	// lda #>end
	m_programData[offs++] = 0xa9;
	m_programData[offs++] = (uint_least8_t) (end >> 8);
	// sta $2e
	m_programData[offs++] = 0x85;
	m_programData[offs++] = 0x2e;
	// jsr $a659
	m_programData[offs++] = 0x20;
	m_programData[offs++] = 0x59;
	m_programData[offs++] = 0xa6;
	// jmp $a7ae
	m_programData[offs++] = 0x4c;
	m_programData[offs++] = 0xae;
	m_programData[offs++] = 0xa7;

	// Use Exomizer to compress the program data. The first two bytes
	// of m_programData are skipped as these contain the load address.
	int start = end;
	uint_least8_t* compressedData = new uint_least8_t[0x10000];
	m_programSize = exomizer(m_programData + 2, m_programSize - 2, load, start, compressedData);
	delete[] m_programData;
	m_programData = compressedData;
    }*/

    // print memory map
    if (m_verbose)
    {
	/*cerr << "C64 memory map:" << endl;
	cerr << "  $" << toHexWord(load) << "-$" << toHexWord(end)
	     << "  BASIC program" << endl;*/
	if (m_compress)
	{
	    /*cerr << "  $" << toHexWord(end) << "-$"
		 << toHexWord(end + bootCodeSize)
		 << "  Post decompression boot code" << endl;*/
	}
    }

    return true;
}


bool
Psid64::formatStilText()
{
/*    m_stilText.clear();

    if (m_hvscRoot.empty())
    {
	return true;
    }

    // strip hvsc path from the file name
    string hvscFileName = m_fileName;
    size_t index = hvscFileName.find(m_hvscRoot);
    if (index != string::npos)
    {
	hvscFileName.erase(0, index + m_hvscRoot.length());
    }

    // convert backslashes to slashes (for DOS and Windows filenames)
    replace(hvscFileName.begin(), hvscFileName.end(), '\\', '/');

    string str;
    if (!m_stil->hasCriticalError() && m_useGlobalComment)
    {
	char* globalComment = m_stil->getGlobalComment(hvscFileName.c_str());
	if (globalComment != NULL)
	{
	    str += globalComment;
	}
    }
    if (!m_stil->hasCriticalError())
    {
	char* stilEntry = m_stil->getEntry(hvscFileName.c_str(), 0);
	if (stilEntry != NULL)
	{
	    str += stilEntry;
	}
    }
    if (!m_stil->hasCriticalError())
    {
	char* bugEntry = m_stil->getBug(hvscFileName.c_str(), 0);
	if (bugEntry != NULL)
	{
	    str += bugEntry;
	}
    }
    if (m_stil->hasCriticalError())
    {
	m_statusString = m_stil->getErrorStr();
	return false;
    }

    // convert the stil text and remove all double whitespace characters
    unsigned int n = str.length();
    m_stilText.reserve(n);

    // start the scroll text with some space characters (to separate end
    // from beginning and to make sure the color effect has reached the end
    // of the line before the first character is visible)
    for (unsigned int i = 0; i < (STIL_EOT_SPACES-1); ++i)
    {
	m_stilText += Screen::iso2scr(' ');
    }

    bool space = true;
    bool realText = false;
    for (unsigned int i = 0; i < n; ++i)
    {
	if (isspace(str[i]))
	{
	    space = true;
	}
	else
	{
	    if (space) {
	       m_stilText += Screen::iso2scr(' ');
	       space = false;
	    }
	    m_stilText += Screen::iso2scr(str[i]);
	    realText = true;
	}
    }

    // check if the message contained at least one graphical character
    if (realText)
    {
	// end-of-text marker
	m_stilText += 0xff;
    }
    else
    {
	// no STIL text at all
	m_stilText.clear();
    }
*/
    return true;
}


bool
Psid64::getSongLengths()
{
    bool have_songlengths = false;
    for (int i = 0; i < m_tuneInfo.songs; ++i)
    {
	// retrieve song length database information
	m_tune.selectSong(i + 1);

	int_least32_t length = 0;

	#if 0
	int_least32_t length = m_database.length (m_tune);
	if (length > 0)
	{
	    // maximum representable length is 99:59
	    if (length > 5999)
	    {
		length = 5999;
	    }

	    // store song length as binary-coded decimal minutes and seconds
	    uint_least8_t min = length / 60;
	    uint_least8_t sec = length % 60;
	    m_songlengthsData[i] = ((min / 10) << 4) | (min % 10);
	    m_songlengthsData[i + m_tuneInfo.songs] = ((sec / 10) << 4) | (sec % 10);
#if 0
	    if (m_verbose)
	    {
		cerr << "Length of song " << i + 1 << ": "
		     << setfill('0') << setw(2) << static_cast<int>(min) << ":"
		     << setfill('0') << setw(2) << static_cast<int>(sec) << endl;
	    }
#endif

	    // As floating point divisions are quite expensive on a 6502 instead
	    // a counter is used in the driver code to determine when to update
	    // the progress bar. The maximum error introduced by this method is:
	    // bar_pixels / ticks_per_sec / 2, which is 0.253333s for a 19 chars
	    // wide progress bar and 5 (NTSC) or 6 (PAL) ticks per frame.
	    const int_least32_t bar_pixels = BAR_WIDTH * 8;
	    const int_least32_t ticks_per_sec = 300; // PAL: 50 Hz * 6, NTSC: 60 Hz * 5
	    int_least32_t bartpi = roundDiv(length * ticks_per_sec, bar_pixels);
	    m_songlengthsData[i + (2 * m_tuneInfo.songs)] = bartpi & 0xff;
	    m_songlengthsData[i + (3 * m_tuneInfo.songs)] = (bartpi >> 8) & 0xff;
	    have_songlengths = true;
	}
	else
	#endif
	{
	    // no song length data for this song
	    m_songlengthsData[i] = 0x00;
	    m_songlengthsData[i + m_tuneInfo.songs] = 0x00;
	    m_songlengthsData[i + (2 * m_tuneInfo.songs)] = 0x00;
	    m_songlengthsData[i + (3 * m_tuneInfo.songs)] = 0x00;
	}
    }

    // Only need a block with song length data if at least one song has song
    // length data.
    if (have_songlengths)
    {
	m_songlengthsSize = 4 * m_tuneInfo.songs;
    }
    else
    {
	m_songlengthsSize = 0;
    }

    return true;
}


uint_least8_t
Psid64::findSonglengthsSpace(bool* pages, uint_least8_t scr,
                             uint_least8_t chars, uint_least8_t driver,
                             uint_least8_t stil, uint_least8_t stil_pages,
		             uint_least8_t size) const
{
    uint_least8_t firstPage = 0;
    for (unsigned int i = 0; i < MAX_PAGES; ++i)
    {
	if (pages[i] || (scr && (scr <= i) && (i < (scr + NUM_SCREEN_PAGES)))
	    || (chars && (chars <= i) && (i < (chars + NUM_CHAR_PAGES)))
	    || ((driver <= i) && (i < (driver + NUM_EXTDRV_PAGES)))
	    || (stil && (stil <= i) && (i < (stil + stil_pages))))
	{
	    if ((i - firstPage) >= size)
	    {
		return firstPage;
	    }
	    firstPage = i + 1;
	}
    }

    return 0;
}



uint_least8_t
Psid64::findStilSpace(bool* pages, uint_least8_t scr,
                      uint_least8_t chars, uint_least8_t driver,
		      uint_least8_t size) const
{
    uint_least8_t firstPage = 0;
    for (unsigned int i = 0; i < MAX_PAGES; ++i)
    {
	if (pages[i] || (scr && (scr <= i) && (i < (scr + NUM_SCREEN_PAGES)))
	    || (chars && (chars <= i) && (i < (chars + NUM_CHAR_PAGES)))
	    || ((driver <= i) && (i < (driver + NUM_EXTDRV_PAGES))))
	{
	    if ((i - firstPage) >= size)
	    {
		return firstPage;
	    }
	    firstPage = i + 1;
	}
    }

    return 0;
}


uint_least8_t
Psid64::findDriverSpace(bool* pages, uint_least8_t scr,
                        uint_least8_t chars, uint_least8_t size) const
{
    uint_least8_t firstPage = 0;
    for (unsigned int i = 0; i < MAX_PAGES; ++i)
    {
	if (pages[i] || (scr && (scr <= i) && (i < (scr + NUM_SCREEN_PAGES)))
	    || (chars && (chars <= i) && (i < (chars + NUM_CHAR_PAGES))))
	{
	    if ((i - firstPage) >= size)
	    {
		return firstPage;
	    }
	    firstPage = i + 1;
	}
    }

    return 0;
}

static bool hasCustomCharset = false;

void
Psid64::findFreeSpace()
/*--------------------------------------------------------------------------*
   In          : -
   Out         : m_driverPage		startpage of driver, 0 means no driver
		 m_screenPage		startpage of screen, 0 means no screen
		 m_charPage		startpage of chars, 0 means no chars
		 m_stilPage		startpage of stil, 0 means no stil
		 m_songlengthsPage	startpage of song lengths, 0 means N/A
   Return value: -
   Description : Find free space in the C64 memory map for the screen and the
		 driver code. Of course the driver code takes priority over
		 the screen.
   Globals     : psid			PSID header and data
   History     : 15-AUG-2001  RH  Initial version
		 21-SEP-2001  RH  Added support for screens located in the
				  memory ranges $4000-$8000 and $c000-$d000.
 *--------------------------------------------------------------------------*/
{
    bool pages[MAX_PAGES];
    unsigned int startp = m_tuneInfo.relocStartPage;
    unsigned int maxp = m_tuneInfo.relocPages;
    unsigned int i;
    unsigned int j;
    unsigned int k;
    uint_least8_t scr;
    uint_least8_t chars;
    uint_least8_t driver;

    // calculate size of the STIL text in pages
    uint_least8_t stilSize = 0;//(m_stilText.length() + 255) >> 8;
    uint_least8_t songlengthsSize = (m_songlengthsSize + 255) >> 8;

	hasCustomCharset = true;
restartBuild:
    stilSize = 0;//(m_stilText.length() + 255) >> 8;
    songlengthsSize = (m_songlengthsSize + 255) >> 8;
    startp = m_tuneInfo.relocStartPage;
    maxp = m_tuneInfo.relocPages;

    m_screenPage = (uint_least8_t) (0x00);
    m_driverPage = (uint_least8_t) (0x00);
    m_charPage = (uint_least8_t) (0x00);
    m_stilPage = (uint_least8_t) (0x00);
    m_songlengthsPage = (uint_least8_t) (0x00);

    if (startp == 0x00)
    {
	// Used memory ranges.
	unsigned int used[] = {
	    0x00, 0x03,
	    0xa0, 0xbf,
	    0xd0, 0xff,
	    0x00, 0x00		// calculated below
	};

	// Finish initialization by setting start and end pages.
	used[6] = m_tuneInfo.loadAddr >> 8;
	used[7] = (m_tuneInfo.loadAddr + m_tuneInfo.c64dataLen - 1) >> 8;

	// Mark used pages in table.
	for (i = 0; i < MAX_PAGES; ++i)
	{
	    pages[i] = false;
	}
	for (i = 0; i < sizeof(used) / sizeof(*used); i += 2)
	{
	    for (j = used[i]; j <= used[i + 1]; ++j)
	    {
		pages[j] = true;
	    }
	}
    }
    else if ((startp != 0xff) && (maxp != 0))
    {
	// the available pages have been specified in the PSID file
	unsigned int endp = min((startp + maxp), MAX_PAGES);

	// check that the relocation information does not use the following
	// memory areas: 0x0000-0x03FF, 0xA000-0xBFFF and 0xD000-0xFFFF
	if ((startp < 0x04)
	    || ((0xa0 <= startp) && (startp <= 0xbf))
	    || (startp >= 0xd0)
	    || ((endp - 1) < 0x04)
	    || ((0xa0 <= (endp - 1)) && ((endp - 1) <= 0xbf))
	    || ((endp - 1) >= 0xd0))
	{
	    return;
	}

	for (i = 0; i < MAX_PAGES; ++i)
	{
	    pages[i] = ((startp <= i) && (i < endp)) ? false : true;
	}
    }
    else
    {
	// not a single page is available
	return;
    }

    driver = 0;
    for (i = 0; i < 4; ++i)
    {
	// Calculate the VIC bank offset. Screens located inside banks 1 and 3
	// require a copy the character rom in ram. The code below uses a
	// little trick to swap bank 1 and 2 so that bank 0 and 2 are checked
	// before 1 and 3.
	uint_least8_t bank = (((i & 1) ^ (i >> 1)) ? i ^ 3 : i) << 6;

	for (j = 0; j < 0x40; j += 4)
	{
	    // screen may not reside within the char rom mirror areas
	    if (!(bank & 0x40) && (0x10 <= j) && (j < 0x20))
		continue;

	    // check if screen area is available
	    scr = bank + j;
	    if (pages[scr] || pages[scr + 1] || pages[scr + 2]
		|| pages[scr + 3])
		continue;

		// hack: try space for a custom charset!
		if ( hasCustomCharset || bank & 0x40 )
		{
			// The char rom needs to be copied to RAM so let's try to find
			// a suitable location.
			for ( k = 0; k < 0x40; k += 8 )
			{
				// char rom area may not overlap with screen area
				if ( k == ( j & 0x38 ) )
					continue;

				// check if character rom area is available
				chars = bank + k;
				if ( pages[ chars ] || pages[ chars + 1 ]
					 || pages[ chars + 2 ] || pages[ chars + 3 ] )
					continue;

				driver =
					findDriverSpace( pages, scr, chars, NUM_EXTDRV_PAGES );
				if ( driver )
				{
					m_driverPage = driver;
					m_screenPage = scr;
					m_charPage = chars;
					if ( stilSize )
					{
						m_stilPage = findStilSpace( pages, scr, chars,
													driver, stilSize );
					}
					if ( songlengthsSize )
					{
						m_songlengthsPage = findSonglengthsSpace(
							pages, scr, chars, driver, m_stilPage,
							stilSize, songlengthsSize );
					}
					return;
				}
			}
		}
	    else
	    {
		driver = findDriverSpace(pages, scr, 0, NUM_EXTDRV_PAGES);
		if (driver)
		{
		    m_driverPage = driver;
		    m_screenPage = scr;
		    if (stilSize)
		    {
			m_stilPage = findStilSpace(pages, scr, 0, driver,
						   stilSize);
		    }
		    if (songlengthsSize)
		    {
			m_songlengthsPage = findSonglengthsSpace(
			    pages, scr, 0, driver, m_stilPage, stilSize,
			    songlengthsSize);
		    }
		    return;
		}
	    }
	}
    }

	if ( !driver && hasCustomCharset )
	{
		hasCustomCharset = false;
		goto restartBuild;
	}

    if (!driver)
    {
	driver = findDriverSpace(pages, 0, 0, NUM_MINDRV_PAGES);
	m_driverPage = driver;
    }
}


//-------------------------------------------------------------------------
// Temporary hack till real bank switching code added

//  Input: A 16-bit effective address
// Output: A default bank-select value for $01.
uint8_t Psid64::iomap(uint_least16_t addr)
{
    // Force Real C64 Compatibility
    if (m_tuneInfo.compatibility == SIDTUNE_COMPATIBILITY_R64)
	return 0;     // Special case, converted to 0x37 later

    if (addr == 0)
	return 0;     // Special case, converted to 0x37 later
    if (addr < 0xa000)
	return 0x37;  // Basic-ROM, Kernal-ROM, I/O
    if (addr  < 0xd000)
	return 0x36;  // Kernal-ROM, I/O
    if (addr >= 0xe000)
	return 0x35;  // I/O only
    return 0x34;  // RAM only
}


void
Psid64::initDriver(uint_least8_t** mem, uint_least8_t** ptr, int* n)
{
    static const uint_least8_t psid_driver[] = {
#include "psiddrv.h"
    };
    static const uint_least8_t psid_extdriver[] = {
#include "psidextdrv.h"
    };
    const uint_least8_t* driver;
    uint_least8_t* psid_mem;
    uint_least8_t* psid_reloc;
    int psid_size;
    uint_least16_t reloc_addr;
    uint_least16_t addr;

    *ptr = NULL;
    *n = 0;

    // select driver
    if (m_screenPage == 0x00)
    {
	psid_size = sizeof(psid_driver);
	driver = psid_driver;
    }
    else
    {
	psid_size = sizeof(psid_extdriver);
	driver = psid_extdriver;
    }

    // Relocation of C64 PSID driver code.
    psid_mem = psid_reloc = new uint_least8_t[psid_size];
    if (psid_mem == NULL)
    {
	return;
    }
    memcpy(psid_reloc, driver, psid_size);
    reloc_addr = m_driverPage << 8;

    // undefined references in the driver code need to be added to globals
    GLOBALS_BLOCK globals;
    //setThemeGlobals(globals, m_theme);
    int screen = m_screenPage << 8;
    globals.screen = screen;
    int screen_songnum = 0;
    if (m_tuneInfo.songs > 1)
    {
	screen_songnum = screen + (10*40) + 24;
	if (m_tuneInfo.songs >= 100) ++screen_songnum;
	if (m_tuneInfo.songs >= 10) ++screen_songnum;
    }
    globals.screen_songnum = screen_songnum;
    int sid2base;
    if (((m_tuneInfo.secondSIDAddress & 1) == 0)
        && (((0x42 <= m_tuneInfo.secondSIDAddress) && (m_tuneInfo.secondSIDAddress <= 0x7e))
         || ((0xe0 <= m_tuneInfo.secondSIDAddress) && (m_tuneInfo.secondSIDAddress <= 0xfe))))
    {
	sid2base = (m_tuneInfo.secondSIDAddress * 0x10) + 0xd000;
    }
    else
    {
	sid2base = 0xd400;
    }
    globals.sid2base = sid2base;
    int sid3base;
    if (((m_tuneInfo.thirdSIDAddress & 1) == 0)
        && (((0x42 <= m_tuneInfo.thirdSIDAddress) && (m_tuneInfo.thirdSIDAddress <= 0x7e))
         || ((0xe0 <= m_tuneInfo.thirdSIDAddress) && (m_tuneInfo.thirdSIDAddress <= 0xfe)))
        && (m_tuneInfo.thirdSIDAddress != m_tuneInfo.secondSIDAddress))
    {
	sid3base = (m_tuneInfo.thirdSIDAddress * 0x10) + 0xd000;
    }
    else
    {
	sid3base = 0xd400;
    }
    globals.sid3base = sid3base;
    globals.stil = m_stilPage * 0x100;
    if (m_songlengthsPage != 0x00)
    {
        globals.songlengths_min = m_songlengthsPage * 0x100;
        globals.songlengths_sec = (m_songlengthsPage * 0x100) + m_tuneInfo.songs;
        globals.songtpi_lo = (m_songlengthsPage * 0x100) + (2 * m_tuneInfo.songs);
        globals.songtpi_hi = (m_songlengthsPage * 0x100) + (3 * m_tuneInfo.songs);
    }
    else
    {
        globals.songlengths_min = 0x0000;
        globals.songlengths_sec = 0x0000;
        globals.songtpi_lo = 0x0000;
        globals.songtpi_hi = 0x0000;
    }

    if (!reloc65 ((char **) &psid_reloc, &psid_size, reloc_addr, &globals))
    {
	//cerr << ": Relocation error." << endl;
	return;
    }

    // Skip JMP table
    addr = 6;

    // Store parameters for PSID player.
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.initAddr ? 0x4c : 0x60);
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.initAddr & 0xff);
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.initAddr >> 8);
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.playAddr ? 0x4c : 0x60);
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.playAddr & 0xff);
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.playAddr >> 8);
    psid_reloc[addr++] = (uint_least8_t) (m_tuneInfo.songs);

    // get the speed bits (the driver only has space for the first 32 songs)
    uint_least32_t speed = 0;
    unsigned int songs = min(m_tuneInfo.songs, 32);
    for (unsigned int i = 0; i < songs; ++i)
    {
	if (m_tune[i + 1].songSpeed != SIDTUNE_SPEED_VBI)
	{
	    speed |= (1 << i);
	}
    }
    psid_reloc[addr++] = (uint_least8_t) (speed & 0xff);
    psid_reloc[addr++] = (uint_least8_t) ((speed >> 8) & 0xff);
    psid_reloc[addr++] = (uint_least8_t) ((speed >> 16) & 0xff);
    psid_reloc[addr++] = (uint_least8_t) (speed >> 24);

    psid_reloc[addr++] = (uint_least8_t) ((m_tuneInfo.loadAddr < 0x31a) ? 0xff : 0x05);
    psid_reloc[addr++] = iomap (m_tuneInfo.initAddr);
    psid_reloc[addr++] = iomap (m_tuneInfo.playAddr);

    *mem = psid_mem;
    *ptr = psid_reloc;
    *n = psid_size;
}


/*void
Psid64::addFlag(bool &hasFlags, const string &flagName)
{
    if (hasFlags)
    {
	m_screen->write(", ");
    }
    else
    {
	hasFlags = true;
    }
    m_screen->write(flagName);
}*/

void
Psid64::addFlag(bool &hasFlags, const char *flagName)
{
    if (hasFlags)
    {
	m_screen->write(", ");
    }
    else
    {
	hasFlags = true;
    }
    m_screen->write(flagName);
}

static char tmpStr[512];

char*
Psid64::toHexWord(uint16_t value) const
{
	sprintf( tmpStr, "%X", value );
	return tmpStr;
}

char*
Psid64::toNumStr(int value) const
{
	sprintf( tmpStr, "%d", value );
	return tmpStr;
}

/*string
Psid64::toHexWord(uint16_t value) const
{
    ostringstream oss;

    oss << hex << uppercase << setfill('0') << setw(4) <<  value;

    return string(oss.str());
}


string
Psid64::toNumStr(int value) const
{
    ostringstream oss;

    oss << dec << value;

    return string(oss.str());
}*/


void
Psid64::drawScreen()
{
    m_screen->clear();

    // set title
    m_screen->move(2,1);
//    m_screen->write("PSID64 v" VERSION " by Roland Hermans!");
    m_screen->write("PSID64 v1.2 by Roland Hermans");

    // characters for color line effect
/*    m_screen->poke( 4-3, 0, 0x70);
    m_screen->poke(35-4, 0, 0x6e);
    m_screen->poke( 4-3, 1, 0x5d);
    m_screen->poke(35-4, 1, 0x5d);
    m_screen->poke( 4-3, 2, 0x6d);
    m_screen->poke(35-4, 2, 0x7d);*/
    for (unsigned int i = 0; i < 29; ++i)
    {
	m_screen->poke(5 + i-3, 0, 0x40);
	m_screen->poke(5 + i-3, 2, 0x40);
    }

    // information lines
    m_screen->move(2, 4);
    m_screen->write("Name   : ");
//    m_screen->write(string(m_tuneInfo.infoString[0]).substr(0,31));
	m_tuneInfo.infoString[0][32] = 0;
    m_screen->write(m_tuneInfo.infoString[0]);

    m_screen->write("\n  Author : ");
	m_tuneInfo.infoString[1][32] = 0;
    m_screen->write(m_tuneInfo.infoString[1]);

    m_screen->write("\n  Release: ");
	m_tuneInfo.infoString[2][32] = 0;
    m_screen->write(m_tuneInfo.infoString[2]);

    m_screen->write("\n  Load   : $");
    m_screen->write(toHexWord(m_tuneInfo.loadAddr));
    m_screen->write("-$");
    m_screen->write(toHexWord(m_tuneInfo.loadAddr + m_tuneInfo.c64dataLen));

    m_screen->write("\n  Init   : $");
    m_screen->write(toHexWord(m_tuneInfo.initAddr));

    m_screen->write("\n  Play   : ");
    if (m_tuneInfo.playAddr)
    {
	m_screen->write("$");
	m_screen->write(toHexWord(m_tuneInfo.playAddr));
    }
    else
    {
	m_screen->write("N/A");
    }

    m_screen->write("\n  Songs  : ");
    m_screen->write(toNumStr(m_tuneInfo.songs));
    if (m_tuneInfo.songs > 1)
    {
	m_screen->write(" (now playing");
    }

    bool hasFlags = false;
    m_screen->write("\n  Flags  : ");
    if (m_tuneInfo.compatibility == SIDTUNE_COMPATIBILITY_PSID)
    {
	addFlag(hasFlags, "PlaySID");
    }
    switch (m_tuneInfo.clockSpeed)
    {
    case SIDTUNE_CLOCK_PAL:
	addFlag(hasFlags, "PAL");
	break;
    case SIDTUNE_CLOCK_NTSC:
	addFlag(hasFlags, "NTSC");
	break;
    case SIDTUNE_CLOCK_ANY:
	addFlag(hasFlags, "PAL/NTSC");
        break;
    default:
        break;
    }
    switch (m_tuneInfo.sidModel)
    {
    case SIDTUNE_SIDMODEL_6581:
	addFlag(hasFlags, "6581");
	break;
    case SIDTUNE_SIDMODEL_8580:
	addFlag(hasFlags, "8580");
	break;
    case SIDTUNE_SIDMODEL_ANY:
	addFlag(hasFlags, "6581/8580");
	break;
    default:
        break;
    }
    int sid2base;
    if (((m_tuneInfo.secondSIDAddress & 1) == 0)
        && (((0x42 <= m_tuneInfo.secondSIDAddress) && (m_tuneInfo.secondSIDAddress <= 0x7e))
         || ((0xe0 <= m_tuneInfo.secondSIDAddress) && (m_tuneInfo.secondSIDAddress <= 0xfe))))
    {
	sid2base = (m_tuneInfo.secondSIDAddress * 0x10) + 0xd000;
    }
    else
    {
	sid2base = 0;
    }
    int sid3base;
    if (((m_tuneInfo.thirdSIDAddress & 1) == 0)
        && (((0x42 <= m_tuneInfo.thirdSIDAddress) && (m_tuneInfo.thirdSIDAddress <= 0x7e))
         || ((0xe0 <= m_tuneInfo.thirdSIDAddress) && (m_tuneInfo.thirdSIDAddress <= 0xfe)))
        && (m_tuneInfo.thirdSIDAddress != m_tuneInfo.secondSIDAddress))
    {
	sid3base = (m_tuneInfo.thirdSIDAddress * 0x10) + 0xd000;
    }
    else
    {
	sid3base = 0;
    }
    if (sid2base != 0)
    {
		// todo
		char b[512];
//        ostringstream oss;
	//oss << ((sid3base == 0) ? " at $" : "@") << toHexWord(sid2base);
	switch (m_tuneInfo.sid2Model)
	{
	case SIDTUNE_SIDMODEL_6581:
		sprintf( b, "6581%s%X",((sid3base == 0) ? " at $" : "@"), sid2base );
	    addFlag(hasFlags, b );
	    break;
	case SIDTUNE_SIDMODEL_8580:
		sprintf( b, "8580%s%X",((sid3base == 0) ? " at $" : "@"), sid2base );
   	    addFlag(hasFlags, b );
//addFlag(hasFlags, "8580" + oss.str());
	    break;
	case SIDTUNE_SIDMODEL_ANY:
		sprintf( b, "6581/8580%s%X",((sid3base == 0) ? " at $" : "@"), sid2base );
   	    addFlag(hasFlags, b );
	    //addFlag(hasFlags, "6581/8580" + oss.str());
	    break;
	default:
		sprintf( b, "SID%s%X",((sid3base == 0) ? " at $" : "@"), sid2base );
   	    addFlag(hasFlags, b );
//	    addFlag(hasFlags, "SID" + oss.str());
            break;
	}
    }
    if (sid3base != 0)
    {
		char b[64];
      //  ostringstream oss;
	//oss << "@" << toHexWord(sid3base);
	switch (m_tuneInfo.sid3Model)
	{
	case SIDTUNE_SIDMODEL_6581:
		sprintf( b, "6581@%X", sid3base );
	    addFlag(hasFlags, b );
//	    addFlag(hasFlags, "6581" + oss.str());
	    break;
	case SIDTUNE_SIDMODEL_8580:
		sprintf( b, "8580@%X", sid3base );
	    addFlag(hasFlags, b );
//	    addFlag(hasFlags, "8580" + oss.str());
	    break;
	case SIDTUNE_SIDMODEL_ANY:
		sprintf( b, "6581/8580@%X", sid3base );
	    addFlag(hasFlags, b );
	    //addFlag(hasFlags, "6581/8580" + oss.str());
	    break;
	default:
		sprintf( b, "SID@%X", sid3base );
	    addFlag(hasFlags, b );
//	    addFlag(hasFlags, "SID" + oss.str());
            break;
	}
    }
    if (!hasFlags)
    {
	m_screen->write("-");
    }

    //m_screen->write("\n");
/*    m_screen->write("\n  Player : ");
    string playerName = m_playerId;
    if (playerName.empty())
    {
        playerName = "<?>";
    }
    else
    {
        std::replace(playerName.begin(), playerName.end(), '_', ' ');
    }
    m_screen->write(playerName);*/

    m_screen->write("\n  Clock  :   :   ");
    if (m_songlengthsPage != 0)
    {
	uint_least8_t bar_char = (m_charPage == 0) ? 0xa0 : 0x7f;
 	for (unsigned int i = 0; i < BAR_WIDTH; ++i)
	{
	    m_screen->poke(BAR_X + i, 13, bar_char);
	}
	m_screen->poke(BAR_X + BAR_WIDTH + 3, 13, 0x3a);
    }

    // some additional text
    m_screen->write("\n\n  ");
    if (m_tuneInfo.songs <= 1)
    {
	m_screen->write("   [1");
    }
    else if (m_tuneInfo.songs <= 10)
    {
	m_screen->write("  [1-");
	m_screen->putchar((m_tuneInfo.songs % 10) + '0');
    }
    else if (m_tuneInfo.songs <= 11)
    {
	m_screen->write(" [1-0, A");
    }
    else
    {
	m_screen->write("[1-0, A-");
	m_screen->putchar(m_tuneInfo.songs <= 36 ? m_tuneInfo.songs - 11 + 'A' : 'Z');
    }
    m_screen->write("] Select song [+] Next song\n");
    m_screen->write("  [-] Previous song [DEL] Blank screen\n");
    if (m_tuneInfo.playAddr)
    {
	m_screen->write("    [~] Fast forward [\x5e] raster time\n");
/*		for ( int i = 0; i < 40; i++ )
		{
			char b[2];
			b[0] = i+80; b[1]=0;
			m_screen->write(b);
		}
	m_screen->write("\n");*/
    }
    m_screen->write(" [RUN/STOP] Stop [CBM] Go to Sidekick64\n");

    // flashing bottom line (should be exactly 38 characters)
    m_screen->move(1,24);
    m_screen->write("Website: http://psid64.sourceforge.net");

	if ( hasCustomCharset )
	{
		int a = 91;

		for ( int j = 0; j < 4; j++ )
			for ( int i = 0; i < 7; i++ )
			{
//                m_screen->poke( 16 + i + (j+20-5) * 40, (a++) );
                m_screen->poke( 33 + i + j * 40, (a++) );
				//c64screen[ i + 33 + j * 40 ] = (a++);
				//c64color[ i + 33 + j * 40 ] = j < 2 ? skinValues.SKIN_MENU_TEXT_ITEM : skinValues.SKIN_MENU_TEXT_KEY;
			}
	}

    // initialize sprite for progress bar
    for (unsigned int i = 0; i < 63; ++i)
    {
	m_screen->poke(BAR_SPRITE_SCREEN_OFFSET + i, 0x00);
    }
}
