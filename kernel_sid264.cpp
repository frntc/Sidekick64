/*
  _________.__    .___      __   .__        __          _________.___________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     /   _____/|   \______ \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     \_____  \ |   ||    |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /        \|   ||    `   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    /_______  /|___/_______  /
        \/         \/    \/     \/       \/     \/            \/             \/ 
 
 kernel_sid264.cpp

 RasPiC64 - A framework for interfacing the C64 (and C16/+4) and a Raspberry Pi 3B/3B+
          - Sidekick SID: a SID and SFX Sound Expander Emulation for the C16/+4
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
#include "kernel_sid264.h"
#ifdef COMPILE_MENU
#include "kernel_menu.h"
#include "launch264.h"

//u32 launchPrg;
#endif

#undef USE_VCHIQ_SOUND

//                 _________.___________         ____                      ________    ______  ____________  
//_______   ____  /   _____/|   \______ \       /  _ \       ___.__. _____ \_____  \  /  __  \/_   \_____  \ 
//\_  __ \_/ __ \ \_____  \ |   ||    |  \      >  _ </\    <   |  |/     \  _(__  <  >      < |   |/  ____/ 
// |  | \/\  ___/ /        \|   ||    `   \    /  <_\ \/     \___  |  Y Y  \/       \/   --   \|   /       \ 
// |__|    \___  >_______  /|___/_______  /    \_____\ \     / ____|__|_|  /______  /\______  /|___\_______ \
//             \/        \/             \/            \/     \/          \/       \/        \/             \/
#include "resid/sid.h"
using namespace reSID;

//u32 CLOCKFREQ = 1773447;//886723;
u32 CLOCKFREQ = 2*985248;	

//MUX zum TESTEN auf IO1
//IO1 kann man später einsparen, in dem man verODERt mit ROMH (C1low und C1high) und dann die Adresse ausdekodiert

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

u32 fmFakeOutput = 0;

// counts the #cycles when the C64-reset line is pulled down (to detect a reset)
u32 resetCounter,
	cyclesSinceReset,  
	resetPressed, resetReleased;

s32 outputDigiblaster = 0, digiblasterVolume = 256, tedVolume = 0;

// hack
static u32 h_nRegOffset;
static u32 h_nRegMask;

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

void setSIDConfiguration( u32 mode, u32 sid1, u32 sid2, u32 sid2addr, u32 rr, u32 addr, u32 exp, s32 v1, s32 p1, s32 v2, s32 p2, s32 v3, s32 p3, s32 digiblasterVol, s32 sidfreq, s32 tedVol )
{
	SID_MODEL[ 0 ] = ( sid1 == 0 ) ? 6581 : 8580;
	SID_DigiBoost[ 0 ] = ( sid1 == 2 ) ? 1 : 0;
	SID_MODEL[ 1 ] = ( sid2 == 0 ) ? 6581 : 8580;
	SID_DigiBoost[ 1 ] = ( sid2 == 2 ) ? 1 : 0;

	// panning = 0 .. 14, vol = 0 .. 15
	// volumes -> 0 .. 255
	cfgVolSID1_Left  = v1 * ( 14 - p1 ) * 255 / 210;
	cfgVolSID1_Right = v1 * ( p1 ) * 255 / 210;
	cfgVolSID2_Left  = v2 * ( 14 - p2 ) * 255 / 210;
	cfgVolSID2_Right = v2 * ( p2 ) * 255 / 210;
	cfgVolOPL_Left  = v3 * ( 14 - p3 ) * 255 / 210;
	cfgVolOPL_Right = v3 * ( p3 ) * 255 / 210;

	if ( sid2 == 3 ) 
	{ 
		cfgSID2_Disabled = 1; cfgVolSID2_Left = cfgVolSID2_Right = 0;
	} else 
		cfgSID2_Disabled = 0;

	if ( addr == 0 ) cfgSID2_PlaySameAsSID1 = 1; else cfgSID2_PlaySameAsSID1 = 0;
	if ( mode == 0 ) cfgMixStereo = 1; else cfgMixStereo = 0;

	if ( addr == 0 )
		cfgSID2_Addr = 0xfd40; else
		cfgSID2_Addr = 0xfe80; 
	cfgRegisterRead = rr;
	if ( exp )
		cfgEmulateOPL2 = 0xfde2; else
		cfgEmulateOPL2 = 0; 

	if ( !exp )
	{
		cfgVolOPL_Left = cfgVolOPL_Right = 0;
	}
	digiblasterVolume = 255 * digiblasterVol / 15;
	if ( sidfreq == 0 )
		CLOCKFREQ = 1773447; else
		CLOCKFREQ = 2*985248;	

	tedVolume = 255 * tedVol / 15;
}


//___  ___  __      ___                      ___    __       
// |  |__  |  \    |__   |\/| |  | |     /\   |  | /  \ |\ | 
// |  |___ |__/    |___  |  | \__/ |___ /~~\  |  | \__/ | \| 
// taken directly from Yape, please see license in the header file                                                           
#include "TEDsound.h"


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

		for ( int j = 0; j < 24; j++ )
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
	}

#ifdef EMULATE_OPL2
	if ( cfgEmulateOPL2 )
	{
		pOPL = ym3812_init( 3579545, SAMPLERATE );
		ym3812_reset_chip( pOPL );
		fmFakeOutput = 0;
	}
#endif

	outputDigiblaster = 0;

	// ring buffer init
	ringWrite = 0;
	for ( int i = 0; i < RING_SIZE; i++ )
		ringTime[ i ] = 0;


	tedSoundInit( SAMPLERATE );
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

static u32 vu_Mode = 0;
static u32 vuMeter[4] = { 0, 0, 0, 0 };
static u32 vu_nLEDs = 0;

#ifdef COMPILE_MENU
void KernelSIDFIQHandler( void *pParam );

void KernelSIDRun( CGPIOPinFIQ2 m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0 )
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
	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	SETCLR_GPIO( bNMI | bDMA, 0 );

	#ifdef USE_OLED
	// I know this is a gimmick, but I couldn't resist ;-)
	splashScreen( raspi_sid_splash );
	#endif

	//	logger->Write( "", LogNotice, "initialize SIDs..." );
	initSID();

	#ifdef COMPILE_MENU
	if ( FILENAME == NULL && !hasData )
	{
		launchPrg_l264 = 0;
		disableCart_l264 = 1;
	} else
	{
		//logger->Write( "", LogNotice, "launch '%s'", FILENAME );
		launchPrg_l264 = 1;
		if ( launchGetProgram( FILENAME, hasData, prgDataExt, prgSizeExt ) )
			launchInitLoader( false ); else
			launchPrg_l264 = 0;
	}
	#endif

	// first byte of prgData will be used for transmitting ceil(prgSize/256)
	for ( u32 i = prgSize_l264; i > 0; i-- )
		prgData_l264[ i ] = prgData_l264[ i - 1 ];
	prgData_l264[0] = ( ( prgSize_l264 + 255 ) >> 8 );

	//
	// setup FIQ
	//
	#ifdef COMPILE_MENU
	m_InputPin.ConnectInterrupt( KernelSIDFIQHandler, kernelMenu );
	#else
	m_InputPin.ConnectInterrupt( this->FIQHandler, this );
	#endif
	h_nRegOffset = m_InputPin.nRegOffset();
	h_nRegMask = m_InputPin.nRegMask();

	m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );

#ifndef COMPILE_MENU

	latchSetClearImm( LATCH_RESET, LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

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
	logger->Write( "", LogNotice, "Measured clock frequency: %u Hz", (u32)CLOCKFREQ );
#endif

	for ( int i = 0; i < NUM_SIDS; i++ )
		sid[ i ]->set_sampling_parameters( CLOCKFREQ, SAMPLE_INTERPOLATE, SAMPLERATE );

	//
	// initialize sound output (either PWM which is output in the FIQ handler, or via HDMI)
	//
	initSoundOutput( &m_pSound, pVCHIQ );

	for ( int i = 0; i < NUM_SIDS; i++ )
		for ( int j = 0; j < 24; j++ )
			sid[ i ]->write( j, 0 );

	#ifdef EMULATE_OPL2
	if ( cfgEmulateOPL2 )
	{
		fmFakeOutput = 0;
		ym3812_reset_chip( pOPL );
	}
	#endif

//	logger->Write( "", LogNotice, "start emulating..." );
	cycleCountC64 = 0;
	unsigned long long nCyclesEmulated = 0;
	unsigned long long samplesElapsed = 0;

	// how far did we consume the commands in the ring buffer?
	unsigned int ringRead = 0;

	#ifdef COMPILE_MENU
	// let's be very convincing about the caches ;-)
	for ( u32 i = 0; i < 10; i++ )
	{
		launchPrepareAndWarmCache();

		// FIQ handler
		CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 4*1024 );
		FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 4*1024 );
	}

	if ( !launchPrg_l264 )
		SETCLR_GPIO( bNMI | bDMA, 0 );

	DELAY(10<<10);
	latchSetClearImm( LATCH_RESET, LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	if ( launchPrg_l264 )
	{
		while ( !disableCart_l264 )
		{
			#ifdef COMPILE_MENU
			TEST_FOR_JUMP_TO_MAINMENU( cycleCountC64, resetCounter )
			#endif
			asm volatile ("wfi");
		}
	} 
	#endif

	resetCounter = 0;
	cycleCountC64 = 0;
	nCyclesEmulated = 0;
	samplesElapsed = 0;
	ringRead = 0;
	for ( int i = 0; i < NUM_SIDS; i++ )
		for ( int j = 0; j < 24; j++ )
			sid[ i ]->write( j, 0 );

	#ifdef EMULATE_OPL2
	if ( cfgEmulateOPL2 )
	{
		fmFakeOutput = 0;
		ym3812_reset_chip( pOPL );
	}
	#endif

	logger->Write( "", LogNotice, "start emulating..." );

	// new main loop mainloop
	while ( true )
	{
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( cycleCountC64, resetCounter )
		#endif

		if ( resetCounter > 3 && resetReleased )
		{
			resetCounter = 0;

			for ( int i = 0; i < NUM_SIDS; i++ )
				for ( int j = 0; j < 24; j++ )
					sid[ i ]->write( j, 0 );

			#ifdef EMULATE_OPL2
			if ( cfgEmulateOPL2 )
			{
				fmFakeOutput = 0;
				ym3812_reset_chip( pOPL );
			}
			#endif
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

		#ifndef USE_PWM_DIRECT
		static u32 nSamplesInThisRun = 0;
		#endif

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

			unsigned long long samplesElapsedBefore = samplesElapsed;

			do { // do SID emulation until time passed to create an additional sample (i.e. there may be several cycles until a sample value is created)
				#ifdef USE_PWM_DIRECT
				u32 cyclesToEmulate = 16;
				#else			
				u32 cyclesToEmulate = 2;
				#endif
				sid[ 0 ]->clock( cyclesToEmulate );
				#ifndef SID2_DISABLED
				if ( !cfgSID2_Disabled )
					sid[ 1 ]->clock( cyclesToEmulate );
				#endif

				outRegisters[ 27 ] = sid[ 0 ]->read( 27 );
				outRegisters[ 28 ] = sid[ 0 ]->read( 28 );

				nCyclesEmulated += cyclesToEmulate;

				// apply register updates (we do one-cycle emulation steps, but in case we need to catch up...)
				unsigned int readUpTo = ringWrite;

				if ( ringRead != readUpTo && nCyclesEmulated >= ringTime[ ringRead ] )
				{
					unsigned char A, D;
					decodeGPIO( ringBufGPIO[ ringRead ], &A, &D );

					u32 tedCommand = ( ringBufGPIO[ ringRead ] >> A6 ) & 1;

					if ( tedCommand )
					{
						writeSoundReg( A, D );
					} else
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
						outRegisters[ A & 31 ] = D;
						//#if !defined(SID2_DISABLED) && defined(SID2_PLAY_SAME_AS_SID1)
						if ( !cfgSID2_Disabled && cfgSID2_PlaySameAsSID1 )
							sid[ 1 ]->write( A & 31, D );
						//#endif
					}

					ringRead++;
					ringRead &= ( RING_SIZE - 1 );
				}

				samplesElapsed = ( ( unsigned long long )nCyclesEmulated * ( unsigned long long )SAMPLERATE ) / ( unsigned long long )CLOCKFREQ;

			} while ( samplesElapsed == samplesElapsedBefore );

			s16 val1 = sid[ 0 ]->output();
			s16 val2 = 0;
			s16 valOPL = 0;

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

			short tedV = 0;

			if ( tedVolume > 0 )
				tedV = TEDcalcNextSample();

			// yes, it's 1 byte shifted in the buffer, need to fix
			right = ( val1 * cfgVolSID1_Left  + val2 * cfgVolSID2_Left  + valOPL * cfgVolOPL_Left + 2 * outputDigiblaster * digiblasterVolume + tedV * tedVolume ) >> 8;
			left  = ( val1 * cfgVolSID1_Right + val2 * cfgVolSID2_Right + valOPL * cfgVolOPL_Right + 2 * outputDigiblaster * digiblasterVolume + tedV * tedVolume ) >> 8;

			right = max( -32767, min( 32767, right ) );
			left  = max( -32767, min( 32767, left ) );

			#ifdef USE_PWM_DIRECT
			putSample( left, right );
			#else
			putSample( left );
			putSample( right );
			#endif

			// vu meter
			static u32 vu_nValues = 0;
			static float vu_Sum[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
			
			if ( vu_Mode != 2 )
			{
				float t = (left+right) / (float)32768.0f * 0.8f;
				vu_Sum[ 0 ] += t * t * 1.0f;

				vu_Sum[ 1 ] += val2 * val2 / (float)32768.0f / (float)32768.0f * 0.25f;
				vu_Sum[ 2 ] += val1 * val1 / (float)32768.0f / (float)32768.0f * 0.25f;
				vu_Sum[ 3 ] += valOPL * valOPL / (float)32768.0f / (float)32768.0f * 0.25f;

				if ( ++ vu_nValues == 256*4 )
				{
					for ( u32 i = 0; i < 4; i++ )
					{
						float vu_Volume = 50.0f * (log10( 1.0f + sqrt( (float)vu_Sum[ i ] / (float)vu_nValues ) ) );
						u32 v = vu_Volume * 1024.0f;
						if ( i == 0 )
						{
							vu_nLEDs = v >> 8;
							if ( vu_nLEDs > 4 ) vu_nLEDs = 4;
						}
						vuMeter[ i ] = v >> 2;
						vuMeter[ i ] *= vuMeter[ i ];
						vuMeter[ i ] >>= 8;

						vu_Sum[ i ] = 0;
					}

					vu_nValues = 0;
				}
			}

			// ugly code which renders 3 oscilloscopes (SID1, SID2, FM) to HDMI and 1 for the OLED
			#include "oscilloscope_hack.h"
		}
	#endif
	}

	m_InputPin.DisableInterrupt();
}


#ifdef USE_PWM_DIRECT
static unsigned long long samplesElapsedBeforeFIQ = 0;
#endif


#ifdef COMPILE_MENU
void KernelSIDFIQHandler( void *pParam )
#else
void CKernel::FIQHandler (void *pParam)
#endif
{
	unsigned long long samplesElapsedFIQ;
	static s32 latchDelayOut = 10;

	register u32 D;

	#ifdef COMPILE_MENU
	if ( launchPrg_l264 && !disableCart_l264 )
	{
		LAUNCH_FIQ( cycleCountC64, resetCounter,  h_nRegOffset, h_nRegMask )
	}
	#endif

	START_AND_READ_EXPPORT264

	if ( CPU_WRITES_TO_BUS )
	{
		READ_D0to7_FROM_BUS( D )
	} else

 	// this is an ugly hack to signal the menu code that it can reset (only necessary on a +4)
	if ( /*BUS_AVAILABLE264 && CPU_READS_FROM_BUS && */GET_ADDRESS264 >= 0xfd90 && GET_ADDRESS264 <= 0xfd97 )
	{
		WRITE_D0to7_TO_BUS( GET_ADDRESS264 - 0xfd90 + 1 )
		PeripheralEntry();
		write32( ARM_GPIO_GPEDS0 + h_nRegOffset, h_nRegMask );
		PeripheralExit();
		write32( ARM_GPIO_GPCLR0, bCTRL257 );
		FINISH_BUS_HANDLING
		return;
	}

	PeripheralEntry();
	write32( ARM_GPIO_GPEDS0 + h_nRegOffset, h_nRegMask );
	PeripheralExit();
	write32( ARM_GPIO_GPCLR0, bCTRL257 );

	if ( CPU_RESET ) {
		resetReleased = 0; resetPressed = 1; resetCounter ++;
	} else {
		if ( resetPressed )	resetReleased = 1;
		resetPressed = 0;
	}

	// this is a weird attempt of correctly mapping the clock cycles
	{
		if ( armCycleCounter > 500 * 15/10 )
			cycleCountC64 ++;
	}
	
	RESET_CPU_CYCLE_COUNTER
	armCycleCounter = 0;

	static u32 fCount = 0;
	fCount ++;
	fCount &= 255;

	cycleCountC64 ++;

	u32 swizzle = 0;

	#ifdef COMPILE_MENU
	// preload cache
	if ( !( launchPrg_l264 && !disableCart_l264 ) )
	{
		CACHE_PRELOADL1STRMW( &ringWrite );
		//CACHE_PRELOADL1STRMW( &ringBufGPIO[ ringWrite ] );
		//CACHE_PRELOADL1STRMW( &ringTime[ ringWrite ] );
		CACHE_PRELOADL1STRM( &sampleBuffer[ smpLast ] );
		CACHE_PRELOADL1STRM( &outRegisters[ 0 ] );
		CACHE_PRELOADL1STRM( &outRegisters[ 16 ] );
	}
	#endif

	//  __   ___       __      __     __  
	// |__) |__   /\  |  \    /__` | |  \ 
	// |  \ |___ /~~\ |__/    .__/ | |__/ 
	//
	if ( cfgRegisterRead && BUS_AVAILABLE264 && GET_ADDRESS264 >= 0xfd40 && GET_ADDRESS264 <= 0xfd5f && CPU_READS_FROM_BUS )
	{
		u32 A = ( g2 >> A0 ) & 31;
		u32 D = outRegisters[ A ];
		WRITE_D0to7_TO_BUS( D )
		//FINISH_BUS_HANDLING
		//return;
		goto get_out;
	} else

	//  __   ___       __      ___       
	// |__) |__   /\  |  \    |__   |\/| 
	// |  \ |___ /~~\ |__/    |     |  | 
	//                                   
	if ( cfgEmulateOPL2 )
	{
		#ifdef EMULATE_OPL2_THIS_PART_IS_DISABLED
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
		#endif
		//       __    ___  ___     ___       
		// |  | |__) |  |  |__     |__   |\/| 
		// |/\| |  \ |  |  |___    |     |  | 
		//                                    
		#ifdef EMULATE_OPL2
		if ( BUS_AVAILABLE264 && cfgEmulateOPL2 && GET_ADDRESS264 >= cfgEmulateOPL2 && GET_ADDRESS264 <= (cfgEmulateOPL2+0x10) && CPU_WRITES_TO_BUS )
		{
			register u32 A = ( GET_ADDRESS264 - cfgEmulateOPL2 + 0x40 );
			if ( GET_ADDRESS264 == cfgEmulateOPL2 ) A = 0x40; else A = 0x50;
			register u32 remapAddr = (A&31) << A0;

			ringBufGPIO[ ringWrite ] = ( remapAddr ) | ( D << D0 ) | bIO2;
			ringTime[ ringWrite ] = cycleCountC64;
			ringWrite ++;
			ringWrite &= ( RING_SIZE - 1 );

			//FINISH_BUS_HANDLING
			//return;
		goto get_out;
		} 
		#endif // EMULATE_OPL2
	} 
	//       __    ___  ___     __     __  
	// |  | |__) |  |  |__     /__` | |  \ 
	// |/\| |  \ |  |  |___    .__/ | |__/ 
	//                                   
	if ( BUS_AVAILABLE264 && ( GET_ADDRESS264 >= 0xfd40 && GET_ADDRESS264 <= 0xfd58 ) && CPU_WRITES_TO_BUS )
	{
		register u32 A = GET_ADDRESS264 - 0xfd40;
		register u32 remapAddr = (A&31) << A0;

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		ringBufGPIO[ ringWrite ] = ( remapAddr | ( D << D0 ) ) & ~bIO2;
		#pragma GCC diagnostic pop
		ringTime[ ringWrite ] = cycleCountC64;
		ringWrite ++;
		ringWrite &= ( RING_SIZE - 1 );
		
		// optionally we could directly set the SID-output registers (instead of where the emulation runs)
		//u32 A = ( g2 >> A0 ) & 31;
		//outRegisters[ A ] = g1 & D_FLAG;

		goto get_out;
	}

	if ( BUS_AVAILABLE264 && cfgSID2_Addr == 0xfe80 && GET_ADDRESS264 >= 0xfe80 && GET_ADDRESS264 <= 0xfe98 && CPU_WRITES_TO_BUS )
	{
		register u32 A = GET_ADDRESS264 - 0xfe80;
		register u32 remapAddr = (A&31) << A0;
		remapAddr |= SID2_MASK;

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		ringBufGPIO[ ringWrite ] = ( remapAddr | ( D << D0 ) ) & ~bIO2;
		#pragma GCC diagnostic pop
		ringTime[ ringWrite ] = cycleCountC64;
		ringWrite ++;
		ringWrite &= ( RING_SIZE - 1 );
		goto get_out;
	}

	if ( BUS_AVAILABLE264 && ( GET_ADDRESS264 == 0xfd5e ) && CPU_WRITES_TO_BUS )
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		outputDigiblaster = (s32)D - 128;
		#pragma GCC diagnostic pop
		goto get_out;
	}

	if ( tedVolume > 0 && BUS_AVAILABLE264 && ( GET_ADDRESS264 >= 0xff0e && GET_ADDRESS264 <= 0xff12 ) && CPU_WRITES_TO_BUS )
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

		register u32 A = GET_ADDRESS264 - 0xff0e;
		register u32 remapAddr = (A&31) << A0;
		remapAddr |= 1 << A6;

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		ringBufGPIO[ ringWrite ] = ( remapAddr | ( D << D0 ) );
		#pragma GCC diagnostic pop
		ringTime[ ringWrite ] = cycleCountC64;
		ringWrite ++;
		ringWrite &= ( RING_SIZE - 1 );

		#pragma GCC diagnostic pop
		goto get_out;
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
	samplesElapsedFIQ = ( ( unsigned long long )cycleCountC64 * ( unsigned long long )SAMPLERATE ) / ( unsigned long long )CLOCKFREQ;

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
		goto get_out;
	} 
	#endif
	
	static u32 omitLatch = 0;

	static u32 lastButtonPressed = 0;

	if ( lastButtonPressed > 0 )
		lastButtonPressed --;

	if ( BUTTON_PRESSED && lastButtonPressed == 0 )
	{
		omitLatch = 1 - omitLatch;
		vu_Mode = ( vu_Mode + 1 ) & 3;
		lastButtonPressed = 100000;
	}

	if ( omitLatch )
		goto get_out;
	//           ___  __       
	// |     /\   |  /  ` |__| 
	// |___ /~~\  |  \__, |  | 
	//
	#ifdef USE_LATCH_OUTPUT
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
	#endif

#if 1
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
		swizzle = 
			( (fCount & 128) >> 7 ) |
			( (fCount & 64) >> 5 ) |
			( (fCount & 32) >> 3 ) |
			( (fCount & 16) >> 1 ) |
			( (fCount & 8) << 1 ) |
			( (fCount & 4) << 3 ) |
			( (fCount & 2) << 5 ) |
			( (fCount & 1) << 7 );

		u32 led =
			( ( swizzle < vuMeter[ 1 ] ) ? LATCH_LED1 : 0 ) |
			( ( swizzle < vuMeter[ 2 ] ) ? LATCH_LED2 : 0 ) |
			( ( swizzle < vuMeter[ 3 ] ) ? LATCH_LED3 : 0 );

		setLatchFIQ( led );
		clrLatchFIQ( ( (~led) & LATCH_LED_ALL ) | LATCH_LED0 );		
	} else
		clrLatchFIQ( LATCH_LED_ALL );		
#endif

get_out:

	if ( resetCounter > 3  )
	{
		disableCart_l264 = transferStarted_l264 = 0;
	}

	//FINISH_BUS_HANDLING
	write32( ARM_GPIO_GPCLR0, bCTRL257 );
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