/*
  _________.__    .___      __   .__        __          _________.___________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     /   _____/|   \______ \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     \_____  \ |   ||    |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /        \|   ||    `   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    /_______  /|___/_______  /
        \/         \/    \/     \/       \/     \/            \/             \/ 
 
 kernel_sid.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick SID: a SID and SFX Sound Expander Emulation 
		    (using reSID by Dag Lem and FMOPL by Jarek Burczynski, Tatsuyuki Satoh, Marco van den Heuvel, and Acho A. Tang)
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
#include <math.h>
#include "kernel_sid.h"
#ifdef COMPILE_MENU
#include "kernel_menu.h"
#include "launch.h"

static u32 launchPrg;
#endif

#undef USE_VCHIQ_SOUND

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_sid_bg2.tga";
static const char FILENAME_SPLASH_RGB2[] = "SD:SPLASH/sk64_sid_bg.tga";
static const char FILENAME_LED_RGB[] = "SD:SPLASH/sk64_sid_led.tga";

//                 _________.___________         ____                      ________    ______  ____________  
//_______   ____  /   _____/|   \______ \       /  _ \       ___.__. _____ \_____  \  /  __  \/_   \_____  \ 
//\_  __ \_/ __ \ \_____  \ |   ||    |  \      >  _ </\    <   |  |/     \  _(__  <  >      < |   |/  ____/ 
// |  | \/\  ___/ /        \|   ||    `   \    /  <_\ \/     \___  |  Y Y  \/       \/   --   \|   /       \ 
// |__|    \___  >_______  /|___/_______  /    \_____\ \     / ____|__|_|  /______  /\______  /|___\_______ \
//             \/        \/             \/            \/     \/          \/       \/        \/             \/
#include "resid/sid.h"
using namespace reSID;

u32 CLOCKFREQ = 985248;	// exact clock frequency of the C64 will be measured at start up

// SID types and digi boost (only for MOS8580)
unsigned int SID_MODEL[2] = { 8580, 8580 };
unsigned int SID_DigiBoost[2] = { 0, 0 };

// do not change this value
#define NUM_SIDS 2 
SID *sid[ NUM_SIDS ];

#ifdef EMULATE_OPL2
FM_OPL *pOPL;
u32 fmOutRegister;
#endif

// a ring buffer storing SID-register writes (filled in FIQ handler)
// TODO should be much smaller
#define RING_SIZE (1024*128)
u32 ringBufGPIO[ RING_SIZE ];
unsigned long long ringTime[ RING_SIZE ];
u32 ringWrite;

// prepared GPIO output when SID-registers are read
u32 outRegisters[ 32 ];
u32 outRegisters_2[ 32 ];

u32 fmFakeOutput = 0;
u32 fmAutoDetectStep = 0;
u32 sidFakeOutput = 0;
u32 sidAutoDetectStep = 0;
u32 sidAutoDetectStep_2 = 0;
uint8_t sidAutoDetectRegs[ 32 ];
uint8_t sidAutoDetectRegs_2[ 32 ];

// counts the #cycles when the C64-reset line is pulled down (to detect a reset)
u32 resetCounter,
	cyclesSinceReset,  
	resetPressed, resetReleased;

// this is the actual configuration of the emulation
u32 cfgRegisterRead = 0;			// don't use this when you have a SID(-replacement) in the C64
u32 cfgEmulateOPL2 = 1;
u32 cfgSID2_Disabled = 0;
u32 cfgSID2_PlaySameAsSID1 = 0;
u32 cfgMixStereo = 1;
u32 cfgSID2_Addr = 0;
s32 cfgVolSID1_Left, cfgVolSID1_Right;
s32 cfgVolSID2_Left, cfgVolSID2_Right;
s32 cfgVolOPL_Left, cfgVolOPL_Right;

extern u32 wireSIDAvailable;

void setSIDConfiguration( u32 mode, u32 sid1, u32 sid2, u32 sid2addr, u32 rr, u32 addr, u32 exp, s32 v1, s32 p1, s32 v2, s32 p2, s32 v3, s32 p3 )
{
	SID_MODEL[ 0 ] = ( sid1 == 0 ) ? 6581 : 8580;
	SID_DigiBoost[ 0 ] = ( sid1 == 2 ) ? 1 : 0;
	SID_MODEL[ 1 ] = ( sid2 == 0 ) ? 6581 : 8580;
	SID_DigiBoost[ 1 ] = ( sid2 == 2 ) ? 1 : 0;

	// panning = 0 .. 14, vol = 0 .. 15
	// volumes -> 0 .. 255
	cfgVolSID1_Left  = v1 * ( 14 - p1 );
	cfgVolSID1_Right = v1 * ( p1 );
	cfgVolSID2_Left  = v2 * ( 14 - p2 );
	cfgVolSID2_Right = v2 * ( p2 );
	cfgVolOPL_Left  = v3 * ( 14 - p3 );
	cfgVolOPL_Right = v3 * ( p3 );

	int maxVolFactor = max( cfgVolSID1_Left, max( cfgVolSID1_Right, max( cfgVolSID2_Left, max( cfgVolSID2_Right, max( cfgVolOPL_Left, cfgVolOPL_Right ) ) ) ) );

	cfgVolSID1_Left  = cfgVolSID1_Left  * 256 / maxVolFactor;
	cfgVolSID1_Right = cfgVolSID1_Right * 256 / maxVolFactor;
	cfgVolSID2_Left  = cfgVolSID2_Left  * 256 / maxVolFactor;
	cfgVolSID2_Right = cfgVolSID2_Right * 256 / maxVolFactor;
	cfgVolOPL_Left   = cfgVolOPL_Left   * 256 / maxVolFactor;
	cfgVolOPL_Right  = cfgVolOPL_Right  * 256 / maxVolFactor;
	
	if ( sid2 == 3 ) 
	{ 
		cfgSID2_Disabled = 1; cfgVolSID2_Left = cfgVolSID2_Right = 0;
	} else 
		cfgSID2_Disabled = 0;

	if ( addr == 0 ) cfgSID2_PlaySameAsSID1 = 1; else cfgSID2_PlaySameAsSID1 = 0;
	if ( mode == 0 ) cfgMixStereo = 1; else cfgMixStereo = 0;


	cfgSID2_Addr = sid2addr; // 0 = $d420, 1 = $d500, 2 = $de00
	cfgRegisterRead = rr;
	cfgEmulateOPL2 = exp;

	if ( !wireSIDAvailable )
	{
		cfgVolSID1_Left  = 0;
		cfgVolSID1_Right = 0;
		cfgVolSID2_Left  = 0;
		cfgVolSID2_Right = 0;
		cfgSID2_Disabled = 0;
		cfgRegisterRead  = 0;
	}

	if ( !exp )
	{
		cfgVolOPL_Left = cfgVolOPL_Right = 0;
	}
}

//  __     __                __      ___                   ___ 
// /__` | |  \     /\  |\ | |  \    |__   |\/|    | |\ | |  |  
// .__/ | |__/    /~~\ | \| |__/    |     |  |    | | \| |  |  
//                                                            
void initSID()
{
	resetCounter = 0;

	for ( int i = 0; i < NUM_SIDS; i++ )
	{
		sid[ i ] = new SID;

		for ( int j = 0; j < 25; j++ )
			sid[ i ]->write( j, 0 );

		if ( SID_MODEL[ i ] == 6581 )
		{
			sid[ i ]->set_chip_model( MOS6581 );
		} else
		{
			sid[ i ]->set_chip_model( MOS8580 );
			if ( SID_DigiBoost[ i ] == 0 )
			{
				sid[ i ]->set_voice_mask( 0x07 );
				sid[ i ]->input( 0 );
			} else
			{
				sid[ i ]->set_voice_mask( 0x0f );
				sid[ i ]->input( -32768 );
			}
		}

		int SID_passband = 90;
		int SID_gain = 97;
		int SID_filterbias = 500;

		sid[ i ]->adjust_filter_bias( SID_filterbias / 1000.0f );
		sid[ i ]->set_sampling_parameters( CLOCKFREQ, SAMPLE_FAST, SAMPLERATE, SAMPLERATE * SID_passband / 200.0f, SID_gain / 100.0f );
	}

#ifdef EMULATE_OPL2
	if ( cfgEmulateOPL2 )
	{
		pOPL = ym3812_init( 3579545, SAMPLERATE );
		ym3812_reset_chip( pOPL );
		fmFakeOutput = 0;
	}
#endif
	fmFakeOutput =
	fmAutoDetectStep = 0;

	sidFakeOutput =
	sidAutoDetectStep =
	sidAutoDetectStep_2 = 0;

	for ( int i = 0; i < 32; i++ )
		outRegisters[ i ] = outRegisters_2[ i ] = 0;

	for ( int i = 0; i < 20; i++ )
	{
		sidAutoDetectRegs[ i ] = 0;
		sidAutoDetectRegs_2[ i ] = 0;
	}

	// ring buffer init
	ringWrite = 0;
	for ( int i = 0; i < RING_SIZE; i++ )
		ringTime[ i ] = 0;
}

void quitSID()
{
	for ( int i = 0; i < NUM_SIDS; i++ )
		delete sid[ i ];
}

unsigned long long cycleCountC64;


#ifdef COMPILE_MENU
extern CLogger			*logger;
extern CTimer			*pTimer;
extern CScheduler		*pScheduler;
extern CInterruptSystem	*pInterrupt;
extern CVCHIQDevice		*pVCHIQ;
extern CScreenDevice	*screen;
CSoundBaseDevice	*m_pSound;
#else

CLogger				*logger;
CTimer				*pTimer;
CScheduler			*pScheduler;
CInterruptSystem	*pInterrupt;
CVCHIQDevice		*pVCHIQ;
CScreenDevice		*screen;


boolean CKernel::Initialize( void )
{
	boolean bOK = TRUE;

#ifdef USE_HDMI_VIDEO
	if ( bOK ) bOK = m_Screen.Initialize();

	if ( bOK )
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice( m_Options.GetLogDevice(), FALSE );
		if ( pTarget == 0 )
			pTarget = &m_Screen;

		bOK = m_Logger.Initialize( pTarget );
		logger = &m_Logger;
	}
#endif

	if ( bOK ) bOK = m_Interrupt.Initialize();
	if ( bOK ) bOK = m_Timer.Initialize();

#ifdef USE_VCHIQ_SOUND
	if ( bOK ) bOK = m_VCHIQ.Initialize();
	pVCHIQ = &m_VCHIQ;
#endif

	pTimer = &m_Timer;
	pScheduler = &m_Scheduler;
	pInterrupt = &m_Interrupt;
	screen = &m_Screen;

	return bOK;
}
#endif

static u32 renderDone = 0;

static u32 visMode = 0;
static u32 visModeGotoNext = 0;

static float px = 120.0f;
static float dx = 0.0f;
static u32 startRow = 1, endRow = 1;
static float vuValueAvg = 0.01f;
static u32 visUpdate = 1;
static int scopeX = 0;
static u8 scopeValues[ 240 ] = {0};
static u32 nPrevLEDs[3] = {0, 0, 0};
static float ledValueAvg[3] = { 0.01f, 0.01f, 0.01f };

static void initVisualization()
{
	visMode = 0;
	visModeGotoNext = 0;
	px = 120.0f;
	dx = 0.0f;
	startRow = 1; endRow = 1;
	vuValueAvg = 0.01f;
	visUpdate = 1;
	scopeX = 0;
	memset( scopeValues, 0, 256 );
	nPrevLEDs[0] = nPrevLEDs[1] = nPrevLEDs[2] = 0;
	ledValueAvg[0] = ledValueAvg[1] = ledValueAvg[2] = 0.01f;
}

static unsigned char tftBackground2[ 240 * 240 * 2 ];
static unsigned char tftLEDs[ 240 * 240 * 2 ];

static u32 vu_Mode = 0;
static u32 vuMeter[4] = { 0, 0, 0, 0 };
static u32 vu_nLEDs = 0xffff;

static u32 allUsedLEDs = 0;

static int busValue = 0;
static int busValueTTL = 0;
static unsigned long long nCyclesEmulated = 0;
static unsigned long long samplesElapsed = 0;

static u32 resetFromCodeState = 0;
static u32 _playingPSID = 0;

static void prepareOnReset( bool refresh = false )
{
	fmFakeOutput =
	fmAutoDetectStep = 0;

	sidFakeOutput =
	sidAutoDetectStep =
	sidAutoDetectStep_2 = 0;

	for ( int i = 0; i < 20; i++ )
	{
		sidAutoDetectRegs[ i ] = 0;
		sidAutoDetectRegs_2[ i ] = 0;
	}

	if ( launchPrg )
		{disableCart = 0; SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );} else
		{SETCLR_GPIO( bNMI | bDMA | bGAME | bEXROM, 0 );}

	if ( !refresh )
	{
		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();
	}

	if ( _playingPSID )
	{
		extern unsigned char charset[ 4096 ];
		CACHE_PRELOADL2STRM( &charset[ 2048 ] );
		FORCE_READ_LINEAR32( (void*)&charset[ 2048 ], 1024 );
	}

	if ( launchPrg )
	{
		launchPrepareAndWarmCache();
	}

	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 6*1024 );
	FORCE_READ_LINEAR32a( (void*)&FIQ_HANDLER, 6*1024, 32768 );

	resetCounter = cycleCountC64 = nCyclesEmulated = samplesElapsed = 0;
}

//static u32 haltC64 = 0, letgoC64 = 0;
//static u32 SID_not_initialized = 1;


#ifdef COMPILE_MENU
void KernelSIDFIQHandler( void *pParam );

void KernelSIDRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0 )
#else
void CKernel::Run( void )
#endif
{
	// initialize ARM cycle counters (for accurate timing)
	initCycleCounter();

	// initialize GPIOs
	gpioInit();
	SET_BANK2_OUTPUT

	// initialize latch and software I2C buffer
	initLatch();

	#ifndef COMPILE_MENU
	int screenType = 0;
	#endif

	if ( screenType == 0 )
		allUsedLEDs = LATCH_LED_ALL;
	if ( screenType == 1 )
		allUsedLEDs = LATCH_LED0to1;
	
	latchSetClearImm( 0, LATCH_RESET | allUsedLEDs | LATCH_ENABLE_KERNAL );

	SETCLR_GPIO( bNMI | bDMA | bGAME | bEXROM, 0 );

	#ifdef COMPILE_MENU
	_playingPSID = playingPSID;

	if ( screenType == 0 )
	{
		splashScreen( raspi_sid_splash );
	} else
	if ( screenType == 1 )
	{
		tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 );

		int w, h; 
		extern char FILENAME_LOGO_RGBA[128];
		extern unsigned char tempTGA[ 256 * 256 * 4 ];

		if ( tftLoadTGA( DRIVE, FILENAME_LOGO_RGBA, tempTGA, &w, &h, true ) )
		{
			tftBlendRGBA( tempTGA, tftBackground, 0 );
		}

		tftCopyBackground2Framebuffer();

		u32 c1 = rgb24to16( 166, 250, 128 );
		u32 c2 = rgb24to16( 98, 216, 204 );
		u32 c3 = rgb24to16( 233, 114, 36 );
		char b1[64], b2[64], b3[64];
		int charWidth = 16;
		const char *addr[3] = { "$d420", "$d500", "$de00" };

		sprintf( b1, "%s", SID_MODEL[ 0 ] == 6581 ? "6581":"8580" );

		if ( cfgSID2_Disabled )
			b2[ 0 ] = 0; else
		{
			if ( cfgSID2_PlaySameAsSID1 )
			sprintf( b2, "+%s/$d400", SID_MODEL[ 1 ] == 6581 ? "6581":"8580"); else
			sprintf( b2, "+%s/%s", SID_MODEL[ 1 ] == 6581 ? "6581":"8580", addr[cfgSID2_Addr]);
		}

		if ( cfgEmulateOPL2 )
			sprintf( b3, "+fm" ); else
			b3[ 0 ] = 0;

		int l = strlen( b1 ) + strlen( b2 ) + strlen( b3 );
		if ( l * 16 >= 240 )
			charWidth = 13;
		int sx = max( 0, ( 240 - charWidth * l ) / 2 - 2 );
		tftPrint( b1, sx, 224, c1, charWidth == 16 ? 0 : -3 );	sx += strlen( b1 ) * charWidth;
		tftPrint( b2, sx, 224, c2, charWidth == 16 ? 0 : -3 );	sx += strlen( b2 ) * charWidth;
		tftPrint( b3, sx, 224, c3, charWidth == 16 ? 0 : -3 );

		tftInit();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
		tftConvertFrameBuffer12Bit();

		tftClearDirty();
		extern void tftPrepareDirtyUpdates();
		tftPrepareDirtyUpdates();
		tftUse12BitColor();

		memcpy( tftBackground2, tftBackground, 240 * 240 * 2 );
		tftLoadBackgroundTGA( DRIVE, FILENAME_LED_RGB, 8 );
		memcpy( tftLEDs, tftBackground, 240 * 240 * 2 );

		tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB2, 8 );

		initVisualization();
	} 
	#endif

	//if ( SID_not_initialized )
	{
		//letgoC64 = 0; haltC64 = 1;

		//	logger->Write( "", LogNotice, "initialize SIDs..." );
		initSID();

		//
		// initialize sound output (either PWM which is output in the FIQ handler, or via HDMI)
		//
		initSoundOutput( &m_pSound, pVCHIQ );

		//haltC64 = 0; letgoC64 = 1;
		//SID_not_initialized = 0;
	}

	#ifdef COMPILE_MENU
	disableCart = 0;

	if ( FILENAME == NULL && !hasData )
	{
		SETCLR_GPIO( bNMI | bDMA | bGAME | bEXROM, 0 );
		launchPrg = 0;
		disableCart = 1;
	} else
	{
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
		launchPrg = 1;
		if ( launchGetProgram( FILENAME, hasData, prgDataExt, prgSizeExt ) )
			launchInitLoader( false, c128PRG ); else
			launchPrg = 0;
	}
	#endif

	//
	// setup FIQ
	//
	resetReleased = 0xff;
	//haltC64 = letgoC64 = 0;
	resetFromCodeState = 0;

	#ifdef COMPILE_MENU
	prepareOnReset();
	m_InputPin.ConnectInterrupt( KernelSIDFIQHandler, kernelMenu );
	#else
	m_InputPin.ConnectInterrupt( this->FIQHandler, this );
	#endif
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

#ifndef COMPILE_MENU

	latchSetClear( LATCH_RESET, allUsedLEDs | LATCH_ENABLE_KERNAL );

	cycleCountC64 = 0;
	while ( cycleCountC64 < 10 ) 
	{
		pScheduler->MsSleep( 100 );
	}


	//
	// measure clock rate of the C64 (more accurate syncing with emulation, esp. for HDMI output)
	//
	cycleCountC64 = 0;
	unsigned long long startTime = pTimer->GetClockTicks();
	unsigned long long curTime;

	do {
		curTime = pTimer->GetClockTicks();
	} while ( curTime - startTime < 1000000 );

	unsigned long long clockFreq = cycleCountC64 * 1000000 / ( curTime - startTime );
	CLOCKFREQ = clockFreq;
	logger->Write( "", LogNotice, "Measured C64 clock frequency: %u Hz", (u32)CLOCKFREQ );
#endif

//	logger->Write( "", LogNotice, "start emulating..." );
	cycleCountC64 = 0;
	nCyclesEmulated = 0;
	samplesElapsed = 0;

	// how far did we consume the commands in the ring buffer?
	unsigned int ringRead = 0;

	#ifdef COMPILE_MENU
	prepareOnReset( true );

	resetCounter = 0;
	latchSetClear( LATCH_RESET, allUsedLEDs | LATCH_ENABLE_KERNAL );

startHereAfterReset:

	latchSetClear( LATCH_RESET, 0 );
	if ( launchPrg )
	{
		while ( !disableCart )
		{
			#ifdef COMPILE_MENU
			TEST_FOR_JUMP_TO_MAINMENU( cycleCountC64, resetCounter )
			#endif
			asm volatile ("wfi");

			if ( resetFromCodeState == 2 )
			{
				EnableIRQs();
				m_InputPin.DisableInterrupt();
				m_InputPin.DisconnectInterrupt();
				return;		
			}
			
			if ( cycleCountC64 > 2000000 )
			{
				cycleCountC64 = 0;
				latchSetClear( 0, LATCH_RESET );
				DELAY(1<<20);
				latchSetClear( LATCH_RESET, 0 );
			}
		}
		//latchSetClearImm( LATCH_LED0, 0 );
	} 
	#endif

	resetReleased = 0;
	resetCounter = cycleCountC64 = 0;
	nCyclesEmulated = 0;
	samplesElapsed = 0;
	ringRead = ringWrite = 0;

	latchSetClear( 0, allUsedLEDs );

	// new main loop mainloop
	while ( true )
	{
		#ifdef COMPILE_MENU
		//TEST_FOR_JUMP_TO_MAINMENU( cycleCountC64, resetCounter )
		if ( cycleCountC64 > 2000000 && resetCounter > 500000 ) {
				EnableIRQs();
				m_InputPin.DisableInterrupt();
				m_InputPin.DisconnectInterrupt();
				quitSID();
				return;
			}
		#endif

		if ( resetReleased == 1 )
		{
			resetReleased = 0xff;
			resetCounter = 0;
		
			for ( int i = 0; i < NUM_SIDS; i++ )
			{
				for ( int j = 0; j < 25; j++ )
					sid[ i ]->write( j, 0 );
			}

			#ifdef EMULATE_OPL2
			if ( cfgEmulateOPL2 )
			{
				fmFakeOutput = 0;
				ym3812_reset_chip( pOPL );
			}
			#endif
		
			ringRead = ringWrite;

			prepareOnReset( true );
			latchSetClear( allUsedLEDs, LATCH_RESET );
			DELAY(1<<10);
			latchSetClear( LATCH_RESET, allUsedLEDs );
			resetCounter = resetReleased = resetPressed = 0;
			goto startHereAfterReset;
		}

		#ifdef COMPILE_MENU
		//if ( screenType_ == 0 )
		{
			if ( renderDone == 2 )
			{
				if ( !sendFramebufferDone() )
					sendFramebufferNext( 1 );		

				if ( sendFramebufferDone() )
					renderDone = 3;
			}
			if ( bufferEmptyI2C() && renderDone == 1 )
			{
				sendFramebufferStart();
				renderDone = 2;
			}
		}
		#endif

	#ifndef EMULATION_IN_FIQ

		#ifndef USE_PWM_DIRECT
		static u32 nSamplesInThisRun = 0;
		#endif

		s16 val1, val2;
		s32 valOPL;

		unsigned long long cycleCount = cycleCountC64;
		while ( cycleCount > nCyclesEmulated )
		{
		#ifndef USE_PWM_DIRECT
			static int start = 0;
			if ( nSamplesInThisRun > 2205 / 8 )
			{
				if ( !start )
				{
					m_pSound->Start();
					start = 1;
				} else
				{
					//pScheduler->MsSleep( 1 );
					pScheduler->Yield();
				}
				nSamplesInThisRun = 0;
			}
			nSamplesInThisRun++;
		#endif

			CACHE_PRELOADL2STRMW( &smpCur );

		#ifdef SMPCARRY

			static u32 carrySamples = 0;
			u32 samplesToEmulateX65536 = ( ( unsigned long long )65536 * ( unsigned long long )CLOCKFREQ ) / ( unsigned long long )SAMPLERATE + ( unsigned long long )carrySamples;

			u32 samplesToEmulate = samplesToEmulateX65536 >> 16;
			carrySamples = (samplesToEmulateX65536 & 65535);

//			do 
			{ // do SID emulation until time passed to create an additional sample (i.e. there may be several cycles until a sample value is created)
				#ifdef USE_PWM_DIRECT
				u32 cyclesToEmulate = samplesToEmulate;
				#else			
				u32 cyclesToEmulate = 2;
				#endif

		#else
			unsigned long long samplesElapsedBefore = samplesElapsed;

			//long long cycleNextSampleReady = ( ( unsigned long long )(samplesElapsedBefore+1) * ( unsigned long long )CLOCKFREQ ) / ( unsigned long long )SAMPLERATE;
			//int cyclesToNextSample = cycleNextSampleReady - nCyclesEmulated;

			do { // do SID emulation until time passed to create an additional sample (i.e. there may be several cycles until a sample value is created)
				#ifdef USE_PWM_DIRECT
				u32 cyclesToEmulate = min( 8, cycleCount - nCyclesEmulated );
				#else			
				u32 cyclesToEmulate = 2;
				#endif
		#endif

				//problem: wegen dem innern do-while kann nCyclesEmulated tatsächlich größer als cycleCount werden -> dann ist cyclesToEmulate 0 oder negativ.
				//frage: warum laufen wir zu lange innen und was macht das mit der emulation?

/*				if ( cyclesToEmulate > cyclesToNextSample )
					cyclesToEmulate = cyclesToNextSample;
				if ( cyclesToEmulate == 0 )
					cyclesToEmulate = 1;*/

				if ( ringRead != ringWrite )
				{
					int cyclesToNextWrite = (signed long long)ringTime[ ringRead ] - (signed long long)nCyclesEmulated;

					if ( (int)cyclesToEmulate > cyclesToNextWrite && cyclesToNextWrite > 0 )
						cyclesToEmulate = cyclesToNextWrite;
				}
				
				if ( cyclesToEmulate > 0 )
				{
					sid[ 0 ]->clock( cyclesToEmulate );
					#ifndef SID2_DISABLED
					if ( !cfgSID2_Disabled )
						sid[ 1 ]->clock( cyclesToEmulate );
					#endif

					outRegisters[ 27 ] = sid[ 0 ]->read( 27 );
					outRegisters[ 28 ] = sid[ 0 ]->read( 28 );
					if ( !cfgSID2_Disabled )
					{
						outRegisters_2[ 27 ] = 0;
						outRegisters_2[ 28 ] = 0;
					}

					nCyclesEmulated += cyclesToEmulate;
					//  cyclesToNextSample -= cyclesToEmulate;
				}


				// apply register updates (we do one-cycle emulation steps, but in case we need to catch up...)
				unsigned int readUpTo = ringWrite;

				if ( ringRead != readUpTo && nCyclesEmulated >= ringTime[ ringRead ] )
				{
					unsigned char A, D;
					decodeGPIO( ringBufGPIO[ ringRead ], &A, &D );

					#ifdef EMULATE_OPL2
					if ( cfgEmulateOPL2 && (ringBufGPIO[ ringRead ] & bIO2) )
					{
						if ( ( ( A & ( 1 << 4 ) ) == 0 ) )
							ym3812_write( pOPL, 0, D ); else
							ym3812_write( pOPL, 1, D );
					} else
					#endif
					//#if !defined(SID2_DISABLED) && !defined(SID2_PLAY_SAME_AS_SID1)
					// TODO: generic masks
					if ( !cfgSID2_Disabled && !cfgSID2_PlaySameAsSID1 && (ringBufGPIO[ ringRead ] & SID2_MASK) )
					{
						sid[ 1 ]->write( A & 31, D );
					} else
					//#endif
					{
						sid[ 0 ]->write( A & 31, D );
						//outRegisters[ A & 31 ] = D;
						//#if !defined(SID2_DISABLED) && defined(SID2_PLAY_SAME_AS_SID1)
						if ( !cfgSID2_Disabled && cfgSID2_PlaySameAsSID1 )
							sid[ 1 ]->write( A & 31, D );
						//#endif
					}

					ringRead++;
					ringRead &= ( RING_SIZE - 1 );
				}


		#ifdef SMPCARRY
			}
		#else
				samplesElapsed = ( ( unsigned long long )nCyclesEmulated * ( unsigned long long )SAMPLERATE ) / ( unsigned long long )CLOCKFREQ;

				if ( nCyclesEmulated >= cycleCount && samplesElapsed == samplesElapsedBefore )
					goto NoSampleGeneratedYet;

			} while ( samplesElapsed == samplesElapsedBefore );
		#endif
			CACHE_PRELOADL2STRMW( &sampleBuffer[ smpCur ] );
			val1 = sid[ 0 ]->output();
			val2 = 0;
			valOPL = 0;

		#ifndef SID2_DISABLED
			if ( !cfgSID2_Disabled )
				val2 = sid[ 1 ]->output();
		#endif

		#ifdef EMULATE_OPL2
			if ( cfgEmulateOPL2 )
			{
				ym3812_update_one( pOPL, &valOPL, 1 );
				// TODO asynchronous read back is an issue, needs to be fixed
				fmOutRegister = encodeGPIO( ym3812_read( pOPL, 0 ) ); 
			}
		#endif

			//
			// mixer
			//
			s32 left, right;
			
			// yes, it's 1 byte shifted in the buffer, need to fix
			right = ( val1 * cfgVolSID1_Left  + val2 * cfgVolSID2_Left  + valOPL * cfgVolOPL_Left ) >> 8;
			left  = ( val1 * cfgVolSID1_Right + val2 * cfgVolSID2_Right + valOPL * cfgVolOPL_Right ) >> 8;

			right = max( -31767, min( 31767, right ) );
			left  = max( -31767, min( 31767, left ) );

			#ifdef USE_PWM_DIRECT
			putSample( left, right );
			#else
			putSample( left );
			putSample( right );
			#endif

		#if 1
			// vu meter
			static u32 vu_nValues = 0;
			static float vu_Sum[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
			
			//if ( vu_Mode != 2 )
			{
				float t = (left+right) / (float)32768.0f * 0.5f;
				vu_Sum[ 0 ] += t * t * 1.0f;

				vu_Sum[ 1 ] += val1 * val1 / (float)32768.0f / (float)32768.0f;
				vu_Sum[ 2 ] += val2 * val2 / (float)32768.0f / (float)32768.0f;
				vu_Sum[ 3 ] += valOPL * valOPL / (float)32768.0f / (float)32768.0f;

				if ( ++ vu_nValues == 256*2 )
				{
					for ( u32 i = 0; i < 4; i++ )
					{
						float vu_Volume = max( 0.0f, 2.0f * (log10( 0.1f + sqrt( (float)vu_Sum[ i ] / (float)vu_nValues ) ) + 1.0f) );
						u32 v = vu_Volume * 1024.0f;
						if ( i == 0 )
						{
							// moving average
							float v = min( 1.0f, (float)vuMeter[ 0 ] / 1024.0f );
							static float led4Avg = 0.0f;
							led4Avg = led4Avg * 0.8f + v * ( 1.0f - 0.8f );

							vu_nLEDs = max( 0, min( 4, (led4Avg * 8.0f) ) );
							if ( vu_nLEDs > 4 ) vu_nLEDs = 4;
						}
						vuMeter[ i ] = v;
						vu_Sum[ i ] = 0;
					}

					vu_nValues = 0;
				}
			}

			#ifdef COMPILE_MENU
			if ( screenType == 0 )
			{
				#include "oscilloscope_hack.h"
			} else
			if ( screenType == 1 )
			{
				const float scaleVis = 1.0f;
				const u32 nLevelMeters = 3;
				#include "tft_sid_vis.h"
			} 
			#endif
		#endif
		NoSampleGeneratedYet:;
		}
	#endif
	}

	m_InputPin.DisableInterrupt();
}


//static u32 releaseDMA = 0;

#ifdef COMPILE_MENU
void KernelSIDLaunchFIQHandler( void *pParam )
{
	if ( !disableCart )
	{
		register u32 D;

		// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
		START_AND_READ_ADDR0to7_RW_RESET_CS

		// update some counters
		UPDATE_COUNTERS_MIN( cycleCountC64, resetCounter )

		// read the rest of the signals
		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

		LAUNCH_FIQ( resetCounter )
	}
}

void KernelSIDFIQHandler( void *pParam )
#else
void CKernel::FIQHandler (void *pParam)
#endif
{
	register u32 D;

	#ifdef COMPILE_MENU
	if ( launchPrg && !disableCart )
	{
		register u32 D;

		CACHE_PRELOADL2STRM( &prgData[ currentOfs ] );

		// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
		START_AND_READ_ADDR0to7_RW_RESET_CS

		// update some counters
		UPDATE_COUNTERS_MIN( cycleCountC64, resetCounter )

		// read the rest of the signals
		WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

		LAUNCH_FIQ( resetCounter )
	}
	#endif
	static s32 latchDelayOut = 10;

	START_AND_READ_ADDR0to7_RW_RESET_CS

	if ( CPU_RESET && !resetReleased ) {
		resetPressed = 1; resetCounter ++;
	} else {
		if ( resetPressed && resetCounter > 100 )	
		{
			resetReleased = 1;
			disableCart = transferStarted = 0;
			SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
			FINISH_BUS_HANDLING
			return;
		}
		resetPressed = 0;
	}

	static u32 fCount = 0;
	fCount ++;
	fCount &= 255;

	cycleCountC64 ++;

/*	if ( haltC64 )
	{
		haltC64 = 0;
		WAIT_UP_TO_CYCLE( WAIT_TRIGGER_DMA ); 
		CLR_GPIO( bDMA ); 
	} 
	if ( letgoC64 )
	{
		letgoC64 = 0;
		WAIT_UP_TO_CYCLE( WAIT_RELEASE_DMA ); 
		SET_GPIO( bDMA ); 
	}

	if ( SID_not_initialized )
	{
		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
		return;
	}*/

	#ifdef COMPILE_MENU
	// preload cache
	if ( !( launchPrg && !disableCart ) )
	{
		CACHE_PRELOADL1STRMW( &ringWrite );
		CACHE_PRELOADL1STRM( &sampleBuffer[ smpLast ] );
		//CACHE_PRELOADL1STRM( &outRegisters[ 0 ] );
		CACHE_PRELOADL1STRM( &outRegisters[ 16 ] );
	}
	#endif

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	#ifdef COMPILE_MENU
/*	if ( resetCounter > 3  )
	{
		disableCart = transferStarted = 0;
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
		FINISH_BUS_HANDLING
		return;
	}*/
	#endif

	if ( busValueTTL < 0 )
	{
		busValue = 0;  
	} else
		busValueTTL --;

	//  __   ___       __      __     __  
	// |__) |__   /\  |  \    /__` | |  \ 
	// |  \ |___ /~~\ |__/    .__/ | |__/ 
	//
	if ( cfgRegisterRead && CPU_READS_FROM_BUS && SID_ACCESS )
	{
		u32 A = ( g2 >> A0 );
		u32 D;// = outRegisters[ A ];

		A |= ( (GET_ADDRESS8to12) & 1 ) << 8;

		if ( ( cfgSID2_Addr == 0 && (A & 0x20) ) ||
			 ( cfgSID2_Addr == 1 && (A & 0x100) ) )
		{
			A &= 31;
			if ( sidAutoDetectStep_2 == 1 && A == 0x1b )
			{
				sidAutoDetectStep_2 = 0;
				if ( SID_MODEL[ 1 ] == 8580 )
					D = 2; else
					D = 3;
			} else
			{
				if ( A >= 0x19 && A <= 0x1c )
					D = 0; else //outRegisters_2[ A & 31 ]; else
					D = busValue;
			}
		} else
		{
			A &= 31;
			if ( sidAutoDetectStep == 1 && A == 0x1b )
			{
				sidAutoDetectStep = 0;
				if ( SID_MODEL[ 0 ] == 8580 )
					D = 2; else
					D = 3;
			} else
			{
				if ( A >= 0x19 && A <= 0x1c )
					D = outRegisters[ A ]; else
					D = busValue;
			}
		}

		WRITE_D0to7_TO_BUS( D )

		FINISH_BUS_HANDLING
		return;
	} else
	//  __   ___       __      ___       
	// |__) |__   /\  |  \    |__   |\/| 
	// |  \ |___ /~~\ |__/    |     |  | 
	//                                   
	#ifdef EMULATE_OPL2
	if ( cfgEmulateOPL2 || _playingPSID )
	{
		if ( _playingPSID )
		{
			if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x55 )
			{
				static u32 oc = 0;
				extern unsigned char charset[ 4096 ];
				u32 D = charset[ 2048 + oc ];
				oc ++; oc &= 1023;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2STRM( &charset[ 2048 + oc ] );
				OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
				return;
			}
			if ( resetFromCodeState == 0 && IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x11 )
			{
				READ_D0to7_FROM_BUS( D )
				if ( D == 0x22 )
					resetFromCodeState = 1;
				OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
				return;
			}
			if ( resetFromCodeState == 1 && IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x33 )
			{
				READ_D0to7_FROM_BUS( D )
				if ( D == 0x44 )
				{
					resetFromCodeState = 2;
					latchSetClear( 0, LATCH_RESET );
				}
				OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
				return;
			}
		}

		if ( ( CPU_READS_FROM_BUS && IO2_ACCESS ) && ( GET_IO12_ADDRESS == 0x60 ) )
		{
			//
			// this is not a real read of the YM3812 status register!
			// only a fake that let's the detection routine be satisfied
			//
			u32 D = fmFakeOutput; 
			fmFakeOutput = 0xc0 - fmFakeOutput;

			WRITE_D0to7_TO_BUS( D )

			FINISH_BUS_HANDLING
			return;
		} else
		// reading of the external Sound Expander Keyboard => we don't have it, return 0xFF
		if ( ( CPU_READS_FROM_BUS && IO2_ACCESS ) && ( GET_IO12_ADDRESS >= 0x08 ) && ( GET_IO12_ADDRESS <= 0x0F ) )
		{
			WRITE_D0to7_TO_BUS( 0xFF )

			FINISH_BUS_HANDLING
			return;
		} else
		//       __    ___  ___     ___       
		// |  | |__) |  |  |__     |__   |\/| 
		// |/\| |  \ |  |  |___    |     |  | 
		//                                    
		if ( CPU_WRITES_TO_BUS && IO2_ACCESS ) 
		{
			READ_D0to7_FROM_BUS( D )

			// this is mimicing some behaviour of the YM let it be detected
			u32 A = ( ( g2 & A_FLAG ) >> A0 ) & ( 1 << 4 ); // A == 0 -> address register, otherwise data register
			
			if ( A == 0 && D == 0x04 )
				fmAutoDetectStep = 1;
			if ( A > 0 && D == 0x60 && fmAutoDetectStep == 1 )
				fmAutoDetectStep = 2;
			if ( A == 0 && D == 0x04 && fmAutoDetectStep == 2 )
				fmAutoDetectStep = 3;
			if ( A > 0 && D == 0x80 && fmAutoDetectStep == 3 )
			{
				fmAutoDetectStep = 4;
				fmFakeOutput = 0;
			}
				
			ringBufGPIO[ ringWrite ] = ( g2 & A_FLAG ) | ( D << D0 ) | bIO2;
			ringTime[ ringWrite ] = cycleCountC64;
			ringWrite ++;
			ringWrite &= ( RING_SIZE - 1 );

			FINISH_BUS_HANDLING
			return;
		} 
	} 
	#endif // EMULATE_OPL2
	//       __    ___  ___     __     __  
	// |  | |__) |  |  |__     /__` | |  \ 
	// |/\| |  \ |  |  |___    .__/ | |__/ 
	//                                   
	if ( CPU_WRITES_TO_BUS && SID_ACCESS ) 
	{
		READ_D0to7_FROM_BUS( D )

		register u32 A = GET_ADDRESS0to7;
		register u32 remapAddr = (A&31) << A0;

		A |= ( (GET_ADDRESS8to12) & 1 ) << 8;

		if ( ( cfgSID2_Addr == 0 && (A & 0x20) ) ||
			 ( cfgSID2_Addr == 1 && (A & 0x100) ) )
		{
			remapAddr |= SID2_MASK;
			if ( sidAutoDetectStep_2 == 0 &&
				 sidAutoDetectRegs_2[ 0x12 ] == 0xff &&
				 sidAutoDetectRegs_2[ 0x0e ] == 0xff &&
				 sidAutoDetectRegs_2[ 0x0f ] == 0xff &&
				 (A&31) == 0x12 && D == 0x20 )
			{
				sidAutoDetectStep_2 = 1;
			}
			sidAutoDetectRegs_2[ A & 31 ] = D;
		} else
		{
			if ( sidAutoDetectStep == 0 &&
				 sidAutoDetectRegs[ 0x12 ] == 0xff &&
				 sidAutoDetectRegs[ 0x0e ] == 0xff &&
				 sidAutoDetectRegs[ 0x0f ] == 0xff &&
				 (A&31) == 0x12 && D == 0x20 )
			{
				sidAutoDetectStep = 1;
			}
			sidAutoDetectRegs[ A & 31 ] = D;
		}


		busValue = D;
		if ( SID_MODEL[ 0 ] == 8580 )
			busValueTTL = 0xa2000; else // 8580
			busValueTTL = 0x1d00; // 6581

		ringBufGPIO[ ringWrite ] = ( remapAddr | ( D << D0 ) ) & ~bIO2;
		ringTime[ ringWrite ] = cycleCountC64;
		ringWrite ++;
		ringWrite &= ( RING_SIZE - 1 );
		
		FINISH_BUS_HANDLING
		return;
	}
	if ( CPU_WRITES_TO_BUS && IO1_ACCESS && cfgSID2_Addr == 2 ) 
	{
		READ_D0to7_FROM_BUS( D )

		register u32 A = GET_ADDRESS0to7;
		register u32 remapAddr = ( (A&31) << A0 ) | SID2_MASK;

		ringBufGPIO[ ringWrite ] = ( remapAddr | ( D << D0 ) ) & ~bIO2;
		ringTime[ ringWrite ] = cycleCountC64;
		ringWrite ++;
		ringWrite &= ( RING_SIZE - 1 );

		FINISH_BUS_HANDLING
		return;
	}

	//  ___                      ___    __                     ___    __  
	// |__   |\/| |  | |     /\   |  | /  \ |\ |    | |\ |    |__  | /  \ 
	// |___  |  | \__/ |___ /~~\  |  | \__/ | \|    | | \|    |    | \__X 
	// OPTIONAL and omitted for this release
	//																	
	#ifdef EMULATION_IN_FIQ
	run_emulation:
	#include "fragment_emulation_in_fiq.h"
	#endif		

	//  __                 __       ___  __       ___ 
	// |__) |  |  |\/|    /  \ |  |  |  |__) |  |  |  
	// |    |/\|  |  |    \__/ \__/  |  |    \__/  |  
	// OPTIONAL
	//											
	#ifdef USE_PWM_DIRECT
	static unsigned long long samplesElapsedBeforeFIQ = 0;

	unsigned long long samplesElapsedFIQ = ( ( unsigned long long )cycleCountC64 * ( unsigned long long )SAMPLERATE ) / ( unsigned long long )CLOCKFREQ;

	if ( samplesElapsedFIQ != samplesElapsedBeforeFIQ && !CPU_RESET )
	{
		write32( ARM_GPIO_GPCLR0, bCTRL257 ); 
		samplesElapsedBeforeFIQ = samplesElapsedFIQ;

		u32 s = getSample();
		u16 s1 = s & 65535;
		u16 s2 = s >> 16;

		s32 d1 = (s32)( ( *(s16*)&s1 + 32768 ) * PWMRange ) >> 17;
		s32 d2 = (s32)( ( *(s16*)&s2 + 32768 ) * PWMRange ) >> 17;
		/*if ( d1 < 0 ) d1 = 0;
		if ( d1 >= (s32)PWMRange ) d1 = (s32)PWMRange - 1;
		if ( d2 < 0 ) d2 = 0;
		if ( d2 >= (s32)PWMRange ) d2 = (s32)PWMRange - 1;*/
		write32( ARM_PWM_DAT1, d1 );
		write32( ARM_PWM_DAT2, d2 );
		RESET_CPU_CYCLE_COUNTER
		return;
	} 
	#endif

	//           ___  __       
	// |     /\   |  /  ` |__| 
	// |___ /~~\  |  \__, |  | 
	//
	#ifdef COMPILE_MENU
	#ifdef USE_LATCH_OUTPUT
	if ( screenType == 0 )
	{
		if ( --latchDelayOut == 1 && renderDone == 3 )
		{
			prefetchI2C();
		}
		if ( latchDelayOut <= 0 && renderDone == 3 )
		{
			latchDelayOut = 2;
			prepareOutputLatch();
			if ( bufferEmptyI2C() ) renderDone = 0;
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;

		}
	}
	#endif

	u32 swizzle = 
		( (fCount & 128) >> 7 ) |
		( (fCount & 64) >> 5 ) |
		( (fCount & 32) >> 3 ) |
		( (fCount & 16) >> 1 ) |
		( (fCount & 8) << 1 ) |
		( (fCount & 4) << 3 ) |
		( (fCount & 2) << 5 ) |
		( (fCount & 1) << 7 );
	#endif

	static u32 lastButtonPressed = 0;

	if ( lastButtonPressed > 0 )
		lastButtonPressed --;

	static u32 buttonIsPressed = 0;

	if ( BUTTON_PRESSED && lastButtonPressed == 0 )
		buttonIsPressed ++; else
		buttonIsPressed = 0;

	if ( buttonIsPressed > 50 )
	{
		visModeGotoNext = 1;
		vu_Mode = ( vu_Mode + 1 ) & 3;
		lastButtonPressed = 100000;
	}

	#ifdef COMPILE_MENU
	if ( screenType == 0 )
	{
		if ( vu_nLEDs != 0xffff )
		{
			if ( vu_Mode == 0 )
			{
				setLatchFIQ( LATCH_ON[ vu_nLEDs ] );
				clrLatchFIQ( LATCH_OFF[ vu_nLEDs ] );
			} else
			if ( vu_Mode == 1 )
			{
				if ( swizzle < vuMeter[ 0 ] )
					setLatchFIQ( LATCH_LED_ALL ); else
					clrLatchFIQ( LATCH_LED_ALL );		
			} else
			if ( vu_Mode == 2 )
			{
				u32 led =
					( ( swizzle < vuMeter[ 1 ] ) ? LATCH_LED1 : 0 ) |
					( ( swizzle < vuMeter[ 2 ] ) ? LATCH_LED2 : 0 ) |
					( ( swizzle < vuMeter[ 3 ] ) ? LATCH_LED3 : 0 );

				setLatchFIQ( led );
				clrLatchFIQ( ( (~led) & LATCH_LED_ALL ) | LATCH_LED0 );		
			} else
				clrLatchFIQ( LATCH_LED_ALL );		
		}
	} else
	{
		prepareOutputLatch4Bit();
		outputLatch();
	}
	#endif
	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}

#ifndef COMPILE_MENU
int main( void )
{
	CKernel kernel;
	if ( kernel.Initialize() )
		kernel.Run();

	halt();
	return EXIT_HALT;
}
#endif