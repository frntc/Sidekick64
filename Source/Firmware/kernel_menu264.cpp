/*
  _________.__    .___      __   .__        __            _____                       
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __       /     \   ____   ____  __ __ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      /  \ /  \_/ __ \ /    \|  |  \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /    Y    \  ___/|   |  \  |  /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \____|__  /\___  >___|  /____/ 
        \/         \/    \/     \/       \/     \/             \/     \/     \/       
 
 kernel_menu264.cpp

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

#include "kernel_menu264.h"
#include "dirscan.h"
#include "config.h"
#include "264screen.h"
#include "charlogo.h"

// we will read these files
static const char DRIVE[] = "SD:";
static const char FILENAME_PRG[] = "SD:C16/rpimenu16.prg";		// .PRG to start
static const char FILENAME_CBM80[] = "SD:C16/launch16.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CONFIG[] = "SD:C16/sidekick264.cfg";		
static const char FILENAME_NEORAM[] = "SD:C16/launchNeoRAM.prg";

static const char FILENAME_SPLASH_HDMI[]   = "SD:SPLASH/Sidekick-Logo.raw";		
static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk264_main.tga";		
static const char FILENAME_TFT_FONT[] = "SD:SPLASH/PXLfont88665b-RF2.3-C64sys.bin";		

char FILENAME_LOGO_RGBA[128] = "SD:SPLASH/sk264_logo_blend.tga";

static u32	disableCart     = 0;
static u32	resetCounter    = 0;
static u32	transferStarted = 0;
static u32	currentOfs      = 0;

// the menu .PRG and program to launch
static u32 prgSize AAA;
static unsigned char prgData[ 65536 ] AAA;

u32 prgSizeLaunch AAA;
unsigned char prgDataLaunch[ 65536 ] AAA;

u32 prgCurSize = 0;
unsigned char *prgCurLaunch;

// CBM80 to launch the menu
static unsigned char cart_pool[ 32768 + 128 ] AAA;
static unsigned char *cartL1 AA;
static unsigned char cart_c16[ 32768 ] AAA;
static unsigned char cartCBM80[ 16384 + 128 ] AA;

// custom charset
static unsigned char charset[ 4096 ] AAA;

u32 releaseDMA = 0;
u32 doneWithHandling = 0;
u32 pullIRQ = 0;
u32 c64CycleCount = 0;
u32 nBytesRead = 0;
u32 first = 1;

u32 cartMode = 0; // 0 == launcher, 1 == c1lo cartridge from SD-card, 2 == C1lo+hi cartridge

// this FILENAME may contain 2 strings at ofs 0 and 2048
#define MAX_FILENAME_LENGTH	2048
char FILENAME[ MAX_FILENAME_LENGTH * 2 ];

static u32 launchKernel = 0;
u32 lastChar = 0;

static u8 *curTransfer = &c64screen[ 0 ];

char filenameKernal[ 2048 ];

CLogger			*logger;
CScreenDevice	*screen;

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


u32 doActivateCart = 0;

void deactivateCart()
{
	doActivateCart = 0;
	disableCart = 1;
	//latchSetClearImm( LED_DEACTIVATE_CART1_HIGH, LED_DEACTIVATE_CART1_LOW | LATCH_RESET | LATCH_ENABLE_KERNAL );
	latchSetClearImm( 0, LATCH_RESET | LATCH_ENABLE_KERNAL );
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
		flushI2CBuffer( true );
		tftBlendRGBA( 0, 0, 0, 128, tftFrameBuffer, 8 );
		tftSendFramebuffer16BitImm( tftFrameBuffer );
		//flush4BitBuffer( true );
		//DELAY( 1 << 17 );
	} 

	//latchSetClearImm( LATCH_RESET | LED_DEACTIVATE_CART2_HIGH, LED_DEACTIVATE_CART2_LOW | LATCH_ENABLE_KERNAL );
	latchSetClearImm( LATCH_RESET, LATCH_ENABLE_KERNAL );

}

void activateCart()
{
	disableCart = 0;
	transferStarted = 0;
	doActivateCart = 0;

	if ( cartMode > 0 )
	{
		cartMode = 0;
		memset( &cartL1[8192], 0, 32768-8192 );
		memcpy( cartL1, cartCBM80, 8192 );
	}

	latchSetClearImm( 0, LATCH_RESET | LATCH_ENABLE_KERNAL );
//	DELAY( 1 << 20 );

	if ( screenType == 0 )
	{
		oledSetContrast( 255 );
		flushI2CBuffer();
	} else
	if ( screenType == 1 )
	{
		tftCopyBackground2Framebuffer();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
		flush4BitBuffer( true );
//		DELAY( 1 << 17 );
	}

	latchSetClearImm( LATCH_RESET, LATCH_ENABLE_KERNAL );
}

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
CVCHIQDevice		*pVCHIQ;
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

	initGlobalMemPool( 128 * 1024 * 1024 );

	u32 size = 0;

#if 1
	u8 tempHDMI[ 640 * 480 * 3 ];
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
	readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, cartCBM80, &size );

	cartL1 = (unsigned char *)( ((u64)&cart_pool+64) & ~63 );

	cartMode = 0;
	memcpy( cartL1, cartCBM80, 8192 );

	latchSetClearImm( LED_INIT2_HIGH, LED_INIT2_LOW );

	// read .PRG
	readFile( logger, (char*)DRIVE, (char*)FILENAME_PRG, prgData, &prgSize );

	// first byte of prgData will be used for transmitting ceil(prgSize/256)
	for ( u32 i = prgSize; i > 0; i-- )
		prgData[ i ] = prgData[ i - 1 ];
	prgData[0] = ( ( prgSize + 255 ) >> 8 );

	int startAddr = prgData[ 1 ] + prgData[ 2 ] * 256;
	prgData[ prgData[0] * 256 + 3 ] = ( startAddr + prgSize - 2 ) >> 8;
	prgData[ prgData[0] * 256 + 3 + 1 ] = ( startAddr + prgSize - 2 ) & 255;


	latchSetClearImm( LED_INIT3_HIGH, LED_INIT3_LOW );

	logger->Write( "RaspiMenu", LogNotice, "scan dir" );

	extern void scanDirectories264( char *DRIVE );
	scanDirectories264( (char *)DRIVE );

	latchSetClearImm( LED_INIT4_HIGH, LED_INIT4_LOW );

	//
	// guess automatic timings from current RPi clock rate
	//
	u32 rpiClockMHz = (u32)(m_CPUThrottle.GetClockRate () / 1000000);
	if ( rpiClockMHz < 1500 )
		setDefaultTimings264( AUTO_TIMING_RPI0_264 ); else
		setDefaultTimings264( AUTO_TIMING_RPI3_264 ); 

	logger->Write( "RaspiMenu", LogNotice, "read cfg" );
	if ( !readConfig( logger, (char*)DRIVE, (char*)FILENAME_CONFIG ) )
	{
		latchSetClearImm( LED_INITERR_HIGH, LED_INITERR_LOW );
		logger->Write( "RaspiMenu", LogPanic, "error reading .cfg" );
	}

	u32 t;
	if ( skinFontFilename[0] != 0 && readFile( logger, (char*)DRIVE, (char*)skinFontFilename, charset, &t ) )
	{
		skinFontLoaded = 1;
		memcpy( 1024 + charset+8*(91), skcharlogo_raw, 224 );
		memcpy( 2048 + charset+8*(91), skcharlogo_raw, 224 );
		memcpy( charset + 94 * 8, charset + 2048 + 233 * 8, 8 );
	} 

	readSettingsFile();

#ifdef HDMI_SOUND
	hdmiSoundAvailable = 1;
	hdmiSoundDevice = new CHDMISoundBaseDevice( 48000 );
	if ( hdmiSoundDevice )
	{
		hdmiSoundDevice->SetWriteFormat( SoundFormatSigned24, 2 );
		if (!hdmiSoundDevice->Start ())
		{
			//m_Logger.Write ("SK264", LogPanic, "Cannot start sound device");
			hdmiSoundAvailable = 0;
		} else
			hdmiSoundDevice->Cancel();		
	} else
		hdmiSoundAvailable = 0;
#endif

	applySIDSettings();
	renderC64();
	disableCart = 0;

	latchSetClearImm( LED_INIT5_HIGH, LED_INIT5_LOW );

	return bOK;
}


// cache warmup

volatile u8 forceRead;

__attribute__( ( always_inline ) ) inline void warmCache( void *fiqh, bool screen=true )
{
	if ( screen )
	{
		CACHE_PRELOAD_DATA_CACHE( c64screen, 1024, CACHE_PRELOADL2STRM );
		CACHE_PRELOAD_DATA_CACHE( c64color, 1024, CACHE_PRELOADL2STRM );
	}
	CACHE_PRELOAD_DATA_CACHE( prgData, 65536, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( cartL1, 32768, CACHE_PRELOADL2KEEP );

	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)fiqh, 2048*2 );

	FORCE_READ_LINEARa( cartL1, 32768, 32768 * 10 )
	FORCE_READ_LINEARa( prgData, prgSize, 65536 * 4 )
	FORCE_READ_LINEARa( (void*)fiqh, 2048*2, 65536 );
}

void CKernelMenu::Run( void )
{
	nBytesRead		= 0;
	c64CycleCount	= 0;
	lastChar		= 0;
	resetCounter    = 0;
	transferStarted = 0;
	currentOfs      = 0;
	launchKernel	= 0;
	updateMenu      = 0;
	subGeoRAM		= 0;
	//subSID			= 0;
	subHasLaunch	= -1;
	FILENAME[0]		= 0;
	first			= 1;
	prgCurSize      = prgSize;
	prgCurLaunch    = &prgData[0];

	if ( screenType == 0 )
	{
		// no idea why, but I have one SSD1306 that requires this to be called twice after power up
		splashScreen( raspi_264_splash );
		splashScreen( raspi_264_splash );
	} else
	if ( screenType == 1 )
	{
		tftLoadCharset( DRIVE, FILENAME_TFT_FONT );
		tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 ); 
		tftCopyBackground2Framebuffer();
		tftInitImm( screenRotation );
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}

	if ( hdmiSoundDevice )
		hdmiSoundDevice->Start();

	if ( !disableCart )
	{
		latchSetClearImm( 0, LATCH_RESET | LATCH_ENABLE_KERNAL );

		// setup FIQ
		DisableIRQs();
		m_InputPin.ConnectInterrupt( m_InputPin.FIQHandler, this );
		m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

		launchKernel = 0;

		warmCache( (void*)this->FIQHandler );
		warmCache( (void*)this->FIQHandler );
		warmCache( (void*)this->FIQHandler );

		// start c64 
		DELAY(1<<10);
		latchSetClearImm( LATCH_RESET, 0 );
	}

	// wait forever
	while ( true )
	{
		asm volatile ("wfi");

	#if 1
		if ( doActivateCart )
			activateCart();
	
		if ( launchKernel )
		{
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			EnableIRQs();
			return;
		}

		if ( updateMenu == 1 )
		{
			handleC64( lastChar, &launchKernel, FILENAME, filenameKernal );
			renderC64();

			/*char tt[64];
			static int lastPrintChar = 0;
			if ( lastChar != 0 )
				lastPrintChar = lastChar;
			sprintf( tt, "key=%d", lastPrintChar );
			printC64( 0,  0, tt, 1, 0 );*/

			//c64screen[0]=lastChar&255;
			lastChar = 0xfffffff;
			doneWithHandling = 1;
			updateMenu = 0;


			if ( launchKernel == 5 ) 
			// either: launch CRT, no need to call an external RPi-kernel
			// or: NeoRAM - Autostart
			{
				if ( FILENAME[ 2048 ] != 0 && strcmp( &FILENAME[ 2048 ], "neoram" ) == 0 )
				{
					launchKernel = 6;
					// trigger IRQ on C16/+4 which tells the menu code that the new screen is ready
					DELAY( 1 << 17 );
					CLR_GPIO( bNMI );
					pullIRQ = 64;
					m_InputPin.DisableInterrupt();
					m_InputPin.DisconnectInterrupt();
					EnableIRQs();
					return;
				} else
				{
					u32 size;
					//logger->Write( "menu", LogNotice, "reading %s", (char*)&FILENAME[0] );
					readFile( logger, (char*)DRIVE, (char*)&FILENAME[0], &cart_c16[0], &size );
					cartMode = 1;
					if ( FILENAME[ 2048 ] != 0 )
					{
						//logger->Write( "menu", LogNotice, "reading %s", (char*)&FILENAME[2048] );
						readFile( logger, (char*)DRIVE, (char*)&FILENAME[2048], &cart_c16[0x4000], &size );
						cartMode = 2;
					}
					memcpy( cartL1, cart_c16, 32768 );
					CLR_GPIO( bCTRL257 );
					latchSetClearImm( LATCH_LED0to1, LATCH_RESET | LATCH_ENABLE_KERNAL );
					DELAY( 1 << 17 );
					SET_GPIO( bNMI );
					latchSetClearImm( LATCH_RESET, LATCH_LED0to1 | LATCH_ENABLE_KERNAL );
				}
			} else
			{
				CACHE_PRELOAD_DATA_CACHE( c64screen, 1024, CACHE_PRELOADL2STRM );
				CACHE_PRELOAD_DATA_CACHE( c64color, 1024, CACHE_PRELOADL2STRM );
				// trigger IRQ on C16/+4 which tells the menu code that the new screen is ready
				DELAY( 1 << 17 );
				CLR_GPIO( bNMI );
				pullIRQ = 64;
			}
		}
	#endif
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}


void CKernelMenu::FIQHandler (void *pParam) {}

void CGPIOPinFIQ2::FIQHandler( void *pParam )
{
	register u32 D;

	START_AND_READ_EXPPORT264

	if ( disableCart ) 
		goto get_out;

	if ( BUS_AVAILABLE264 && CPU_READS_FROM_BUS && GET_ADDRESS264 == 0xfde5 )
	{
		PUT_DATA_ON_BUS_AND_CLEAR257( *( curTransfer++ ) )
		CACHE_PRELOADL2STRM( curTransfer );
		goto get_out;
	}
	if ( BUS_AVAILABLE264 && CPU_READS_FROM_BUS && GET_ADDRESS264 >= 0xfde6 && GET_ADDRESS264 <= 0xfde7 )
	{
		D = prgData[ 1 ] + prgData[ 2 ] * 256;
		PUT_DATA_ON_BUS_AND_CLEAR257( (addr2 == 0xfde6) ? ( ( D + prgSize - 2 ) & 255 ) : ( ( D + prgSize - 2 ) >> 8 ) )
		goto get_out;
	}

	if ( BUS_AVAILABLE264 && C1LO_ACCESS && CPU_READS_FROM_BUS )
	{
		PUT_DATA_ON_BUS_AND_CLEAR257( cartL1[ GET_ADDRESS264 & 0x3fff ] )
		goto get_out;
	} 

	if ( BUS_AVAILABLE264 && C1HI_ACCESS && CPU_READS_FROM_BUS && cartMode == 2 )
	{
		PUT_DATA_ON_BUS_AND_CLEAR257( cartL1[ 0x4000 + (GET_ADDRESS264 & 0x3fff) ] )
		goto get_out;
	} 

	if ( BUS_AVAILABLE264 && CPU_READS_FROM_BUS && GET_ADDRESS264 >= 0xfd90 && GET_ADDRESS264 <= 0xfd97 )
	{
		PUT_DATA_ON_BUS_AND_CLEAR257( GET_ADDRESS264 - 0xfd90 + ( (cartMode==0)?0:1 ) )
		CACHE_PRELOADL2STRM( curTransfer );
		goto get_out;
	}

	if ( CPU_WRITES_TO_BUS )
	{
		READ_D0to7_FROM_BUS( D )

		if ( GET_ADDRESS264 == 0xfd91 )
		{
			// keypress
			lastChar = D;
			updateMenu = 1;
			doneWithHandling = 0;
			goto get_out;
		} else
		if ( GET_ADDRESS264 == 0xfde5 )
		{
			if ( D == 0 )
				curTransfer = &c64screen[ 0 ]; else
			if ( D == 1 )
				curTransfer = &c64color[ 0 ]; else
			if ( D == 2 )
				curTransfer = &charset[ 0 ]; else
			if ( D >= 250 )
				machine264 = D - 252; else
				curTransfer = &prgCurLaunch[0];

			// TODO disable Cart when D=123?

			CACHE_PRELOADL2STRM( curTransfer );
			goto get_out;
		} 
	}


get_out:
	register CGPIOPinFIQ2 *pThis = (CGPIOPinFIQ2 *)pParam;
	PeripheralEntry();
	write32( ARM_GPIO_GPEDS0 + pThis->m_nRegOffset, pThis->m_nRegMask );
	PeripheralExit();

	if ( pullIRQ )
	{
		if ( --pullIRQ == 0 )
			SET_GPIO( bNMI );
	}

	if ( BUTTON_PRESSED && ( disableCart || cartMode > 0 ) )
	{
		doActivateCart = 1;
	}

	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	if ( resetCounter > 3 )
	{
		resetCounter = 0;
		c64CycleCount = 0;
	}

	write32( ARM_GPIO_GPCLR0, bCTRL257 );
	
	RESET_CPU_CYCLE_COUNTER
}


void mainMenu()
{
	CKernelMenu kernel;
	if ( kernel.Initialize() )
		kernel.Run();
	setLatchFIQ( LATCH_LED0 );
	prepareOutputLatch();
	outputLatch();
}

int main( void )
{
	CKernelMenu kernel;
	kernel.Initialize();

	extern void KernelLaunchRun( CGPIOPinFIQ2 m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );
	extern void KernelSIDRun( CGPIOPinFIQ2 m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );
	extern void KernelRLRun( CGPIOPinFIQ2 m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME_KERNAL, const char *FILENAME, const char *FILENAME_RAM, u32 sizeRAM, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );

	while ( true )
	{
		//latchSetClearImm( LATCH_LED1, 0 );

		kernel.Run();

		//latchSetClearImm( LATCH_LED0, LATCH_RESET | LATCH_ENABLE_KERNAL );
		SET_GPIO( bNMI | bDMA ); 

		BEGIN_CYCLE_COUNTER
		WAIT_UP_TO_CYCLE( 5000*1000 )

		//logger->Write( "menu", LogNotice, "launch kernel %d", launchKernel );

		char geoRAMFile[ 2048 ];
		u32 geoRAMSize;

		/* for debugging purposes only*/
		/*if ( launchKernel == 255 ) 
		{
			reboot (); 	
		} else*/
		switch ( launchKernel )
		{
		case 3:
			settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
			KernelRLRun( kernel.m_InputPin, &kernel, NULL, NULL, geoRAMFile, geoRAMSize ); 
			break;
		case 4:
			if ( subSID ) {
				KernelSIDRun( kernel.m_InputPin, &kernel, FILENAME, false );
				break;
			}
			if ( subGeoRAM ) {
				settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
				KernelRLRun( kernel.m_InputPin, &kernel, NULL, FILENAME, geoRAMFile, geoRAMSize, false ); 
				break;
			} else {
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, false );
			}
			doActivateCart = 1;
			disableCart = 0;
			break;
		case 6:
			KernelRLRun( kernel.m_InputPin, &kernel, NULL, FILENAME_NEORAM, FILENAME, 4096, false ); 
			break;
		case 40:
			if ( subSID ) {
				KernelSIDRun( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch );
				break;
			}
			if ( subGeoRAM ) {
				settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
				KernelRLRun( kernel.m_InputPin, &kernel, NULL, FILENAME, geoRAMFile, geoRAMSize, true, prgDataLaunch, prgSizeLaunch );
				break;
			} else {
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch );
			}
			break;
		case 8:
			KernelSIDRun( kernel.m_InputPin, &kernel, NULL );
			break;
		default:
			break;
		}
	}

	return EXIT_HALT;
}
