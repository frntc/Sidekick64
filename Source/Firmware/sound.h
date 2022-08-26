/*
  _________.__    .___      __   .__        __           _________                        .___
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /   _____/ ____  __ __  ____    __| _/
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      \_____  \ /  _ \|  |  \/    \  / __ | 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <       /        (  <_> )  |  /   |  \/ /_/ | 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     /_______  /\____/|____/|___|  /\____ | 
        \/         \/    \/     \/       \/     \/             \/                  \/      \/      
 
 sound.h

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
#ifndef _sound_h_
#define _sound_h_

#include <circle/hdmisoundbasedevice.h>

#define PCMBufferSize (48000/4)
#define QUEUE_SIZE_MSECS 	50		// size of the sound queue in milliseconds duration
extern u32 nSamplesPrecompute; 

extern u32 PWMRange;

extern void initSoundOutput( CSoundBaseDevice **m_pSound = NULL, CVCHIQDevice *m_VCHIQ = NULL, u32 outputPWM = 0, u32 outputHDMI = 0 );
extern void initSoundOutputVCHIQ( CSoundBaseDevice **m_pSound, CVCHIQDevice *m_VCHIQ );
extern void clearSoundBuffer();

extern u32 sampleBuffer[ 128 ];
extern u32 smpLast, smpCur;

#define HDMI_BUF_SIZE 4096
extern u32 sampleBufferHDMI[ HDMI_BUF_SIZE ];

static __attribute__( ( always_inline ) ) inline void putSample( s16 a, s16 b )
{
	u16 *a_ = (u16*)&a, *b_ = (u16*)&b;
	sampleBuffer[ smpCur ++ ] = (u32)*a_ + ( ((u32)*b_) << 16 );
	smpCur &= 127;
}

static __attribute__( ( always_inline ) ) inline s32 getSample()
{
	if ( smpLast == smpCur ) return sampleBuffer[ smpLast ];
	u32 ret = sampleBuffer[ smpLast ++ ];
	smpLast &= 127;
	return ret;
}

static __attribute__( ( always_inline ) ) inline void putSampleHDMI( s32 a, s32 b )
{
	extern CHDMISoundBaseDevice *hdmiSoundDevice;
	sampleBufferHDMI[ smpCur ++ ] = hdmiSoundDevice->ConvertSample( a );
	sampleBufferHDMI[ smpCur ++ ] = hdmiSoundDevice->ConvertSample( b );
	smpCur &= HDMI_BUF_SIZE - 1;
}

static __attribute__( ( always_inline ) ) inline u8 getSampleHDMI( u32 *a, u32 *b )
{
	if ( smpLast == smpCur ) { *a = sampleBufferHDMI[ ( smpLast - 2 + 2048 ) & 2047 ]; *b = sampleBufferHDMI[ ( smpLast - 1 + 2048 ) & 2047 ]; return 0; }
	*a = sampleBufferHDMI[ smpLast ++ ];
	*b = sampleBufferHDMI[ smpLast ++ ];
	smpLast &= HDMI_BUF_SIZE - 1;
	return 1;
}

static __attribute__( ( always_inline ) ) inline u32 getNSamplesHDMI()
{
	return ( smpCur + HDMI_BUF_SIZE - smpLast ) & ( HDMI_BUF_SIZE - 1 );
}

static __attribute__( ( always_inline ) ) inline void skipSamplesHDMI( u32 n )
{
	smpLast += n;
	smpLast &= HDMI_BUF_SIZE - 1;
}

extern short PCMBuffer[ PCMBufferSize ];
extern u32 PCMCountLast, PCMCountCur;

static __attribute__( ( always_inline ) ) inline void putSample( short s )
{
	PCMBuffer[ PCMCountCur ++ ]	= s;
	PCMCountCur %= PCMBufferSize;
}


#endif