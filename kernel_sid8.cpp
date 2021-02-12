/*
  _________.__    .___      __   .__        __          _________.___________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     /   _____/|   \______ \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     \_____  \ |   ||    |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /        \|   ||    `   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    /_______  /|___/_______  /
        \/         \/    \/     \/       \/     \/            \/             \/ 
 
 kernel_sid8.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick SID: a SID and SFX Sound Expander Emulation / SID-8 is a lazy modification to emulate 8 SIDs at once
		    (using reSID by Dag Lem)
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
#include "kernel_sid8.h"
#ifdef COMPILE_MENU
#include "kernel_menu.h"
#include "launch.h"

static u32 launchPrg;
#endif

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

static u32 CLOCKFREQ = 985248;	// exact clock frequency of the C64 will be measured at start up

// SID types and digi boost (only for MOS8580)
static unsigned int SID_MODEL[8] = { 8580, 8580, 8580, 8580, 8580, 8580, 8580, 8580 };
static unsigned int SID_DigiBoost[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// do not change this value
#define NUM_SIDS 8
static SID *sid[ NUM_SIDS ];

#ifdef EMULATE_OPL2
FM_OPL *pOPL;
u32 fmOutRegister;
#endif

// a ring buffer storing SID-register writes (filled in FIQ handler)
// TODO should be much smaller
#define RING_SIZE (1024*128)
static u32 ringBufGPIO[ RING_SIZE ];
static unsigned long long ringTime[ RING_SIZE ];
static u32 ringWrite;

// prepared GPIO output when SID-registers are read
static u32 outRegisters[ 32 ];

// counts the #cycles when the C64-reset line is pulled down (to detect a reset)
static u32 resetCounter,
		   resetPressed, resetReleased;

// this is the actual configuration of the emulation
extern u32 cfgRegisterRead;			// don't use this when you have a SID(-replacement) in the C64
extern u32 cfgEmulateOPL2;
extern u32 cfgSID2_Disabled;
extern u32 cfgSID2_PlaySameAsSID1;
extern u32 cfgMixStereo;
extern u32 cfgSID2_Addr;
extern s32 cfgVolSID1_Left, cfgVolSID1_Right;
extern s32 cfgVolSID2_Left, cfgVolSID2_Right;
extern s32 cfgVolOPL_Left, cfgVolOPL_Right;

extern u32 outputPWM, outputHDMI;

unsigned int SAMPLERATE = 44100;

static int startVCHIQ = 0;

//  __     __                __      ___                   ___ 
// /__` | |  \     /\  |\ | |  \    |__   |\/|    | |\ | |  |  
// .__/ | |__/    /~~\ | \| |__/    |     |  |    | | \| |  |  
//                                                            
void initSID8()
{
	resetCounter = 0;

	for ( int i = 0; i < NUM_SIDS; i++ )
	{
		sid[ i ] = new SID;

		for ( int j = 0; j < 25; j++ )
			sid[ i ]->write( j, 0 );

		// no mistake, take the model of the first for all 8
		if ( (SID_MODEL[ 0 ] % 3) == 6581 )
		{
			sid[ i ]->set_chip_model( MOS6581 );
		} else
		{
			sid[ i ]->set_chip_model( MOS8580 );
			if ( SID_DigiBoost[ 0 ] == 0 )
			{
				sid[ i ]->set_voice_mask( 0x07 );
				sid[ i ]->input( 0 );
			} else
			{
				sid[ i ]->set_voice_mask( 0x0f );
				sid[ i ]->input( -32768 );
			}
		}
	}

	// ring buffer init
	ringWrite = 0;
	for ( int i = 0; i < RING_SIZE; i++ )
		ringTime[ i ] = 0;
}

static unsigned long long cycleCountC64;


#ifdef COMPILE_MENU
extern CLogger			*logger;
extern CTimer			*pTimer;
extern CScheduler		*pScheduler;
extern CInterruptSystem	*pInterrupt;
extern CVCHIQDevice		*pVCHIQ;
extern CScreenDevice	*screen;
static CSoundBaseDevice	*m_pSound;
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

void quitSID8()
{
	if ( outputHDMI && m_pSound != NULL )
	{
		CVCHIQSoundBaseDevice *sd = (CVCHIQSoundBaseDevice*)m_pSound;
		if ( sd->IsActive() )
			sd->SetControl( VCHIQ_SOUND_VOLUME_MIN, VCHIQSoundDestinationAuto );
		while ( sd->IsActive() )
			sd->Cancel();
		delete sd;
		m_pSound = NULL;
	}
	for ( int i = 0; i < NUM_SIDS; i++ )
		delete sid[ i ];
}

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

static unsigned long long nCyclesEmulated = 0;
static unsigned long long samplesElapsed = 0;

static void prepareOnReset( bool refresh = false )
{
	if ( launchPrg )
		{disableCart = 0; SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );} else
		{SETCLR_GPIO( bNMI | bDMA | bGAME | bEXROM, 0 );}

	if ( !refresh )
	{
		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();
	}

	if ( launchPrg )
		launchPrepareAndWarmCache();
		
	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 4*1024 );
	FORCE_READ_LINEAR32a( (void*)&FIQ_HANDLER, 4*1024, 32768 );

	resetCounter = cycleCountC64 = nCyclesEmulated = samplesElapsed = 0;
}

extern u32 SAMPLERATE_ADJUSTED;
extern u32 nSamplesPrecompute;
extern u32 trackSampleProgress;

static s32 avgSamplesAvail = 0;
static s32 avgCounter = 0;
static u32 adjustRateAllowed = 0;
static u32 targetSamplesAvail = 0, targetCount = 0;

extern u32 fillSoundBuffer;
extern bool CVCHIQ_CB_Manual;


#ifdef COMPILE_MENU
void KernelSIDFIQHandler8( void *pParam );

void KernelSIDRun8( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0 )
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

	if ( screenType == 0 )
		allUsedLEDs = LATCH_LED_ALL;
	if ( screenType == 1 )
		allUsedLEDs = LATCH_LED0to1;
	
	latchSetClearImm( 0, LATCH_RESET | allUsedLEDs | LATCH_ENABLE_KERNAL );

	SETCLR_GPIO( bNMI | bDMA | bGAME | bEXROM, 0 );

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

		//u32 c1 = rgb24to16( 166, 250, 128 );
		//u32 c2 = rgb24to16( 98, 216, 204 );
		u32 c3 = rgb24to16( 233, 114, 36 );

		char b1[64];
		int charWidth = 16;

		sprintf( b1, "8 TUNEFUL %sS", SID_MODEL[ 0 ] == 6581 ? "6581":"8580" );

		int l = strlen( b1 );
		if ( l * 16 >= 240 )
			charWidth = 13;
		int sx = max( 0, ( 240 - charWidth * l ) / 2 - 2 );
		tftPrint( b1, sx, 224, c3, charWidth == 16 ? 0 : -3 );	

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

		//logger->Write( "", LogNotice, "initialize SIDs..." );
	initSID8();

	//
	// initialize sound output (either PWM which is output in the FIQ handler, or via HDMI)
	//
	startVCHIQ = 0;
	initSoundOutput( &m_pSound, pVCHIQ, outputPWM, outputHDMI );

	#ifdef COMPILE_MENU
	if ( FILENAME == NULL && !hasData )
	{
		launchPrg = 0;
		disableCart = 1;
	} else
	{
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

	#ifdef COMPILE_MENU
	prepareOnReset();
	m_InputPin.ConnectInterrupt( KernelSIDFIQHandler8, kernelMenu );
	#else
	m_InputPin.ConnectInterrupt( this->FIQHandler, this );
	#endif
	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

#ifndef COMPILE_MENU

	latchSetClearImm( LATCH_RESET, allUsedLEDs | LATCH_ENABLE_KERNAL );

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

	for ( int i = 0; i < NUM_SIDS; i++ )
		sid[ i ]->set_sampling_parameters( CLOCKFREQ, SAMPLE_INTERPOLATE, SAMPLERATE );

	//logger->Write( "", LogNotice, "start emulating..." );
	cycleCountC64 = 0;
	nCyclesEmulated = 0;
	samplesElapsed = 0;

	// how far did we consume the commands in the ring buffer?
	unsigned int ringRead = 0;

	#ifdef COMPILE_MENU
	prepareOnReset( true );

	latchSetClear( LATCH_RESET, allUsedLEDs | LATCH_ENABLE_KERNAL );
	resetCounter = 0;

startHereAfterReset:
	if ( launchPrg )
	{
		while ( !disableCart )
		{
			#ifdef COMPILE_MENU
			TEST_FOR_JUMP_TO_MAINMENU( cycleCountC64, resetCounter )
			#endif
			asm volatile ("wfi");
			if ( cycleCountC64 > 2000000 )
			{
				cycleCountC64 = 0;
				latchSetClear( 0, LATCH_RESET );
				DELAY(1<<20);
				latchSetClear( LATCH_RESET, 0 );
			}
		}
	} 
	#endif

	resetReleased = 0;
	resetCounter = cycleCountC64 = 0;
	nCyclesEmulated = 0;
	samplesElapsed = 0;
	ringRead = ringWrite = 0;

	latchSetClear( 0, allUsedLEDs );

	static u32 hdmiVol = 1;

	#ifdef USE_VCHIQ_SOUND
	static u32 nSamplesInThisRun = 0;
	#endif

	fillSoundBuffer = 0;

	// new main loop mainloop
	while ( true )
	{
		#ifdef COMPILE_MENU
		//TEST_FOR_JUMP_TO_MAINMENU( cycleCountC64, resetCounter )
		if ( cycleCountC64 > 2000000 && resetCounter > 0 && outputHDMI && m_pSound && hdmiVol == 1 ) {
			CVCHIQ_CB_Manual = false;
			if ( outputHDMI )
			{
				CVCHIQSoundBaseDevice *sd = (CVCHIQSoundBaseDevice*)m_pSound;
				if ( sd->IsActive() )
					sd->SetControl( VCHIQ_SOUND_VOLUME_MIN, VCHIQSoundDestinationAuto );
				sd->Cancel();
			}
			hdmiVol = 0;
		}
		if ( cycleCountC64 > 2000000 && resetCounter > 500000 ) {
			CVCHIQ_CB_Manual = false;
			logger->Write( "", LogNotice, "adjusted sample rate: %u Hz", (u32)SAMPLERATE_ADJUSTED );
			quitSID8();
			EnableIRQs();
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			return;
		}
		#endif

		if ( resetReleased == 1 )
		{
			if ( m_pSound )
			{
				if ( outputHDMI )
				{
					CVCHIQSoundBaseDevice *sd = (CVCHIQSoundBaseDevice*)m_pSound;
					while ( sd->IsActive() ) {}
					//sd->SetControl( VCHIQ_SOUND_VOLUME_DEFAULT, VCHIQSoundDestinationAuto );
					hdmiVol = 1;
				}
				nSamplesInThisRun = startVCHIQ = 0;
				initSoundOutput( &m_pSound, pVCHIQ, outputPWM, outputHDMI );
				avgSamplesAvail = 0;
				avgCounter = 0;
				adjustRateAllowed = 0;
				targetSamplesAvail = 0;
				targetCount = 0;
			}

			resetReleased = 0xff;
			resetCounter = 0;

			for ( int i = 0; i < NUM_SIDS; i++ )
				for ( int j = 0; j < 25; j++ )
					sid[ i ]->write( j, 0 );


			prepareOnReset( true );
			latchSetClear( allUsedLEDs, LATCH_RESET );
			DELAY(1<<10);
			latchSetClear( LATCH_RESET, allUsedLEDs );
			resetCounter = resetReleased = resetPressed = 0;
			goto startHereAfterReset;
		}

	#ifdef USE_OLED
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

	#endif

	#ifndef EMULATION_IN_FIQ

		unsigned long long cycleCount = cycleCountC64;
		while ( cycleCount > nCyclesEmulated )
		{
			CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 6*1024 );
		#ifdef USE_VCHIQ_SOUND
			if ( outputHDMI )
			{
				//extern u32 CVCHIQ_CB_Last,  CVCHIQ_CB_Cur;
				extern CVCHIQSoundBaseDevice *CVCHIQ_CB_Device;
				extern VCHI_CALLBACK_REASON_T CVCHIQ_CB_Reason;
				extern void *CVCHIQ_CB_hMessage;
				if ( CVCHIQ_CB_Device )
				{
					CVCHIQ_CB_Device->Callback (CVCHIQ_CB_Reason, CVCHIQ_CB_hMessage);
					CVCHIQ_CB_Device = NULL;
				}

				if ( samplesElapsed > 8 * nSamplesPrecompute / 2 && !startVCHIQ )
				{
					m_pSound->Start();
					fillSoundBuffer = 1;
					startVCHIQ = 1;
				}
				//s32 nFramesMax = m_pSound->GetQueueSizeFrames() - m_pSound->GetQueueFramesAvail();
				//if ( startVCHIQ && nFramesMax > m_pSound->GetQueueSizeFrames() / 2 )
				if ( fillSoundBuffer )
				{
					fillSoundBuffer = 0;

					extern u32 samplesInBuffer();
					#define TYPE		s16
					#define TYPE_SIZE	sizeof (s16)

					s32 nFramesComputed = samplesInBuffer();
					s32 nFramesMax = m_pSound->GetQueueSizeFrames() - m_pSound->GetQueueFramesAvail();
					trackSampleProgress = 1024 * 1024 + ( nFramesComputed - nFramesMax );

					s32 nWriteFrames = min( nFramesComputed, nFramesMax );

				#if 1
					if ( nWriteFrames * 2 + PCMCountLast > PCMBufferSize )
					{
						m_pSound->Write( &PCMBuffer[ PCMCountLast ], (PCMBufferSize - PCMCountLast) * TYPE_SIZE );
						m_pSound->Write( &PCMBuffer[ 0 ], (2 * nWriteFrames - (PCMBufferSize - PCMCountLast)) * TYPE_SIZE );
					} else
					{
						m_pSound->Write( &PCMBuffer[ PCMCountLast ], 2 * nWriteFrames * TYPE_SIZE );
					}
					PCMCountLast += nWriteFrames * 2;
					PCMCountLast %= PCMBufferSize;
				#else
					short temp[ 16384 ];
					short *p = (short*)temp;
					for ( u32 j = 0; j < nWriteFrames; j++ )
					{
						*(p++) = PCMBuffer[ PCMCountLast++ ];
						*(p++) = PCMBuffer[ PCMCountLast++ ];
						PCMCountLast %= PCMBufferSize;
					}

					unsigned nWriteBytes = 2 * nWriteFrames * TYPE_SIZE;
					m_pSound->Write( &temp[ 0 ], nWriteBytes );
				#endif
				}
				if ( nSamplesInThisRun > 2205 / 8 )
				{
					//trackSampleProgress = 1024 * 1024 + ( nFramesComputed - nFramesMax );
					if ( trackSampleProgress )
					{
						avgSamplesAvail += (s32)trackSampleProgress - 1024 * 1024;
						avgCounter ++;

						if ( avgCounter > 50 )
						{
							s32 avail = (s32)avgSamplesAvail / avgCounter;
							if ( targetSamplesAvail == 0 && ++ targetCount > 2 )
								CVCHIQ_CB_Manual = true; 
							if ( targetSamplesAvail == 0 && ++ targetCount > 10 )
								targetSamplesAvail = max( avail, (s32)nSamplesPrecompute );

							if ( targetSamplesAvail != 0 )
							{
								if ( avail < (s32)targetSamplesAvail )
								{
									if ( avail < 5 * (s32)targetSamplesAvail / 100 && SAMPLERATE_ADJUSTED < SAMPLERATE )
										SAMPLERATE_ADJUSTED = SAMPLERATE; else
										SAMPLERATE_ADJUSTED ++; 
								} else
								if ( avail > (s32)targetSamplesAvail )
								{
									if ( avail > 105 * (s32)targetSamplesAvail / 100 && SAMPLERATE_ADJUSTED > SAMPLERATE )
										SAMPLERATE_ADJUSTED = SAMPLERATE; else
										SAMPLERATE_ADJUSTED --; 
								}
								adjustRateAllowed = 0;
							}
							//logger->Write( "", LogNotice, "samples ahead: %d (target: %d), rate: %u Hz", avail, targetSamplesAvail, SAMPLERATE_ADJUSTED );
							avgSamplesAvail = avgCounter = 0;
						}
						trackSampleProgress = 0;
					}

					{
						if ( nCyclesEmulated < (4*256000) )
						{
							CVCHIQSoundBaseDevice *sd = (CVCHIQSoundBaseDevice*)m_pSound;
							if ( sd->IsActive() )
								sd->SetControl( VCHIQ_SOUND_VOLUME_MIN + (VCHIQ_SOUND_VOLUME_DEFAULT - VCHIQ_SOUND_VOLUME_MIN) * nCyclesEmulated / (4*256000), VCHIQSoundDestinationAuto );
							hdmiVol = 1;
						}
						//pScheduler->MsSleep( 1 );
						pScheduler->Yield();
					}
					nSamplesInThisRun = 0;
				}
				nSamplesInThisRun++;
			}
		#endif

			CACHE_PRELOADL2STRMW( &smpCur );

			static u32 carrySamples = 0;
			u32 samplesToEmulateX65536 = ( ( unsigned long long )65536 * ( unsigned long long )CLOCKFREQ ) / ( unsigned long long )SAMPLERATE_ADJUSTED + ( unsigned long long )carrySamples;

			u32 samplesToEmulate = samplesToEmulateX65536 >> 16;
			carrySamples = (samplesToEmulateX65536 & 65535);

			{
				u32 cyclesToEmulate = samplesToEmulate;

				for ( u32 i = 0; i < NUM_SIDS; i++ )
					sid[ i ]->clock( cyclesToEmulate );

				outRegisters[ 27 ] = sid[ 0 ]->read( 27 );
				outRegisters[ 28 ] = sid[ 0 ]->read( 28 );

				nCyclesEmulated += cyclesToEmulate;

				// apply register updates (we do one-cycle emulation steps, but in case we need to catch up...)
				unsigned int readUpTo = ringWrite;

				if ( ringRead != readUpTo && nCyclesEmulated >= ringTime[ ringRead ] )
				{
					unsigned char A, D;

					u32 rv = ringBufGPIO[ ringRead ];
					D = rv & 255;
					A = (rv>>8)&31;
					u32 whichSID = rv >> 16;
	
					sid[ whichSID ]->write( A, D );

					ringRead++;
					ringRead &= ( RING_SIZE - 1 );
				}

			}

			samplesElapsed = ( ( unsigned long long )nCyclesEmulated * ( unsigned long long )SAMPLERATE_ADJUSTED ) / ( unsigned long long )CLOCKFREQ;

			//
			// mixer
			//

			CACHE_PRELOADL2STRMW( &sampleBuffer[ smpCur ] );
			s32 left = 0, right = 0;
			
			// yes, it's 1 byte shifted in the buffer, need to fix
			s32 l1, l2, l3, l4, r1, r2, r3, r4;
			l1 = sid[1]->output();
			l2 = sid[3]->output();
			l3 = sid[5]->output();
			l4 = sid[7]->output();
			r1 = sid[0]->output();
			r2 = sid[2]->output();
			r3 = sid[4]->output();
			r4 = sid[6]->output();

			left = ( l1 + l2 + l3 + l4 ) >> 1;
			right = ( r1 + r2 + r3 + r4 ) >> 1;

			right = max( -32767, min( 32767, right ) );
			left  = max( -32767, min( 32767, left ) );

			#ifdef USE_PWM_DIRECT
			if ( outputPWM )
				putSample( left, right );
			#endif
			#ifdef USE_VCHIQ_SOUND
			if ( outputHDMI )
			{
				putSample( left );
				putSample( right );
			}
			#endif

		#if 1
			// vu meter
			static u32 vu_nValues = 0;
			static float vu_Sum[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
			
			//if ( vu_Mode != 2 )
			{
				float t = (left+right) / (float)32768.0f * 0.5f;
				vu_Sum[ 0 ] += t * t * 1.0f;

				vu_Sum[ 1 ] += left * left/ (float)32768.0f / (float)32768.0f;
				vu_Sum[ 2 ] += right * right / (float)32768.0f / (float)32768.0f;
				//vu_Sum[ 3 ] += valOPL * valOPL / (float)32768.0f / (float)32768.0f;

				if ( ++ vu_nValues == 256*2 )
				{
					for ( u32 i = 0; i < 3; i++ )
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

			// ugly code which renders 3 oscilloscopes (SID1, SID2, FM) to HDMI and 1 for the OLED
			if ( screenType == 0 )
			{
				#include "oscilloscope_hack.h"
			} else
			if ( screenType == 1 )
			{
				const float scaleVis = 2.0f;
				const u32 nLevelMeters = 2;
				#include "tft_sid_vis.h"
			} 
		#endif
		}
	#endif
	}

	m_InputPin.DisableInterrupt();
}


//static u32 releaseDMA = 0;

#ifdef COMPILE_MENU
void KernelSIDLaunchFIQHandler8( void *pParam )
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

void KernelSIDFIQHandler8( void *pParam )
#else
void CKernel::FIQHandler (void *pParam)
#endif
{
	register u32 D = 0;

	#ifdef COMPILE_MENU
	if ( launchPrg && !disableCart )
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

	#ifdef COMPILE_MENU
	// preload cache
	if ( !( launchPrg && !disableCart ) )
	{
		CACHE_PRELOADL1STRMW( &ringWrite );
		CACHE_PRELOADL1STRM( &sampleBuffer[ smpLast ] );
		CACHE_PRELOADL1STRM( &outRegisters[ 0 ] );
		CACHE_PRELOADL1STRM( &outRegisters[ 16 ] );
	}
	#endif

	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( CPU_WRITES_TO_BUS )
		READ_D0to7_FROM_BUS( D )


	//  __   ___       __      __     __  
	// |__) |__   /\  |  \    /__` | |  \ 
	// |  \ |___ /~~\ |__/    .__/ | |__/ 
	//
	if ( cfgRegisterRead && CPU_READS_FROM_BUS && SID_ACCESS )
	{
		u32 A = ( g2 >> A0 ) & 31;
		u32 D = outRegisters[ A ];

		WRITE_D0to7_TO_BUS( D )

		FINISH_BUS_HANDLING
		return;
	} else
	//       __    ___  ___     __     __  
	// |  | |__) |  |  |__     /__` | |  \ 
	// |/\| |  \ |  |  |___    .__/ | |__/ 
	//                                   
	if ( CPU_WRITES_TO_BUS && SID_ACCESS ) 
	{
		//READ_D0to7_FROM_BUS( D )

		register u32 A = GET_ADDRESS0to7 | ((GET_ADDRESS8to12&1)<<8);
		register u32 whichSID = ((A>>6)&6) | ((A>>5)&1);
		A &= 31;
		
		ringBufGPIO[ ringWrite ] = D | (A << 8) | (whichSID << 16);

		//ringBufGPIO[ ringWrite ] = ( remapAddr | ( D << D0 ) ) & ~bIO2;
		ringTime[ ringWrite ] = cycleCountC64;
		ringWrite ++;
		ringWrite &= ( RING_SIZE - 1 );
		CACHE_PRELOADL1STRMW( &ringBufGPIO[ ringWrite ] );

		// optionally we could directly set the SID-output registers (instead of where the emulation runs)
		//u32 A = ( g2 >> A0 ) & 31;
		//outRegisters[ A ] = g1 & D_FLAG;

		FINISH_BUS_HANDLING
		return;
	}

	//  __                 __       ___  __       ___ 
	// |__) |  |  |\/|    /  \ |  |  |  |__) |  |  |  
	// |    |/\|  |  |    \__/ \__/  |  |    \__/  |  
	// OPTIONAL
	//											
	#ifdef USE_PWM_DIRECT
	if ( outputPWM )
	{
		static unsigned long long samplesElapsedBeforeFIQ = 0;

		unsigned long long samplesElapsedFIQ = ( ( unsigned long long )cycleCountC64 * ( unsigned long long )SAMPLERATE ) / ( unsigned long long )CLOCKFREQ;

		if ( samplesElapsedFIQ != samplesElapsedBeforeFIQ )
		{
			write32( ARM_GPIO_GPCLR0, bCTRL257 ); 
			samplesElapsedBeforeFIQ = samplesElapsedFIQ;

			u32 s = getSample();
			u16 s1 = s & 65535;
			u16 s2 = s >> 16;
			s32 d1 = (s32)( ( *(s16*)&s1 + 32768 ) * PWMRange ) >> 17;
			s32 d2 = (s32)( ( *(s16*)&s2 + 32768 ) * PWMRange ) >> 17;
			write32( ARM_PWM_DAT1, d1 );
			write32( ARM_PWM_DAT2, d2 );
			RESET_CPU_CYCLE_COUNTER
			return;
		} 
	}
	#endif
	

	//           ___  __       
	// |     /\   |  /  ` |__| 
	// |___ /~~\  |  \__, |  | 
	//
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

	#if 1
	if ( screenType == 0 && vu_nLEDs != 0xffff )
	{
		setLatchFIQ( LATCH_ON[ vu_nLEDs ] );
		clrLatchFIQ( LATCH_OFF[ vu_nLEDs ] );
	} else
	{
		prepareOutputLatch4Bit();
		outputLatch();
	}
	#endif

	FINISH_BUS_HANDLING
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