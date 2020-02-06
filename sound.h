/*
  _________.__    .___      __   .__        __           _________                        .___
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __      /   _____/ ____  __ __  ____    __| _/
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      \_____  \ /  _ \|  |  \/    \  / __ | 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <       /        (  <_> )  |  /   |  \/ /_/ | 
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     /_______  /\____/|____/|___|  /\____ | 
        \/         \/    \/     \/       \/     \/             \/                  \/      \/      
 
 sound.h

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - sound output code
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
#ifndef _sound_h_
#define _sound_h_

#define PCMBufferSize 48000

extern u32 PWMRange;

extern void initSoundOutput( CSoundBaseDevice **m_pSound = NULL, CVCHIQDevice *m_VCHIQ = NULL );
extern void clearSoundBuffer();

extern s32 sampleBuffer[ 128 ];
extern u32 smpLast, smpCur;

static __attribute__( ( always_inline ) ) inline void putSample( s16 a, s16 b )
{
	u16 *a_ = (u16*)&a, *b_ = (u16*)&b;
	sampleBuffer[ smpCur ++ ] = (u32)*a_ + ( ((u32)*b_) << 16 );
	smpCur &= 127;
}

static __attribute__( ( always_inline ) ) inline s32 getSample()
{
	u32 ret = sampleBuffer[ smpLast ++ ];
	smpLast &= 127;
	return ret;
}

extern short PCMBuffer[ PCMBufferSize ];
extern u32 PCMCountLast, PCMCountCur;

static __attribute__( ( always_inline ) ) inline void putSample( short s )
{
	PCMBuffer[ PCMCountCur ++ ]	= s;
	PCMCountCur %= PCMBufferSize;
}


#endif