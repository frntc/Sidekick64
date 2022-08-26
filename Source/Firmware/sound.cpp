/*
  _________.__    .___      __   .__        __           _________                        .___
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /   _____/ ____  __ __  ____    __| _/
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      \_____  \ /  _ \|  |  \/    \  / __ | 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <       /        (  <_> )  |  /   |  \/ /_/ | 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     /_______  /\____/|____/|___|  /\____ | 
        \/         \/    \/     \/       \/     \/             \/                  \/      \/      
 
 sound.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - sound output code
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
#include "kernel_sid.h"

#define WRITE_CHANNELS		2		// 1: Mono, 2: Stereo
#define CHUNK_SIZE			2000	// number of samples, written to sound device at once

#define FORMAT		SoundFormatSigned16
#define TYPE		s16
#define TYPE_SIZE	sizeof (s16)
#define FACTOR		((1 << 15)-1)
#define NULL_LEVEL	0

// forward declarations
void initPWMOutput();
void cbSound( void *d );
void clearSoundBuffer();

#ifdef USE_VCHIQ_SOUND
short PCMBuffer[ PCMBufferSize ];
#endif

u32 nSamplesPrecompute; 
static int FirstBufferUpdate = 1;
u32 soundDebugCode;

void initSoundOutput( CSoundBaseDevice **m_pSound, CVCHIQDevice *m_VCHIQ, u32 outputPWM, u32 outputHDMI )
{
	clearSoundBuffer();

#ifdef USE_PWM_DIRECT
	if ( outputPWM )
		initPWMOutput();
#endif
	smpLast = smpCur = 0;
#ifdef USE_VCHIQ_SOUND
//	if ( m_pSound == NULL || m_VCHIQ == NULL || outputHDMI == 0 )
	if ( outputHDMI == 0 )
		return;

	//( *m_pSound ) = new CPWMSoundBaseDevice( &m_Interrupt, SAMPLERATE, CHUNK_SIZE );
	if ( (*m_pSound) == NULL )
	{
		( *m_pSound ) = new CVCHIQSoundBaseDevice( m_VCHIQ, SAMPLERATE, CHUNK_SIZE, VCHIQSoundDestinationHDMI );
		( *m_pSound )->AllocateQueue( QUEUE_SIZE_MSECS );
		( *m_pSound )->SetWriteFormat( FORMAT, WRITE_CHANNELS );
		( *m_pSound )->RegisterNeedDataCallback( cbSound, (void*)( *m_pSound ) );
	}

	extern u32 SAMPLERATE_ADJUSTED;
	SAMPLERATE_ADJUSTED = SAMPLERATE;
	FirstBufferUpdate = 1;

	clearSoundBuffer();
	PCMCountLast = PCMCountCur = 0;
	nSamplesPrecompute = QUEUE_SIZE_MSECS * SAMPLERATE / 1000;
	soundDebugCode = 0;

/*	for ( u32 i = 0; i < QUEUE_SIZE_MSECS * SAMPLERATE / 1000 * 3 / 2; i++ )
	{
		putSample( 0 );
		putSample( 0 );
	}*/

#endif
}

void initSoundOutputVCHIQ( CSoundBaseDevice **m_pSound, CVCHIQDevice *m_VCHIQ )
{
	clearSoundBuffer();

	if ( (*m_pSound) == NULL )
	{
		( *m_pSound ) = new CVCHIQSoundBaseDevice( m_VCHIQ, SAMPLERATE, CHUNK_SIZE, VCHIQSoundDestinationHDMI );
		( *m_pSound )->AllocateQueue( QUEUE_SIZE_MSECS );
		( *m_pSound )->SetWriteFormat( FORMAT, WRITE_CHANNELS );
		( *m_pSound )->RegisterNeedDataCallback( cbSound, (void*)( *m_pSound ) );
	}

	extern u32 SAMPLERATE_ADJUSTED;
	SAMPLERATE_ADJUSTED = SAMPLERATE;
	FirstBufferUpdate = 1;

	clearSoundBuffer();
	PCMCountLast = PCMCountCur = 0;
	nSamplesPrecompute = QUEUE_SIZE_MSECS * SAMPLERATE / 1000;
	soundDebugCode = 0;
}

// __________  __      __  _____         _________                        .___
// \______   \/  \    /  \/     \       /   _____/ ____  __ __  ____    __| _/
//  |     ___/\   \/\/   /  \ /  \      \_____  \ /  _ \|  |  \/    \  / __ | 
//  |    |     \        /    Y    \     /        (  <_> )  |  /   |  \/ /_/ | 
//  |____|      \__/\  /\____|__  /    /_______  /\____/|____/|___|  /\____ | 
//                   \/         \/             \/                  \/      \/ 

// for PWM Output
//
u32 sampleBuffer[ 128 ];
u32 sampleBufferHDMI[ HDMI_BUF_SIZE ];
u32 smpLast, smpCur;

#ifdef USE_PWM_DIRECT

#define CLOCK_FREQ			500000000
#define CLOCK_DIVIDER		2 

// PWM control register
#define ARM_PWM_CTL_PWEN1	(1 << 0)
#define ARM_PWM_CTL_MODE1	(1 << 1)
#define ARM_PWM_CTL_RPTL1	(1 << 2)
#define ARM_PWM_CTL_SBIT1	(1 << 3)
#define ARM_PWM_CTL_POLA1	(1 << 4)
#define ARM_PWM_CTL_USEF1	(1 << 5)
#define ARM_PWM_CTL_CLRF1	(1 << 6)
#define ARM_PWM_CTL_MSEN1	(1 << 7)
#define ARM_PWM_CTL_PWEN2	(1 << 8)
#define ARM_PWM_CTL_MODE2	(1 << 9)
#define ARM_PWM_CTL_RPTL2	(1 << 10)
#define ARM_PWM_CTL_SBIT2	(1 << 11)
#define ARM_PWM_CTL_POLA2	(1 << 12)
#define ARM_PWM_CTL_USEF2	(1 << 13)
#define ARM_PWM_CTL_MSEN2	(1 << 14)

// PWM status register
#define ARM_PWM_STA_FULL1	(1 << 0)
#define ARM_PWM_STA_EMPT1	(1 << 1)
#define ARM_PWM_STA_WERR1	(1 << 2)
#define ARM_PWM_STA_RERR1	(1 << 3)
#define ARM_PWM_STA_GAPO1	(1 << 4)
#define ARM_PWM_STA_GAPO2	(1 << 5)
#define ARM_PWM_STA_GAPO3	(1 << 6)
#define ARM_PWM_STA_GAPO4	(1 << 7)
#define ARM_PWM_STA_BERR	(1 << 8)
#define ARM_PWM_STA_STA1	(1 << 9)
#define ARM_PWM_STA_STA2	(1 << 10)
#define ARM_PWM_STA_STA3	(1 << 11)
#define ARM_PWM_STA_STA4	(1 << 12)

u32 PWMRange;

void initPWMOutput()
{
	__attribute__((unused)) CGPIOPin   *m_Audio1 = new CGPIOPin( GPIOPinAudioLeft, GPIOModeAlternateFunction0 );
	__attribute__((unused)) CGPIOPin   *m_Audio2 = new CGPIOPin( GPIOPinAudioRight, GPIOModeAlternateFunction0 );
	CGPIOClock *m_Clock  = new CGPIOClock( GPIOClockPWM, GPIOClockSourcePLLD );

	u32 nSampleRate = SAMPLERATE;
	PWMRange = ( CLOCK_FREQ / CLOCK_DIVIDER + nSampleRate / 2 ) / nSampleRate;

	PeripheralEntry();

	m_Clock->Start( CLOCK_DIVIDER );
	CTimer::SimpleusDelay( 2000 );

	write32( ARM_PWM_RNG1, PWMRange );
	write32( ARM_PWM_RNG2, PWMRange );

	u32 nControl = ARM_PWM_CTL_PWEN1 | ARM_PWM_CTL_PWEN2;
	write32( ARM_PWM_CTL, nControl );

	CTimer::SimpleusDelay( 2000 );

	PeripheralExit();

	smpLast = smpCur = 0;
}
#endif

//   ___ ___________      _____  .___       _________                        .___
//  /   |   \______ \    /     \ |   |     /   _____/ ____  __ __  ____    __| _/
// /    ~    \    |  \  /  \ /  \|   |     \_____  \ /  _ \|  |  \/    \  / __ | 
// \    Y    /    `   \/    Y    \   |     /        (  <_> )  |  /   |  \/ /_/ | 
//  \___|_  /_______  /\____|__  /___|    /_______  /\____/|____/|___|  /\____ | 
//        \/        \/         \/                 \/                  \/      \/ 
#ifdef USE_VCHIQ_SOUND
u32 PCMCountLast, PCMCountCur;

u32 samplesInBuffer()
{
	u32 nFrames;
	if ( PCMCountLast <= PCMCountCur )
		nFrames = PCMCountCur - PCMCountLast; else
		nFrames = PCMCountCur + PCMBufferSize - PCMCountLast; 
	return nFrames;
}

u32 samplesInBufferFree()
{
	return PCMBufferSize - 16 - samplesInBuffer();
}

bool pcmBufferFull()
{
	if ( ( (PCMCountCur+1) == (PCMCountLast) ) ||
		 ( PCMCountCur == (PCMBufferSize-1) && PCMCountLast == 0 ) )
		return true;

	return false;	     
}
#endif

//
// callback called when more samples are needed by the HDMI sound playback
// this code is incredibly ugly and inefficient (more memory copies than necessary)
//
#ifdef USE_VCHIQ_SOUND

//
// don't look too close... 
// this is more than ugly, but the version without this copying has a bug when reaching the end of the buffer
// presumably this happens when writing part 1 of two does not transfer all bytes and then we're loosing a frame 
// once this is fixed, call 2x m_pSound->Write instead of copying the data to temp
//
void cbSound( void *d )
{
	extern u32 fillSoundBuffer;
	if ( fillSoundBuffer != 0xffffffff )
	{
		fillSoundBuffer = 1;
		return; 
	}

	CSoundBaseDevice *m_pSound = (CSoundBaseDevice*)d;
	extern u32 trackSampleProgress;

	s32 nFramesComputed = samplesInBuffer();
	s32 nFramesMax = m_pSound->GetQueueSizeFrames() - m_pSound->GetQueueFramesAvail();
	trackSampleProgress = 1024 * 1024 + ( nFramesComputed - nFramesMax );

	s32 nWriteFrames = min( nFramesComputed, nFramesMax );

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
	return;
}

#endif

void clearSoundBuffer()
{
#ifdef USE_PWM_DIRECT
	memset( sampleBuffer, 0, sizeof( u32 ) * 128 );
#endif
#ifdef USE_VCHIQ_SOUND
	memset( PCMBuffer, 0, sizeof( short ) * PCMBufferSize );
#endif
}

