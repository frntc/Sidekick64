/*
  _________.__    .___      __   .__        __            _____                       
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __       /     \   ____   ____  __ __ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      /  \ /  \_/ __ \ /    \|  |  \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /    Y    \  ___/|   |  \  |  /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \____|__  /\___  >___|  /____/ 
        \/         \/    \/     \/       \/     \/             \/     \/     \/       
 
 kernel_menu.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Menu: ugly glue code to expose some functionality in one menu with browser
 Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

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

// we will read these files
static const char DRIVE[] = "SD:";
static const char FILENAME_PRG[] = "SD:C64/rpimenu.prg";		// .PRG to start
static const char FILENAME_CBM80[] = "SD:C64/launch.cbm80";		// launch code (CBM80 8k cart)
static const char FILENAME_CONFIG[] = "SD:C64/sidekick64.cfg";		
static const char FILENAME_SIDKICK_CONFIG[] = "SD:C64/SIDKick_CFG.prg";		


static const char FILENAME_SPLASH_HDMI[] = "SD:SPLASH/Sidekick-Logo.raw";		
static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_main.tga";		
static const char FILENAME_SPLASH_RGB128[] = "SD:SPLASH/sk128_main.tga";		
static const char FILENAME_TFT_FONT[] = "SD:SPLASH/PXLfont88665b-RF2.3-C64sys.bin";		

static char FILENAME_LOGO_RGBA128[] = "SD:SPLASH/sk128_logo_blend.tga";
char FILENAME_LOGO_RGBA[128] = "SD:SPLASH/sk64_logo_blend.tga";

// setting EXROM and GAME (low = 0, high = 1)
#define EXROM_ACTIVE	0
#define GAME_ACTIVE		1

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

static u32	configGAMEEXROMSet, 
			configGAMEEXROMClr;
static u32	disableCart     = 0;
static u32	resetCounter    = 0;
static u32	transferStarted = 0;
static u32	currentOfs      = 0;
static u32  transferPart    = 0;

// the menu .PRG and program to launch
static u32 prgSize AAA;
static unsigned char prgData[ 65536 ] AAA;

u32 prgSizeLaunch AAA;
unsigned char prgDataLaunch[ 65536 ] AAA;

// CBM80 to launch the menu
static unsigned char cart_pool[ 16384 + 128 ] AAA;
static unsigned char *cartCBM80 AA;

// this memory can be called in the IO-areas
static unsigned char injectCode[ 256 ] AAA;

// custom charset
unsigned char charset[ 4096 ] AAA;

u32 releaseDMA = 0;
u32 doneWithHandling = 0;
u32 c64CycleCount = 0;
u32 nBytesRead = 0;
u32 first = 1;

#define MAX_FILENAME_LENGTH	512
char FILENAME[ MAX_FILENAME_LENGTH * 2 ];
char menuItemStr[ 512 ];

u32 wireSIDAvailable = 0;
static u32 wireSIDGotHigh = 0;
static u32 wireSIDGotLow = 0;

u32 wireKernalAvailable = 0;
static u32 wireKernalDetectMode = 0;
static u32 wireKernalTrackAccess = 0;


static u32 launchKernel = 0;
static u32 lastChar = 0;
static u32 startForC128 = 0;

static u32 screenTransferBytes;
static u8 *screenTransfer = &c64screen[ 0 ];
static u32 colorTransferBytes;
static u8 *colorTransfer = &c64screen[ 0 ];
static u8 *charsetTransfer = &charset[ 0 ];

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
	disableCart = 1;
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
	
	DELAY( 1 << 17 );
	latchSetClearImm( LATCH_RESET | LED_DEACTIVATE_CART2_HIGH, LED_DEACTIVATE_CART2_LOW | LATCH_ENABLE_KERNAL );
}


// cache warmup

volatile u8 forceRead;

static void *pFIQ = NULL;

__attribute__( ( always_inline ) ) inline void warmCache( void *fiqh )
{
	CACHE_PRELOAD_DATA_CACHE( c64screen, 1024, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( c64color, 1024, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( cartCBM80, 512, CACHE_PRELOADL1KEEP );
	CACHE_PRELOAD_DATA_CACHE( prgData, 65536, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( injectCode, 256, CACHE_PRELOADL2KEEP );

	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)fiqh, 2048*2 );

	FORCE_READ_LINEAR32a( cartCBM80, 512, 512 * 16 );

	FORCE_READ_LINEARa( prgData, prgSize, 65536 * 4 )
	if ( fiqh )
	{
		FORCE_READ_LINEARa( (void*)fiqh, 1024*3, 65536 );
	}
}


void activateCart()
{
	initScreenAndLEDCodes();
	disableCart = 0;
	transferStarted = 0;

	latchSetClearImm( LED_ACTIVATE_CART1_HIGH, LED_ACTIVATE_CART1_LOW | LATCH_RESET | LATCH_ENABLE_KERNAL );
	SETCLR_GPIO( configGAMEEXROMSet | bNMI | bDMA, configGAMEEXROMClr | bCTRL257 );
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
	CleanDataCache();
	InvalidateDataCache();
	InvalidateInstructionCache();
	warmCache( pFIQ );
	DELAY(1<<18);
	warmCache( pFIQ );
	DELAY(1<<18);

	latchSetClearImm( LATCH_RESET | LED_ACTIVATE_CART2_HIGH, LED_ACTIVATE_CART2_LOW | LATCH_ENABLE_KERNAL );
}

char filenameKernal[ 2048 ];


CLogger			*logger;
CScreenDevice	*screen;

#ifdef COMPILE_MENU_WITH_SOUND
CTimer				*pTimer;
CScheduler			*pScheduler;
CInterruptSystem	*pInterrupt;
CVCHIQDevice		*pVCHIQ;
#endif

//u32 temperature;

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

		bOK = m_Logger->Initialize( pTarget );
		logger = m_Logger;
	}
#endif

	if ( bOK ) bOK = m_Interrupt.Initialize();
	if ( bOK ) bOK = m_Timer.Initialize();
	m_EMMC.Initialize();

#ifdef COMPILE_MENU_WITH_SOUND
	pTimer = &m_Timer;
	pScheduler = &m_Scheduler;
	pInterrupt = &m_Interrupt;

	if ( bOK ) bOK = m_VCHIQ.Initialize();
	pVCHIQ = &m_VCHIQ;
#endif

	// initialize ARM cycle counters (for accurate timing)
	initCycleCounter();

	// initialize GPIOs
	gpioInit();

	// initialize latch and software I2C buffer
	initLatch();

	initScreenAndLEDCodes();
	latchSetClearImm( LED_INIT1_HIGH, LED_INIT1_LOW );

	u32 size = 0;

	u8 tempHDMI[ 640 * 480 * 3 ];
	readFile( logger, (char*)DRIVE, (char*)FILENAME_SPLASH_HDMI, tempHDMI, &size );
	u32 xOfs = ( screen->GetWidth() - 640 ) / 2;
	u32 yOfs = ( screen->GetHeight() - 480 ) / 2;
	u8 *p = tempHDMI;
	for ( u32 j = 0; j < 480; j++ )
		for ( u32 i = 0; i < 640; i++ )
		{
			#define _CONV_RGB(red, green, blue)	  (((red>>3) & 0x1F) << 11 \
								| ((green>>3) & 0x1F) << 6 \
								| ((blue>>3) & 0x1F))
			
			screen->SetPixel( i+xOfs, j+yOfs,  _CONV_RGB( p[ 0 ], p[ 1 ], p[ 2 ] )  ); 
			p += 3;
		}
	// read launch code
	cartCBM80 = (unsigned char *)( ((u64)&cart_pool+64) & ~63 );
	readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, cartCBM80, &size );

	latchSetClearImm( LED_INIT2_HIGH, LED_INIT2_LOW );

	// read .PRG
	readFile( logger, (char*)DRIVE, (char*)FILENAME_PRG, prgData, &prgSize );

	latchSetClearImm( LED_INIT3_HIGH, LED_INIT3_LOW );

	extern void scanDirectories( char *DRIVE );
	scanDirectories( (char *)DRIVE );

	latchSetClearImm( LED_INIT4_HIGH, LED_INIT4_LOW );

	if ( !readConfig( logger, (char*)DRIVE, (char*)FILENAME_CONFIG ) )
	{
		latchSetClearImm( LED_INITERR_HIGH, LED_INITERR_LOW );
		logger->Write( "RaspiMenu", LogPanic, "error reading .cfg" );
	}

	u32 t;
	if ( skinFontFilename[0] != 0 && readFile( logger, (char*)DRIVE, (char*)skinFontFilename, charset, &t ) )
	{
		skinFontLoaded = 1;
		//memcpy( 1024 + charset+8*(91), skcharlogo_raw, 224 ); <- this is the upper case font used in the browser, skip logo to keep all PETSCII characters
		memcpy( 2048 + charset+8*(91), skcharlogo_raw, 224 );
		//memcpy( 0 + charset+8*(91), skcharlogo_raw, 224 );
		//writeFile( logger, "SD:", "font.temp", &charset[2048], 2048 );
	} 

	readSettingsFile();
	applySIDSettings();
	renderC64();
	startInjectCode();
	disableCart = 0;

	latchSetClearImm( LED_INIT5_HIGH, LED_INIT5_LOW );

	return bOK;
}


// a hacky way of defining code that can be executed from the C64 (currently only POKE)
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


u32 updateLogo = 0;
unsigned char tftC128Logo[ 240 * 240 * 2 ];
extern unsigned char tftBackground[ 240 * 240 * 2 ];


void CKernelMenu::Run( void )
{
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

	wireSIDAvailable = 0;
	wireSIDGotLow    = 0;
	wireSIDGotHigh   = 0;

	wireKernalAvailable   = 0;
	wireKernalDetectMode  = 0;
	wireKernalTrackAccess = 0;

	if ( !disableCart )
	{
		// set GAME and EXROM as defined above (+ set NMI, DMA and latch outputs)
		configGAMEEXROMSet = ( (GAME_ACTIVE == 0)  ? 0 : bGAME ) | ( (EXROM_ACTIVE == 0) ? 0 : bEXROM ) | bNMI | bDMA;
		configGAMEEXROMClr = ( (GAME_ACTIVE == 0)  ? bGAME : 0 ) | ( (EXROM_ACTIVE == 0) ? bEXROM : 0 ); 
		SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );

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
		//if ( modeC128 )
			//tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB128, 8 ); else
			//tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 ); 
		tftCopyBackground2Framebuffer();
		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}

	if ( !disableCart )
	{
		// setup FIQ
		DisableIRQs();
		m_InputPin.ConnectInterrupt( this->FIQHandler, this );
		m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

		launchKernel = 0;

		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();

		pFIQ = (void*)this->FIQHandler;
		warmCache( pFIQ );
		DELAY(1<<18);
		warmCache( pFIQ );
		DELAY(1<<18);

		// start c64 
		SET_GPIO( bNMI | bDMA );
		latchSetClearImm( LATCH_RESET, 0 );
	}

	// wait forever
	while ( true )
	{
		if ( first && nBytesRead < 32 && c64CycleCount > 1000000 )
		{
			first = 0;
			latchSetClearImm( LATCH_LED0, LATCH_RESET );
			DELAY(1<<27);
			latchSetClearImm( LATCH_RESET | LATCH_LED0, 0 );
		}

		asm volatile ("wfi");

		if ( launchKernel )
		{
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			EnableIRQs();
			return;
		}

		static u32 refresh = 0;
		if ( screenType == 0 )
		{
			u32 v = 1 << ( ( c64CycleCount >> 18 ) % 6 );
			u32 l_on = ( v + ( ( v & 16 ) >> 2 ) + ( ( v & 32 ) >> 4 ) ) & 15;
			u32 l_off = ( ~l_on ) & 15;
			latchSetClear( l_on * LATCH_LED0, l_off * LATCH_LED0 );
		} else
		if ( screenType == 1 )
		{
			u32 v = ( c64CycleCount >> 18 ) & 1;
			u32 l_on  = v ? LATCH_LED0 : LATCH_LED1;
			u32 l_off = v ? LATCH_LED1 : LATCH_LED0;
			latchSetClear( l_on, l_off );
		}

		if ( updateMenu == 1 )
		{
			wireSIDAvailable = 0;
			if ( wireSIDGotLow && wireSIDGotHigh )
				wireSIDAvailable = 1;
			wireSIDGotLow = wireSIDGotHigh = 0;

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
			lastChar = 0xfffffff;
			refresh++;
			//temperature = m_CPUThrottle.GetTemperature();
			renderC64();
//		DELAY(1<<28);
			doneWithHandling = 1;
			updateMenu = 0;
		}
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

void CKernelMenu::FIQHandler (void *pParam)
{
	register u32 D;

	if ( updateMenu && !doneWithHandling )
		return;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	if ( CPU_RESET ) { 
		resetCounter ++; 
		FINISH_BUS_HANDLING
		//activateCart();
		return;
	} else  
		resetCounter = 0; 

	c64CycleCount ++;
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( BUTTON_PRESSED )
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

	if ( disableCart || ( ( g3 & (bROML | bIO1 | bIO2 | bBA ) ) == (bROML | bIO1 | bIO2 | bBA) ) )
	{
		FINISH_BUS_HANDLING
		return;
	}

	if ( CPU_READS_FROM_BUS && ACCESS( ROM_LH ) )
	{
		WRITE_D0to7_TO_BUS( cartCBM80[ GET_ADDRESS ] );
		nBytesRead ++;
		FINISH_BUS_HANDLING
		return;
	}

	// read byte of program to be dropped
	if ( CPU_READS_FROM_BUS && IO1_ACCESS ) 
	{
		u32 A = GET_IO12_ADDRESS;
			
		if ( A == 1 )	
		{
			// $DE01 -> get number of 256-byte pages
			if ( transferPart == 1 ) // PRG part above $a000
				D = 0; else
				D = ( prgSize + 255 ) >> 8;
			WRITE_D0to7_TO_BUS( D )
			CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
			FINISH_BUS_HANDLING
			forceRead = prgData[ currentOfs ];
		} else
		{
			// $DE00 -> get next byte
			//D = prgData[ currentOfs++ ];
			//currentOfs %= ((prgSize+255)>>8)<<8;
			D = forceRead;	currentOfs ++;
			WRITE_D0to7_TO_BUS( D )
			CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
			FINISH_BUS_HANDLING
			forceRead = prgData[ currentOfs ];
		}

		return;
	}

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS ) // write to IO1
	{
		u32 A = GET_IO12_ADDRESS;
		if ( A == 2 )
			transferPart = 1; else
			transferPart = 0;
		currentOfs = 0;
		transferStarted = 1;
		forceRead = prgData[ currentOfs ]; // todo: necessary?
		FINISH_BUS_HANDLING
		return;
	} 

	if ( CPU_WRITES_TO_BUS && IO2_ACCESS ) // write to IO2
	{
		u32 A = GET_IO12_ADDRESS;

		READ_D0to7_FROM_BUS( D )

		if ( A == 0 && D == 1 )
		{
			// start screen transfer
			screenTransfer = &c64screen[ 0 ];	screenTransferBytes = 0;
			colorTransfer = &c64color[ 0 ];		colorTransferBytes = 0;
			CACHE_PRELOADL2STRM( screenTransfer );
			CACHE_PRELOADL2STRM( colorTransfer );
		} else
		if ( A == 1 )
		{
			// keypress
			lastChar = D;
			updateMenu = 1;
		} else
		if ( A == 2 ) // C128 detection
		{
			modeC128 = D;
			updateMenu = 1;
			updateLogo = 1;
		} else
		if ( A == 3 ) // VIC detection
		{
			modeVIC = D;
			updateMenu = 1;
		} else
		if ( A == 5 ) // SIDKick detection
		{
			hasSIDKick = D;
			updateMenu = 1;
		} else
		if ( A == 4 )
		{
			charsetTransfer = &charset[ 0 ];
			CACHE_PRELOADL2STRM( charsetTransfer );
		} else
		if ( A == 16 )
		{
			wireKernalDetectMode = 1;
			wireKernalTrackAccess = 0;
			wireKernalAvailable = 0;
			setLatchFIQ( LATCH_ENABLE_KERNAL );
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		} else
		if ( A == 17 )
		{
			if ( wireKernalTrackAccess )
				wireKernalAvailable = 1;
			wireKernalDetectMode = 0;
			clrLatchFIQ( LATCH_ENABLE_KERNAL );
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		}

		if ( updateMenu )
		{
			doneWithHandling = 0;

			WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
			CLR_GPIO( bDMA ); 
			releaseDMA = NUM_DMA_CYCLES;
		}

		FINISH_BUS_HANDLING
		return;
	}

	if ( CPU_READS_FROM_BUS && IO2_ACCESS ) // read from IO2
	{
		u32 A = GET_IO12_ADDRESS;
		D = 0;

		if ( A == 0 )
			D = *( screenTransfer++ ); else
		if ( A == 1 )
		{
			D = ( *( colorTransfer + 1 ) << 4 ) | *( colorTransfer );
			colorTransfer += 2;
		} else
		if ( A == 4 )
		{
			D = *( charsetTransfer ++ );
		} else
			D = injectCode[ A ];

		WRITE_D0to7_TO_BUS( D )

		CACHE_PRELOADL2STRM( screenTransfer );
		CACHE_PRELOADL2STRM( colorTransfer );
		CACHE_PRELOADL2STRM( charsetTransfer );

		FINISH_BUS_HANDLING
		return;
	}

	if ( doneWithHandling && releaseDMA > 0 && --releaseDMA == 0 )
	{
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

void mainMenu()
{
	CKernelMenu kernel;
	if ( kernel.Initialize() )
		kernel.Run();
	//setLatchFIQ( LATCH_LEDO );
	prepareOutputLatch();
	outputLatch();
}

int main( void )
{
	CKernelMenu kernel;
	kernel.Initialize();

	extern void KernelKernalRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME );
	extern void KernelGeoRAMRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu );
	extern void KernelLaunchRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0 );
	extern void KernelEFRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr );
	extern void KernelFC3Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelKCSRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelSS5Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelAR6Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	//extern void KernelRRRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelSIDRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0 );
	extern void KernelSIDRun8( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 );
	extern void KernelRKLRun( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME_KERNAL, const char *FILENAME, const char *FILENAME_RAM, u32 sizeRAM, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 );
	extern void KernelCartRun128( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, const char *menuItemStr );


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
		WAIT_UP_TO_CYCLE( 5000*1000 )


		char geoRAMFile[ 2048 ];
		u32 geoRAMSize;

		if ( modeC128 )
		{
			strcpy( FILENAME_LOGO_RGBA, FILENAME_LOGO_RGBA128 );			
		}

		u32 loadC128PRG = 0;
		u32 playingPSID = 0;

		/* for debugging purposes only*/
		if ( launchKernel == 60 ) //fc3
		{
			reboot (); 	
		} else

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
					KernelSIDRun8( kernel.m_InputPin, &kernel, FILENAME, false, NULL, 0, loadC128PRG ); else
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
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, false, NULL, 0, loadC128PRG );
			}
			break;
		case 41:
			playingPSID = 1; // intentionally no break
		case 40: // launch something from a disk image or PRG in memory (e.g. a converted .SID-file)
			//logger->Write( "RaspiMenu", LogNotice, "filename from d64: %s", FILENAME );
			if ( subSID ) {
				applySIDSettings();
				if ( octaSIDMode )
					KernelSIDRun8( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch, startForC128 ); else
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
		case 5:
			KernelEFRun( kernel.m_InputPin, &kernel, FILENAME, menuItemStr );
			break;
		case 6:
			KernelFC3Run( kernel.m_InputPin, &kernel );
			break;
		case 60:
			KernelFC3Run( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 61:
			KernelKCSRun( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 62:
			KernelSS5Run( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 7:
			KernelAR6Run( kernel.m_InputPin, &kernel );
			break;
		case 70:
			KernelAR6Run( kernel.m_InputPin, &kernel, FILENAME );
			break;
		/*case 71:
			KernelRRRun( kernel.m_InputPin, &kernel, FILENAME );
			break;*/
		case 8:
			applySIDSettings();
			if ( octaSIDMode )
				KernelSIDRun8( kernel.m_InputPin, &kernel, NULL ); else
				KernelSIDRun( kernel.m_InputPin, &kernel, NULL );
			break;
		case 9:
			KernelCartRun128( kernel.m_InputPin, &kernel, FILENAME, menuItemStr );
			break;
		case 10:
			logger->Write( "RaspiMenu", LogNotice, "filename georam: %s", FILENAME );

			KernelRKLRun( kernel.m_InputPin, &kernel, NULL, "SD:C64/georam_launch.prg", FILENAME, 4096, false, NULL, 0, 0 ); 
			break;
		case 11: // launch SIDKick Config
			playingPSID = 1;
			KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME_SIDKICK_CONFIG, false, NULL, 0, 0, playingPSID );
			break;
		default:
			break;
		}
	}

	return EXIT_HALT;
}
