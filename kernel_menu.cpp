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
static unsigned char charset[ 4096 ] AAA;

u32 releaseDMA = 0;
u32 doneWithHandling = 0;
u32 c64CycleCount = 0;
u32 nBytesRead = 0;
u32 first = 1;

#define MAX_FILENAME_LENGTH	512
char FILENAME[ MAX_FILENAME_LENGTH * 2 ];

static u32 launchKernel = 0;
static u32 lastChar = 0;

static u32 screenTransferBytes;
static u8 *screenTransfer = &c64screen[ 0 ];
static u32 colorTransferBytes;
static u8 *colorTransfer = &c64screen[ 0 ];
static u8 *charsetTransfer = &charset[ 0 ];


void deactivateCart()
{
	disableCart = 1;
	latchSetClearImm( LATCH_LED_ALL, LATCH_RESET | LATCH_ENABLE_KERNAL );
	SET_GPIO( bGAME | bEXROM | bNMI | bDMA );

	for ( int j = 255; j > 63; j -- )
	{
		oledSetContrast( j );
		flushI2CBuffer( true );
		DELAY( 1 << 17 );
	}

	latchSetClearImm( LATCH_RESET, LATCH_LED_ALL | LATCH_ENABLE_KERNAL );
}

void activateCart()
{
	disableCart = 0;
	transferStarted = 0;

	latchSetClearImm( LATCH_LED_ALL, LATCH_RESET | LATCH_ENABLE_KERNAL );
	SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr | bCTRL257 );
	latchSetClearImm( LATCH_LED_ALL, LATCH_ENABLE_KERNAL );

	DELAY( 1 << 20 );

	oledSetContrast( 255 );
	flushI2CBuffer();
	latchSetClearImm( LATCH_RESET, LATCH_LED_ALL | LATCH_ENABLE_KERNAL );
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
	latchSetClearImm( LATCH_LED0, LATCH_LED1to3 );

	// initialize ARM cycle counters (for accurate timing)
	initCycleCounter();

	// initialize GPIOs
	gpioInit();

	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( LATCH_LED0, LATCH_LED1to3 );

	// read launch code
	u32 size = 0;

	cartCBM80 = (unsigned char *)( ((u64)&cart_pool+64) & ~63 );
	readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, cartCBM80, &size );

	latchSetClearImm( LATCH_LED1, 0 );

	// read .PRG
	readFile( logger, (char*)DRIVE, (char*)FILENAME_PRG, prgData, &prgSize );

	latchSetClearImm( LATCH_LED2, 0 );

	extern void scanDirectories( char *DRIVE );
	scanDirectories( (char *)DRIVE );

	latchSetClearImm( LATCH_LED3, 0 );

	if ( !readConfig( logger, (char*)DRIVE, (char*)FILENAME_CONFIG ) )
	{
		latchSetClearImm( LATCH_LED_ALL, 0 );
		logger->Write( "RaspiMenu", LogPanic, "error reading .cfg" );
	}

	u32 t;
	if ( skinFontFilename[0] != 0 && readFile( logger, (char*)DRIVE, (char*)skinFontFilename, charset, &t ) )
	{
		skinFontLoaded = 1;
		memcpy( 1024 + charset+8*(91), skcharlogo_raw, 224 );
		memcpy( 2048 + charset+8*(91), skcharlogo_raw, 224 );
	} 

	readSettingsFile();
	applySIDSettings();
	renderC64();
	startInjectCode();
	disableCart = 0;

	latchSetClearImm( 0, LATCH_LED_ALL );

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


// cache warmup

volatile u8 forceRead;

__attribute__( ( always_inline ) ) inline void warmCache( void *fiqh )
{
	CACHE_PRELOAD_DATA_CACHE( c64screen, 1024, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( c64color, 1024, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( cartCBM80, 8192, CACHE_PRELOADL2KEEP );
	CACHE_PRELOAD_DATA_CACHE( prgData, 65536, CACHE_PRELOADL2STRM );
	CACHE_PRELOAD_DATA_CACHE( injectCode, 256, CACHE_PRELOADL2KEEP );

	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)fiqh, 2048*2 );

	FORCE_READ_LINEARa( cartCBM80, 8192, 8192 * 10 )
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
	subSID			= 0;
	subHasKernal	= -1;
	subHasLaunch	= -1;
	FILENAME[0]		= 0;
	first			= 1;

	#ifdef USE_OLED
	splashScreen( raspi_c64_splash );
	#endif

	if ( !disableCart )
	{
		// set GAME and EXROM as defined above (+ set NMI, DMA and latch outputs)
		configGAMEEXROMSet = ( (GAME_ACTIVE == 0)  ? 0 : bGAME ) | ( (EXROM_ACTIVE == 0) ? 0 : bEXROM ) | bNMI | bDMA;
		configGAMEEXROMClr = ( (GAME_ACTIVE == 0)  ? bGAME : 0 ) | ( (EXROM_ACTIVE == 0) ? bEXROM : 0 ); 
		SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );

		latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

		// setup FIQ
		DisableIRQs();
		m_InputPin.ConnectInterrupt( this->FIQHandler, this );
		m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

		launchKernel = 0;

		warmCache( (void*)this->FIQHandler );

		// start c64 
		DELAY(1<<10);
		latchSetClearImm( LATCH_RESET, 0 );
	}

	// wait forever
	while ( true )
	{
		if ( first && nBytesRead < 32 && c64CycleCount > 1000000 )
		{
			first = 0;
			latchSetClearImm( LATCH_LEDR, LATCH_RESET );
			DELAY(1<<27);
			latchSetClearImm( LATCH_RESET | LATCH_LEDR, 0 );
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
		//u32 v = 1 << ( refresh % 6 );
		u32 v = 1 << ( ( c64CycleCount >> 18 ) % 6 );
		u32 l_on = ( v + ( ( v & 16 ) >> 2 ) + ( ( v & 32 ) >> 4 ) ) & 15;
		u32 l_off = ( ~l_on ) & 15;
		latchSetClear( l_on * LATCH_LED0, l_off * LATCH_LED0 );

		if ( updateMenu == 1 )
		{
			handleC64( lastChar, &launchKernel, FILENAME, filenameKernal );
			lastChar = 0xfffffff;
			refresh++;
			renderC64();
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

	START_AND_READ_ADDR0to7_RW_RESET_CS

	if ( CPU_RESET ) { 
		resetCounter ++; 
		FINISH_BUS_HANDLING
		return;
	} else  
		resetCounter = 0; 

	c64CycleCount ++;
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( BUTTON_PRESSED )
	{
		activateCart();
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
			D = ( prgSize + 255 ) >> 8;
		} else
		{
			// $DE00 -> get next byte
			D = prgData[ currentOfs++ ];
			currentOfs %= ((prgSize+255)>>8)<<8;
		}

		WRITE_D0to7_TO_BUS( D )
		forceRead = prgData[ currentOfs ]; // todo: necessary?
		FINISH_BUS_HANDLING
		return;
	}

	if ( CPU_WRITES_TO_BUS && IO1_ACCESS ) // write to IO1
	{
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
		} else
		if ( A == 3 ) // VIC detection
		{
			modeVIC = D;
			updateMenu = 1;
		}
		if ( A == 4 )
		{
			charsetTransfer = &charset[ 0 ];
			CACHE_PRELOADL2STRM( charsetTransfer );
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
	setLatchFIQ( LATCH_LEDO );
	prepareOutputLatch();
	outputLatch();
}

int main( void )
{
	CKernelMenu kernel;
	kernel.Initialize();

	//extern void KernelCartRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu );
	extern void KernelKernalRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME );
	extern void KernelGeoRAMRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu );
	extern void KernelLaunchRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );
	extern void KernelEFRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME );
	extern void KernelFC3Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelAR6Run( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, char *FILENAME = NULL );
	extern void KernelSIDRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );
	extern void KernelSIDRun8( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );
	extern void KernelRKLRun( CGPIOPinFIQ	m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME_KERNAL, const char *FILENAME, const char *FILENAME_RAM, u32 sizeRAM, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 );

	extern u32 octaSIDMode;


	while ( true )
	{
		latchSetClearImm( LATCH_LED1, 0 );

		kernel.Run();

		latchSetClearImm( LATCH_LED_ALL, LATCH_RESET | LATCH_ENABLE_KERNAL );
		SET_GPIO( bNMI | bDMA ); 

		BEGIN_CYCLE_COUNTER
		WAIT_UP_TO_CYCLE( 5000*1000 )


		char geoRAMFile[ 2048 ];
		u32 geoRAMSize;

		switch ( launchKernel )
		{
		/*case 1:
			KernelCartRun( kernel.m_InputPin, &kernel );
			break;*/
		case 2:
			KernelKernalRun( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 3:
			settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
			if ( subHasKernal == -1 )
				KernelRKLRun( kernel.m_InputPin, &kernel, NULL, NULL, geoRAMFile, geoRAMSize ); else
				//KernelGeoRAMRun( kernel.m_InputPin, &kernel ); else
				KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, NULL, geoRAMFile, geoRAMSize );
			break;
		case 4:
			if ( subSID ) {
				if ( octaSIDMode )
				{
					KernelSIDRun8( kernel.m_InputPin, &kernel, FILENAME, false ); 
				} else
					KernelSIDRun( kernel.m_InputPin, &kernel, FILENAME, false ); 
				break;
			}
			if ( subGeoRAM ) {
				settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
				if ( subHasKernal == -1 ) {
					KernelRKLRun( kernel.m_InputPin, &kernel, NULL, FILENAME, geoRAMFile, geoRAMSize, false ); 
					break;
				} else {
					KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, FILENAME, geoRAMFile, geoRAMSize, false ); 
					break;
				}
			} else {
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, false );
			}
			break;
		case 40:
			if ( subSID ) {
				if ( octaSIDMode )
					KernelSIDRun8( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch ); else
					KernelSIDRun( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch );
				break;
			}
			if ( subGeoRAM ) {
				settingsGetGEORAMInfo( geoRAMFile, &geoRAMSize );
				if ( subHasKernal == -1 ) {
					KernelRKLRun( kernel.m_InputPin, &kernel, NULL, FILENAME, geoRAMFile, geoRAMSize, true, prgDataLaunch, prgSizeLaunch );
					break;
				} else {
					KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, FILENAME, geoRAMFile, geoRAMSize, true, prgDataLaunch, prgSizeLaunch );
					//KernelRKLRun( kernel.m_InputPin, &kernel, filenameKernal, FILENAME, true, prgDataLaunch, prgSizeLaunch );
					break;
				}
			} else {
				KernelLaunchRun( kernel.m_InputPin, &kernel, FILENAME, true, prgDataLaunch, prgSizeLaunch );
			}
			break;
		case 5:
			KernelEFRun( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 6:
			KernelFC3Run( kernel.m_InputPin, &kernel );
			break;
		case 60:
			KernelFC3Run( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 7:
			KernelAR6Run( kernel.m_InputPin, &kernel );
			break;
		case 70:
			KernelAR6Run( kernel.m_InputPin, &kernel, FILENAME );
			break;
		case 8:
			if ( octaSIDMode )
				KernelSIDRun8( kernel.m_InputPin, &kernel, NULL ); else
				KernelSIDRun( kernel.m_InputPin, &kernel, NULL );
			break;
		default:
			break;
		}
	}

	return EXIT_HALT;
}
