/*
  _________.__    .___      __   .__        __            _____                       
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __       /     \   ____   ____  __ __ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      /  \ /  \_/ __ \ /    \|  |  \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /    Y    \  ___/|   |  \  |  /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \____|__  /\___  >___|  /____/ 
        \/         \/    \/     \/       \/     \/             \/     \/     \/       
 
 kernel_menu.cpp

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

#include "kernel_menu.h"
#include "dirscan.h"
#include "config.h"
#include "c64screen.h"
#include "charlogo.h"

#include <math.h>

// we will read these files
static const char DRIVE[] = "SD:";
//static const char FILENAME_PRG[]  = "SD:C64/rpimenu.prg";		// .PRG to start
static const char FILENAME_MENU[]   = "SD:C64/menu.bin";			// 16k cart for menu
static const char FILENAME_CBM80[]  = "SD:C64/launch.cbm80";		// launch code (CBM80 8k cart)
static const char FILENAME_CONFIG[] = "SD:C64/sidekick64.cfg";		
static const char FILENAME_SIDKICK_CONFIG_1[] = "SD:C64/SIDKick_CFG1.prg";		
static const char FILENAME_SIDKICK_CONFIG_2[] = "SD:C64/SIDKick_CFG2.prg";		

static const char FILENAME_SPLASH_HDMI[]   = "SD:SPLASH/Sidekick-Logo.raw";		
static const char FILENAME_SPLASH_RGB[]    = "SD:SPLASH/sk64_main.tga";		
static const char FILENAME_SPLASH_RGB128[] = "SD:SPLASH/sk128_main.tga";		
static const char FILENAME_TFT_FONT[]      = "SD:SPLASH/PXLfont88665b-RF2.3-C64sys.bin";		

static char FILENAME_LOGO_RGBA128[] = "SD:SPLASH/sk128_logo_blend.tga";
char FILENAME_LOGO_RGBA[] = "SD:SPLASH/sk64_logo_blend.tga";

//
// background animation
//
bool showAnimation;
u8 *tempX;
unsigned char *animationRLE;
unsigned char *bitflagsRLE;
u32 *animationState;
u32 *animationStateInitial;


static u32	disableCart     = 0;
static u32	resetCounter    = 0;
static u32	transferStarted = 0;
static u32	currentOfs      = 0;

static u8   firstMenuAfterBoot = 1;

u32 prgSizeLaunch AAA;
unsigned char prgDataLaunch[ 65536 ] AAA;

// CBM80 to launch the menu
static unsigned char cart_pool2[ 16384 + 128 ] AAA;
static unsigned char *cartMenu AA;

// this memory can be called in the IO-areas
static unsigned char injectCode[ 256 ] AAA;

// custom charset
unsigned char charset[ 4096 ] AAA;

u32 releaseDMA = 0;
u32 doneWithHandling = 1;
u32 c64CycleCount = 0;
u32 nBytesRead = 0;
u32 first = 1;
u32 nmiMinCyclesWait = 0;
u32 menuUpdateTimeOut = 1024 * 1024 * 1024;
u32 menuTimeOutReset = 0;

#define MAX_FILENAME_LENGTH	512
char FILENAME[ MAX_FILENAME_LENGTH * 2 ];
char menuItemStr[ 512 ];

u32 wireSIDAvailable = 0;
static u32 wireSIDGotHigh = 0;
static u32 wireSIDGotLow = 0;

u32 wireKernalAvailable = 0;
u8 typeSIDAddr[ 5 ];
static u32 wireKernalDetectMode = 0;
static u32 wireKernalTrackAccess = 0;

static u32 launchKernel = 0;
static int lastChar = 0;
static u32 startForC128 = 0;

static u32 LED_DEACTIVATE_CART1_HIGH;	
static u32 LED_DEACTIVATE_CART1_LOW;	
static u32 LED_DEACTIVATE_CART2_HIGH;	
static u32 LED_DEACTIVATE_CART2_LOW;	

static u32 LED_ACTIVATE_CART1_HIGH;	
static u32 LED_ACTIVATE_CART1_LOW;		
static u32 LED_ACTIVATE_CART2_HIGH;	
static u32 LED_ACTIVATE_CART2_LOW;		

static u32 LED_INIT1_HIGH;	
static u32 LED_INIT1_LOW;	
static u32 LED_INIT2_HIGH;	
static u32 LED_INIT2_LOW;	
static u32 LED_INIT3_HIGH;	
static u32 LED_INIT3_LOW;	
static u32 LED_INIT4_HIGH;	
static u32 LED_INIT4_LOW;	
static u32 LED_INIT5_HIGH;	
static u32 LED_INIT5_LOW;	
static u32 LED_INITERR_HIGH;
static u32 LED_INITERR_LOW;

static void initScreenAndLEDCodes()
{
	if ( screenType == 0 ) // OLED with SCL and SDA (i.e. 2 Pins) -> 4 LEDs
	{
		 LED_DEACTIVATE_CART1_HIGH	= LATCH_LED_ALL;
		 LED_DEACTIVATE_CART1_LOW	= 0;
		 LED_DEACTIVATE_CART2_HIGH	= 0;
		 LED_DEACTIVATE_CART2_LOW	= LATCH_LED_ALL;

		 LED_ACTIVATE_CART1_HIGH	= LATCH_LED_ALL;
		 LED_ACTIVATE_CART1_LOW		= 0;
		 LED_ACTIVATE_CART2_HIGH	= 0;
		 LED_ACTIVATE_CART2_LOW		= LATCH_LED_ALL;

		 LED_INIT1_HIGH				= LATCH_LED0;
		 LED_INIT1_LOW				= LATCH_LED1to3;
		 LED_INIT2_HIGH				= LATCH_LED1;
		 LED_INIT2_LOW				= 0;
		 LED_INIT3_HIGH				= LATCH_LED2;
		 LED_INIT3_LOW				= 0;
		 LED_INIT4_HIGH				= LATCH_LED3;
		 LED_INIT4_LOW				= 0;
		 LED_INIT5_HIGH				= 0;
		 LED_INIT5_LOW				= LATCH_LED_ALL;
		 LED_INITERR_HIGH			= LATCH_LED_ALL;
		 LED_INITERR_LOW			= 0;
	} else
	if ( screenType == 1 ) // RGB TFT with SCL, SDA, DC, RES -> 2 LEDs
	{
		 LED_DEACTIVATE_CART1_HIGH	= (LATCH_LED0|LATCH_LED1);
		 LED_DEACTIVATE_CART1_LOW	= 0; 
		 LED_DEACTIVATE_CART2_HIGH	= 0;
		 LED_DEACTIVATE_CART2_LOW	= (LATCH_LED0|LATCH_LED1);

		 LED_ACTIVATE_CART1_HIGH	= (LATCH_LED0|LATCH_LED1);
		 LED_ACTIVATE_CART1_LOW		= 0;
		 LED_ACTIVATE_CART2_HIGH	= 0;
		 LED_ACTIVATE_CART2_LOW		= (LATCH_LED0|LATCH_LED1);

		 LED_INIT1_HIGH				= LATCH_LED0;
		 LED_INIT1_LOW				= LATCH_LED1;
		 LED_INIT2_HIGH				= LATCH_LED1;
		 LED_INIT2_LOW				= LATCH_LED0;
		 LED_INIT3_HIGH				= LATCH_LED0;
		 LED_INIT3_LOW				= LATCH_LED1;
		 LED_INIT4_HIGH				= LATCH_LED1;
		 LED_INIT4_LOW				= LATCH_LED0;
		 LED_INIT5_HIGH				= 0;
		 LED_INIT5_LOW				= (LATCH_LED0|LATCH_LED1);
		 LED_INITERR_HIGH			= (LATCH_LED0|LATCH_LED1);
		 LED_INITERR_LOW			= 0;
	}
}

void deactivateCart()
{
	initScreenAndLEDCodes();
	first = 0;
	latchSetClearImm( LED_DEACTIVATE_CART1_HIGH, LED_DEACTIVATE_CART1_LOW | LATCH_RESET | LATCH_ENABLE_KERNAL );
	SET_GPIO( bGAME | bEXROM | bNMI | bDMA );

	if ( screenType == 0 )
	{
		for ( int j = 255; j > 63; j -- )
		{
			oledSetContrast( j );
			flushI2CBuffer( true );
			DELAY( 1 << 17 );
		}
	} else
	if ( screenType == 1 )
	{
		flush4BitBuffer( true );
		tftBlendRGBA( 0, 0, 0, 128, tftFrameBuffer, 8 );
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	} 
	
	DELAY( 1 << 10 );
	latchSetClearImm( LATCH_RESET | LED_DEACTIVATE_CART2_HIGH, LED_DEACTIVATE_CART2_LOW | LATCH_ENABLE_KERNAL );

	DELAY( 1 << 27 );

	c64CycleCount = 0;
	disableCart = 1;
}


void globalMemoryAllocation()
{
	initGlobalMemPool( 128 * 1024 * 1024 );

	tempX = (u8*)getPoolMemory( 256 * 1024 );
	animationRLE = (u8*)getPoolMemory( 128 * 1024 );
	bitflagsRLE  = (u8*)getPoolMemory( 128 * 1024 / 8 );
	animationState = (u32*)getPoolMemory( sizeof( u32 ) * 192 * 147 / 8 );
	animationStateInitial = (u32*)getPoolMemory( sizeof( u32 ) * 192 * 147 / 8 );

	extern u8 *flash_cacheoptimized_pool;//[ 1024 * 1024 + 8 * 1024 ] AAA;
	u64 p = (u64)getPoolMemory( 1024 * 1024 + 8 * 1024 + 128 );
	p = ( p + 128ULL ) & ~128ULL;
	flash_cacheoptimized_pool = (u8*)p;

	extern u8 *geoRAM_Pool;
	p = (u64)getPoolMemory( 4096 * 1024 + 256 );
	p = ( p + 128ULL ) & ~128ULL;
	geoRAM_Pool = (u8*)p;
}


// cache warmup

volatile u8 forceRead;
volatile u8 nextByte;
volatile u32 nextByteAddr;

static void *pFIQ = NULL;

__attribute__( ( always_inline ) ) inline void warmCache( void *fiqh )
{
	CACHE_PRELOAD_DATA_CACHE( cartMenu, 16384, CACHE_PRELOADL2KEEP );

	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)fiqh, 1024*3 );

	FORCE_READ_LINEAR32a( cartMenu, 16384, 16384 * 16 );

	if ( fiqh )
	{
		FORCE_READ_LINEARa( (void*)fiqh, 1024*3, 65536 );
	}
}

__attribute__( ( always_inline ) ) inline void warmCacheOnlyPL( void *fiqh )
{
	CACHE_PRELOAD_DATA_CACHE( cartMenu, 16384, CACHE_PRELOADL2KEEP );
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)fiqh, 2048*2 );
}

bool toggleVDCMode;
u8 currentVDCMode = 0;
u8 vdc40ColumnMode = 0;

void activateCart()
{
	initScreenAndLEDCodes();
	disableCart = 0;
	transferStarted = 0;
	first = 1;
	toggleVDCMode = true;

	latchSetClearImm( LED_ACTIVATE_CART1_HIGH, LED_ACTIVATE_CART1_LOW | LATCH_RESET | LATCH_ENABLE_KERNAL );
	SETCLR_GPIO( bNMI | bDMA, bGAME | bEXROM );

	latchSetClearImm( LED_ACTIVATE_CART1_HIGH, LED_ACTIVATE_CART1_LOW | LATCH_ENABLE_KERNAL );

	DELAY( 1 << 20 );

	if ( screenType == 0 )
	{
		oledSetContrast( 255 );
		flushI2CBuffer();
	} else
	if ( screenType == 1 )
	{
		flush4BitBuffer( true );
		tftCopyBackground2Framebuffer();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}
	SyncDataAndInstructionCache();
	warmCache( pFIQ );
	warmCache( pFIQ );

	latchSetClearImm( LATCH_RESET | LED_ACTIVATE_CART2_HIGH, LED_ACTIVATE_CART2_LOW | LATCH_ENABLE_KERNAL );
}

char filenameKernal[ 2048 ];


CLogger			*logger;
CScreenDevice	*screen;

#define HDMI_SOUND

s32 lastHDMISoundSample = 0;
#ifdef HDMI_SOUND
u8 hdmiSoundAvailable = 0;
CHDMISoundBaseDevice *hdmiSoundDevice = NULL;
#endif

#ifdef COMPILE_MENU_WITH_SOUND
CTimer				*pTimer;
CScheduler			*pScheduler;
CInterruptSystem	*pInterrupt;
#ifndef HDMI_SOUND
CVCHIQDevice		*pVCHIQ;
#endif
#endif
CVCHIQDevice		*pVCHIQ;

// create color look up tables for the menu
const unsigned char fadeTab[ 16 ] = { 0, 15, 9, 14, 2, 11, 0, 10, 9, 0, 2, 0, 11, 5, 6, 12 };
unsigned char fadeTabStep[ 16 ][ 5 ];

bool rpiIsZero2 = false;
bool rpiHasAudioJack = true;

u32 globalReset = 0, timeOutReset = 0, bootReset = 0, launchReset = 0;

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
	pScheduler = &m_Scheduler;
	pInterrupt = &m_Interrupt;

	//#ifndef HDMI_SOUND
	if ( bOK ) bOK = m_VCHIQ.Initialize();
	pVCHIQ = &m_VCHIQ;
	//#endif
#endif

	CMachineInfo * m_pMachineInfo;

	if ( m_pMachineInfo->Get()->GetMachineModel () == MachineModelZero2W )
	{
		rpiHasAudioJack = false;
		rpiIsZero2 = true;
	}

	// initialize ARM cycle counters (for accurate timing)
	initCycleCounter();

	// initialize GPIOs
	gpioInit();

	// initialize latch and software I2C buffer
	initLatch();

	initScreenAndLEDCodes();
	latchSetClearImm( LED_INIT1_HIGH, LED_INIT1_LOW );

	u32 size = 0;

	globalMemoryAllocation();

#if 1
	extern u8 *flash_cacheoptimized_pool;
	u8 *tempHDMI = flash_cacheoptimized_pool;
	readFile( logger, (char*)DRIVE, (char*)FILENAME_SPLASH_HDMI, tempHDMI, &size );
	u32 xOfs = ( screen->GetWidth() - 640 ) / 2;
	u32 yOfs = ( screen->GetHeight() - 480 ) / 2;
	u8 *p = tempHDMI;
	for ( u32 j = 0; j < screen->GetHeight(); j++ )
		for ( u32 i = 0; i < screen->GetWidth(); i++ )
		{
			#define _CONV_RGB(red, green, blue)	  (((red>>3) & 0x1F) << 11 \
								| ((green>>3) & 0x1F) << 6 \
								| ((blue>>3) & 0x1F))
			
			int a = i - xOfs; if ( a < 0 ) a = 0; if ( a > 639 ) a = 639;
			int b = j - yOfs; if ( b < 0 ) b = 0; if ( b > 479 ) b = 479;
			p = &tempHDMI[ ( a + b * 640 ) * 3 ];
			screen->SetPixel( a, b, _CONV_RGB( p[ 0 ], p[ 1 ], p[ 2 ] )  ); 
		}
#endif

	// read launch code
	cartMenu = (unsigned char *)( ((u64)&cart_pool2+64) & ~63 );
	readFile( logger, (char*)DRIVE, (char*)FILENAME_MENU, cartMenu, &size );
	
	// attention
	cartMenu[ 0x1fe0 ] = 0xff; // this flag indicates that the Sidekick has just booted (to prevent repetious checks for VIC type etc)
	cartMenu[ 0x1fe1 ] = 0x00; // this flag indicates that VDC-support is disabled (can be turned on by C128 detection!)

	latchSetClearImm( LED_INIT2_HIGH, LED_INIT2_LOW );
	latchSetClearImm( LED_INIT3_HIGH, LED_INIT3_LOW );

	extern void scanDirectories( char *DRIVE );
	scanDirectories( (char *)DRIVE );

	latchSetClearImm( LED_INIT4_HIGH, LED_INIT4_LOW );

	//
	// guess automatic timings from current RPi clock rate
	//
	u32 rpiClockMHz = (u32)(m_CPUThrottle.GetClockRate () / 1000000);
	switch ( rpiClockMHz )
	{
	case 1500:
		setDefaultTimings( AUTO_TIMING_RPI3PLUS_C64C128 );
		break;
	case 1400:
	default:
		setDefaultTimings( AUTO_TIMING_RPI3PLUS_C64 );
		break;
	case 1200:
		setDefaultTimings( AUTO_TIMING_RPI0_C64 );
		break;
	case 1300:
		setDefaultTimings( AUTO_TIMING_RPI0_C64C128 );
		break;
	};

	if ( !readConfig( logger, (char*)DRIVE, (char*)FILENAME_CONFIG ) )
	{
		latchSetClearImm( LED_INITERR_HIGH, LED_INITERR_LOW );
		logger->Write( "RaspiMenu", LogPanic, "error reading .cfg" );
	}

	//readFavorites( logger, (char *)DRIVE );

	extern int fileExists( CLogger *logger, const char *DRIVE, const char *FILENAME );

	extern char skinAnimationFilename[ 1024 ];
	showAnimation = false;
	if ( skinAnimationFilename[ 0 ] != 0 )
	{
		if ( fileExists( logger, (char*)DRIVE, (char*)skinAnimationFilename ) > 0 )
		{
			showAnimation = true;
			readFile( logger, (char*)DRIVE, (char*)skinAnimationFilename, tempX, &size ); 
		} else
		if ( fileExists( logger, (char*)DRIVE, (char*)(char*)"SD:C64/animation.zap" ) > 0 )
		{
			showAnimation = true;
			readFile( logger, (char*)DRIVE, (char*)"SD:C64/animation.zap", tempX, &size ); 
		} 

		if ( showAnimation )
		{
			u8 *temp = &tempX[ 0 ];
			u32 compressedSize = *(u32*)temp;
			memcpy( animationRLE, temp + 4, compressedSize );
			temp += 4 + compressedSize;

			u32 bitflagSize = *(u32*)temp;
			memcpy( bitflagsRLE, temp + 4, bitflagSize );
			temp += 4 + bitflagSize;

			memcpy( animationState, temp, (192 * 147 / 8)*4 ); 
			memcpy( animationStateInitial, temp, (192 * 147 / 8)*4 ); 
		}
	}

	u32 t;
	if ( skinFontFilename[0] != 0 && readFile( logger, (char*)DRIVE, (char*)skinFontFilename, charset, &t ) )
	{
		skinFontLoaded = 1;
		//memcpy( 1024 + charset+8*(91), skcharlogo_raw, 224 ); <- this is the upper case font used in the browser, skip logo to keep all PETSCII characters
		memcpy( 2048 + charset+8*(91), skcharlogo_raw, 224 );
		memcpy( charset + 94 * 8, charset + 2048 + 233 * 8, 8 );
	} 

	// prepare a few things for color fading and updates
	for ( int j = 0; j < 5; j++ )
		for ( int i = 0; i < 16; i++ )
		{
			if ( j == 0 ) 
				fadeTabStep[ i ][ j ] = fadeTab[ i ]; else
				fadeTabStep[ i ][ j ] = fadeTab[ fadeTabStep[ i ][ j - 1 ] ]; 
		}

	char col[32] = { (char)skinValues.SKIN_BORDER_COL1, (char)skinValues.SKIN_BORDER_COL2, (char)skinValues.SKIN_BORDER_COL2, 
					 (char)skinValues.SKIN_BORDER_COL1, (char)skinValues.SKIN_BORDER_COL0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
					 (char)skinValues.SKIN_BORDER_COL0, (char)skinValues.SKIN_BORDER_COL1 };
	memcpy( &cartMenu[ 0x1c00 ], col, 16 );

	col[ 0 ] = fadeTab[ skinValues.SKIN_MENU_TEXT_ITEM & 15 ];
	col[ 1 ] = fadeTab[ skinValues.SKIN_MENU_TEXT_KEY & 15 ];
	col[ 2 ] = 1;
	col[ 3 ] = 0;
	col[ 4 ] = skinValues.SKIN_MENU_TEXT_KEY & 15;
	col[ 5 ] = skinValues.SKIN_MENU_TEXT_ITEM & 15;
	memcpy( &cartMenu[ 0x1c10 ], col, 6 );

	readSettingsFile();

#ifdef HDMI_SOUND
	hdmiSoundAvailable = 1;
	hdmiSoundDevice = new CHDMISoundBaseDevice( 48000 );
	if ( hdmiSoundDevice )
	{
		hdmiSoundDevice->SetWriteFormat( SoundFormatSigned24, 2 );
		if (!hdmiSoundDevice->Start ())
		{
			m_Logger.Write ("SK64", LogPanic, "Cannot start sound device");
			hdmiSoundAvailable = 0;
		}
	} else
		hdmiSoundAvailable = 0;
#endif

	applySIDSettings();
	renderC64();
	startInjectCode();
	disableCart = 0;

	latchSetClearImm( LED_INIT5_HIGH, LED_INIT5_LOW );

	return bOK;
}

//
//
//
#define RELOC_BASE	0x7000


// a hacky way of defining code that can be executed from the C64 (currently only POKE)
// this is still used, but actually legacy stuff -- the new menu will create speedcode all over the place, but not using these methods
u32 injectCodeOfs = 16;

void startInjectCode()
{
	memset( injectCode, 0x60, 256 );
	injectCodeOfs = 16;
}

void injectPOKE( u32 a, u8 d )
{
	injectCode[ injectCodeOfs++ ] = 0xA9;
	injectCode[ injectCodeOfs++ ] = d;
	injectCode[ injectCodeOfs++ ] = 0x8D;
	injectCode[ injectCodeOfs++ ] = (a)&255;
	injectCode[ injectCodeOfs++ ] = ((a)>>8)&255;
}

//
// some macros for generating speed code in cartMenu
//
#define LDA( v ) { \
	cartMenu[ curAddr ++ ] = 0xA9;							\
	cartMenu[ curAddr ++ ] = v; }

#define STA( addr ) { \
	cartMenu[ curAddr ++ ] = 0x8D;							\
	cartMenu[ curAddr ++ ] = (addr)&255;					\
	cartMenu[ curAddr ++ ] = ((addr)>>8)&255; }

#define POKE( a, d ) { LDA( d ) STA( a ) }

#define LDX( v ) { \
	cartMenu[ curAddr ++ ] = 0xA2;							\
	cartMenu[ curAddr ++ ] = v; }

#define STX( addr ) { \
	cartMenu[ curAddr ++ ] = 0x8E;							\
	cartMenu[ curAddr ++ ] = (addr)&255;					\
	cartMenu[ curAddr ++ ] = ((addr)>>8)&255; }

#define LDY( v ) { \
	cartMenu[ curAddr ++ ] = 0xA0;							\
	cartMenu[ curAddr ++ ] = v; }

#define STY( addr ) { \
	cartMenu[ curAddr ++ ] = 0x8C;							\
	cartMenu[ curAddr ++ ] = (addr)&255;					\
	cartMenu[ curAddr ++ ] = ((addr)>>8)&255; }

#define JSR( addr ) { \
	cartMenu[ curAddr ++ ] = 0x20;							\
	cartMenu[ curAddr ++ ] = (addr)&255;					\
	cartMenu[ curAddr ++ ] = ((addr)>>8)&255; }

#define BIT_( addr ) { \
	cartMenu[ curAddr ++ ] = 0x2C;							\
	cartMenu[ curAddr ++ ] = (addr)&255;					\
	cartMenu[ curAddr ++ ] = ((addr)>>8)&255; }

#define BPL( ofs ) { \
	cartMenu[ curAddr ++ ] = 0x10;							\
	cartMenu[ curAddr ++ ] = ofs; }

#define NOP cartMenu[ curAddr ++ ] = 0xEA;
#define RTS cartMenu[ curAddr ++ ] = 0x60;

u32 sk64Command = 0xff;
s32 delayUpdateMenu = 0;
s32 forcedUpdateScreen = 0;

u32 updateLogo = 0;
unsigned char tftC128Logo[ 240 * 240 * 2 ];
extern unsigned char tftBackground[ 240 * 240 * 2 ];

u32 endOfBankAddress = 0xffff;

// the text screen is converted to a bitmap (+ dilatation) for updating the background sprite layer
unsigned char framebuffer[ 320 * 200 / 8 ];

void convertScreenToBitmap( unsigned char *framebuffer )
{
	u32 columns = 40; 
	u32 rows = 25;

	memset( framebuffer, 0, 320 * 200 / 8 );

	for ( u32 j = 8; j < rows; j++ )
	{
		for ( u32 i = 1; i < columns-1; i++ )
		{
			unsigned char c = c64screen[ i + j * 40 ];
			int x = i * 8;
			int y = j * 8;

			if ( c != 32 && c != ( 32 + 128 ) )
			for ( int b = 0; b < 8; b++ )
			{
				extern u8 c64screenUppercase;
				unsigned char v = charset[ 2048 - c64screenUppercase * 2048 + c * 8 + b ];

				u32 *p = (u32*)&framebuffer[ ( x / 8 - 1 ) + ( b + y ) * 320 / 8 ];
				u32 m2 = ( (u32)v ) << 8;
				m2 &= 0xff00;
				u32 m = m2;
				u8 *d = (u8*)&m;
				d[ 1 ] = v;
				d[ 0 ] = v >> 7;
				d[ 1 ] |= v << 1;
				d[ 1 ] |= v >> 1;
				d[ 2 ] = ( v << 7 ) & 128;
				*p |= m;
				*( p + 320 / 8 / 4 ) |= m;
				*( p - 320 / 8 / 4 ) |= m;
			}
		}
	}
}

// all this is used for rendering the menu
static int startAfterReset = 1;
static int firstMenu = 1;
static int firstSprites = 1;
static bool allSpeedCodeExecuted = false;

static unsigned char bitmapPrev[ 64 * 64 ];
unsigned char bitmap[ 64 * 64 ];
static int ctn = 0;

static u32 menuSpeedCodeLength = 0;
static u32 menuSpeedCodeMaxLength = 0x800;

u32 freezeNMICycles = 0, countWrites = 0, readyForNMIs = 0;

static unsigned char c64screenPrev[ 1024 ], c64colorPrev[ 1024 ];
static int lastCurOfsText = 0;
static int lastCurOfs = 0;

static int ledActivityBrightness = 0;

static int delayCommandKey = 0;
static int delayCommandTicks = 0;
static int delayFrame = 0;
static int postDelayFrame = 0;
static int postDelayTicks = 0;

static int disableFIQ_Falling = 0;

void CKernelMenu::Run( void )
{
	delayUpdateMenu = 0;
	nBytesRead		= 0;
	c64CycleCount	= 0;
	lastChar		= 0;
	startForC128	= 0;
	resetCounter    = 0;
	transferStarted = 0;
	currentOfs      = 0;
	launchKernel	= 0;
	updateMenu      = 0;
	updateLogo      = 0;
	subHasKernal	= -1;
	subHasLaunch	= -1;
	FILENAME[0]		= 0;
	first			= 1;

	delayUpdateMenu = 0;
	doneWithHandling = 1;
	firstMenu = firstSprites = 1;
	startAfterReset = 1;

	memset( bitmapPrev, 255, 64 * 64 );
	ctn = 0;

	menuSpeedCodeLength = 0;
	menuSpeedCodeMaxLength = 0x800;

	freezeNMICycles = 0;
	countWrites = 0;
	readyForNMIs = 0;

	memset( c64colorPrev, 255, 1024 );
	memset( c64screenPrev, 255, 1024 );
	lastCurOfsText = 0;
	lastCurOfs = 0;
	int firstRunAfterReset = 1;

	wireSIDAvailable = 0;
	wireSIDGotLow    = 0;
	wireSIDGotHigh   = 0;

	wireKernalAvailable   = 0;
	wireKernalDetectMode  = 0;
	wireKernalTrackAccess = 0;

	latchSetClearImm( LATCH_LED0, LATCH_RESET );

	if ( !disableCart )
	{
		SETCLR_GPIO( bNMI, bGAME | bEXROM | bDMA );

		freezeNMICycles = 0;
		countWrites = 0;
		readyForNMIs = 0;

		latchSetClearImm( 0, LATCH_RESET | LATCH_ENABLE_KERNAL );
	}

	if ( screenType == 0 )
	{
		splashScreen( raspi_c64_splash );
	} else
	if ( screenType == 1 )
	{
		tftLoadCharset( DRIVE, FILENAME_TFT_FONT );
		tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB128, 8 ); 
		memcpy( tftC128Logo, tftBackground, 240 * 240 * 2 );
		tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 ); 
		if ( modeC128 )
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB128, 8 ); else
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 ); 
		tftCopyBackground2Framebuffer();
		tftInitImm( screenRotation );
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}

	cartMenu[ 8192 ] = 0x60;

	if ( !disableCart )
	{
		// setup FIQ
		DisableIRQs();
		m_InputPin.ConnectInterrupt( this->FIQHandler, this );
		m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );
		m_InputPin.EnableInterrupt2( GPIOInterruptOnFallingEdge );
		EnableIRQs();

		launchKernel = 0;

		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();

		pFIQ = (void*)this->FIQHandler;
		warmCache( pFIQ );
		warmCache( pFIQ );
		DELAY(1<<20);

		CACHE_PRELOAD_INSTRUCTION_CACHE( this->FIQHandler, 2048*2 );
		FORCE_READ_LINEARa( this->FIQHandler, 1024*3, 65536 );
	}

	// start c64 
	SET_GPIO( bNMI | bDMA );
	//latchSetClearImm( LATCH_RESET, 0 );
	latchSetClear( LATCH_RESET, 0 );

	latchSetClear( 0, LATCH_LED0 | LATCH_LED1 );

	delayCommandKey = 0;
	delayFrame = 0;
	postDelayTicks = 7;
	postDelayFrame = 1;
	int screenFadeTasks = 0;
	toggleVDCMode = true;
	disableFIQ_Falling = 0;

	// wait forever
	while ( true )
	{
		if ( !disableCart )
		{
			if ( menuTimeOutReset )
			{
				timeOutReset ++;
				toggleVDCMode = true;
				latchSetClear( LATCH_LED0, LATCH_RESET );
				DELAY(1<<12);
				latchSetClear( LATCH_RESET | LATCH_LED0, 0 );
				menuTimeOutReset = 0;
			}
			if ( ( ( first == 1 ) && nBytesRead > 0 ) ||
				 ( ( first == 2 ) && nBytesRead < 32 && c64CycleCount > 1000000 ) )
			{
				bootReset ++;
				if ( first == 1 ) first = 2; else first = 0;
				latchSetClear( LATCH_LED0, LATCH_RESET );
				DELAY(1<<12);
				latchSetClear( LATCH_RESET | LATCH_LED0, 0 );
			}
		}
		
		asm volatile ("wfi");

		if ( launchKernel && allSpeedCodeExecuted )
		{
			launchReset ++;
			allSpeedCodeExecuted = false;
			m_InputPin.DisableInterrupt();
			m_InputPin.DisableInterrupt2();
			m_InputPin.DisconnectInterrupt();
			EnableIRQs();
			return;
		}

		// status LED flashing
		if ( screenType == 0 )
		{
			u32 v = 1 << ( ( c64CycleCount >> (18+1) ) % 6 );
			u32 l_on = ( v + ( ( v & 16 ) >> 2 ) + ( ( v & 32 ) >> 4 ) ) & 15;
			u32 l_off = ( ~l_on ) & 15;
			latchSetClear( l_on * LATCH_LED0, l_off * LATCH_LED0 );
		} else
		if ( screenType == 1 )
		{
			u32 v = ( c64CycleCount >> (12+1) ) & 511;
			if ( v < 256 )
				ledActivityBrightness = v; else
				ledActivityBrightness = 511 - v;				
		}

		//
		//
		// handle key input, update the C64/C128-screen and generate speed code (+lots of handling for fading, controlling VDC output etc.)
		//
		//
		if ( updateMenu == 1 )
		{
			u32 tempTimeOut = menuUpdateTimeOut;
			menuUpdateTimeOut = 1024 * 1024 * 1024;

			static int swapColorProfile = 0;

			//
			// scroll speed, color fading orchestration etc.
			// 

			// reduced scroll speed?
			if ( skinValues.SKIN_BROWSER_SCROLL_SPEED == 0 )
			{
				static int veryLastChar = -1;
				if ( veryLastChar == lastChar && ( lastChar == 158 || lastChar == 29 || lastChar == 145 || lastChar == 17 ) )
				{
					veryLastChar = -1;
					lastChar = 0;
				} else
					veryLastChar = lastChar;
			}

			if ( lastChar == 61 )
				swapColorProfile = 1;

			bool initializeSpriteColors = false;
			const int DEFAULT_FADE_SLOWER = 1;
			static int FADE_SLOWER = DEFAULT_FADE_SLOWER;
			if ( skinValues.SKIN_COLORFADING )
			{
				if ( delayCommandTicks )
				{
					if ( --delayCommandTicks == 0 )
					{
						FADE_SLOWER = DEFAULT_FADE_SLOWER;
						lastChar = delayCommandKey;
						delayFrame = 0;
						if ( !( lastChar == 1094 || lastChar == 1222 ) )
						{
							postDelayTicks = 6*FADE_SLOWER;
							postDelayFrame = 1;
						}
						delayCommandKey = 0;
						if ( swapColorProfile )
						{
							nextSkinProfile();
							if ( !skinValues.SKIN_COLORFADING )
							{
								initializeSpriteColors = true;
								delayCommandKey = delayFrame = 0;
								postDelayTicks = 7;
								postDelayFrame = 1;
								screenFadeTasks = 0;
							}
						}
						swapColorProfile = 0;
					}
				} else
				if ( postDelayTicks )
				{
					if ( --postDelayTicks == 0 )
						postDelayFrame = 0;
				} else
				{
					if ( swapColorProfile || (screenFadeTasks = checkForScreenFade( lastChar, menuItemStr, &startForC128 )) )
					{
						if ( screenFadeTasks == 1 )
							delayCommandTicks = 8*FADE_SLOWER; else
							delayCommandTicks = 10*FADE_SLOWER; 
						if ( swapColorProfile )
							screenFadeTasks = 3;
						FADE_SLOWER = DEFAULT_FADE_SLOWER;
						delayCommandKey = lastChar;
						postDelayFrame = 0;
						postDelayTicks = 0;
						delayFrame = 1;
						lastChar = 0;
					} 
				}
			} else
			{
				if ( swapColorProfile )
				{
					nextSkinProfile();
					initializeSpriteColors = true;
					screenFadeTasks = 3;
					delayCommandTicks = 0;
					delayCommandKey = 0;
					delayFrame = 0;
					postDelayTicks = 6*FADE_SLOWER;
					postDelayFrame = 1;
					swapColorProfile = 0;
				}
			}
			if ( lastChar == 2 )
			{
				if ( skinValues.SKIN_COLORFADING )
				{
					//if ( firstMenuAfterBoot )
						//FADE_SLOWER = DEFAULT_FADE_SLOWER * 2;
					screenFadeTasks = 3;
					delayCommandTicks = 0;
					delayCommandKey = 0;
					delayFrame = 0;
					postDelayTicks = 6*FADE_SLOWER;
					postDelayFrame = 1;
				} else
				{
					initializeSpriteColors = true;
				}
				lastChar = 0;
				firstMenuAfterBoot = 0;
			}

			if ( vdcSupport && ( lastChar == 1094 || lastChar == 1222 ) )
			{
				toggleVDCMode = true;
				currentVDCMode = ( currentVDCMode + 1 ) % 3;
				// invalidate everything in screen buffers
				firstMenu = 1;
				//if ( currentVDCMode == 1 ) currentVDCMode ++;
			}

			if ( vdcSupport && ( lastChar == 94 || lastChar == 222 ) )
			{
				if ( lastChar == 222 )
					vdc40ColumnMode = 1; else
					vdc40ColumnMode = 2;
				delayCommandKey = lastChar + 1000;
				screenFadeTasks = postDelayFrame = postDelayTicks = delayFrame = lastChar = 0;
				delayCommandTicks = 6;
			}

			wireSIDAvailable = 0;
			if ( wireSIDGotLow && wireSIDGotHigh )
				wireSIDAvailable = 1;

			if ( updateLogo == 1 && screenType == 1 && modeC128 )
			{
				updateLogo = 2;
				memcpy( tftBackground, tftC128Logo, 240 * 240 * 2 );
				flush4BitBuffer( true );
				tftCopyBackground2Framebuffer();
				tftSendFramebuffer16BitImm( tftFrameBuffer );
			}

			startForC128 = 0;
			handleC64( lastChar, &launchKernel, FILENAME, filenameKernal, menuItemStr, &startForC128 );
	
			if ( screenFadeTasks == 3 && delayCommandTicks <= 2 )
			{
				memset( c64screen, 32, 1000 );
				memset( c64color, 32, 1000 );
			} else
			{
				if ( ( delayCommandKey == 1094 || delayCommandKey == 1222 ) && currentVDCMode == 2 )
				{
					memset( c64color, 0, 1000 );
					extern u32 menuScreen;
					if ( menuScreen == 0x01 )
						printC64( 0, 22, "    press (shift+)\x9e for vdc output!     ", 1, 0 ); else
						printC64( 0, 22, "    press (shift+)\x9e for VDC output!     ", 1, 0 ); 
				} else
					renderC64();
			}

			/*char tt[64];
			static int lastPrintChar = 0;
			if ( lastChar != 0 )
				lastPrintChar = lastChar;
			//sprintf( tt, "key=%d", lastPrintChar );
			sprintf( tt, "vdc=%d, support=%d, %d", currentVDCMode, vdcSupport, vdc40ColumnMode );
			printC64( 0,  0, tt, 1, 0 );*/

			if ( skinValues.SKIN_COLORFADING && !( lastChar == 1094 || lastChar == 1222 ) )
			if ( delayFrame || postDelayFrame )
			{
				int x = delayFrame - 1;
				if ( postDelayFrame ) x = (6*FADE_SLOWER - postDelayFrame);

				int s = min( 4, max( 0, x/(2*FADE_SLOWER) ) );
				for ( int i = 0; i < 1000; i++ )
				{
					c64color[ i ] = fadeTabStep[ c64color[ i ] ][ s ];
					if ( c64color[ i ] == 0 )
						c64screen[ i ] = 32;
				}
			}

			convertScreenToBitmap( framebuffer );

			if ( showAnimation && currentVDCMode < 2 )
			{
				ctn++;
				if ( currentVDCMode == 1 )
				{
					memset( bitmap, 0, 64 * 64 );
				} else
				{
					if ( skinValues.SKIN_BACKGROUND_GFX_SPEED <= 1 || ( ( ctn % skinValues.SKIN_BACKGROUND_GFX_SPEED ) == 0 ) )
					{
						#include "render_sprite_animation.h"
					}
				}
			}

			lastChar = 0;

			if ( modePALNTSC == 1 || modePALNTSC == 2 )
				menuSpeedCodeMaxLength = 0x780; else
				menuSpeedCodeMaxLength = 0x800;

			memset( &cartMenu[ 0x2000 ], 0x60, 0x2000 );

			// generate copy code for screen transfer
			u32 curValA = 0xffff, curValX = 0xffff;
			u32 curAddr = 0x2000, curOfs = 0;

			//
			// this is ugly and hardcoded:
			// generate code for sprite and gradient color fading
			//
			if ( ( ( skinValues.SKIN_COLORFADING && !( lastChar == 1094 || lastChar == 1222 ) ) && ( delayFrame || postDelayFrame ) ) || initializeSpriteColors )
			{
				int x = delayFrame - 1;
				if ( postDelayFrame ) x = (6*FADE_SLOWER - postDelayFrame);

				int s = min( 4, max( 0, x/(2*FADE_SLOWER) ) );

				if ( ( screenFadeTasks & 2 ) || initializeSpriteColors )
				{
					char c0, c1, c2;
					if ( postDelayFrame == 6*FADE_SLOWER || initializeSpriteColors )
					{
						c0 = skinValues.SKIN_BORDER_COL0 & 15;
						c1 = skinValues.SKIN_BORDER_COL1 & 15;
						c2 = skinValues.SKIN_BORDER_COL2 & 15;
					} else
					{
						c0 = fadeTabStep[ skinValues.SKIN_BORDER_COL0 & 15 ][ s ];
						c1 = fadeTabStep[ skinValues.SKIN_BORDER_COL1 & 15 ][ s ];
						c2 = fadeTabStep[ skinValues.SKIN_BORDER_COL2 & 15 ][ s ];
					}

					// this is how the colors go to memory
					//char col[32] = { c1, c2, c2, c1, c0, 0, 0, 0, 0, 0, 0, 0, 0, 0, c0, c1 };
					POKE( RELOC_BASE + 0x0e00 + 32 - 2, c1 );
					POKE( RELOC_BASE + 0x0e00 + 32 - 1, c2 );
					POKE( RELOC_BASE + 0x0e00 + 32 - 0, c2 );
					POKE( RELOC_BASE + 0x0e00 + 32 + 1, c1 );
					POKE( RELOC_BASE + 0x0e00 + 32 + 2, c0 );

					POKE( RELOC_BASE + 0x0e00 + 32 + 12, c0 );
					POKE( RELOC_BASE + 0x0e00 + 32 + 13, c1 );
					POKE( RELOC_BASE + 0x0e00 + 78 - 2, c1 );
					POKE( RELOC_BASE + 0x0e00 + 78 - 1, c2 );
					POKE( RELOC_BASE + 0x0e00 + 78 - 0, c2 );
					POKE( RELOC_BASE + 0x0e00 + 78 + 1, c1 );
					POKE( RELOC_BASE + 0x0e00 + 78 + 2, c0 );
					POKE( RELOC_BASE + 0x0e00 + 78 + 12, c0 );
					POKE( RELOC_BASE + 0x0e00 + 78 + 13, c1 );

					if ( postDelayFrame == 6 * FADE_SLOWER || initializeSpriteColors )
						{ POKE( RELOC_BASE + 0x0e00 + 98, skinValues.SKIN_BACKGROUND_GFX_COLOR ); } else
						{ POKE( RELOC_BASE + 0x0e00 + 98, fadeTabStep[ skinValues.SKIN_BACKGROUND_GFX_COLOR ][ s ] ); }
				}

				int s2 = min( 3, s + 1 );

				if ( initializeSpriteColors )
				{
					s = 0; s2 = 1;
				}
				// highlight color
				u8 highlightCol = 1;
				const u8 darkColors[ 7 ] = { 0, 2, 4, 6, 8, 9, 11 };
				for ( int i = 0; i < 7; i++ )
					if ( skinValues.SKIN_MENU_TEXT_ITEM == darkColors[ i ] || 
						 skinValues.SKIN_MENU_TEXT_KEY == darkColors[ i ] )
						highlightCol = 15;
				if ( postDelayFrame == 6*FADE_SLOWER || initializeSpriteColors )
				{
					POKE( RELOC_BASE + 0x0e00 + 92, fadeTabStep[ skinValues.SKIN_MENU_TEXT_ITEM & 15 ][ s ] );
					POKE( RELOC_BASE + 0x0e00 + 93, fadeTabStep[ skinValues.SKIN_MENU_TEXT_KEY & 15 ][ s ] );
					POKE( RELOC_BASE + 0x0e00 + 94, highlightCol );
					POKE( RELOC_BASE + 0x0e00 + 96, skinValues.SKIN_MENU_TEXT_KEY & 15 );
					POKE( RELOC_BASE + 0x0e00 + 97, skinValues.SKIN_MENU_TEXT_ITEM & 15 );
				} else
				{
					POKE( RELOC_BASE + 0x0e00 + 92, fadeTabStep[ skinValues.SKIN_MENU_TEXT_ITEM & 15 ][ s2 ] );
					POKE( RELOC_BASE + 0x0e00 + 93, fadeTabStep[ skinValues.SKIN_MENU_TEXT_KEY & 15 ][ s2 ] );
					POKE( RELOC_BASE + 0x0e00 + 94, fadeTabStep[ highlightCol ][ s ] );
					POKE( RELOC_BASE + 0x0e00 + 96, fadeTabStep[ skinValues.SKIN_MENU_TEXT_KEY & 15 ][ s ] );
					POKE( RELOC_BASE + 0x0e00 + 97, fadeTabStep[ skinValues.SKIN_MENU_TEXT_ITEM & 15][ s ] );
				}
				initializeSpriteColors = false;
				if ( delayFrame ) delayFrame ++;
				if ( postDelayFrame ) postDelayFrame ++;
			}

			u8 vdcDirtyFlags[ 50 ]; // 25 lines of chars and attributes
			if ( firstMenu ) 
			{
				firstMenu = 0;
				for ( int i = 0; i < 1024; i++ )
				{
					c64screenPrev[ i ] = 255;
					c64colorPrev[ i ] = 255;
				}
				memset( vdcDirtyFlags, 1, 50 );
			} else
			{
				// always update one line in color RAM where the menu code might change it
				for ( int i = 80+10; i < 80+30; i++ )
					c64colorPrev[ i ] = 255;
				vdcDirtyFlags[ 25 + 0 ] = 1;
				vdcDirtyFlags[ 25 + 2 ] = 1;
			}

			curOfs = lastCurOfsText; 
			int ofsIncr = 1;

			int nBytesToTransfer = 1000;

			if ( currentVDCMode == 2 )
			{
				for ( int j = 0; j < 25; j++ )
				{
					int curOfs = j * 40;
					for ( int i = 0; i < 40; i++ )
					{
						if ( c64screen[ curOfs ] != c64screenPrev[ curOfs ] )
						{
							memcpy( &c64screenPrev[ j * 40 ], &c64screen[ j * 40 ], 40 );
							vdcDirtyFlags[ j ] = 1;
							goto colorTest;
						}
						curOfs ++;
					}
				colorTest:
					curOfs = j * 40;
					for ( int i = 0; i < 40; i++ )
					{
						if ( c64color[ curOfs ] != c64colorPrev[ curOfs ] )
						{
							memcpy( &c64colorPrev[ j * 40 ], &c64color[ j * 40 ], 40 );
							vdcDirtyFlags[ 25 + j ] = 1;
							goto nextLine;
						}
						curOfs ++;
					}
				nextLine:;
				}
			} else
			while ( nBytesToTransfer > 0 )
			{
				nBytesToTransfer--;

				u8 byteA = c64screen[ curOfs ];
				u8 byteX = c64color[ curOfs ];

				if ( byteA != c64screenPrev[ curOfs ] )
				{
					c64screenPrev[ curOfs ] = byteA;
					if ( byteA != curValA )
					{
						curValA = byteA;
						LDA( byteA );
					}
					STA( 0x400 + curOfs );

					vdcDirtyFlags[ curOfs / 40 ] = 1;
				}

				if ( byteX != c64colorPrev[ curOfs ] )
				{
					c64colorPrev[ curOfs ] = byteX;
					if ( byteX != curValX )
					{
						curValX = byteX;
						LDX( byteX );
					}
					STX( 0xd800 + curOfs );
					vdcDirtyFlags[ 25 + curOfs / 40 ] = 1;
				}

				if ( curAddr >= 0x2000 + menuSpeedCodeMaxLength - 10 )
					goto cantCopyEverythingThisTimeText;

				curOfs += ofsIncr;
				if ( curOfs >= 1000 )
				{
					curOfs = 0;
					if ( firstRunAfterReset )
					{
						firstSprites = 1;
						firstRunAfterReset = 0;
					}
				}
			}

		cantCopyEverythingThisTimeText:
			lastCurOfsText = curOfs;

			extern u32 showLogo;
			static u32 showLogoPrev = 0xffffffff;

			POKE( 0x8fc, showLogo );

			if ( showLogoPrev != showLogo )
			{
				menuSpeedCodeMaxLength *= 2;

				for ( u32 j = 0; j < 4; j++ )
					for ( u32 i = 0; i < 7; i++ )
					{
						u32 curOfsL = i + 33 + j * 40;

						u8 byteA = c64screen[ curOfsL ];
						u8 byteX = c64color[ curOfsL ];

						if ( !showLogo )
							byteA = byteX = 32;

						if ( byteA != c64screenPrev[ curOfsL ] )
						{
							c64screenPrev[ curOfsL ] = byteA;
							if ( byteA != curValA )
							{
								curValA = byteA;
								LDA( byteA );
							}
							STA( 0x400 + curOfsL );
						}

						if ( byteX != c64colorPrev[ curOfsL ] )
						{
							c64colorPrev[ curOfsL ] = byteX;
							if ( byteX != curValX )
							{
								curValX = byteX;
								LDX( byteX );
							}
							STX( 0xd800 + curOfsL );
						}
					}
			}
			
			showLogoPrev = showLogo;

			if ( vdcSupport && toggleVDCMode )
			{
				POKE( 0x08f8, currentVDCMode );			//enableVDC = $08f8
				POKE( 0x08f7, vdc40ColumnMode );		// switchVDCMode = $08f7
				toggleVDCMode = false;
			}

			// copy injected code from c64screen.cpp
			memcpy( &cartMenu[ curAddr ], &injectCode[ 16 ], injectCodeOfs - 16 );
			curAddr += injectCodeOfs - 16;

			// generate code for VDC-update
			if ( currentVDCMode )
			{
				// next line of chars/attributes to update
				static int vdcCurPosChar = 0;
				static int vdcCurPosAttr = 0;

				/*int nUpdates = 0;
				for ( int i = 0; i < 50; i++ )
					nUpdates += vdcDirtyFlags[ i ];

				static int forcedUpdatePosition = 0;
				if ( nUpdates < 10 )
				{
					for ( int i = 0; i < 2; i++ )
					{
						vdcDirtyFlags[ forcedUpdatePosition ] =
						vdcDirtyFlags[ forcedUpdatePosition + 25 ] = 1;
						forcedUpdatePosition ++;
						forcedUpdatePosition %= 25;
					}
				}*/

				#define VDC_WRITE( reg, data ) \
					LDY( reg );		\
					STY( 0xd600 );	\
					LDY( data );	\
					BIT_( 0xd600 );	\
					BPL( 0xfb );	\
					STY( 0xd601 );

				int nLines = 5, nLinesScanned = 0, lastTransferredLine = -1;

				curValA = 0xffff;

				LDX( 31 );
				while ( nLines > 0 && nLinesScanned < 25 )
				{
					if ( vdcDirtyFlags[ vdcCurPosChar ] )
					{
						// first line at all, start of screen or jump in address
						if ( lastTransferredLine <= 0 || ((lastTransferredLine+1)%25) != vdcCurPosChar )
						{
							int ofs = vdcCurPosChar * 40;
							VDC_WRITE( 19, ( ofs & 255 ) );
							VDC_WRITE( 18, ( ofs >> 8 ) );
						}
						lastTransferredLine = vdcCurPosChar;
						
						u8 *src = &c64screen[ vdcCurPosChar * 40 ];
						for ( int i = 0; i < 40; i++ )
						{
							u8 byteA = *(src++);
				
							STX( 0xd600 );

							if ( byteA != curValA )
							{
								curValA = byteA;
								LDA( byteA );
							}

							BIT_( 0xd600 );
							BPL( 0xfb );
							STA( 0xd601 );
						}
						vdcDirtyFlags[ vdcCurPosChar ] = 0;
						nLines --;
					}
					vdcCurPosChar ++;
					vdcCurPosChar %= 25;
					nLinesScanned ++;
				}

				nLines = 5;
				nLinesScanned = 0;
				lastTransferredLine = -1;

				const unsigned char vdcColorConversion[ 16 ] = {
				    0x00, 0x0f, 0x08, 0x07, 0x0a, 0x04, 0x02, 0x0d, 0x0e, 0x0c, 0x09, 0x01, 0x01, 0x05, 0x03, 0x0e
				};

				curValA = 0xffff;

				LDX( 31 );
				while ( nLines > 0 && nLinesScanned < 25 )
				{
					if ( vdcDirtyFlags[ 25 + vdcCurPosAttr ] )
					{
						// first line at all, start of screen or jump in address
						if ( lastTransferredLine <= 0 || ((lastTransferredLine+1)%25) != vdcCurPosAttr )
						{
							int ofs = vdcCurPosAttr * 40;
							VDC_WRITE( 19, ( ofs & 255 ) );
							VDC_WRITE( 18, ( 8 + ( ofs >> 8 ) ) );
						}
						lastTransferredLine = vdcCurPosAttr;
						
						u8 *src = &c64color[ vdcCurPosAttr * 40 ];
						for ( int i = 0; i < 40; i++ )
						{
							u8 byteA = *(src++) & 15;

							STX( 0xd600 );

							if ( byteA != curValA )
							{
								curValA = byteA;
								extern u8 c64screenUppercase;
								LDA( vdcColorConversion[ byteA ] | ((~c64screenUppercase)<<7) );
							}

							BIT_( 0xd600 );
							BPL( 0xfb );
							STA( 0xd601 );
						}
						vdcDirtyFlags[ 25 + vdcCurPosAttr ] = 0;
						nLines --;
					}
					vdcCurPosAttr ++;
					vdcCurPosAttr %= 25;
					nLinesScanned ++;
				}

			}			


			menuSpeedCodeLength = curAddr - 0x2000;

			// payload
			if ( firstSprites ) 
			{
				firstSprites = 0;
				for ( int i = 0; i < 64 * 64; i++ )
					bitmapPrev[ i ] = ~bitmap[ i ];
			}

			// generate copy speed code to transfer sprite data
			curValA = 0xffff;

			LDX( 0 );
			LDY( 255 );

			curOfs = lastCurOfs; ofsIncr = 1;

			u32 animationStartAddr = curAddr;

			int speedCodeMaxLength = 0x800;
			
			if ( menuSpeedCodeLength > 0 )
			{
				if ( modePALNTSC == 1 || modePALNTSC == 2 )
					speedCodeMaxLength -= max( 0, ( menuSpeedCodeLength - 0x400 ) ); else
					speedCodeMaxLength -= max( 0, ( menuSpeedCodeLength - 0x700 ) );
			}

			nBytesToTransfer = 7 * 8 * 64;

			// transfer animation only if VIC-output is active
			if ( currentVDCMode < 2 )
			while ( nBytesToTransfer > 0 ) 
			{
				nBytesToTransfer --;

				u8 byteA = bitmap[ curOfs ];

				if ( byteA != bitmapPrev[ curOfs ] )
				{
					bitmapPrev[ curOfs ] = byteA;

					if ( byteA == 0 )
					{
						cartMenu[ curAddr ++ ] = 0x8E; // STX
					} else
					if ( byteA == 255 )
					{
						cartMenu[ curAddr ++ ] = 0x8C; // STY
					} else
					{
						if ( byteA != curValA )
						{
							curValA = byteA;
							LDA( byteA );
						} 
						cartMenu[ curAddr ++ ] = 0x8D; // STA
					}
					cartMenu[ curAddr ++ ] = (0x2000+curOfs)&255;
					cartMenu[ curAddr ++ ] = ((0x2000+curOfs)>>8)&255;
				}

				if ( curAddr >= animationStartAddr + speedCodeMaxLength - 10 )
					goto cantCopyEverythingThisTime2;

				curOfs += ofsIncr;
				curOfs %= 56 * 64;
			}

		cantCopyEverythingThisTime2:
			lastCurOfs = curOfs;

			if ( launchKernel )
			{
				// jmp to minimal reset
				//POKE( 0xd011, 0 );
				/*
				0000  2C 11 D0               BIT $D011
				0003  10 FB                  BPL $0000
				0005  2C 11 D0               BIT $D011
				0008  30 FB                  BMI $0005
				*/
				/*cartMenu[ curAddr ++ ] = 0x2c;
				cartMenu[ curAddr ++ ] = 0x11;
				cartMenu[ curAddr ++ ] = 0xd0;
				
				cartMenu[ curAddr ++ ] = 0x10;
				cartMenu[ curAddr ++ ] = 0xfb;
				
				cartMenu[ curAddr ++ ] = 0x2c;
				cartMenu[ curAddr ++ ] = 0x11;
				cartMenu[ curAddr ++ ] = 0xd0;

				cartMenu[ curAddr ++ ] = 0x30;
				cartMenu[ curAddr ++ ] = 0xfb;*/


				JSR( RELOC_BASE + 0x0d00 );
			}

			RTS
			endOfBankAddress = curAddr - 1; 

			sk64Command = 0xfe;

			SyncDataAndInstructionCache();
			warmCache( pFIQ );
			warmCache( pFIQ );

			allSpeedCodeExecuted = false;
			menuUpdateTimeOut = tempTimeOut;
			nextByteAddr = 0x2000;
			nextByte = cartMenu[ nextByteAddr ];
			delayUpdateMenu = 50;
			readyForNMIs = 1;
			updateMenu = 0;
			doneWithHandling = 1;
		}

		//
		// copy font to memory
		//
		if ( updateMenu == 2 )
		{
			u32 tempTimeOut = menuUpdateTimeOut;
			menuUpdateTimeOut = 1024 * 1024 * 1024;

			memcpy( &cartMenu[ 0x3000 ], charset, 4096 );

			// code to copy 64*64 byte from $b000-$bfff to $3000+
			const unsigned char copyCodeCharset[] = 
			{
				0xa2, 0x00, 0xbd, 0x00, 0xb0, 0x9d, 0x00, 0x30, 0xbd, 0x00, 0xb1, 0x9d, 0x00, 0x31, 0xbd, 0x00, 
				0xb2, 0x9d, 0x00, 0x32, 0xbd, 0x00, 0xb3, 0x9d, 0x00, 0x33, 0xbd, 0x00, 0xb4, 0x9d, 0x00, 0x34, 
				0xbd, 0x00, 0xb5, 0x9d, 0x00, 0x35, 0xbd, 0x00, 0xb6, 0x9d, 0x00, 0x36, 0xbd, 0x00, 0xb7, 0x9d, 
				0x00, 0x37, 0xbd, 0x00, 0xb8, 0x9d, 0x00, 0x38, 0xbd, 0x00, 0xb9, 0x9d, 0x00, 0x39, 0xbd, 0x00, 
				0xba, 0x9d, 0x00, 0x3a, 0xbd, 0x00, 0xbb, 0x9d, 0x00, 0x3b, 0xbd, 0x00, 0xbc, 0x9d, 0x00, 0x3c, 
				0xbd, 0x00, 0xbd, 0x9d, 0x00, 0x3d, 0xbd, 0x00, 0xbe, 0x9d, 0x00, 0x3e, 0xbd, 0x00, 0xbf, 0x9d, 
				0x00, 0x3f, 0xca, 0xd0, 0x9d, 0x60
			};

			memcpy( &cartMenu[ 0x2000 ], copyCodeCharset, 102 );
			endOfBankAddress = 0x2000 + 102; 

			sk64Command = 0xfe;

			SyncDataAndInstructionCache();
			warmCache( pFIQ );
			warmCache( pFIQ );

			allSpeedCodeExecuted = false;
			menuUpdateTimeOut = tempTimeOut;
			nextByteAddr = 0x2000;
			nextByte = cartMenu[ nextByteAddr ];
			delayUpdateMenu = 50;
			readyForNMIs = 1;
			updateMenu = 0;
			doneWithHandling = 1;
		}

	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

// approximate(!) current raster line times 63
static u16 currentRasterLine = 0;

static u32 sendCommand = 0;
static u8 c64Command = 0, c64Data = 0;

void CKernelMenu::FIQHandler (void *pParam)
{
	START_AND_READ_ADDR0to7_RW_RESET_CS

	if ( disableCart )
	{
		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING

		if ( BUTTON_PRESSED && c64CycleCount > 1000000 ) 
			activateCart();

		c64CycleCount ++;
		return;
	}

	static int setNMICycles = 0;

	c64CycleCount ++;

	if ( CPU_RESET ) { 
		resetCounter ++; 

		if ( resetCounter > 10 )
		{
			menuUpdateTimeOut = 1024 * 1024 * 1024;
			globalReset ++;
			c64CycleCount = 0;
			nBytesRead = 0;
			updateMenu = 0;
			delayUpdateMenu = 0;
			doneWithHandling = 1;
			firstMenu = firstSprites = 1;
			startAfterReset = 1;
			freezeNMICycles = countWrites = 0;
			cartMenu[ 8192 ] = 0x60;
			goto fancyLEDs;
		}

		goto fancyLEDs;
	} else  
		resetCounter = 0; 

	if ( CPU_WRITES_TO_BUS )
		countWrites ++; else
		countWrites = 0;

	if ( ( updateMenu && !doneWithHandling ) || !doneWithHandling )
		goto fancyLEDs;

	if ( delayUpdateMenu )
		goto checkForNMI;


	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( disableCart && BUTTON_PRESSED )
	{
		FINISH_BUS_HANDLING
		activateCart();
		return;
	}

	if ( SID_ACCESS )
		wireSIDGotLow = 1; else
		wireSIDGotHigh = 1;

	if ( wireKernalDetectMode && KERNAL_ACCESS )
	{
		wireKernalTrackAccess = 1;
		FINISH_BUS_HANDLING
		return;
	}

	if ( disableCart || ( ( g3 & (bROML | bROMH | bIO1 | bIO2 | bBA ) ) == (bROML | bROMH | bIO1 | bIO2 | bBA) ) )
		goto checkForNMI;

	if ( CPU_READS_FROM_BUS && ROML_ACCESS )
	{
		const unsigned char readySignal[ 6 ] = { 0xf0, 0x0f, 0x88, 0x77, 0xaa, 0x55 };
		if ( GET_ADDRESS >= 0x1ff0 )
		{
			if ( GET_ADDRESS == 0x1ff6 )
			{
				WRITE_D0to7_TO_BUS( firstMenuAfterBoot );
				FINISH_BUS_HANDLING
				return;
			}
			if ( GET_ADDRESS <= 0x1ff5 )
			{
				WRITE_D0to7_TO_BUS( readySignal[ GET_ADDRESS - 0x1ff0 ] );
				FINISH_BUS_HANDLING
				return;
			}
			if ( GET_ADDRESS == 0x1ffe )
			{
				sendCommand = 2;
				// we return this value when the C64 wants to send a signal => could return something meaningful here!
				WRITE_D0to7_TO_BUS( 170 );
				FINISH_BUS_HANDLING
				return;
			}
			if ( GET_ADDRESS == 0x1fff )
			{
				WRITE_D0to7_TO_BUS( sk64Command );
				FINISH_BUS_HANDLING
				sk64Command = 0xff;
				return;
			}
		}
		if ( GET_ADDRESS >= 0x1d00 && GET_ADDRESS < 0x1e00 )
		{
			c64Data = GET_ADDRESS & 255; 
			sendCommand = 1;
			//WRITE_D0to7_TO_BUS( 0 );
			WRITE_D0to7_TO_BUS( cartMenu[ GET_ADDRESS ] );
			goto fancyLEDs;
		} else
		if ( GET_ADDRESS >= 0x1e00 && GET_ADDRESS < 0x1f00 )
		{
			sendCommand = 0;
			c64Command = GET_ADDRESS & 255;

			SET_GPIO( bNMI );

			// handle the command here!
			switch ( c64Command )
			{
			case 1: // update screen
				menuUpdateTimeOut = 80000 * 2 + 200000 * currentVDCMode * vdc40ColumnMode;
				lastChar = c64Data;
				delayUpdateMenu = 10000000;
				updateMenu = 1;
				doneWithHandling = 0;
				// put return value here
				WRITE_D0to7_TO_BUS( 128 );
				break;
			case 2: // copy charset
				delayUpdateMenu = 10000000;
				updateMenu = 2;
				doneWithHandling = 0;
				// put return value here
				WRITE_D0to7_TO_BUS( 0 );
				break;
			case 4: // minimum raster lines to wait for NMI
				// c64Data is currentRasterline
				// we want to wait until line 270
				currentRasterLine = c64Data * 63 * 2;
				if ( c64Data > 200 || currentVDCMode > 0 )
					nmiMinCyclesWait = 10; else
					// PAL: nmiMinCyclesWait = (280-(int)c64Data) * 63 * 2;
					// NTSC (and PAL):
					nmiMinCyclesWait = (265-(int)c64Data * 2) * 63 * 2;
				//nmiMinCyclesWait = c64Data * 63;
				WRITE_D0to7_TO_BUS( 0 );
				break;
			case 5: // type of SID at $d400
			case 6: // type of SID at $d420
			case 7: // type of SID at $d500
			case 8: // type of SID at $de00
			case 9: // type of SID at $df00
				typeSIDAddr[ c64Command - 5 ] = c64Data;
				WRITE_D0to7_TO_BUS( 0 );
				break;
			case 10: // enable/disable kernal replacement
				if ( c64Data )
				{
					wireKernalDetectMode = 1;
					wireKernalTrackAccess = 0;
					setLatchFIQ( LATCH_ENABLE_KERNAL );
					OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
					return;
				} else
				{
					if ( wireKernalTrackAccess )
						wireKernalAvailable = 1; else
						wireKernalAvailable = 0; 
					wireKernalDetectMode = 0;
					clrLatchFIQ( LATCH_ENABLE_KERNAL );
					OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
					return;
				}
				break;
			case 11: // C128 detection
				modeC128 = c64Data;
				if ( modeC128 && vdcSupport )
				{
					cartMenu[ 0x1fe1 ] = 0x01;
					//disableFIQ_Falling = 1;
				} else
					vdcSupport = 0;
				updateLogo = 1;
				break;
			case 12: // VIC detection
				modeVIC = c64Data & 15;
				modePALNTSC = c64Data >> 4;
				break;
			case 13: // SIDKick detection
				hasSIDKick = c64Data;
				break;
			case 14: // reset SID detection
				wireSIDAvailable = 0;
				wireSIDGotLow = wireSIDGotHigh = 0;
				break;
			default:
				WRITE_D0to7_TO_BUS( 0 );
				break;
			}
			goto fancyLEDs;
			return;
		}

		WRITE_D0to7_TO_BUS( cartMenu[ GET_ADDRESS ] );
		nBytesRead ++;
		goto fancyLEDs;
	}

	if ( CPU_READS_FROM_BUS && ROMH_ACCESS )
	{
		if ( GET_ADDRESS + 8192 == nextByteAddr )
		{
			WRITE_D0to7_TO_BUS( nextByte );
		} else
		{
			WRITE_D0to7_TO_BUS( cartMenu[ GET_ADDRESS + 8192 ] );
		}

		nextByteAddr = GET_ADDRESS + 8192 + 1;
		nextByte = cartMenu[ nextByteAddr ];

		CACHE_PRELOADL2KEEP( &cartMenu[ ( GET_ADDRESS + 8192 ) & ~63 ] );
		CACHE_PRELOADL2KEEP( &cartMenu[ ( GET_ADDRESS + 8192 + 64 ) & ~63 ] );

		if ( endOfBankAddress == GET_ADDRESS + 8192 )
		{
			allSpeedCodeExecuted = true;
			cartMenu[ 0x2000 ] = 0x60;
			endOfBankAddress = 1 << 24;
		}

		goto fancyLEDs;
	}

checkForNMI:

	if ( !disableCart )
	{
/*		if ( nmiMinCyclesWait > 0 )
		{
			nmiMinCyclesWait --;
		} */

		if ( currentVDCMode == 1 )
		{
			u32 t = currentRasterLine % 19656;
			if ( ( t > 63 * 53 && t < 63 * 70 && readyForNMIs && delayUpdateMenu > 0 ) ||
				 ( currentRasterLine > 200000 && readyForNMIs && delayUpdateMenu > 0 ) )
			{
				delayUpdateMenu = 0;
			}
		} else
		if ( nmiMinCyclesWait == 0 && readyForNMIs && delayUpdateMenu > 0 )
		{
			delayUpdateMenu --;
		}

		if ( delayUpdateMenu == 0 && readyForNMIs )
		{
			readyForNMIs = 0;
			freezeNMICycles = 100;
			CLR_GPIO( bNMI | bCTRL257 ); 
			goto fancyLEDs;
		}

		if ( freezeNMICycles /*&& countWrites == 3*/ )
		{
			setNMICycles = 10;
			freezeNMICycles = 0;
			delayUpdateMenu = 0;
			goto fancyLEDs;
		} 

		if ( menuUpdateTimeOut < 1024 * 1024 * 1024 )
		{
			if ( menuUpdateTimeOut > 0 )
				menuUpdateTimeOut --; else
				menuTimeOutReset = 1;
		}

		if ( setNMICycles && --setNMICycles == 0 )
		{
			SET_GPIO( bNMI );
		}
	}

fancyLEDs:
	if ( !disableCart && nmiMinCyclesWait > 0 )
	{
		nmiMinCyclesWait --;
	} 
	currentRasterLine ++;

	if ( screenType == 1 )
	{
		static int cnt = 0;
		cnt ++;
		cnt &= 255;
		if ( cnt < ledActivityBrightness )
			latchSetClear( LATCH_LED0, 0 ); else
			latchSetClear( 0, LATCH_LED0 );
		if ( cnt < 255-ledActivityBrightness )
			latchSetClear( LATCH_LED1, 0 ); else
			latchSetClear( 0, LATCH_LED1 );
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

void mainMenu()
{
	CKernelMenu kernel;
	if ( kernel.Initialize() )
		kernel.Run();
	prepareOutputLatch();
	outputLatch();
}

int main( void )
{

	CKernelMenu kernel;
	kernel.Initialize();

	extern void KernelKernalRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME );
	extern void KernelGeoRAMRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu );
	extern void KernelLaunchRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0, u8 noInitStartup = 0 );
	
	extern void KernelEFRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr, const char *FILENAME_KERNAL = NULL );
	extern void KernelFC3Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL, const char *FILENAME_KERNAL = NULL );
	extern void KernelFMRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL, const char *FILENAME_KERNAL = NULL );
	extern void KernelWSRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL, const char *FILENAME_KERNAL = NULL );
	extern void KernelKCSRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL, const char *FILENAME_KERNAL = NULL );
	extern void KernelSS5Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL, const char *FILENAME_KERNAL = NULL );
	extern void KernelAR6Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL, const char *FILENAME_KERNAL = NULL );
	//extern void KernelRRRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelSIDRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0 );
	extern void KernelSIDRun8( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 );
	extern void KernelRKLRun( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME_KERNAL, const char *FILENAME, const char *FILENAME_RAM, u32 sizeRAM, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 );
	extern void KernelCartRun128( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr );
	extern void KernelMODplayRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0, bool playHDMIinParallel = false, u32 musicPlayer = 0 );

	extern u32 octaSIDMode;

	subGeoRAM		= 0;
	subSID			= 0;


	while ( true )
	{
		latchSetClearImm( LATCH_LED1, 0 );

		kernel.Run();

		latchSetClearImm( LATCH_LED0, LATCH_RESET | LATCH_ENABLE_KERNAL );
		SET_GPIO( bNMI | bDMA ); 

		BEGIN_CYCLE_COUNTER

		char geoRAMFile[ 2048 ];
		u32 geoRAMSize;
		
		if ( modeC128 )
		{
			strcpy( FILENAME_LOGO_RGBA, FILENAME_LOGO_RGBA128 );			
		}

		u32 loadC128PRG = 0;
		u32 playingPSID = 0;

		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();

/*		bool recreateHDMISound = false;

		if ( subSID || octaSIDMode || launchKernel == 42 || launchKernel == 43 || launchKernel == 8 )
		{
		} else
		{
			recreateHDMISound = true;
			hdmiSoundDevice->Cancel();
			delete hdmiSoundDevice;
			hdmiSoundDevice = NULL;
		}*/

		/* for debugging purposes only*/
		/*if ( launchKernel == 255 ) 
		{
			reboot (); 	
		} else*/

		switch ( launchKernel )
		{
		case 2:
			KernelKernalRun( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 3:
			settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
			if ( subHasKernal == -1 )
				KernelRKLRun( kernel.m_InputPin, &kernel, NULL, NULL, geoRAMFile, geoRAMSize ); else
				KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, NULL, geoRAMFile, geoRAMSize );
			break;
		case 4:
			if ( strstr( FILENAME, "PRG128" ) )
				loadC128PRG = 1;

			if ( subSID ) {
				applySIDSettings();
				if ( octaSIDMode )
				{
					//hdmiSoundDevice->Cancel(); delete hdmiSoundDevice; hdmiSoundDevice = NULL; recreateHDMISound = true;
					KernelSIDRun8( kernel.m_InputPin, &kernel, FILENAME, false, NULL, 0, loadC128PRG ); 
				} else
					KernelSIDRun( kernel.m_InputPin, &kernel, FILENAME, false, NULL, 0, loadC128PRG ); 
				break;
			}
			if ( subGeoRAM ) {
				settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
				if ( subHasKernal == -1 ) {
					KernelRKLRun( kernel.m_InputPin, &kernel, NULL, FILENAME, geoRAMFile, geoRAMSize, false, NULL, 0, loadC128PRG ); 
					break;
				} else {
					KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, FILENAME, geoRAMFile, geoRAMSize, false, NULL, 0, loadC128PRG ); 
					break;
				}
			} else {
				if ( subHasKernal == -1 ) {
					KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, false, NULL, 0, loadC128PRG );
				} else {
					KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, FILENAME, NULL, 0, false, NULL, 0, loadC128PRG ); 
				}
				break;
			}
			break;
		case 41:
			playingPSID = 1; // intentionally no break
		case 40: // launch something from a disk image or PRG in memory (e.g. a converted .SID-file)
			//logger->Write( "RaspiMenu", LogNotice, "filename from d64: %s", FILENAME );
			if ( subSID ) {
				applySIDSettings();
				if ( octaSIDMode )
				{
					//hdmiSoundDevice->Cancel(); delete hdmiSoundDevice; hdmiSoundDevice = NULL; recreateHDMISound = true;
					KernelSIDRun8( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch, startForC128 ); 
				} else
					KernelSIDRun( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch, startForC128, playingPSID );
				break;
			}
			if ( subGeoRAM ) {
				settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
				if ( subHasKernal == -1 ) {
					KernelRKLRun( kernel.m_InputPin, &kernel, NULL, FILENAME, geoRAMFile, geoRAMSize, true, prgDataLaunch, prgSizeLaunch, startForC128 );
					break;
				} else {
					KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, FILENAME, geoRAMFile, geoRAMSize, true, prgDataLaunch, prgSizeLaunch, startForC128 );
					break;
				}
			} else {
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch, startForC128, playingPSID );
			}
			break;
		case 42:
		case 43:
		case 44:
		case 45:
			KernelMODplayRun( kernel.m_InputPin, &kernel, FILENAME, false, prgDataLaunch, prgSizeLaunch, startForC128, playingPSID, launchKernel == 43 || launchKernel == 45, launchKernel > 43 ? 1 : 0 );
			CleanDataCache();
			InvalidateDataCache();
			InvalidateInstructionCache();
			break;
		case 5:
			if ( subHasKernal == -1 )
				KernelEFRun( kernel.m_InputPin, &kernel, FILENAME, menuItemStr ); else
				KernelEFRun( kernel.m_InputPin, &kernel, FILENAME, menuItemStr, filenameKernal ); 
			break;
		case 6:
			if ( subHasKernal == -1 )
				KernelFC3Run( kernel.m_InputPin, &kernel ); else
				KernelFC3Run( kernel.m_InputPin, &kernel, filenameKernal );
			break;
		case 60:
			if ( subHasKernal == -1 )
				KernelFC3Run( kernel.m_InputPin, &kernel, FILENAME ); else
				KernelFC3Run( kernel.m_InputPin, &kernel, FILENAME, filenameKernal );
			break;
		case 61:
			if ( subHasKernal == -1 )
				KernelKCSRun( kernel.m_InputPin, &kernel, FILENAME ); else
				KernelKCSRun( kernel.m_InputPin, &kernel, FILENAME, filenameKernal );
			break;
		case 62:
			if ( subHasKernal == -1 )
				KernelSS5Run( kernel.m_InputPin, &kernel, FILENAME ); else
				KernelSS5Run( kernel.m_InputPin, &kernel, FILENAME, filenameKernal );
			break;
		case 63:
			if ( subHasKernal == -1 )
				KernelFMRun( kernel.m_InputPin, &kernel, FILENAME ); else
				KernelFMRun( kernel.m_InputPin, &kernel, FILENAME, filenameKernal );
			break;
		case 64:
			if ( subHasKernal == -1 )
				KernelWSRun( kernel.m_InputPin, &kernel, FILENAME ); else
				KernelWSRun( kernel.m_InputPin, &kernel, FILENAME, filenameKernal );
			break;
		case 7:
			if ( subHasKernal == -1 )
				KernelAR6Run( kernel.m_InputPin, &kernel ); else
				KernelAR6Run( kernel.m_InputPin, &kernel, filenameKernal );
			break;
		case 70:
			if ( subHasKernal == -1 )
				KernelAR6Run( kernel.m_InputPin, &kernel, FILENAME ); else
				KernelAR6Run( kernel.m_InputPin, &kernel, FILENAME, filenameKernal );
			break;
		/*case 71:
			KernelRRRun( kernel.m_InputPin, &kernel, FILENAME );
			break;*/
		case 8:
			applySIDSettings();
			if ( octaSIDMode )
			{
				//hdmiSoundDevice->Cancel(); delete hdmiSoundDevice; hdmiSoundDevice = NULL; recreateHDMISound = true;
				KernelSIDRun8( kernel.m_InputPin, &kernel, NULL ); 
			} else
				KernelSIDRun( kernel.m_InputPin, &kernel, NULL );
			break;
		case 9:
			KernelCartRun128( kernel.m_InputPin, &kernel, FILENAME, menuItemStr );
			break;
		case 10:
			KernelRKLRun( kernel.m_InputPin, &kernel, NULL, "SD:C64/georam_launch.prg", FILENAME, 4096, false, NULL, 0, 0 ); 
			break;
		case 11: // launch SIDKick Config
			playingPSID = 1;
			if ( hasSIDKick == 1 )
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME_SIDKICK_CONFIG_1, false, NULL, 0, 0, playingPSID ); else
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME_SIDKICK_CONFIG_2, false, NULL, 0, 0, playingPSID ); 
			break;
		default:
			break;
		}
		
		/*if ( recreateHDMISound && hdmiSoundAvailable )
		{
			hdmiSoundDevice = new CHDMISoundBaseDevice( 48000 );
			if ( hdmiSoundDevice )
			{
				hdmiSoundDevice->SetWriteFormat( SoundFormatSigned24, 2 );
				hdmiSoundDevice->Start();
			}
		}*/

	}

	return EXIT_HALT;
}
