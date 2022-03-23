/*
  _________.__    .___      __   .__        __            _____                       
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __       /     \   ____   ____  __ __ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      /  \ /  \_/ __ \ /    \|  |  \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /    Y    \  ___/|   |  \  |  /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \____|__  /\___  >___|  /____/ 
        \/         \/    \/     \/       \/     \/             \/     \/     \/       
 
 kernel_menu20.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Menu: ugly glue code to expose some functionality in one menu with browser
 Copyright (c) 2019-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

#include "kernel_menu20.h"
#include "dirscan.h"
#include "disk_emulation.h"
#include "config.h"
#include "vic20screen.h"
#include "vic20config.h"
#include <math.h>
#include "charlogo.h"

u8 activateVFLIHack = 0;
u8 bankVFLI = 0x0f;

#define VIC_MODE
//#define VIC_EXCLUSIVE
#define HDMI_SOUND
#define SAMPLERATE 32000

// we will read these files
static const char DRIVE[] = "SD:";
static const char FILENAME_PRG[] = "SD:VC20/rpimenu20.prg";		// .PRG to start
static const char FILENAME_A0CBM[] = "SD:VC20/launch20.a0cbm";	// launch code (A0CBM 8k cart)
static const char FILENAME_A0CBM35K[] = "SD:VC20/launch20_35k.a0cbm";	// launch code (A0CBM 8k cart)
static const char FILENAME_A0CRTSYNC[] = "SD:VC20/crtsync20.a0cbm";	// sync code (A0CBM 8k cart)
static const char FILENAME_A0BASICSYNC[] = "SD:VC20/basicsync20.a0cbm";	// sync code (A0CBM 8k cart)
static const char FILENAME_CONFIG[] = "SD:VC20/sidekick20.cfg";
static const char FILENAME_CHARROM[] = "SD:VC20/charrom.bin";

static const char FILENAME_SPLASH_HELP[] = "SD:VC20/sk20_help2.raw";
static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk20_main.tga";
static const char FILENAME_SPLASH_LAUNCH[] = "SD:SPLASH/sk20_launch.tga";
static const char FILENAME_SPLASH_DISK[] = "SD:SPLASH/sk20_disk.tga";
static const char FILENAME_SPLASH_CART[] = "SD:SPLASH/sk20_cart.tga";
static const char FILENAME_SPLASH_BASIC[] = "SD:SPLASH/sk20_basic.tga";

#define VIC20_READ_WRITE		(1)
#define VIC20_READ_ONLY 		(2)
#define VIC20_RAMx_DISABLED		(0<<0)
#define VIC20_RAMx_READ_WRITE	(1<<0)
#define VIC20_RAMx_READ_ONLY	(2<<0)
#define VIC20_BLK1(m)			((m)<<2)
#define VIC20_BLK1_DISABLED		(0<<2)
#define VIC20_BLK1_READ_WRITE	(1<<2)
#define VIC20_BLK1_READ_ONLY	(2<<2)
#define VIC20_BLK2(m)			((m)<<4)
#define VIC20_BLK2_DISABLED		(0<<4)
#define VIC20_BLK2_READ_WRITE	(1<<4)
#define VIC20_BLK2_READ_ONLY	(2<<4)
#define VIC20_BLK3(m)			((m)<<6)
#define VIC20_BLK3_DISABLED		(0<<6)
#define VIC20_BLK3_READ_WRITE	(1<<6)
#define VIC20_BLK3_READ_ONLY	(2<<6)
#define VIC20_BLK5(m)			((m)<<8)
#define VIC20_BLK5_DISABLED		(0<<8)
#define VIC20_BLK5_READ_WRITE	(1<<8)
#define VIC20_BLK5_READ_ONLY	(2<<8)

u32 vic20MemoryConfigurationDefault = VIC20_RAMx_READ_WRITE | VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE | VIC20_BLK5_READ_WRITE;
u32 vic20MemoryConfigurationManual = vic20MemoryConfigurationDefault;
u32 vic20MemoryConfiguration = vic20MemoryConfigurationDefault;

static const char FILENAME_FONT5[] = "SD:VC20/font5x8b.raw";
static const char FILENAME_FONT6[] = "SD:VC20/font6x8b.raw";
int fontW = 5,
	fontH = 8;
unsigned char fontBitmap[ 256 * 64 ];

static u32	disableCart     = 0;
static u32	resetCounter    = 0;
static u32	transferStarted = 0;

// the menu .PRG and program to launch
u32 prgSize AAA;
unsigned char prgData[ 65536 + 64 ] AAA;

u32 prgSizeLaunch AAA;
unsigned char prgDataLaunch[ 65536 ] AAA;

u32 prgCurSize = 0;
unsigned char *prgCurLaunch;

u8 helpscreenRAW[ 30720 ] AAA;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static u8 launchCode[ 2048 ] AAA;
static u8 launchCode35k[ 2048 ] AAA;
static u8 crtSyncCode[ 256 ] AAA;
static u8 basicSyncCode[ 256 ] AAA;
u8 injectVICSYNC = 0, disableDiskIO = 0, basicSyncDisableBLK5 = 0;

u8 cart_ram[ 1024 * ( 52 + 16 + 4 ) ] AAA;
u8 *charROM = &cart_ram[ 1024 * 68 ];
u8 *vfli_ram = &cart_ram[ 1024 * 52 ]; // only for VFLI

u8 framebuffer[ 4096 * 1 ] AAA;
u8 colorbuffer[ 256 ] AAA;
u8 *pTransfer = &framebuffer[ 0 ];
static u8 nextByte = 0;

static bool loadingPrg = false;
static bool savingPrg = false;
unsigned char magicString[6] = { 41, 181, 67, 191, 101, 227 };

// disk API defined in launch code
#define FILENAME_ADDR           0x9c00
#define FILENAME_LEN_ADDR       0x9cff
#define BASIC_COMMANDS_ADDR     0x9d00
#define DEVICE_NUMBER     		0x9d39

void setDriveAndBasicCommand( const char *cmd )
{
	extern u8 cfgDeviceNumber;

	int idx = 0;
	memcpy( (char *)&cart_ram[ BASIC_COMMANDS_ADDR ], cmd, strlen( cmd ) );
	for ( int i = 0; i < (int)strlen( cmd ); i++ )
	{
		if ( cmd[ i ] == 2 )
		{
			if ( cfgDeviceNumber >= 10 )
				cart_ram[ BASIC_COMMANDS_ADDR + (idx++) ] = '0' + (cfgDeviceNumber/10);
			cart_ram[ BASIC_COMMANDS_ADDR + (idx++) ] = '0' + (cfgDeviceNumber%10);
		} else
		if ( cmd[ i ] == 3 )
		{
			cart_ram[ BASIC_COMMANDS_ADDR + (idx++) ] = 0; 
		} else
			cart_ram[ BASIC_COMMANDS_ADDR + (idx++) ] = cmd[ i ]; 
	
	}
	cart_ram[ DEVICE_NUMBER ] = cfgDeviceNumber;
}


u32 releaseDMA = 0;
u32 doneWithHandling = 1;
u32 pullIRQ = 0;
u32 releaseIRQ = 0;
u32 c64CycleCount = 0;
u32 nBytesRead = 0;
u32 first = 1;

u32 cartMode = 0; // 0 == launcher, 1 == c1lo cartridge from SD-card, 2 == C1lo+hi cartridge


// this FILENAME may contain 2 strings at ofs 0 and 2048
#define MAX_FILENAME_LENGTH	2048
char FILENAME[ MAX_FILENAME_LENGTH * 2 ];

static u32 launchKernel = 0;
u32 lastChar = 0;

char filenameKernal[ 2048 ];

CLogger			*logger;
CScreenDevice	*screen;


u32 loadFileinMemory( const char *fn, u32 *memCfg = NULL, u32 mode = VIC20_READ_ONLY )
{
	u32 size, addr;
	u8 temp[ 65536 ];

	readFile( logger, DRIVE, fn, temp, &size );

	addr = temp[ 0 ] + temp[ 1 ] * 256;

	memcpy( &cart_ram[ addr ], &temp[ 2 ], size - 2 );

	if ( memCfg )
	{
		if ( addr >= 0x2000 && addr < 0x4000 )
		{
			*memCfg &= ~VIC20_BLK1( 3 );
			*memCfg |= VIC20_BLK1( mode );
		} else
		if ( addr >= 0x4000 && addr < 0x6000 )
		{
			*memCfg &= ~VIC20_BLK2( 3 );
			*memCfg |= VIC20_BLK2( mode );
		} else
		if ( addr >= 0x6000 && addr < 0x8000 )
		{
			*memCfg &= ~VIC20_BLK3( 3 );
			*memCfg |= VIC20_BLK3( mode );
		} else
		if ( addr >= 0xa000 && addr < 0xc000 )
		{
			*memCfg &= ~VIC20_BLK5( 3 );
			*memCfg |= VIC20_BLK5( mode );
		}
	}
	return addr;
}


u32 doActivateCart = 0;

extern void initVIC656x( bool VIC20PAL, bool debugBorders );
extern void tickVIC656x();

const bool VIC20_TYPE_PAL  = true;
const bool VIC20_TYPE_NTSC = false;

#include "vic656x_inline.h"

static u8 disableLatchesFIQ = 0;

int loadLogoTGA( const char *ofn )
{
	if ( screenType != 1 ) 
		return 0;

	char fn[ 1024 ];
	strncpy( fn, ofn, 1023 );
	int p = strlen( fn );
	while ( --p > 0 )
	{
		if ( fn[ p ] == '.' ) break;
	}
	fn[ p ] = 0;
	strcat( fn, ".tga" );

	disableLatchesFIQ = 1;
	clearI2CBuffer();
	#define CMD_RES	(2<<2)
	#define CMD_DC 	(3<<2)
	put4BitCommand( CMD_RES + 1 );
	put4BitCommand( CMD_DC  + 1 );

	int res = tftLoadBackgroundTGA( DRIVE, fn, 8 );
	if ( res )
	{
		tftCopyBackground2Framebuffer();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}
	disableLatchesFIQ = 0;
	return res;
}


void tftDisplayTGA( const char *fn )
{
	if ( screenType != 1 ) 
		return;

	disableLatchesFIQ = 1;
	flushI2CBuffer( false );

	for ( int i = 0; i < 120; i+=3 )
	{
		tftSetPartial( i, 240 - i );
		DELAY( 1 << 19 );
	}
	tftCommandImm( 0x28 );

	extern int screenRotation;
	tftInitImm( screenRotation );
	tftSetPartial( 240, 240 );
		
	tftLoadBackgroundTGA( DRIVE, fn, 8 );
	tftCopyBackground2Framebuffer();
	tftSendFramebuffer16BitImm( tftFrameBuffer );
	tftCommandImm( 0x29 );

	for ( int i = 120; i >= 0; i-=3 )
	{
		tftSetPartial( i, 240 - i );
		DELAY( 1 << 19 );
	}
	tftSetPartial( 0, 240 );

	flush4BitBuffer( false );

	disableLatchesFIQ = 0;
}

// cache warmup

volatile u8 forceRead;

__attribute__( ( always_inline ) ) inline void warmCache( void *fiqh )
{
	u32 totalMemSize = 72 * 1024;

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &cart_ram[ 0 ], totalMemSize, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( cart_ram, totalMemSize, totalMemSize * 8);

	CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], prgSize, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( prgData, prgSize, prgSize * 8 );

	CACHE_PRELOAD_DATA_CACHE( &crtSyncCode[ 0 ], 256, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( crtSyncCode, 256, 256 * 8 );
	CACHE_PRELOAD_DATA_CACHE( &basicSyncCode[ 0 ], 256, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( basicSyncCode, 256, 256 * 8 );

	CACHE_PRELOAD_DATA_CACHE( framebuffer, 4096, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( colorbuffer, 256, CACHE_PRELOADL2STRM );

	if ( fiqh )
	{
		CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)fiqh, 1024 * 8 );
		FORCE_READ_LINEARa( (void*)fiqh, 1024 * 8, 65536 );
	}

	CACHE_PRELOAD_DATA_CACHE( &cart_ram[ 0 ], totalMemSize, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( cart_ram, totalMemSize, totalMemSize * 8);

	CACHE_PRELOAD_DATA_CACHE( (u8*)&vic656x, sizeof( VICSTATE ), CACHE_PRELOADL1KEEP );
	FORCE_READ_LINEAR32a( cart_ram, sizeof( VICSTATE ), sizeof( VICSTATE ) * 8);
}

static void *myFIQHandler = NULL;


void resetStates()
{
	releaseDMA = 0;
	pullIRQ = 0;
	releaseIRQ = 0;
	c64CycleCount = 0;
	nBytesRead = 0;
	first = 1;
	launchKernel = 0;
	lastChar = 0;
}

void CKernelMenu::activateCart( bool imm, bool noFadeOut )
{
	disableLatchesFIQ = 1;
	m_InputPin.DisableInterrupt();
	resetStates();
	disableDiskIO = 0;
	disableCart = 0;
	transferStarted = 0;
	pullIRQ = releaseIRQ = 0;

	// if we had launched a CRT which requires that syncing, reactivate
	if ( injectVICSYNC == 2 || injectVICSYNC == 4 )
	{
		injectVICSYNC --; 
		if ( basicSyncDisableBLK5 )
			vic20MemoryConfiguration |= VIC20_BLK5_READ_ONLY;
	} else
		injectVICSYNC = 0;

	vic20MemoryConfiguration = vic20MemoryConfigurationDefault;

	latchSetClear( LATCH_LED0to1, LATCH_RESET | LATCH_ENABLE_KERNAL );
	flush4BitBuffer( false );
	latchSetClearImm( LATCH_LED0to1, LATCH_RESET | LATCH_ENABLE_KERNAL ); 

	SET_GPIO( bNMI | bDMA | bGAME | bEXROM );
	CLR_GPIO( bCTRL257 );
	SET_BANK2_OUTPUT

	if ( screenType == 0 )
	{
		splashScreen( sk20_splash );
	} else
	if ( screenType == 1 )
	{
		tftDisplayTGA( FILENAME_SPLASH_RGB );
	}

	if ( cartMode == 0 )
	{
		vic20MemoryConfiguration = vic20MemoryConfigurationDefault;
		memset( cart_ram, 0, 65536 );
		memcpy( &cart_ram[ 0xa000 ], launchCode35k, 8192 );

		const char basic_commands[] = "LOAD\"SIDEKICK20\",\2,1\3RUN:\3\0";
		setDriveAndBasicCommand( basic_commands );
	}

	doneWithHandling = 1;

	activateVFLIHack = 0;
	if ( cfgVIC_Emulation )
		initVIC656x( cfgVIC_Emulation == 2 ? VIC20_TYPE_NTSC : VIC20_TYPE_PAL );

	warmCache( (void*)myFIQHandler );
	warmCache( (void*)myFIQHandler );
	DELAY( 1 << 26 );

	c64CycleCount = 0;
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );
	DELAY( 1 << 26 );
	doActivateCart = 0;
	latchSetClearImm( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
	latchSetClear( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
	disableLatchesFIQ = 0;

	// some orchestration of a secondary reset (RPi caches again...)
	DELAY( 1 << 24 );
	latchSetClear( 0, LATCH_RESET | LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
	DELAY( 1 << 24 );
	latchSetClear( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
}

void CKernelMenu::resetVC20()
{
	disableLatchesFIQ = 1;
	m_InputPin.DisableInterrupt();
	SET_GPIO( bNMI | bDMA | bGAME | bEXROM );
	CLR_GPIO( bCTRL257 );
	SET_BANK2_OUTPUT
	latchSetClearImm( LATCH_LED0to1, LATCH_RESET | LATCH_ENABLE_KERNAL );
	latchSetClear( LATCH_LED0to1, LATCH_RESET | LATCH_ENABLE_KERNAL );

	flushI2CBuffer( true );

	activateVFLIHack = 0;
	//if ( cfgVIC_Emulation )
	//	initVIC656x( cfgVIC_Emulation == 2 ? VIC20_TYPE_NTSC : VIC20_TYPE_PAL );

	warmCache( (void*)myFIQHandler );
	c64CycleCount = 0;
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );
	DELAY( 1 << 20 );
	doneWithHandling = 1;
	doActivateCart = 0;
	latchSetClearImm( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
	latchSetClear( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
	disableLatchesFIQ = 0;

	DELAY( 1 << 24 );
	latchSetClear( 0, LATCH_RESET | LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
	DELAY( 1 << 24 );
	latchSetClear( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
}

#ifdef COMPILE_MENU_WITH_SOUND
CTimer				*pTimer;
//CScheduler			*pScheduler;
CInterruptSystem	*pInterrupt;
CVCHIQDevice		*pVCHIQ;
#endif

#define SETPIXEL( fb, x, y, cb, c ) \
	{ fb[ ((y) / 16) * 384 + ((x) / 8) * 16 + ( (y) & 15 ) ] |= ( 1 << ( (7-x) & 7 ) ); \
		if ( c ) { cb[ (x/8)+((y/16)*24) ] = c; \
		cb[ ((x)/8)+(((y)/16)*24) ] = c; } }
#define SETPIXEL_NOCOLOR( fb, x, y  ) \
	{ fb[ ((y) / 16) * 384 + ((x) / 8) * 16 + ( (y) & 15 ) ] |= ( 1 << ( (7-x) & 7 ) ); }

void convertScreenToBitmap( unsigned char *framebuffer )
{
	u32 columns = 192 / fontW;
	u32 rows = 160 / fontH;

	if ( columns > 40 ) columns = 40;

	memset( colorbuffer, 0, 256 );
	memset( framebuffer, 0, 4096 );

	for ( u32 j = 0, y = 0; j < rows; j++, y += fontH )
	{
		for ( u32 i = 0, x = 0; i < columns; i++, x += fontW )
		{
			unsigned char c = c64screen[ i + j * 40 ];
			unsigned char col = c64color[ i + j * 40 ];

			u32 fx = (c & 31) * fontW;
			u32 fy = ((c&127) >> 5) * fontH;

			if ( c != 32 )
			for ( int b = 0; b < fontH; b++ )
				for ( int a = 0; a < fontW; a++ )
				{
					unsigned char v = fontBitmap[ fx + a + ( fy + b ) * 32 * fontW ];

					// what the hell this is? an attempt to automatically create an italic letter and fixing broken lines
					if ( col & 128 )
					{
						if ( a > 0 && b > 0 )
						{
							u8 vl = fontBitmap[ fx + a - 1 + ( fy + b ) * 32 * fontW ];
							u8 vt = fontBitmap[ fx + a + ( fy + (b - 1) ) * 32 * fontW ];
							u8 vtl = fontBitmap[ fx + a - 1 + ( fy + (b - 1) ) * 32 * fontW ];
							if ( vt >= 128 && vl >= 128 && vtl < 128 )
							{
								if ( ((b>>2) != ((b-1)>>2)) )
									v = 255;
							}
						}
					}

					if ( col & 16 )
					{
						// some nice dilatation for the low-res
						u8 vt = fontBitmap[ fx + a + ( fy + min(fontH-1,b + 1) ) * 32 * fontW ];
						u8 vb = fontBitmap[ fx + a + ( fy + max(0,b - 1) ) * 32 * fontW ];
						if ( vt >= 128 && !(vb >= 128) ) v |= 128;
						if ( vt >= 128 && b == 0 ) v |= 128;
					}

					if ( (c & 0x80) && c != 190 )
						v = 255 - v;

					if ( col & 32 ) // "gray out"
						if ( ((a+b)%3)== 0 ) v = 0;


					if ( v >= 128 )
					{
						// italic
						if ( col & 128 )
						{
							SETPIXEL( framebuffer, (a+x+((fontH-1-b)>>2)), (b+y), colorbuffer, col & 15 ); 
						} else
						{
							SETPIXEL( framebuffer, (a+x), (b+y), colorbuffer, col & 15 );
						}
					}
				}
		}
	}

	extern u32 menuScreen;
	if ( menuScreen != 1 )
	for ( u32 j = 0; j < 4 * 8; j++ )
		for ( u32 i = 0; i < 7 * 8; i++ )
		{
			u32 c = j < 2 * 8 ? 5 : 3;
			u32 charOfs = ( ( i / 8 ) + ( j / 8 ) * 7 ) * 8;
			charOfs += j & 7;
			u8 l = ( skcharlogo_raw[ charOfs ] >> ( 7 - ( i & 7 ) ) ) & 1;
			if ( l > 0 )
				SETPIXEL( framebuffer, (i+192-7*8), (j), colorbuffer, c );
		}
}

static int scanlineIntensity = 256;

const unsigned char viridisRGB[384] = {
    0x2B, 0x00, 0x3C, 0x2B, 0x00, 0x3C, 0x34, 0x00, 0x48, 0x35, 0x00, 0x4A, 0x36, 0x00, 0x4C, 0x36,
    0x00, 0x4F, 0x36, 0x04, 0x52, 0x37, 0x09, 0x55, 0x37, 0x0B, 0x57, 0x37, 0x0D, 0x5A, 0x37, 0x0F,
    0x5C, 0x37, 0x12, 0x5E, 0x37, 0x14, 0x61, 0x37, 0x16, 0x63, 0x37, 0x17, 0x65, 0x37, 0x1A, 0x67,
    0x36, 0x1D, 0x68, 0x36, 0x1E, 0x6A, 0x36, 0x21, 0x6B, 0x36, 0x23, 0x6D, 0x35, 0x25, 0x6E, 0x34,
    0x27, 0x6F, 0x34, 0x29, 0x71, 0x33, 0x2C, 0x71, 0x32, 0x2D, 0x73, 0x32, 0x2F, 0x73, 0x32, 0x32,
    0x75, 0x30, 0x34, 0x75, 0x30, 0x35, 0x76, 0x2F, 0x38, 0x77, 0x2E, 0x3B, 0x77, 0x2E, 0x3C, 0x77,
    0x2D, 0x3E, 0x78, 0x2C, 0x40, 0x79, 0x2C, 0x42, 0x79, 0x2A, 0x45, 0x79, 0x2A, 0x47, 0x7A, 0x2A,
    0x49, 0x7A, 0x28, 0x4B, 0x7A, 0x28, 0x4D, 0x7A, 0x28, 0x4F, 0x7B, 0x27, 0x51, 0x7B, 0x26, 0x53,
    0x7B, 0x26, 0x55, 0x7B, 0x25, 0x57, 0x7B, 0x25, 0x5A, 0x7B, 0x25, 0x5B, 0x7B, 0x24, 0x5E, 0x7B,
    0x23, 0x5F, 0x7B, 0x23, 0x61, 0x7B, 0x22, 0x63, 0x7B, 0x22, 0x65, 0x7B, 0x22, 0x67, 0x7B, 0x21,
    0x69, 0x7B, 0x21, 0x6B, 0x7B, 0x20, 0x6D, 0x7B, 0x20, 0x70, 0x7B, 0x20, 0x71, 0x7B, 0x20, 0x73,
    0x7B, 0x20, 0x75, 0x7B, 0x1F, 0x77, 0x7A, 0x1F, 0x7A, 0x7A, 0x1E, 0x7D, 0x7A, 0x1E, 0x7E, 0x7A,
    0x1E, 0x81, 0x79, 0x1E, 0x81, 0x79, 0x1E, 0x83, 0x78, 0x1E, 0x86, 0x78, 0x1E, 0x88, 0x78, 0x1E,
    0x8B, 0x77, 0x1E, 0x8D, 0x76, 0x1F, 0x8F, 0x75, 0x1F, 0x91, 0x75, 0x1F, 0x93, 0x74, 0x20, 0x95,
    0x73, 0x21, 0x97, 0x72, 0x21, 0x99, 0x71, 0x22, 0x9C, 0x70, 0x24, 0x9E, 0x6F, 0x25, 0xA0, 0x6E,
    0x26, 0xA2, 0x6D, 0x27, 0xA5, 0x6B, 0x29, 0xA6, 0x6A, 0x2B, 0xA8, 0x69, 0x2E, 0xAB, 0x67, 0x2F,
    0xAD, 0x65, 0x31, 0xAF, 0x63, 0x34, 0xB1, 0x62, 0x37, 0xB3, 0x60, 0x39, 0xB5, 0x5E, 0x3D, 0xB8,
    0x5B, 0x3F, 0xB9, 0x59, 0x43, 0xBB, 0x58, 0x46, 0xBD, 0x55, 0x4A, 0xBE, 0x53, 0x4D, 0xBF, 0x51,
    0x51, 0xC3, 0x50, 0x56, 0xC4, 0x4C, 0x59, 0xC6, 0x4A, 0x5E, 0xC8, 0x47, 0x62, 0xC9, 0x45, 0x66,
    0xCA, 0x42, 0x6B, 0xCD, 0x40, 0x70, 0xCE, 0x3E, 0x75, 0xCF, 0x3A, 0x7A, 0xD0, 0x37, 0x7F, 0xD2,
    0x35, 0x84, 0xD4, 0x32, 0x8A, 0xD5, 0x2F, 0x8F, 0xD5, 0x2D, 0x94, 0xD7, 0x2A, 0x9A, 0xD8, 0x27,
    0xA1, 0xDA, 0x24, 0xA6, 0xDA, 0x23, 0xAC, 0xDB, 0x20, 0xB2, 0xDC, 0x1E, 0xB8, 0xDD, 0x1B, 0xBE,
    0xDE, 0x19, 0xC4, 0xDE, 0x18, 0xCB, 0xDF, 0x17, 0xD1, 0xE0, 0x16, 0xD6, 0xE0, 0x15, 0xDD, 0xE2,
    0x15, 0xE3, 0xE2, 0x16, 0xEA, 0xE3, 0x18, 0xEF, 0xE3, 0x19, 0xF5, 0xE4, 0x1B, 0xFB, 0xE8, 0x2D
};

void updatePalette( CBcmFrameBuffer *fb, int scanlineIntensity )
{
	u8 *rgb1 = VIC20_palettePAL_Even;
	u8 *rgb2 = VIC20_palettePAL_Odd;

	if ( cfgVIC_Emulation == 2 )
		rgb1 = rgb2 = VIC20_paletteNTSC;

	int f = scanlineIntensity;
	for ( int i = 0; i < 16; i++ )
	{
		u16 c;
		c = RGBA565( rgb1[ 0 ], rgb1[ 1 ], rgb1[ 2 ] );
		fb->SetPalette( i, c );
		//c = RGBA565( rgb2[ 0 ], rgb2[ 1 ], rgb2[ 2 ] );
		c = RGBA565( (rgb2[ 0 ]*f)>>8, (rgb2[ 1 ]*f)>>8, (rgb2[ 2 ]*f)>>8 );
		fb->SetPalette( i+16, c );
		rgb1 += 3;
		rgb2 += 3;
	}

	const u8 *v = viridisRGB;
	for ( int i = 0; i < 128; i++ )
	{
		fb->SetPalette( i + 128, RGBA565( v[ 0 ], v[ 1 ], v[ 2 ] ) );
		v += 3;
	}

	fb->UpdatePalette();
}

#ifdef HDMI_SOUND
CHDMISoundBaseDevice *hdmiSoundDevice;
#endif

boolean CKernelMenu::Initialize( void )
{
	boolean bOK = TRUE;

	m_CPUThrottle.SetSpeed( CPUSpeedMaximum );

#ifdef USE_HDMI_VIDEO
	if ( bOK ) bOK = m_Screen.Initialize();

	if ( bOK )
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice( m_Options.GetLogDevice(), FALSE );
		if ( pTarget == 0 )
			pTarget = &m_Screen;

		screen = &m_Screen;

		bOK = m_Logger.Initialize( pTarget );
		logger = &m_Logger;
	}
#endif

	if ( bOK ) bOK = m_Interrupt.Initialize();
	if ( bOK ) bOK = m_Timer.Initialize();
	m_EMMC.Initialize();

#ifdef COMPILE_MENU_WITH_SOUND
	pTimer = &m_Timer;
	//pScheduler = &m_Scheduler;
	pInterrupt = &m_Interrupt;

	//if ( bOK ) bOK = m_VCHIQ.Initialize();
	//pVCHIQ = &m_VCHIQ;
#endif
	latchSetClearImm( LATCH_LED0, LATCH_LED1 );

	// initialize ARM cycle counters (for accurate timing)
	initCycleCounter();

	// initialize GPIOs
	gpioInit();

	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( LATCH_LED0, LATCH_LED1 );

	u32 size = 0;

	readFile( logger, (char*)DRIVE, (char*)FILENAME_SPLASH_HELP, helpscreenRAW, &size );
		
	for ( u32 j = 0; j < screen->GetHeight(); j++ )
		for ( u32 i = 0; i < screen->GetWidth(); i++ )
			screen->SetPixel( i, j,  0 );

	CBcmFrameBuffer *fb = screen->GetFrameBuffer();

	// read launch code and .PRG
	readFile( logger, (char*)DRIVE, (char*)FILENAME_A0CBM, launchCode, &size );
	readFile( logger, (char*)DRIVE, (char*)FILENAME_A0CBM35K, launchCode35k, &size );
	readFile( logger, (char*)DRIVE, (char*)FILENAME_A0CRTSYNC, crtSyncCode, &size );
	readFile( logger, (char*)DRIVE, (char*)FILENAME_A0BASICSYNC, basicSyncCode, &size );

	u32 charROMSize;
	readFile( logger, (char*)DRIVE, (const char*)FILENAME_CHARROM, charROM, &charROMSize );

	// todo: this needs to be updated
	scanDirectoriesVIC20( (char *)DRIVE );

	latchSetClearImm( LATCH_LED1, 0 );

	if ( !readConfig( logger, (char*)DRIVE, (char*)FILENAME_CONFIG ) )
	{
		latchSetClearImm( LATCH_LED0to1, 0 );
		logger->Write( "RaspiMenu", LogPanic, "error reading .cfg" );
	}

	extern u8 cfgFontWidth;
	extern void setLayout( bool wideChars = false );

	if ( cfgFontWidth == 5 )
	{
		setLayout( false );
		readFile( logger, (char*)DRIVE, (char*)FILENAME_FONT5, fontBitmap, &size );
		fontW = 5;
	} else
	{
		setLayout( true );
		readFile( logger, (char*)DRIVE, (char*)FILENAME_FONT6, fontBitmap, &size );
		fontW = 6;
	}

	readSettingsFile();

	scanlineIntensity = cfgVIC_ScanlineIntensity;
	updatePalette( fb, scanlineIntensity );

	renderC64();
	convertScreenToBitmap( framebuffer );

	latchSetClearImm( 0, LATCH_LED0to1 );

	if ( screenType == 0 )
	{
		splashScreen( sk20_splash );
	} else
	if ( screenType == 1 )
	{
		extern int screenRotation;
		tftInitImm( screenRotation );
	}

#ifdef HDMI_SOUND
	if ( cfgVIC_Emulation )
	{
		hdmiSoundDevice = new CHDMISoundBaseDevice( SAMPLERATE );
		hdmiSoundDevice->SetWriteFormat( SoundFormatSigned24, 1 );
		if ( !hdmiSoundDevice->Start() )
		{
			cfgVIC_Emulation = 0;
		}
	}
#endif

	return bOK;
}

void chooseMemoryConfiguration( u16 loadAddress )
{
	extern u8 usesMemorySelection, curMemorySelection;

	if ( !usesMemorySelection || curMemorySelection == 0 )
	{
		// use auto settings
		switch ( loadAddress )
		{
		case 0x1201: // BLK1 is mandatory, enable everything
			vic20MemoryConfiguration = VIC20_RAMx_READ_WRITE | VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE | VIC20_BLK5_READ_WRITE;
			break;
		case 0x0401: // RAM 123 is mandatory, rest is disabled
			vic20MemoryConfiguration = VIC20_RAMx_READ_WRITE;
			break;
		default:
		case 0x1001: // unexpanded VIC
			vic20MemoryConfiguration = 0;
			break;
		}
	} else
	//if ( usesMemorySelection )
	{
		const u32 memConfig[ 8 - 2 ] = {
			0,
			VIC20_RAMx_READ_WRITE,
			VIC20_BLK1_READ_WRITE,
			VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE,
			VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE,
			VIC20_RAMx_READ_WRITE | VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE | VIC20_BLK5_READ_WRITE
		};
		if ( curMemorySelection >= 2 )
			vic20MemoryConfiguration = memConfig[ curMemorySelection - 2 ]; else
			vic20MemoryConfiguration = vic20MemoryConfigurationManual;
	}
}


u16 *pScreen;
u32 screenWidth;
u32 pitch;

static bool loadFileFromSD( CLogger *logger, char *vic20Filename, u8 *data, u32 *size )
{
	if ( cartMode == 0 )
	{
		return readFile( logger, (char*)DRIVE, (const char*)FILENAME_PRG, data, size );
	}

	return loadPrgFile( logger, DRIVE, FILENAME, vic20Filename, data, size );
}

static bool saveFileToSD( CLogger *logger, char *vic20Filename, u8 *data, u32 size )
{
	return savePrgFile( logger, DRIVE, FILENAME, vic20Filename, data, size );
}

static u8 ledActivityBrightness1 = 0;
static u8 ledActivityBrightness2 = 0;
static u32 hdmiSampleData;

static u8 visMode = 0;
static u8 osciX = 0;			
static u8 osciV[ 64 ][ 4 ];

void CKernelMenu::Run( void )
{
	CBcmFrameBuffer *fb = screen->GetFrameBuffer();
	pScreen = (u16*)(uintptr)fb->GetBuffer();
	screenWidth = screen->GetWidth();
	//screenHeight = screen->GetHeight();
	pitch = fb->GetPitch() / 2;

	nBytesRead		= 0;
	c64CycleCount	= 0;
	lastChar		= 0;
	resetCounter    = 0;
	transferStarted = 0;
	launchKernel	= 0;
	updateMenu      = 0;
	subGeoRAM		= 0;
	subMenuItem		= 0;
	subHasLaunch	= -1;
	FILENAME[0]		= 0;
	first			= 1;
	prgCurSize      = prgSize;
	prgCurLaunch    = &prgData[0];

	memset( osciV, 0, sizeof( osciV ) );

	latchSetClearImm( 0, LATCH_RESET | LATCH_LED0to1 | LATCH_ENABLE_KERNAL );

	// setup FIQ
	launchKernel = 0;

	warmCache( (void*)this->FIQHandler );

	m_InputPin.ConnectInterrupt( this->FIQHandler, this );
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

	myFIQHandler = (void*)this->FIQHandler;
	cartMode = 0;
	activateCart( true, true );

	// forever
	while ( true )
	{
		asm volatile ("wfi");
		
		if ( cfgVIC_Emulation )
		{
			{
				const u32 visX = 16, visY = 8;
				static u8 w2do = 0;

				if ( visMode & 2 )
				{
					static u32 o = 0;
					o += 2; if ( o >= 0xc000 ) o = 0; // for some weird reason &= didn't work...

					u16 v = ( cart_ram[ o ] + cart_ram[ o + 1 ] ) / 4;

					v += 128;
					int oo = o >> 1;
					int ox = oo & 63, oy = oo >> 6;

					if ( ( o >= 0x0400 && o < 0x1000 && ( vic20MemoryConfiguration & ( 3 << 0 ) ) == 0 ) ||
						 ( o >= 0x2000 && o < 0x4000 && ( vic20MemoryConfiguration & ( 3 << 2 ) ) == 0 ) ||
						 ( o >= 0x4000 && o < 0x6000 && ( vic20MemoryConfiguration & ( 3 << 4 ) ) == 0 ) ||
						 ( o >= 0x6000 && o < 0x8000 && ( vic20MemoryConfiguration & ( 3 << 6 ) ) == 0 ) ||
						 ( o >= 0xa000 && o < 0xc000 && ( vic20MemoryConfiguration & ( 3 << 8 ) ) == 0 ) )
					{
						if ( (ox & 7) == (oy & 7) ) v = 255; else v = 128;
					}

					u8 *pScreen8 = (u8*)pScreen;
					pScreen8 = &pScreen8[ visX + (visY+108) * pitch * 2 ];
					pScreen8[ ((oo)&63) + (((oo)>>6)) * pitch * 2 ] = v;
				}

				if ( visMode & 1 )
				{
					u8 *pScreen8 = (u8*)pScreen;
					pScreen8 = &pScreen8[ visX + visY * pitch * 2 ];
					static int div = 0;
					if ( ++div > 128 )
					{
						register int i = w2do;
						{
							int y = 24 - osciV[ osciX ][ i ] + i * 24;
							*(u8*)&pScreen8[ osciX + y * pitch * 2 ] = 0;
							osciV[ osciX ][ i ] = (vic656x.ch_out[ i ] * 8);
							y = 24 - osciV[ osciX ][ i ] + i * 24;
							*(u8*)&pScreen8[ osciX + y * pitch * 2 ] = 1;
						}
						w2do ++;
						if ( w2do >=4 ) 
						{
							w2do = 0;
							osciX ++;
							osciX &= 63;
							div = 0;
						}
					}
				}

			}

			sampleUpdateVIC656x();
			if ( vic656x.hasSampleOutput )
			{
				register s32 s = (s32)( vic656x.curSampleOutput ) << 7;
				hdmiSampleData = hdmiSoundDevice->ConvertSample( s );
			}
		}

		if ( cartMode == 0 )
		{
			u32 v = ( c64CycleCount >> (12+1) ) & 255;
			if ( v < 128 )
				ledActivityBrightness1 = v; else
				ledActivityBrightness1 = 255 - v;
			ledActivityBrightness2 = 128 - ledActivityBrightness1;
		} else
		{
			if ( cfgVIC_Emulation )
			{
				ledActivityBrightness1 = ledActivityBrightness2 = min( 511, vic656x.curSampleOutput >> 5 );
			} else
				ledActivityBrightness1 = ledActivityBrightness2 = 0;
		}

		if ( doActivateCart )
			activateCart();

		if ( updateMenu == 1 )
		{
			u32 c64CycleCountTemp = c64CycleCount;
			pullIRQ = releaseIRQ = 0;
			updateMenu = 0;

			// todo: reused a variable, rename
			extern int subMenuItem;

			if ( lastChar == 45 ) // minus
			{
				scanlineIntensity -= 8;
				if ( scanlineIntensity < 0 ) scanlineIntensity = 0;
				updatePalette( fb, scanlineIntensity );
			} else
			if ( lastChar == 43 ) // plus
			{
				scanlineIntensity += 8;
				if ( scanlineIntensity > 256 ) scanlineIntensity = 256;
				updatePalette( fb, scanlineIntensity );
			} 

			handleC64( lastChar, &launchKernel, FILENAME, filenameKernal );
			
			if ( launchKernel == 0x7fffffff )
				reboot();

			if ( launchKernel == 0x101 )
			{
				launchKernel = 0;
				// todo: toggle visualization here
				visMode = ( visMode + 1 ) & 3;
				{
					u8 *pScreen8 = (u8*)pScreen;
					for ( u32 j = 0; j < screen->GetHeight(); j++ )
						for ( u32 i = 0; i < 64 + 16; i++ )
						{
							pScreen8[ i + j * pitch * 2 ] = 0;
						}
				}

				lastChar = 0xfffffff;

				CACHE_PRELOAD_DATA_CACHE( framebuffer, 4096, CACHE_PRELOADL2STRM );
				warmCache( (void*)this->FIQHandler );
				doneWithHandling = 1;

				// trigger NMI on the VIC20 which tells the menu code that the new screen is ready
				pullIRQ = 64;
				c64CycleCount = c64CycleCountTemp;
			} 
			// F3: go to basic with disk emu
			if ( launchKernel >= 7 && launchKernel <= 9 )
			{
				// goto Basic
				memset( cart_ram, 0, 65536 );
				memcpy( &cart_ram[ 0xa000 ], launchCode, 8192 );

				if ( launchKernel == 9 ) // start PRG from D64
				{
					if ( FILENAME[ 0 ] != 0  )
					{
						logger->Write( "exec", LogNotice, "filename: '%s'", &FILENAME[ 256 ] );
						
						if ( screenType == 1 )
						{
							if ( !loadLogoTGA( (const char*)&FILENAME[ 256 ] ) )
								tftDisplayTGA( FILENAME_SPLASH_LAUNCH );
						}

						u32 loadAddress = prgDataLaunch[ 0 ] + prgDataLaunch[ 1 ] * 256;

						chooseMemoryConfiguration( loadAddress );

						char basic_commands[ 50 ];
						sprintf( basic_commands, "LOAD\"%s\",\2,1\3RUN:\3", FILENAME );
						setDriveAndBasicCommand( basic_commands );
					} else
					{
						vic20MemoryConfiguration = vic20MemoryConfigurationManual;
						setDriveAndBasicCommand( "" );
					}

					// required for initializing the launcher/disk emulation
					if ( ( vic20MemoryConfiguration >> 8 ) == 0 )
							vic20MemoryConfiguration |= VIC20_BLK5_READ_ONLY;
				} else
				{
					logger->Write( "exec", LogNotice, "filename: '%s'", FILENAME );

					if ( screenType == 1 )
					{
						if ( !loadLogoTGA( (const char*)FILENAME ) )
							tftDisplayTGA( FILENAME_SPLASH_DISK );
					}

					vic20MemoryConfiguration = vic20MemoryConfigurationManual;

					if ( ( vic20MemoryConfiguration >> 8 ) == 0 )
						vic20MemoryConfiguration |= VIC20_BLK5_READ_ONLY;

					const char basic_commands[] = "\0";
					memcpy( (char *)&cart_ram[ BASIC_COMMANDS_ADDR ], basic_commands, sizeof( basic_commands ) );
					extern u8 cfgDeviceNumber;
					cart_ram[ DEVICE_NUMBER ] = cfgDeviceNumber;
				}
				initDiskEmulation( "" );

				launchKernel = 0;
				cartMode = 2;
				warmCache( (void*)this->FIQHandler );
				doneWithHandling = 1;
				disableCart = 0;
				pullIRQ = 1;
				resetVC20();
			} else
			// F8: go to Basic
			if ( subMenuItem )
			{
				if ( screenType == 1 )
					tftDisplayTGA( FILENAME_SPLASH_BASIC );

				memset( cart_ram, 0, 65536 );

				vic20MemoryConfiguration = vic20MemoryConfigurationManual;

				if ( cfgVIC_Emulation )
				{
					injectVICSYNC = 3;
					disableDiskIO = 1;
					if ( ( vic20MemoryConfiguration >> 8 ) == 0 )
					{
						vic20MemoryConfiguration |= VIC20_BLK5_READ_ONLY;
						basicSyncDisableBLK5 = 1;
					} else
						basicSyncDisableBLK5 = 0;

				}

				cartMode = 3;
				warmCache( (void*)this->FIQHandler );
				doneWithHandling = 1;
				disableCart = 0;
				pullIRQ = 1;
				resetVC20();
				subMenuItem = 0;
			} else
			if ( launchKernel == 4 ) // launch PRG
			{
				logger->Write( "exec", LogNotice, "filename: '%s'", FILENAME );

				if ( screenType == 1 )
				{
					if ( !loadLogoTGA( (const char*)FILENAME ) )
						tftDisplayTGA( FILENAME_SPLASH_LAUNCH );
				}

				launchKernel = 0;
				readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData + 2, &prgSize );
				*((u16 *)prgData) = prgSize;

				u32 loadAddress = prgData[ 2 ] + prgData[ 3 ] * 256;

				chooseMemoryConfiguration( loadAddress );

				// required for initializing the launcher/disk emulation
				if ( ( vic20MemoryConfiguration >> 8 ) == 0 )
						vic20MemoryConfiguration |= VIC20_BLK5_READ_ONLY;

				launchKernel = 0;

				memcpy( &cart_ram[ 0xa000 ], launchCode, 8192 );

				const char basic_commands[] = "LOAD\"*\",\2,1\3RUN:\3\0";
				setDriveAndBasicCommand( basic_commands );
				logger->Write( "diskemu", LogWarning, "FILENAME='%s'", FILENAME );
				initDiskEmulation( FILENAME );
				cartMode = 2;
				warmCache( (void*)this->FIQHandler );
				disableCart = 0;
				pullIRQ = 1;
				resetVC20();
			} else
			if ( launchKernel == 5 ) // launch CRT, no need to call an external RPi-kernel
			{
				logger->Write( "exec", LogNotice, "filename: '%s'", FILENAME );
				if ( screenType == 1 )
				{
					if ( !loadLogoTGA( (const char*)FILENAME ) )
						tftDisplayTGA( FILENAME_SPLASH_CART );
				}

				launchKernel = 0;
				memset( cart_ram, 0, 65536 );

				CRT_HEADER header;
				u32 flagsBLK, nBLKs; //<- use this to flag which BLKs are filled with ROM/RAM
				u8 bankswitchType;

				readCRTFile( logger, &header, (char*)DRIVE, (char*)FILENAME, (u8*)cart_ram, &bankswitchType, &flagsBLK, &nBLKs, true );

				vic20MemoryConfiguration = flagsBLK;

				// use manual memory configuration if desired -- but overwrite with the necessary flags according to the cartridge
				extern u8 usesMemorySelection, curMemorySelection;
				if ( usesMemorySelection && curMemorySelection > 0 )
				{
					const u32 memConfig[ 8 - 2 ] = {
						0,
						VIC20_RAMx_READ_WRITE,
						VIC20_BLK1_READ_WRITE,
						VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE,
						VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE,
						VIC20_RAMx_READ_WRITE | VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE | VIC20_BLK5_READ_WRITE
					};
					if ( curMemorySelection >= 2 )
						vic20MemoryConfiguration = memConfig[ curMemorySelection - 2 ]; else
						vic20MemoryConfiguration = vic20MemoryConfigurationManual;
				} 

				if ( cfgVIC_Emulation )
				{
					injectVICSYNC = 1;
					disableDiskIO = 1;
				}

				warmCache( (void*)this->FIQHandler );

				// this must not happen before the RPi is ready to serve the bus!
				cartMode = 4;
				disableCart = 0;
				resetVC20();
			} else
			if ( launchKernel == 6 ) // combination of multiple CRTs
			{
				if ( screenType == 1 )
					tftDisplayTGA( FILENAME_SPLASH_CART );
				launchKernel = 0;
				memset( cart_ram, 0, 65536 );

				extern u8 nSelectedItems;
				extern char selItemFilenames[ 16 ][ 2048 ];

				vic20MemoryConfiguration = 0;

				extern u8 usesMemorySelection, curMemorySelection;

				// use manual memory configuration if desired -- but overwrite with the necessary flags according to the cartridge
				if ( usesMemorySelection )
				{
					const u32 memConfig[ 8 - 2 ] = {
						0,
						VIC20_RAMx_READ_WRITE,
						VIC20_BLK1_READ_WRITE,
						VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE,
						VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE,
						VIC20_RAMx_READ_WRITE | VIC20_BLK1_READ_WRITE | VIC20_BLK2_READ_WRITE | VIC20_BLK3_READ_WRITE | VIC20_BLK5_READ_WRITE
					};
					if ( curMemorySelection >= 2 )
						vic20MemoryConfiguration = memConfig[ curMemorySelection - 2 ]; else
						vic20MemoryConfiguration = vic20MemoryConfigurationManual;
				}

				for ( int si = 0; si < nSelectedItems; si ++ )
				{
					loadFileinMemory( &selItemFilenames[ si ][ 0 ], &vic20MemoryConfiguration, VIC20_READ_ONLY );
				}

				if ( cfgVIC_Emulation )
				{
					injectVICSYNC = 1;
					disableDiskIO = 1;
				}
				basicSyncDisableBLK5 = 0;

				cartMode = 4;
				warmCache( (void*)this->FIQHandler );

				// this must not happen before the RPi is ready to serve the bus!
				disableCart = 0;
				resetVC20();
			} else
			{
				renderC64();

				#if 0
				char temp[40];
				/*sprintf( temp, "%d", (u32)lastChar );
				printC64( 0, 1, temp, skinValues.SKIN_BROWSER_TEXT_HEADER, 0 );*/

				static int curTimPos = 0;
				if ( lastChar == 73 ) curTimPos += 3;
				if ( lastChar == 75 ) curTimPos++;
				curTimPos %= 4;
				int delta = 0;
				if ( lastChar == 79 ) delta = -5;
				if ( lastChar == 80 ) delta = +5;
				if ( curTimPos == 0 && delta ) 	WAIT_FOR_SIGNALS = max( 0, WAIT_FOR_SIGNALS + delta );
				if ( curTimPos == 1 && delta ) 	WAIT_CYCLE_MULTIPLEXER = max( 0, WAIT_CYCLE_MULTIPLEXER + delta );
				if ( curTimPos == 2 && delta ) 	WAIT_CYCLE_READ = max( 0, WAIT_CYCLE_READ + delta );
				if ( curTimPos == 3 && delta ) 	WAIT_CYCLE_WRITEDATA = max( 0, WAIT_CYCLE_WRITEDATA + delta );
				sprintf( temp, "SIGNALS     = %d", (u32)WAIT_FOR_SIGNALS );
				printC64( 0, 0, temp, skinValues.SKIN_BROWSER_TEXT_HEADER, 0 );
				sprintf( temp, "MULTIPLEXER = %d", (u32)WAIT_CYCLE_MULTIPLEXER );
				printC64( 0, 1, temp, skinValues.SKIN_BROWSER_TEXT_HEADER, 0 );
				sprintf( temp, "READ        = %d", (u32)WAIT_CYCLE_READ );
				printC64( 0, 2, temp, skinValues.SKIN_BROWSER_TEXT_HEADER, 0 );
				sprintf( temp, "WRITE       = %d", (u32)WAIT_CYCLE_WRITEDATA );
				printC64( 0, 3, temp, skinValues.SKIN_BROWSER_TEXT_HEADER, 0 );
				#endif


				convertScreenToBitmap( framebuffer );

/*				if ( launchKernel == 0x102 ) // toggle VFLI
				{
					launchKernel = 0;
					if ( cfgVIC_Emulation )
					{
						cfgVIC_VFLI_Support = 1 - cfgVIC_VFLI_Support;
						extern char *errorMsg;
						extern char errorMessages[7][41];

						if ( cfgVIC_VFLI_Support )
							errorMsg = errorMessages[ 5 ]; else
							errorMsg = errorMessages[ 6 ];
						previousMenuScreen = menuScreen;
						menuScreen = MENU_ERROR;
					}
				}*/

				if ( launchKernel == 0x100 )
				{
					launchKernel = 0;
					memset( colorbuffer, 0, 256 );
					memset( framebuffer, 0, 4096 );

					for ( int j = 0; j < 160; j++ )
						for ( int i = 0; i < 192; i++ )
						{
							u8 c = 1;
							if ( j < 32 )
							{
								if ( i < 192 / 2 )
									c = 5; else 
									c = 3; 
							} else
							if ( ( j >= 32 && j < 48 ) || ( j >= 96 && j < 112 ) )
							{
								if ( i < 192 / 2 )
									c = 1; else
									c = 3;
							} else
							if ( i < 16 ) 
							{
								c = 5; 
							} else 
								c = 3;
						
							if ( helpscreenRAW[ i + j * 192 ] )
								SETPIXEL( framebuffer, i, j, colorbuffer, c );
						}
				}

				lastChar = 0xfffffff;

				CACHE_PRELOAD_DATA_CACHE( framebuffer, 4096, CACHE_PRELOADL2STRM );
				warmCache( (void*)this->FIQHandler );
				doneWithHandling = 1;

				// trigger NMI on the VIC20 which tells the menu code that the new screen is ready
				pullIRQ = 64;
				c64CycleCount = c64CycleCountTemp;
			}
			continue;
		}

		if ( loadingPrg || savingPrg )
		{
			char *vic20Filename = (char *)&cart_ram[ FILENAME_ADDR ];
			u8 vic20FilenameLen = cart_ram[ FILENAME_LEN_ADDR ];
			vic20Filename[ vic20FilenameLen ] = 0; // null terminate string

			if ( loadingPrg )
			{
				loadingPrg = false;

				if ( loadFileFromSD( logger, vic20Filename, prgData + 3, &prgSize ) )
				{
					prgData[ 0 ] = 0; // file found
					*(u16*)(prgData + 1) = prgSize;
				}
				else
				{
					prgData[ 0 ] = 0xff; // file not found
				}
			}
			else // if ( savingPrg )
			{
				savingPrg = false;
				prgSize = (u16)(pTransfer - &prgData[ 0 ]);

				if ( saveFileToSD( logger, vic20Filename, prgData, prgSize ) )
				{
					prgData[ 0 ] = 0; // file saved
				}
				else
				{
					prgData[ 0 ] = 0xff; // file exists error
				}
			}

			pTransfer = &prgData[ 0 ];
			nextByte = *pTransfer;
			SyncDataAndInstructionCache();
			warmCache( (void*)this->FIQHandler );
			warmCache( (void*)this->FIQHandler );
			doneWithHandling = 1;
		}
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

extern volatile VICSTATE vic656x AAA;

#define REG(name, base, offset, base4, offset4)	\
			static const u32 Reg##name = (base) + (offset);
#define REGBIT(reg, name, bitnr)	static const u32 Bit##reg##name = 1U << (bitnr);

REG (MaiData, ARM_HD_BASE, 0x20, ARM_HD_BASE, 0x1C);
REG (MaiControl, ARM_HD_BASE, 0x14, ARM_HD_BASE, 0x10);
	REGBIT (MaiControl, Full, 11);

#define HANDLE_READ( addr, addr_ofs, prefetchValue, prefetchAddr ) \
	{ WRITE_D0to7_TO_BUS( cart_ram[ ( addr & 0x1fff ) + addr_ofs ] ); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

void CKernelMenu::FIQHandler (void *pParam)
{
	register u8 D;


	/*if ( !doneWithHandling && ( loadingPrg || savingPrg ) )
	{
		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
		return;
	}*/

	if ( !doneWithHandling || doActivateCart )
		return;

	{
	START_AND_READ_ADDR0to7_RW_RESET_CS

	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	extern volatile VICSTATE vic656x AAA;

	// making use of waiting time:

	if ( CPU_WRITES_TO_BUS )
	{
		// 1st part of getting D0..7
		SET_BANK2_INPUT															
		write32( ARM_GPIO_GPCLR0, (1 << GPIO_OE) );					

		// here we have some time, but doesn't really make a difference to execute this here
		if ( cfgVIC_Emulation && vic656x.hasSampleOutput )
		{
			// sound output
			//if ( hdmiSoundDevice->IsWritable() )
			if ( !( read32( RegMaiControl ) & BitMaiControlFull ) )
			{
				write32( RegMaiData, hdmiSampleData );
				write32( RegMaiData, hdmiSampleData );
			}
			vic656x.hasSampleOutput = 0;
		}		
	}

	if ( resetCounter > 1000 && !doActivateCart && c64CycleCount < 300000 )
	{
		// if we had launched a CRT which requires that syncing, reactivate
		if ( injectVICSYNC == 2 || injectVICSYNC == 4 )
		{
			injectVICSYNC --; 
			if ( basicSyncDisableBLK5 )
				vic20MemoryConfiguration |= VIC20_BLK5_READ_ONLY;
		}

		disableDiskIO = 0;
		activateVFLIHack = 0;
		if ( CPU_WRITES_TO_BUS )
			SET_BANK2_OUTPUT
		FINISH_BUS_HANDLING
		return;
	}

	#ifdef VIC_MODE
	u16 A15 = 0;
	if ( cfgVIC_Emulation )
		A15 = ((g2>>A13)&1);
	#else
	u16 A15 = 0;
	#endif

	if ( pullIRQ && --pullIRQ == 0 )
	{
		CLR_GPIO( bNMI );
		releaseIRQ = 1000;
	}
	if ( releaseIRQ && --releaseIRQ == 0 )
	{
		SET_GPIO( bNMI );
	}

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	u16 addr = GET_ADDRESS | ( g3 & bBA ? (1 << 13) : 0 );

	#define RAM123_ACCESS ( ROML_ACCESS && !( g3 & bBA ) )
	#define BLK1_ACCESS   ( ROML_ACCESS && ( g3 & bBA ) )
	#define BLK2_ACCESS   ( IO1_ACCESS )
	#define BLK3_ACCESS   ( IO2_ACCESS )
	#define BLK5_ACCESS   ( ROMH_ACCESS && ( g3 & bBA ) )
	#define IO23_ACCESS   ( ROMH_ACCESS && !( g3 & bBA ) )

	u8 masked_BLK1_ACCESS = BLK1_ACCESS && ( (vic20MemoryConfiguration >> 2) & 3 );
	u8 masked_BLK2_ACCESS = BLK2_ACCESS && ( (vic20MemoryConfiguration >> 4) & 3 );
	u8 masked_BLK3_ACCESS = BLK3_ACCESS && ( (vic20MemoryConfiguration >> 6) & 3 );
	u8 masked_BLK5_ACCESS = BLK5_ACCESS && ( (vic20MemoryConfiguration >> 8) & 3 );

	if ( CPU_WRITES_TO_BUS )
	{
		// 2nd part of getting D0..7
		WAIT_UP_TO_CYCLE( WAIT_CYCLE_WRITEDATA );								
		D = ( read32( ARM_GPIO_GPLEV0 ) >> D0 ) & 255;							
		write32( ARM_GPIO_GPSET0, 1 << GPIO_OE );								
		SET_BANK2_OUTPUT														
	}

	// this is only required when the real VIC is REPLACED -- this is currently disabled
	#if 0
	//
	// reading VIC registers
	//
	if ( !BLK1_ACCESS && !BLK2_ACCESS && !BLK3_ACCESS && !BLK5_ACCESS && !RAM123_ACCESS && !IO23_ACCESS && CPU_READS_FROM_BUS && A15 )
	{
		//addr += 0x8000;
		//if ( addr >= 0x9000 && addr <= 0x900f )
		if ( addr >= 0x1000 && addr <= 0x100f )
		{
			// attention:
			// in real hardware the VIC emulation ticks in parallel to the CPU. THis is not the case, we need to serve the bus before we have the time to emulate the VIC.
			// for this reason we add 1 cycle to the counters and recompute the rasterline
			s32 rasterLine = 0;
			if ( vic656x.vLines == VIC20_PAL_V_LINES )
			{
				s32 v = (s32)vic656x.vCount;
				s32 h = (s32)vic656x.hCount;
				s32 vicCycle = v * VIC20_PAL_H_CYCLES + h;
				vicCycle += 1;
				rasterLine = ( vicCycle / VIC20_PAL_H_CYCLES ) % VIC20_PAL_V_LINES;
			} else
			{
				s32 v = (s32)vic656x.vCount;
				s32 h = (s32)vic656x.hCount;
				s32 vicCycle = v * VIC20_NTSC_H_CYCLES + h;
				vicCycle += 261 * 65;
				vicCycle -= 28 - 65 - 1;
				rasterLine = ( vicCycle / VIC20_NTSC_H_CYCLES ) % VIC20_NTSC_V_LINES;
			}

			addr &= 0x0f;
			switch ( addr )
			{
			case 0x03:
				D = ( vic656x.regs[ 3 ] & 0x7f ) | ( ( rasterLine & 1 ) << 7 );
				break;
			case 0x04:
				D = rasterLine >> 1;
				break;
/*			case 0x06:
			case 0x07: // light pen
				D = 0;
				break;
			case 0x08:
			case 0x09: // potentiometer
				D = 255;
				break;*/
			default:
				D = vic656x.regs[ addr ];
			}
			CLR_GPIO( bGAME );
			register u32 DD = ( (D) & 255 ) << D0;
			write32( ARM_GPIO_GPSET0, DD  );
			write32( ARM_GPIO_GPCLR0, (D_FLAG & ( ~DD )) | (1 << GPIO_OE) | bCTRL257 );
			WAIT_UP_TO_CYCLE( WAIT_CYCLE_READ + 0 );
			write32( ARM_GPIO_GPSET0, (1 << GPIO_OE) | bGAME );
			goto emulate_VIC_and_Quit;
		}
	}
	#endif

	if ( masked_BLK1_ACCESS )
	{
		u32 memAccess = (vic20MemoryConfiguration >> 2) & 3;
		if ( CPU_READS_FROM_BUS ) 
			{ HANDLE_READ( addr, 0x2000, prefetchblk1, prefetchblk1addr ) } else
		if ( CPU_WRITES_TO_BUS && memAccess == 1 ) 
			{ cart_ram[ ( addr & 0x1fff ) + 0x2000 ] = D; }
	} else
	if ( masked_BLK2_ACCESS )
	{
		u32 memAccess = (vic20MemoryConfiguration >> 4) & 3;
		if ( CPU_READS_FROM_BUS ) 
			{ HANDLE_READ( addr, 0x4000, prefetchblk2, prefetchblk2addr ) } else
		if ( CPU_WRITES_TO_BUS && memAccess == 1 ) 
			{ cart_ram[ ( addr & 0x1fff ) + 0x4000 ] = D; }
	} else
	if ( masked_BLK3_ACCESS )
	{
		u32 memAccess = (vic20MemoryConfiguration >> 6) & 3;
		if ( CPU_READS_FROM_BUS ) 
			{ HANDLE_READ( addr, 0x6000, prefetchblk3, prefetchblk3addr ) } else
		if ( CPU_WRITES_TO_BUS && memAccess == 1 ) 
			{ cart_ram[ ( addr & 0x1fff ) + 0x6000 ] = D; }
	} else
	if ( masked_BLK5_ACCESS )
	{
		u32 memAccess = (vic20MemoryConfiguration >> 8) & 3;
		if ( CPU_READS_FROM_BUS  ) 
		{ 
			if ( injectVICSYNC & 1 )
			{
				if ( injectVICSYNC == 1 )
				{ 
					WRITE_D0to7_TO_BUS( crtSyncCode[ ( addr & 0x1fff ) ] ); 
					if ( ( addr & 0x1fff ) == 0x0e )
						injectVICSYNC = 2;
				} else
				{ 
					WRITE_D0to7_TO_BUS( basicSyncCode[ ( addr & 0x1fff ) ] ); 
					if ( ( addr & 0x1fff ) == 0x1a )
					{
						injectVICSYNC = 4;
						if ( basicSyncDisableBLK5 )
							vic20MemoryConfiguration &= ~VIC20_BLK5_READ_ONLY;
					}
				} 
			} else
			{
				HANDLE_READ( addr, 0xa000, prefetchblk5, prefetchblk5addr )
			}
		} else
			if ( CPU_WRITES_TO_BUS && memAccess == 1 ) { /*READ_D0to7_FROM_BUS( cart_ram[ ( addr & 0x1fff ) + 0xa000 ] );*/ cart_ram[ ( addr & 0x1fff ) + 0xa000 ] = D; }
	} else
	//
	// access to register at $9fff (IO3)
	//
	if ( IO23_ACCESS && addr == 0x1fff ) 
	{
		if ( !disableDiskIO )
		{
			if ( CPU_WRITES_TO_BUS )
			{
				// any write will (re)start the PRG transfer
				switch ( D )
				{
				case 0:	// start loading PRG
					loadingPrg = true;
					doneWithHandling = 0;
					break;
				case 1: // start transfer of framebuffer
					pTransfer = &framebuffer[ 0 ];
					nextByte = *pTransfer;
					CACHE_PRELOADL2KEEP( pTransfer );
					break;
				case 2: // start transfer of colorbuffer
					pTransfer = &colorbuffer[ 0 ];
					nextByte = *pTransfer;
					CACHE_PRELOADL2KEEP( pTransfer );
					break;
				case 4:	// start saving PRG
					savingPrg = true;
					doneWithHandling = 0;
					break;
				case 7:
					disableCart = 1;
					break;
				default:
					{
						SET_GPIO( bNMI );
						// keypress
						lastChar = D;
						updateMenu = 1;
						doneWithHandling = 0;
					}
				}
			} else
			// if ( CPU_READS_FROM_BUS )
			{
				// get next byte
				WRITE_D0to7_TO_BUS( nextByte )
				pTransfer ++;
				nextByte = *pTransfer;
				CACHE_PRELOADL2KEEP( pTransfer );
			}
		}
	} else
	//
	// access to (emulated) RAM/ROM
	//
	if ( RAM123_ACCESS && (vic20MemoryConfiguration & 3) && addr >= 0x400 && addr < 0x1000  && !A15 )
	{
		if ( CPU_READS_FROM_BUS )
			{ HANDLE_READ( addr, 0x0000, prefetch123, prefetch123addr )	} else
			{ cart_ram[ addr ] = D; }
	} else
	//
	// RAM at $9c00-$9eff (IO3)
	//
	if ( IO23_ACCESS && addr >= 0x1c00 && addr < 0x1f00 )
	{
		if ( CPU_READS_FROM_BUS )
			{ HANDLE_READ( addr, 0x8000, prefetchio23, prefetchio23addr ) } else
			{ cart_ram[ ( addr & 0x1fff ) + 0x8000 ] = D; }
	} else
	#ifndef VIC_EXCLUSIVE
    if ( IO23_ACCESS && addr == 0x1ffe && A15 ) 
	{
		if ( CPU_WRITES_TO_BUS )
		{
			// these values depend on the sync code running on the VIC20:
			// where is the raster beam when the stable raster code writes to $9ffe
			if ( cfgVIC_Emulation == 1 ) // PAL
			{
				vic656x.hCount = 11;
				vic656x.vCount = 74;
			} else
			{
				// values for NTSC VIC
				vic656x.hCount = 39;
				vic656x.vCount = 73;
			}
		}
	} else
	#endif
	// only when A15 is connected
	if ( cfgVIC_Emulation && CPU_WRITES_TO_BUS )
	{
		u32 A = addr + (A15 << 15);
		if ( ( A < 0x8000 ) || 
		     ( A >= 0x9000 && A < 0xa000 ) )
			cart_ram[ A ] = D;
	}

	if ( BUTTON_PRESSED )
	{
		injectVICSYNC = 0; 
		basicSyncDisableBLK5 = 0;
		cartMode = 0;
		pullIRQ = releaseIRQ = 0;
		disableCart =  0;
		doActivateCart = 1;
		doneWithHandling = 0;
		FINISH_BUS_HANDLING
		return;
	}

#ifdef VIC_MODE
	if ( cfgVIC_Emulation )
	{
		// VFLI
		if ( cfgVIC_VFLI_Support && !BLK1_ACCESS && !BLK2_ACCESS && !BLK3_ACCESS && !BLK5_ACCESS && !RAM123_ACCESS && !IO23_ACCESS && CPU_WRITES_TO_BUS && A15 )
		{
			// VIA for VFLI hack, $9112
			if ( addr == 0x1112 && D == 15 )
				activateVFLIHack = 1;
			if ( addr == 0x1110 ) // $9110
				bankVFLI = D & 0x0f;

			if ( activateVFLIHack && addr >= 0x1400 && addr < 0x1600 ) // color RAM
			{
				u16 caddr = ( addr & 0x3ff ) | ( bankVFLI << 10 );
				#ifdef VFLI_COLORRAM_4BIT
				register u8 nibble = ( caddr & 1 ) << 2;
				register u8 nibble_mask = nibble ? 0xf0 : 0x0f;
				vfli_ram[ caddr >> 1 ] = ( vfli_ram[ caddr >> 1 ] & ~nibble_mask ) | ( ( (u8)D & 0x0f ) << nibble );
				#else
				vfli_ram[ caddr ] = D & 0xf;
				#endif
			}
		}

		// write to VIC registers
		if ( !BLK1_ACCESS && !BLK2_ACCESS && !BLK3_ACCESS && !BLK5_ACCESS && !RAM123_ACCESS && !IO23_ACCESS && CPU_WRITES_TO_BUS && A15 )
		{
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
			if ( addr >= 0x1000 && addr <= 0x100f )
				writeRegisterVIC656x( addr & 0x0f, D );
			#pragma GCC diagnostic pop
		}
	}
#endif

	}

#ifdef VIC_MODE
	if ( cfgVIC_Emulation )
	{
		tickVIC656x();

		// sound output
		if ( cfgVIC_Emulation && vic656x.hasSampleOutput )
		{
			if ( !( read32( RegMaiControl ) & BitMaiControlFull ) )
			{
				write32( RegMaiData, hdmiSampleData );
				write32( RegMaiData, hdmiSampleData );
			}

			vic656x.hasSampleOutput = 0;
		}		
	}
#endif

	if ( disableLatchesFIQ )
	{
		FINISH_BUS_HANDLING
	} else
	{
		static u8 cnt = 0;
		cnt ++;
		static u8 statusLED1 = 2;
		static u8 statusLED2 = 2;

		if ( cnt < ledActivityBrightness1 && statusLED1 != 1 )
		{
			statusLED1 = 1;
			latchSetClear( LATCH_LED0, 0 ); 
		} else
		if ( statusLED1 != 0 )
		{
			statusLED1 = 0;
			latchSetClear( 0, LATCH_LED0 );
		}
		if ( cnt < ledActivityBrightness2 && statusLED2 != 1 )
		{
			statusLED2 = 1;
			latchSetClear( LATCH_LED1, 0 ); 
		} else
		if ( statusLED2 != 0 )
		{
			statusLED2 = 0;
			latchSetClear( 0, LATCH_LED1 );
		}

		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
	}
}
#pragma GCC diagnostic pop




void mainMenu()
{
	CKernelMenu kernel;
	if ( kernel.Initialize() )
		kernel.Run();
}

int main( void )
{
	CKernelMenu kernel;
	kernel.Initialize();

	while ( true )
	{
		kernel.Run();
	}

	return EXIT_HALT;
}
