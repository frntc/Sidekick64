/*
  _________.__    .___      __   .__        __        _____            .___.__              .__                             
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   /  _  \  __ __  __| _/|__| ____ ______ |  | _____  ___.__. ___________ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /  /  /_\  \|  |  \/ __ | |  |/  _ \\____ \|  | \__  \<   |  |/ __ \_  __ \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <  /    |    \  |  / /_/ | |  (  <_> )  |_> >  |__/ __ \\___  \  ___/|  | \/
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \ \____|__  /____/\____ | |__|\____/|   __/|____(____  / ____|\___  >__|   
        \/         \/    \/     \/       \/     \/         \/           \/           |__|             \/\/         \/       

 kernel_MODplay.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick MODplay: example how to implement audio streaming (for C64 and C128)

			- This part uses:
			  + Pex 'Mahoney' Tufvesson sample playing technique and his lookup tables
			  + PocketMOD by rombankzero (https://github.com/rombankzero/pocketmod)
			  + STSound Library by Arnaud Carré (aka Leonard/Oxygene, https://github.com/arnaud-carre/StSound)

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

#include "kernel_MODplay.h"
#include <math.h>
#include "config.h"

#define POCKETMOD_IMPLEMENTATION
#include "pocketmod.h"

#include "mahoney_lut.h"

#include "STSoundLib/StSoundLibrary.h"
#include "STSoundLib/YmMusic.h"

// we will read this .PRG file
static const char DRIVE[] = "SD:";
#ifndef COMPILE_MENU
static const bool c128PRG = false;
#endif
static const char FILENAME_PLAYER[] = "SD:C64/audioplayer.prg";		// .PRG to start
static const char FILENAME_PLAYER2_PAL[] = "SD:C64/audioplayer2_pal.prg";		// .PRG to start
static const char FILENAME_PLAYER2_OLD_NTSC[] = "SD:C64/audioplayer2_old_ntsc.prg";		// .PRG to start
static const char FILENAME_PLAYER2_NEW_NTSC[] = "SD:C64/audioplayer2_new_ntsc.prg";		// .PRG to start
static const char FILENAME_CBM80[] = "SD:C64/launch_noinit.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM128[] = "SD:C64/launch128.cbm80";	// launch code (C128 cart)

static const char FILENAME_SPLASH_MOD[] = "SD:SPLASH/sk64_mod.tga";
static const char FILENAME_SPLASH_WAV[] = "SD:SPLASH/sk64_wav.tga";
static const char FILENAME_SPLASH_YM[] = "SD:SPLASH/sk64_ym.tga";
static const char FILENAME_SID_ICON[] = "SD:SPLASH/modplay_sid.raw";		// .PRG to start

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

const u32 cycleFirstKeyHandling = 500000;

static u32	configGAMEEXROMSet, configGAMEEXROMClr;
static u32	resetCounter;
static u32  c64CycleCount;
static u32	disableCart, transferStarted, currentOfs, transferPart;

static u32 prgSize;
static u8 prgData[ 65536 ] AAA;
static u32 startAddr, prgSizeAboveA000, prgSizeBelowA000, endAddr;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static u8 launchCode[ 8192 ] AAA;

static u32 resetFromCodeState = 0;
static u32 _playingPSID = 0;

extern volatile u8 forceReadLaunch;


static u32 nBytesRead, stage;

#define WAV_MEMSIZE_KB 32768

static u8 playFileType = 0; // 0 = MOD, 1 = WAV, 2 = YM
static u8 visMode = 0;
static u8 rasterEffects = 1;

// type of SID at $d400, $d420, $d500, $de00, $df00: 2 = 8580, 3 = 6581, something else: undefined
extern u8 typeSIDAddr[ 5 ];
u8 nSIDs = 1;
u16 addrSID1 = 0xd400;
u16 addrSID2 = 0;
static const u8 *mahoneyLUTSID1;
static const u8 *mahoneyLUTSID2;

static u8 exitPlayer = 0;
static s32 wavPosition = 0;
static u8 wavMemoryAllocated = 0;
static u8 *wavMemory;//[ WAV_MEMSIZE_KB * 1024 ] AAA;
static const u8 *mahoneyLUT;

// sprite-multiplexed screen
static u8 fb[ 56 * 64 ];
static u8 fb_2[ 56 * 64 ];

#define DIRTYRB_SIZE 16384
#define DIRTYRB_MASK (DIRTYRB_SIZE-1)
static u32 dirtyRB[ DIRTYRB_SIZE ], dirtyWrite = 0, dirtyRead = 0;
static u8 pauseScreenUpdate = 0;

// for visualization
float in_real[ 512 * 4 ];
float out_real[ 512 * 4 ];
float out_imag[ 512 * 4 ];
static u8 osc1[ 8 * 24 ];
static u8 osc2[ 8 * 24 ];
static u8 oscPos = 0;

static u32 fadeIn, fadeOut;
static u16 fadeVolume;


static u32 MOD_sampleRate = 48000;
static u32 MOD_SCAN_sampleRate = 8000;

static u8 outputViaHDMI = 0;

#define HDMI_SOUND_MODPLAY

#ifdef HDMI_SOUND_MODPLAY
static s32 lastWavPositionHDMI = 0x7fffffff;
static u8 firstHDMIOut = 1, HDMIstarted = 0;
extern CHDMISoundBaseDevice *hdmiSoundDevice;
#endif

struct WAVHEADER {
	u8  riff[ 4 ];
	u32 filesize;
	u8  wave[ 4 ];
	u8  fmtChunkMarker[ 4 ];
	u32 fmtLength;
	u32 fmtType;
	u32 nChannels;
	u32 sampleRate;
	u32 byteRate;
	u32 blockAlign;
	u32 bpp;
	u8  dataChunkHeader[ 4 ];
	u32 dataSize;
};

static struct WAVHEADER header;

u32 nWAVSamples = 0;

static void convertWAV2RAW_inplace( u8 *_data );

static void prepareOnReset( bool refresh = false )
{
	if ( !refresh )
	{
		CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();
		SyncDataAndInstructionCache();
	}

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], prgSize, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( prgData, prgSize, prgSize * 8 );

	// launch code / CBM80
	CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 512, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( launchCode, 512, 512 * 16 );

	for ( u32 i = 0; i < prgSizeAboveA000; i++ )
		forceReadLaunch = prgData[ prgSizeBelowA000 + i + 2 ];

	for ( u32 i = 0; i < prgSizeBelowA000 + 2; i++ )
		forceReadLaunch = prgData[ i ];

	CACHE_PRELOAD_DATA_CACHE( mahoneyLUTSID1, 256, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( mahoneyLUTSID1, 256, 256 * 16 );
	CACHE_PRELOAD_DATA_CACHE( mahoneyLUTSID2, 256, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( mahoneyLUTSID2, 256, 256 * 16 );

	CACHE_PRELOAD_DATA_CACHE( dirtyRB, DIRTYRB_SIZE * sizeof( u32 ), CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( dirtyRB, DIRTYRB_SIZE * sizeof( u32 ), DIRTYRB_SIZE * sizeof( u32 ) * 16 );

	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 6 * 1024 );
	//FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 6 * 1024 );
}


void computeFFT( float *inp, float *outR, float *outI, u32 s, u32 stride = 1 )
{
	if ( s == 1 )
	{
		outR[ 0 ] = inp[ 0 ];
		outI[ 0 ] = 0.0f;
		return;
	}

	s >>= 1;

	computeFFT( inp, outR, outI, s, stride * 2 );
	computeFFT( &inp[ stride ], &outR[ s ], &outI[ s ], s, stride * 2 );

	for ( u32 i = 0; i < s; i++ )
	{
		float aR = outR[ i ];
		float aI = outI[ i ];
		float bR = outR[ i + s ];
		float bI = outI[ i + s ];

		// 2*PI*i/olds
		float fR =  cosf( 3.14159265358979323846f * i / s );
		float fI = -sinf( 3.14159265358979323846f * i / s );

		float biasR = bR * fR - bI * fI;
		float biasI = bI * fR + bR * fI;

		outR[ i ]     = aR + biasR;
		outR[ i + s ] = aR - biasR;
		outI[ i ]     = aI + biasI;
		outI[ i + s ] = aI - biasI;
	}
}


void setpixel( int x_, int y_ )
{
	int sh = x_ / 24;
	y_ += sh;
	if ( y_ >= 147 ) return;
	int sv = y_ / 21;
	int x = x_ % 24;
	int y = y_ % 21;
	int ofs = ( x / 8 ) + y * 3;
	int bit = 7 - (x & 7);
	u16 P = sh * 64 + sv * 8 * 64 + ofs;
	fb[ P ] |= 1 << bit;
}

void clrpixel( int x_, int y_ )
{
	int sh = x_ / 24;
	y_ += sh;
	if ( y_ >= 147 ) return;
	int sv = y_ / 21;
	int x = x_ % 24;
	int y = y_ % 21;
	int ofs = ( x / 8 ) + y * 3;
	int bit = 7 - (x & 7);

	u16 P = sh * 64 + sv * 8 * 64 + ofs;
	fb[ P ] &= ~(1 << bit);
}

// x must be a multiple of 8!
void printSpriteLayer( u8 *framebuffer, const char *s, int x_, int y_ )
{
	for ( u32 i = 0; i < strlen( s ); i++ )
	{
		u8 c = s[ i ], c2;
		c2 = c;
		if ( ( c >= 'a' ) && ( c <= 'z' ) )
			c2 = c + 1 - 'a';

		//if ( c != 32 && c != ( 32 + 128 ) )
		for ( int b = 0; b < 8; b++ )
		{
			u8 c64screenUppercase = 0;
			extern u8 charset[ 4096 ] AAA;

			u8 v = charset[ 2048 - c64screenUppercase * 2048 + c2 * 8 + b ];

			int sh = ( x_ + i * 8 ) / 24;
			int y = y_ + b + sh;
			if ( y >= 147 ) return;
			int sv = y / 21;
			int x = ( x_ + i * 8 ) % 24;
			y = y % 21;
			int ofs = ( x / 8 ) + y * 3;
			u16 P = sh * 64 + sv * 8 * 64 + ofs;
			{
				framebuffer[ P ] = v;
				dirtyRB[ dirtyWrite ++ ] = ( (u32)P << 16 ) | (u32)v;
				dirtyWrite &= DIRTYRB_MASK;
			}
		}
	}
}


static float clip(float value)
{
    value = value < -1.0f ? -1.0f : value;
    value = value > +1.0f ? +1.0f : value;
    return ( value + 1.0f ) * 0.5f;
}

static pocketmod_context context;
static u8 *mod_data;//, *slash;
static u8 *ringbuf;
static u32 *ringbufHDMI;
static u32 mod_size, rbRead, rbWrite;
#define ringbufSize 16384

pocketmod_context modJumpContext[ 128 ];

static int dheight[ 48 ], firstRun = 1;
static u8 colorbar[ 24 ];

static float modScale = 0.5f, modOfs = 1.0f, modVolume = 2.0f;
static int modVolumeInt = 100;
static int modJumpTo = -1;

CYmMusic ymMusic( MOD_sampleRate );
CYmMusic *pMusic = &ymMusic;

static u8 peakLevel = 128;

static u8 c64CurRasterLine, c64LastRasterLine, c64LastColor;
static u32 c64CurCycleInFrame = 0;
static u32 c64CyclesPerRasterline = 63; // NTSC 65
static u32 c64CyclesPerFrame = 63 * 312; // NTSC 65*263
static unsigned long long c64ClockSpeed = 985240ULL;
static u8 alwaysUpdateEQ = 0;

void computeSamplesAndScreenUpdate( u16 vol )
{
	float buffer[ 1024 ][ 2 ];
	u32 stats[ 1024 ];
	float mean = 0.0f;
	int rendered_samples = 512;
	u32 firstNewSample = rbWrite;

	if ( playFileType == 0 )
	{
		if ( modJumpTo > -1 )
		{
			memcpy( &context, &modJumpContext[ modJumpTo ], sizeof( pocketmod_context ) );
			context.samples_per_second = MOD_sampleRate;
			context.samples_per_tick *= (float)MOD_sampleRate / (float)MOD_SCAN_sampleRate;
			for ( int i = 0; i < context.num_channels; i++ )
				context.channels[ i ].increment *= (float)MOD_SCAN_sampleRate / (float)MOD_sampleRate;
			modJumpTo = -1;
		}

		int bytesToRender = 512 * sizeof( float ) * 2;

		int rendered_bytes = pocketmod_render(&context, buffer, stats, bytesToRender );
		rendered_samples = rendered_bytes / sizeof(float[2]);
		float globalVolume = (float)vol / 256.0f;
		for ( int i = 0; i < rendered_samples; i++) 
		{
			if ( addrSID2 == 0 )
			{
				int v1 = (int)( clip( ( buffer[ i ][ 0 ] + modOfs ) * globalVolume * modScale * modVolume ) * 65535.0f );
				int v2 = (int)( clip( ( buffer[ i ][ 1 ] + modOfs ) * globalVolume * modScale * modVolume ) * 65535.0f );
				int o1 = ( v1 + v2 ) >> 9;

				#ifdef HDMI_SOUND_MODPLAY
				if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = hdmiSoundDevice->ConvertSample( (v1-32768) << 7 ); 
				#endif
				ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o1;

				#ifdef HDMI_SOUND_MODPLAY
				if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = hdmiSoundDevice->ConvertSample( (v2-32768) << 7 ); 
				#endif
				ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o1;
			} else
			{
				//static u32 seed = 123456789;
				//seed = (1664525*seed+1013904223);
				//float ditherNoise = ( seed >> 16 ) / 65536.0f - 0.5f;
				//static float triangleState = 0;
				float r1 = clip( ( buffer[ i ][ 0 ] + modOfs ) * globalVolume * modScale * modVolume );
				r1 *= 256.0f;
				//r1 += ditherNoise - triangleState;
				if ( r1 > 255.0f ) r1 = 255.0f;
				if ( r1 < 0.0f ) r1 = 0.0f;
				int v1 = (int)( r1 * 65535.0f / 256.0f );
				int o1 = v1 >> 8;
				float r2 = clip( ( buffer[ i ][ 1 ] + modOfs ) * globalVolume * modScale * modVolume );
				r2 *= 256.0f;
				//r2 += ditherNoise - triangleState;
				if ( r2 > 255.0f ) r2 = 255.0f;
				if ( r2 < 0.0f ) r2 = 0.0f;
				int v2 = (int)(  r2 * 65535.0f / 256.0f );
				int o2 = v2 >> 8;
				//triangleState = ditherNoise;

				#ifdef HDMI_SOUND_MODPLAY
				if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = hdmiSoundDevice->ConvertSample( (v1-32768) << 7 ); 
				#endif
				ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o1;
				#ifdef HDMI_SOUND_MODPLAY
				if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = hdmiSoundDevice->ConvertSample( (v2-32768) << 7 ); 
				#endif
				ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o2;
			}

			float v = ( buffer[ i ][ 0 ] + buffer[ i ][ 1 ] ) * 0.5f;
			mean += v;
			in_real[ i ] = v; 
		}
	} else
	if ( playFileType == 1 )
	{
		if ( modJumpTo == 1 )
		{
			c64CycleCount += c64ClockSpeed * 3;
			modJumpTo = -1;
		} else
		if ( modJumpTo == 2 )
		{
			c64CycleCount = max( 0, c64CycleCount - c64ClockSpeed * 3 );
			modJumpTo = -1;
		} 

		u32 p = wavPosition % nWAVSamples;
		for ( int i = 0; i < rendered_samples; i++) 
		{
			int o1 = wavMemory[ p * 2 ];
			int o2 = wavMemory[ p * 2 + 1 ];

			#ifdef HDMI_SOUND_MODPLAY
			if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = hdmiSoundDevice->ConvertSample( (o1-128) << 16 ); 
			#endif
			ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o1;
			#ifdef HDMI_SOUND_MODPLAY
			if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = hdmiSoundDevice->ConvertSample( (o2-128) << 16 ); 
			#endif
			ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o2;

			float v = ( wavMemory[ p * 2 ] + wavMemory[ p * 2 + 1 ] ) / 2048.0f;
			mean += v;
			in_real[ i ] = v; 

			p ++;
			if ( p > nWAVSamples )
				p = 0;
		}
	} else
	if ( playFileType == 2 )
	{
		s16 buffer[ 1024 ];
		rendered_samples = 512;
		pMusic->update( buffer, rendered_samples );

		for ( int i = 0; i < rendered_samples; i++) 
		{
			int o1 = buffer[ i ] * 3 / 2;
			if ( o1 > 32767 ) o1 = 32767;
			if ( o1 < -32768 ) o1 = -32768;
			o1 += 32768;
			//int o1 = ( buffer[ i ] + 32768 );
			#ifdef HDMI_SOUND_MODPLAY
			u32 c1 = hdmiSoundDevice->ConvertSample( buffer[ i ] << 8 );
			#endif

			#ifdef HDMI_SOUND_MODPLAY
			if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = c1; 
			#endif
			ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o1 >> 8;
			#ifdef HDMI_SOUND_MODPLAY
			if ( outputViaHDMI ) ringbufHDMI[ ( rbWrite )&( ringbufSize - 1 ) ] = c1; 
			#endif
			ringbuf[ ( rbWrite++ )&( ringbufSize - 1 ) ] = o1 >> 8;

			float v = ( o1 ) / ( 512.0f * 256.0f );
			mean += v;
			in_real[ i ] = v; 
		}

		if ( modJumpTo == 1 && pMusic->isSeekable() )
		{
			pMusic->setMusicTime( pMusic->getPos() + 3000 );
			modJumpTo = -1;
		} else
		if ( modJumpTo == 2 && pMusic->isSeekable() )
		{
			pMusic->setMusicTime( max( 0, pMusic->getPos() - 3000 ) );
			modJumpTo = -1;
		} 

	}

	mean /= (float)rendered_samples;
	for ( int i = 0; i < rendered_samples; i++) {
		in_real[ i ] -= mean;
		float weighingFactor = 0.54 - (0.46 * cosf( 2.0f * 3.141592f * (float)i / (float)rendered_samples));
		in_real[ i ] *= weighingFactor;
	}

	computeFFT(in_real, out_real, out_imag, 512 );

	//
	//
	//
	pauseScreenUpdate = 1;

	if ( dirtyRead == dirtyWrite || alwaysUpdateEQ )
	{
		dirtyRead = dirtyWrite = 0;
		memset( dirtyRB, 0, DIRTYRB_SIZE * sizeof( u32 ) );
		char buf[ 32 ];

		
		//sprintf( buf, "%02d", c64CycleCount );
		//printSpriteLayer( fb, buf, 0, 110 );

		if ( playFileType == 0 )
		{
			static int prevLine = -2, prevPatt = -2, prevSpeed = -2;
			if ( prevLine != context.line )
			{
				prevLine = context.line;
				sprintf( buf, "%02d", max( 0, context.line ) );
				printSpriteLayer( fb, buf, 80, 78+5 );
			}

			if ( prevPatt != context.pattern )
			{
				sprintf( buf, "%02d", context.pattern );
				printSpriteLayer( fb, buf, 80, 70+5 );
			}

			if ( prevSpeed != context.ticks_per_line )
			{
				prevSpeed = context.ticks_per_line;
				sprintf( buf, "%02d", context.ticks_per_line );
				printSpriteLayer( fb, buf, 80, 86+5 );
			}
		} else
		if ( playFileType == 1 )
		{
			static u32 oldSec = 0xffffffff;
			u32 sec = c64CycleCount / c64ClockSpeed;
			if ( sec != oldSec )
			{
				sprintf( buf, "%02d:%02d", sec / 60, sec % 60 );
				printSpriteLayer( fb, buf, 72, 62+5 );
				oldSec = sec;
			}
		} else
		if ( playFileType == 2 )
		{
			static u32 oldSec = 0xffffffff;
			u32 sec = pMusic->getPos() / 1000;
			if ( sec != oldSec )
			{
				sprintf( buf, "%02d:%02d", sec / 60, sec % 60 );
				printSpriteLayer( fb, buf, 72, 70+5 );
				oldSec = sec;
			}
		}

		static u8 lastVisMode = 0;

		memcpy( fb_2, fb, 56 * 64 );

		if ( lastVisMode != visMode )
		{
			lastVisMode = visMode;

			for ( u32 i = 0; i < 8 * 24; i++ )
				for ( int j = 0; j < 24+8; j++ )
					clrpixel( i, j + 16 );
		}

		if ( visMode == 0 || rasterEffects )
		{
			if ( firstRun )
			{
				for ( u32 i = 0; i < 48; i++ )
					dheight[ i ] = 0;
				firstRun = 0;
			}

			// normalize fft (512 = nSamples)
			float s = 1.0f / sqrtf( 512.0f );

			int height[ 48 ], first = 0, last = 1;
			int peak = 0;
			for ( u32 i = 0; i < 48; i++ )
			{
				height[ i ] = 0;
				float x = (float)( i + 1 + 8 ) / (48.0f+8.0f);
				last = (int)( x * x * x * 256.0f );
				for ( int j = first; j < max( first + 1, last ); j++ )
					height[ i ] += 2.0f * ( sqrtf(out_real[j]*out_real[j]+out_imag[j]*out_imag[j]) ) * 24.0f * ( 1.0f + x * 4.0f ) * s;
				first = last;

				if ( rasterEffects )
					dheight[ i ] = ( 220 * dheight[ i ] + 36 * height[ i ] ) >> 8; else
					dheight[ i ] = ( 180 * dheight[ i ] + 76 * height[ i ] ) >> 8;
			}

			for( u32 i = 0; i < 24; i++ )
			{
				colorbar[ i ] = ( dheight[ i * 2 ] + dheight[ i * 2 + 1 ] ) / 5;
				if ( colorbar[ i ] > 7 ) colorbar[ i ] = 7;
				colorbar[ i ] = 7 - colorbar[ i ];
			}


			for ( u32 i = 0; i < 8 * 24; i++ )
			{
				//int v = 2.0f * out_real[i] * 210.0f;
				//int v = 2.0f * ( sqrtf(out_real[i]*out_real[i]+out_imag[i]*out_imag[i]) ) * 21.0f;
				int v = dheight[ i / 4 ];
				if ( v < 0 ) v = 0;
				if ( v > 24 ) v = 24;
				peak += v;
				if ( (i & 3) != 3 )
				for ( int j = 0; j < 24 - v; j++ )
					if ( j & 1 )
					clrpixel( i, j + 16 );
				if ( (i & 3) != 3 )
				for ( u32 j = 24 - v; j < 24; j++ )
					if ( j & 1 )
					setpixel( i, j + 16 );
			}

			peakLevel = peak * 10 / (8 * 24); //peak >> 6;
		} 

		if ( visMode == 1 && !rasterEffects )
		{
			// visMode == 1 
			//if ( firstNewSample != -1 )
			{
				//static u8 updFreq = 0;
				//updFreq++;
				if ( addrSID2 == 0 )
				{
					//if ( updFreq >= 20 )
					{
						//updFreq = 0;
						for ( u32 i = 0; i < 8 * 24; i++ )
						{
							oscPos = i;
							u8 s = ringbuf[ (firstNewSample + (i>>0) * 2 * 2)&( ringbufSize - 1 ) ];
							clrpixel( oscPos, osc1[ oscPos ] + 16 );
							osc1[ oscPos ] = s / 11;
							setpixel( oscPos, osc1[ oscPos ] + 16 );
						}
					}
				} else
				{
					for ( u32 i = 0; i < 4 * 24 - 8; i++ )
					{
						oscPos = i;
						u8 s = ringbuf[ (firstNewSample + (i>>0) * 2 * 2)&( ringbufSize - 1 ) ];
						clrpixel( oscPos, osc1[ oscPos ] + 16 );
						osc1[ oscPos ] = s / 11;
						setpixel( oscPos, osc1[ oscPos ] + 16 );

						s = ringbuf[ (firstNewSample + (i>>0) * 2 * 2 + 1)&( ringbufSize - 1 ) ];
						clrpixel( oscPos + 4 * 24 + 8, osc2[ oscPos ] + 16 );
						osc2[ oscPos ] = s / 11;
						setpixel( oscPos + 4 * 24 + 8, osc2[ oscPos ] + 16 );
					}
				}
			}
		}


		for ( u32 i = 0; i < 3*8*64; i++ )
		{
			if ( fb_2[ i ] != fb[ i ] )
			{
				dirtyRB[ dirtyWrite ++ ] = ( (u32)i << 16 ) | (u32)fb[ i ];
				dirtyWrite &= DIRTYRB_MASK;
			}
		}

	}
	pauseScreenUpdate = 0;
}

static u8 toupper( u8 c )
{
	if ( c >= 'a' && c <= 'z' )
		return c + 'A' - 'a';
	return c;
}

#define REG(name, base, offset, base4, offset4)	\
			static const u32 Reg##name = (base) + (offset);
#define REGBIT(reg, name, bitnr)	static const u32 Bit##reg##name = 1U << (bitnr);

REG (MaiData, ARM_HD_BASE, 0x20, ARM_HD_BASE, 0x1C);
REG (MaiControl, ARM_HD_BASE, 0x14, ARM_HD_BASE, 0x10);
	REGBIT (MaiControl, Full, 11);


#ifdef COMPILE_MENU
void KernelMODplayRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0, bool playHDMIinParallel = false, u32 musicPlayer = 0 )
#else
void CKernelMODplay::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );
	latchSetClear( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	{
		configGAMEEXROMSet = bGAME | bNMI | bDMA;
		configGAMEEXROMClr = bEXROM; 
	}
	SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	extern u32 modePALNTSC;

	if ( modePALNTSC == 1 ) // old NTSC
	{
		c64CyclesPerRasterline = 64; 
		c64CyclesPerFrame     = 64 * 262;
		c64ClockSpeed = 1022727ULL;
		readFile( logger, (char*)DRIVE, (const char*)FILENAME_PLAYER2_OLD_NTSC, prgData, &prgSize );
		musicPlayer = 1; // timing for player #0 not implemented
	} else
	if ( modePALNTSC == 2 ) // new NTSC
	{
		c64CyclesPerRasterline = 65; 
		c64CyclesPerFrame     = 65 * 263; 
		c64ClockSpeed = 1022727ULL;
		readFile( logger, (char*)DRIVE, (const char*)FILENAME_PLAYER2_NEW_NTSC, prgData, &prgSize );
		musicPlayer = 1; // timing for player #0 not implemented
	} else
	if ( modePALNTSC == 3 ) // Drean -- untested!
	{
		c64CyclesPerRasterline = 65; 
		c64CyclesPerFrame     = 65 * 312; 
		c64ClockSpeed = 1022727ULL;
		readFile( logger, (char*)DRIVE, (const char*)FILENAME_PLAYER2_PAL, prgData, &prgSize );
		musicPlayer = 1; // timing for player #0 not implemented
	} else
	{
		c64CyclesPerRasterline = 63; 
		c64CyclesPerFrame     = 63 * 312;
		c64ClockSpeed = 985240ULL;

		if ( musicPlayer == 0 )
			readFile( logger, (char*)DRIVE, (const char*)FILENAME_PLAYER, prgData, &prgSize ); else
			readFile( logger, (char*)DRIVE, (const char*)FILENAME_PLAYER2_PAL, prgData, &prgSize );
	}

	if ( musicPlayer == 0 )
	{
		rasterEffects = 0;
		alwaysUpdateEQ = 0;
	} else
	{
		rasterEffects = 1;
		alwaysUpdateEQ = 1;
	}
	char fn_up[ 1024 ];
	int i = 0;
	while ( i < 1024 && FILENAME[ i ] != 0 )
	{
		fn_up[ i ] = toupper( FILENAME[ i ] );
		i ++;
	}
	fn_up[ i ] = 0;

	//logger->Write( "RaspiFlash", LogNotice, "audio file: '%s'", fn_up );

	if ( strstr( (char*)fn_up, ".MOD" ) )
		playFileType = 0; else
	if ( strstr( (char*)fn_up, ".WAV" ) )
		playFileType = 1; else
	if ( strstr( (char*)fn_up, ".YM" ) )
		playFileType = 2; else
		return;

	#ifdef COMPILE_MENU
	_playingPSID = playingPSID;

	if ( screenType == 0 )
	{
		splashScreen( sidekick_launch_oled );
	} else
	if ( screenType == 1 )
	{
		switch ( playFileType )
		{
		case 0: 
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_MOD, 8 );
			break;
		case 1: 
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_WAV, 8 );
			break;
		case 2: 
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_YM, 8 );
			break;
		};
			
		int w, h; 
		extern char FILENAME_LOGO_RGBA[128];
		extern u8 tempTGA[ 256 * 256 * 4 ];

		if ( tftLoadTGA( DRIVE, FILENAME_LOGO_RGBA, tempTGA, &w, &h, true ) )
		{
			tftBlendRGBA( tempTGA, tftBackground, 0 );
		}

		tftCopyBackground2Framebuffer();
		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}
	#endif

	if ( !wavMemoryAllocated )
	{
		wavMemoryAllocated = 1;
		wavMemory = (u8*)getPoolMemory( WAV_MEMSIZE_KB * 1024 );
	}

	#ifdef HDMI_SOUND_MODPLAY
	lastWavPositionHDMI = 0x7fffffff;
	firstHDMIOut = 1;
	HDMIstarted = 0;
	if ( playHDMIinParallel )
	{
		outputViaHDMI = 1;
		//hdmiSoundDevice->Cancel();
		//hdmiSoundDevice->Start();
		/*while ( hdmiSoundDevice->IsWritable() )
		{
			hdmiSoundDevice->WriteSample( 0 );
			hdmiSoundDevice->WriteSample( 0 );
		}*/
	} else
		outputViaHDMI = 0;
	#endif

	// read launch code and .PRG
	u32 size;
	if ( c128PRG )
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM128, launchCode, &size ); else
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, launchCode, &size );

	wavPosition = 0;
	mahoneyLUT = lookup6581;

	ringbuf = (u8*)&wavMemory[ (WAV_MEMSIZE_KB-64) * 1024 ];

	if ( playHDMIinParallel )
		ringbufHDMI = (u32*)&wavMemory[ (WAV_MEMSIZE_KB-256) * 1024 ];

	memset( ringbuf, 0, ringbufSize * sizeof( u8 ) );
	memset( ringbufHDMI, 0, ringbufSize * sizeof( u32 ) * 2 );

	if ( playFileType == 0 )
	{
		mod_data = wavMemory;

		getFileSize( logger, (char*)DRIVE, (char*)FILENAME, &size );
		if ( size > (WAV_MEMSIZE_KB-256) * 1024 ) return;

		readFile( logger, (char*)DRIVE, (char*)FILENAME, wavMemory, &size );
		mod_size = size;


		if (!pocketmod_init(&context, mod_data, mod_size, MOD_SCAN_sampleRate)) 
		{
			// error: not a valid MOD file
			return;
		}

		float minV = 1e30f, maxV = -1e30f;

		int contextIdx = 0;
		memcpy( &modJumpContext[ 0 ], &context, sizeof( pocketmod_context ) );

		while ( pocketmod_loop_count( &context ) == 0 )
		{
			float buffer[512][2];
			u32 stats[512];

			int rendered_bytes = pocketmod_render(&context, buffer, stats, sizeof(buffer));
			int rendered_samples = rendered_bytes / sizeof(float[2]);

			if ( context.line == 0 && contextIdx < context.pattern )
			{
				contextIdx = context.pattern;
				memcpy( &modJumpContext[ context.pattern ], &context, sizeof( pocketmod_context ) );
			}

			for ( int i = 0; i < rendered_samples; i++) 
			{
				if ( buffer[ i ][ 0 ] < minV ) minV = buffer[ i ][ 0 ];
				if ( buffer[ i ][ 1 ] < minV ) minV = buffer[ i ][ 1 ];
				if ( buffer[ i ][ 0 ] > maxV ) maxV = buffer[ i ][ 0 ];
				if ( buffer[ i ][ 1 ] > maxV ) maxV = buffer[ i ][ 1 ];
			}
		}
		modScale = 2.0f / ( maxV - minV ) * 0.95f;
		modOfs = -( minV + maxV ) * 0.5f;


		if (!pocketmod_init(&context, mod_data, mod_size, MOD_sampleRate)) 
		{
			// error: not a valid MOD file
			return;
		}
	} else
	if ( playFileType == 1 ) 
	{
		getFileSize( logger, (char*)DRIVE, (char*)FILENAME, &size );
		if ( size > (WAV_MEMSIZE_KB-256) * 1024 ) return;
		readFile( logger, (char*)DRIVE, (char*)FILENAME, wavMemory, &size );
		convertWAV2RAW_inplace( wavMemory );
	} else
	if ( playFileType == 2 )
	{
		getFileSize( logger, (char*)DRIVE, (char*)FILENAME, &size );
		if ( size > (WAV_MEMSIZE_KB-256) * 1024 ) return;
		readFile( logger, (char*)DRIVE, (char*)FILENAME, wavMemory, &size );
		pMusic->loadMemory( wavMemory, size ); 
		pMusic->setLoopMode( true );
		pMusic->play();
		pMusic->setLowpassFilter( !true );
	}

	memset( fb, 0, 64 * 56 );

	if ( playFileType == 0 )
	{
		//                     012345678901234567890123
		printSpriteLayer( fb, ".- Sidekick64 MODPlay -.", 0, 0 );

		char buf[ 32 ];
		memcpy( buf, context.source, 20 );
		buf[ 20 ] = 0;
		if ( strlen( buf ) > 14 ) buf[ 14 ] = 0;
		for ( u32 i = 0; i < strlen( buf ); i++ ) 
			if ( !( ( buf[ i ] >= 'a' && buf[ i ] <= 'z' ) ||
				    ( buf[ i ] >= 'A' && buf[ i ] <= 'Z' ) ||
					( buf[ i ] >= '0' && buf[ i ] <= '9' ) || buf[ i ] == '-' ) )
				 buf[ i ] = 32;
		printSpriteLayer( fb, buf, 80, 42+5 );
		sprintf( buf, "Module:" );
		printSpriteLayer( fb, buf, 0, 42+5 );
		//             012345678901234567890123
		sprintf( buf, "Channels: %d", context.num_channels );
		printSpriteLayer( fb, buf, 0, 50+5 );
		sprintf( buf, "Samples:  %d", context.used_num_samples );
		printSpriteLayer( fb, buf, 0, 58+5 );

		sprintf( buf, "Position: %02d", context.line );
		printSpriteLayer( fb, buf, 0, 78+5 );

		sprintf( buf, "Pattern:  %02d/%02d", context.pattern, context.length );
		printSpriteLayer( fb, buf, 0, 70+5 );

		sprintf( buf, "Speed:    %02d", context.ticks_per_line );
		printSpriteLayer( fb, buf, 0, 86+5 );
	}

	if ( playFileType == 1 )
	{
		//                     012345678901234567890123
		printSpriteLayer( fb, ".- Sidekick64 WAVPlay -.", 0, 0 );


		char buf[ 32 ];
		memcpy( buf, &FILENAME[9], 20 );
		buf[ 20 ] = 0;
		if ( strlen( buf ) > 14 ) buf[ 14 ] = 0;
		for ( int i = 0; i < 14; i++ )
			if ( buf[ i ] == '.' ) buf[ i ] = 0;
		for ( u32 i = 0; i < strlen( buf ); i++ ) 
			if ( !( ( buf[ i ] >= 'a' && buf[ i ] <= 'z' ) ||
				    ( buf[ i ] >= 'A' && buf[ i ] <= 'Z' ) ||
					( buf[ i ] >= '0' && buf[ i ] <= '9' ) || buf[ i ] == '-' ) )
				 buf[ i ] = 32;
		sprintf( buf, "File:    %s", buf );
		printSpriteLayer( fb, buf, 0, 42+5 );
		//             012345678901234567890123
		sprintf( buf, "Source:  %d Hz", header.sampleRate );
		printSpriteLayer( fb, buf, 0, 50+5 );

		sprintf( buf, "Time:" );
		printSpriteLayer( fb, buf, 0, 62+5 );
	}

	if ( playFileType == 2 )
	{
		//                     012345678901234567890123
		printSpriteLayer( fb, ".- Sidekick64 YM-Play -.", 0, 0 );

		char buf[ 32 ];

		ymMusicInfo_t info;
		pMusic->getMusicInfo( &info );
		if ( strlen( info.pSongName ) > 15 ) info.pSongName[ 15 ] = 0;
		if ( strlen( info.pSongAuthor ) > 15 ) info.pSongAuthor[ 15 ] = 0;
		if ( strlen( info.pSongPlayer ) > 15 ) info.pSongPlayer[ 15 ] = 0;

		sprintf( buf, "Title:   %s", info.pSongName );
		printSpriteLayer( fb, buf, 0, 42+5 );

		sprintf( buf, "Author:  %s", info.pSongAuthor );
		printSpriteLayer( fb, buf, 0, 50+5 );
		
		sprintf( buf, "Driver:  %s", info.pSongPlayer );
		printSpriteLayer( fb, buf, 0, 58+5 );

		sprintf( buf, "Time:" );
		printSpriteLayer( fb, buf, 0, 70+5 );
	}

	

	modJumpTo = -1;
	modVolumeInt = 100;

	fadeIn = 0;
	fadeOut = 0xffffffff;
	fadeVolume = 0;

	oscPos = 0;
	for ( int i = 0; i < 8 * 24; i++ )
		osc1[ i ] = osc2[ i ] = 128 / 11;

	rbRead = 0; rbWrite = ringbufSize;

	// type of SID at $d400, $d420, $d500, $de00, $df00: 2 = 8580, 3 = 6581, something else: undefined
	const u16 SIDaddresses[] = { 0xd400, 0xd420, 0xd500, 0xde00, 0xdf00 };

	int sid1 = 0, sid2 = 0;

	if ( typeSIDAddr[ 0 ] == 2 )
	{
		mahoneyLUTSID1 = lookup8580; 
		sid1 = 8580;
	} else
	{
		mahoneyLUTSID1 = lookup6581;
		sid1 = 6581;
	}

	addrSID2 = 0; 
	for ( int i = 1; i < 5; i++ )
	{
		if ( typeSIDAddr[ i ] == 2 || typeSIDAddr[ i ] == 3 )
		{
			addrSID2 = SIDaddresses[ i ];
			if ( typeSIDAddr[ i ] == 2 )
			{
				mahoneyLUTSID2 = lookup8580; 
				sid2 = 8580;
			} else
			{
				mahoneyLUTSID2 = lookup6581;
				sid2 = 6581;
			}
			break;
		}
	}

	{
		u8 temp[ 128 * 64 * 6 ];
		readFile( logger, (char*)DRIVE, (char*)FILENAME_SID_ICON, temp, &size );

		int yofs = 0;
		if ( sid1 == 6581 && sid2 == 0 ) yofs = 0;
		if ( sid1 == 8580 && sid2 == 0 ) yofs = 64;
		if ( sid1 == 6581 && sid2 == 6581 ) yofs = 128;
		if ( sid1 == 8580 && sid2 == 8580 ) yofs = 192;
		if ( sid1 == 6581 && sid2 == 8580 ) yofs = 256;
		if ( sid1 == 8580 && sid2 == 6581 ) yofs = 320;

		for ( int j = 0; j < 60; j++ )
			for ( int i = 0; i < 101; i++ )
			{
				if ( temp[ (j+yofs) * 128 + i ] )
					setpixel( 91+i, 147-62+j );
			}
	}

	printSpriteLayer( fb, "1/2 seek", 0, 115 );
	printSpriteLayer( fb, "3/4 FFT/OSC", 0, 123 );
	printSpriteLayer( fb, "SPACE blank", 0, 131 );
	printSpriteLayer( fb, "C= exit", 136, 131 );

	// now patch the code for the real SID addresses
	const u8 transferSampleX[] = { 0xAE, 0x55, 0xDF, 0x8E, 0x18, 0xD4, 0xAE, 0x56, 0xDF, 0x8E, 0x38, 0xD4 };
	const u8 transferSampleA[] = { 0xAD, 0x55, 0xDF, 0x8D, 0x18, 0xD4, 0xAD, 0x56, 0xDF, 0x8D, 0x38, 0xD4 };
	const u8 initSIDCode[] =  { 0x9D, 0x20, 0xD4, 0xE8, 0xE0, 0x18 };

	for ( int i = 0; i < (int)prgSize - 12; i++ )
	{
		u8 m1 = 0, m2 = 0;
		for ( int j = 0; j < 12; j++ )
		{
			if ( prgData[ i + j ] == transferSampleX[ j ] ) m1 ++;
			if ( prgData[ i + j ] == transferSampleA[ j ] ) m2 ++;
		}
		if ( m1 == 12 || m2 == 12 )
		{
			if ( addrSID2 == 0 )
			{
				prgData[ i + 10 ] = (0xd020+0x18) & 255;
				prgData[ i + 11 ] = (0xd020+0x18) >> 8;
			} else
			{
				prgData[ i + 10 ] = (addrSID2+0x18) & 255;
				prgData[ i + 11 ] = (addrSID2+0x18) >> 8;
			}
		}

		m1 = 0;
		for ( int j = 0; j < 6; j++ )
		{
			if ( prgData[ i + j ] == initSIDCode[ j ] ) m1 ++;
		}
		if ( m1 == 6 )
		{
			if ( addrSID2 == 0 )
			{
				prgData[ i + 1 ] = (0xc020) & 255;
				prgData[ i + 2 ] = (0xc020) >> 8;
			} else
			{
				prgData[ i + 1 ] = (addrSID2) & 255;
				prgData[ i + 2 ] = (addrSID2) >> 8;
			}
		}
	}

	startAddr = prgData[ 0 ] + prgData[ 1 ] * 256;
	endAddr = startAddr + prgSize - 2;
	prgSizeBelowA000 = 0xa000 - startAddr;
	if ( prgSizeBelowA000 > prgSize - 2 )
	{
		prgSizeBelowA000 = prgSize - 2;
		prgSizeAboveA000 = 0;
	} else
		prgSizeAboveA000 = prgSize - prgSizeBelowA000;

	memcpy( &prgData[ 0x5000 - startAddr + 2 ], fb, 8 * 7 * 64 );

	resetFromCodeState = 0;

	dirtyRead = dirtyWrite = 0;
	pauseScreenUpdate = 0;

	// send color scheme
/*	dirtyRB[ dirtyWrite ++ ] = ( 0x70a5 << 16 ) | skinValues.SKIN_MENU_TEXT_HEADER;	// top row
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a6 << 16 ) | skinValues.SKIN_MENU_TEXT_KEY;	// middle left
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a7 << 16 ) | skinValues.SKIN_MENU_TEXT_ITEM;	// middle right
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a8 << 16 ) | 11;	// bottom left
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a9 << 16 ) | 11;	// bottom right*/
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a5 << 16 ) | 0;	// top row
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a6 << 16 ) | 0;	// middle left
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a7 << 16 ) | 0;	// middle right
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a8 << 16 ) | 0;	// bottom left
	dirtyRB[ dirtyWrite ++ ] = ( 0x70a9 << 16 ) | 0;	// bottom right


	// setup FIQ
	prepareOnReset();

	// ready to go
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

	// warm caches
	prepareOnReset( true );

	nBytesRead = 0; stage = 1;
	u32 cycleCountC64_Stage1 = 0;
	c64CycleCount = resetCounter = 0;
	disableCart = transferStarted = currentOfs = 0;
	transferPart = 1;

	CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 512, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( launchCode, 512, 512 * 16 );

	SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );
	latchSetClear( LATCH_RESET, 0 );

	// wait forever
	while ( true )
	{
		if ( !disableCart && !c128PRG )
		{
			if ( ( ( stage == 1 ) && nBytesRead > 0 ) ||
 				 ( ( stage == 2 ) && nBytesRead < 32 && c64CycleCount - cycleCountC64_Stage1 > 500000 ) )
			{
				if ( stage == 1 ) 
				{ 
					stage = 2; cycleCountC64_Stage1 = c64CycleCount; 
				} else 
				{
					stage = 0;
					latchSetClear( 0, LATCH_RESET );
					DELAY(1<<11);
					latchSetClear( LATCH_RESET, 0 );
					nBytesRead = 0; stage = 1;
					c64CycleCount = 0;
				}
			}
		}

		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( c64CycleCount, resetCounter );

		if ( resetFromCodeState == 2 )
		{
			latchSetClear( 0, LATCH_RESET );
			EnableIRQs();
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			latchSetClearImm( 0, LATCH_RESET );
			return;		
		}
		#endif

		asm volatile ("wfi");

		if ( disableCart )
		{
			if ( firstHDMIOut == 0 && HDMIstarted == 0 )
			{
				HDMIstarted = 1;
			}

		#if 1
			int samplesInBuffer = ( rbWrite - rbRead + ringbufSize ) & ( ringbufSize - 1 );
			if ( samplesInBuffer < 4096 )
			{
				if ( fadeIn != 0xffffffff || fadeOut != 0xffffffff )
				{
					u8 fade = 0;

					if ( fadeIn != 0xffffffff )
					{
						fade = 5 - ( fadeIn >> 2 );
						fadeVolume = fadeIn * 256 / 20;
						fadeIn ++;
						if ( fade == 0 ) { fadeIn = 0xffffffff; fadeVolume = 256; }
					}
					if ( fadeOut != 0xffffffff )
					{
						fade = ( fadeOut >> 2 );
						fadeVolume = 256 - fadeOut * 256 / 20;
						fadeOut ++;
						if ( fade == 5 ) 
						{
							fadeOut = 0xffffffff;
							fadeVolume = 0; 
							resetFromCodeState = 2;
							latchSetClear( 0, LATCH_RESET );
						}
					}

					extern u8 fadeTabStep[ 16 ][ 5 ];
					if ( fade == 0 )
					{
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a5 << 16 ) | skinValues.SKIN_MENU_TEXT_HEADER;	// top row
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a6 << 16 ) | skinValues.SKIN_MENU_TEXT_KEY;	// middle left
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a7 << 16 ) | skinValues.SKIN_MENU_TEXT_ITEM;	// middle right
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a8 << 16 ) | 11;	// bottom left
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a9 << 16 ) | 11;	// bottom right
					} else
					{
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a5 << 16 ) | fadeTabStep[ skinValues.SKIN_MENU_TEXT_HEADER ][ fade - 1 ];	// top row
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a6 << 16 ) | fadeTabStep[ skinValues.SKIN_MENU_TEXT_KEY ][ fade - 1 ];	// middle left
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a7 << 16 ) | fadeTabStep[ skinValues.SKIN_MENU_TEXT_ITEM ][ fade - 1 ];	// middle right
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a8 << 16 ) | fadeTabStep[ 11 ][ fade - 1 ];	// bottom left
						dirtyRB[ dirtyWrite ++ ] = ( 0x70a9 << 16 ) | fadeTabStep[ 11 ][ fade - 1 ];	// bottom right
					}
				}

				computeSamplesAndScreenUpdate( fadeVolume );

			}
		#endif
		}
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

volatile u8 nextSample = 128;

extern unsigned m_nSubFrame;
	
const u8 IEC958Table[ 5 ] = 
{
	0b100,	// consumer, PCM, no copyright, no pre-emphasis
	0,	// category (general mode)
	0,	// source number, take no account of channel number
	2,	// sampling frequency 48khz
	0b1011 | (13 << 4) // 24 bit samples, original freq.
};

__attribute__( ( always_inline ) ) inline u32 ConvertIEC958Sample (u32 nSample, unsigned nFrame)
{
	nSample &= 0xFFFFFF;
	nSample <<= 4;

	if ( nFrame < IEC958_STATUS_BYTES * 8 && (IEC958Table[nFrame / 8] & BIT(nFrame % 8)) )
		nSample |= 0x40000000;

	if (__builtin_parity(nSample))
		nSample |= 0x80000000;

	if (nFrame == 0)
		nSample |= IEC958_B_FRAME_PREAMBLE;

	return nSample;
}

#ifdef COMPILE_MENU
void KernelMODplayFIQHandler( void *pParam )
#else
void CKernelMODplay::FIQHandler (void *pParam)
#endif
{
	register u32 D;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	CACHE_PRELOADL2STRM( &prgData[ currentOfs ] );

	// update some counters
	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

/*	if ( resetCounter > 3 && resetFromCodeState != 2 )
	{
		disableCart = transferStarted = 0;
		nBytesRead = 0; stage = 1;
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
		FINISH_BUS_HANDLING
		return;
	}*/

	// access to CBM80 ROM (launch code)
	if ( !disableCart && CPU_READS_FROM_BUS && ACCESS( ROM_LH ) )
	{
		WRITE_D0to7_TO_BUS( launchCode[ GET_ADDRESS + LAUNCH_BYTES_TO_SKIP ] );
		nBytesRead++;
	}

	if ( !disableCart && IO1_ACCESS ) 
	{
		if ( CPU_WRITES_TO_BUS ) 
		{
			transferStarted = 1;

			// any write to IO1 will (re)start the PRG transfer
			if ( GET_IO12_ADDRESS == 2 )
			{
				currentOfs = prgSizeBelowA000 + 2;
				transferPart = 1; 
				CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ prgSizeBelowA000 + 2 ];
			} else
			{
				currentOfs = 0;
				transferPart = 0;
				CACHE_PRELOADL2KEEP( &prgData[ 0 ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ 0 ];
			}
			return;
		} else
		// if ( CPU_READS_FROM_BUS ) 
		{
			if ( GET_IO12_ADDRESS == 1 )	
			{
				// $DE01 -> get number of 256-byte pages
				if ( transferPart == 1 ) // PRG part above $a000
					D = ( prgSizeAboveA000 + 255 ) >> 8;  else
					D = ( prgSizeBelowA000 + 255 ) >> 8; 
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			if ( GET_IO12_ADDRESS == 4 ) // full 256-byte pages 
			{
				D = ( prgSize - 2 ) >> 8;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			if ( GET_IO12_ADDRESS == 5 ) // bytes on last non-full 256-byte page
			{
				D = ( prgSize - 2 ) & 255;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			if ( GET_IO12_ADDRESS == 2 )	
			{
				// $DE02 -> get BASIC end address
				WRITE_D0to7_TO_BUS( (u8)( endAddr & 255 ) )
				FINISH_BUS_HANDLING
			} else
			if ( GET_IO12_ADDRESS == 3 )	
			{
				// $DE02 -> get BASIC end address
				WRITE_D0to7_TO_BUS( (u8)( (endAddr>>8) & 255 ) )
				FINISH_BUS_HANDLING
			} else
			{
				D = forceReadLaunch;	currentOfs ++;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			}
				
			return;
		}
	}

#if 1
	if ( disableCart )
	{
		c64CurCycleInFrame ++;
		if ( c64CurCycleInFrame >= c64CyclesPerFrame )
			c64CurCycleInFrame = 0;

		static u16 raddr = 0x5000, ofs = 0xc020 - 0x5000;
		static u8  rbyte = 0;
		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x58 )
		{
			WRITE_D0to7_TO_BUS( (u8)((raddr+ofs) & 255) );
			CACHE_PRELOADL2KEEP( &dirtyRB[ dirtyRead ] );
		}
		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x59 )
		{
			WRITE_D0to7_TO_BUS( (u8)((raddr+ofs) >> 8) );
			CACHE_PRELOADL2KEEP( &dirtyRB[ dirtyRead ] );
		}
		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x5a )
		{
			WRITE_D0to7_TO_BUS( rbyte );

			if ( dirtyRead != dirtyWrite && !pauseScreenUpdate )
			{
				ofs = dirtyRB[ dirtyRead ] >> 16;
				rbyte = dirtyRB[ dirtyRead ] & 255;
				dirtyRead ++;
				dirtyRead &= DIRTYRB_MASK;
			} else
			{
				ofs = 0xc020-0x5000;
				rbyte = armCycleCounter & 255;
			}
		}

		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x63 )
		{
//			const u8 g[5] = { 1, 15, 12, 11, 0 };
//			const u8 g[8] = { 1, 7, 3, 10, 4, 11, 6, 0 };
//			const u8 g[8] = { 1, 13, 3, 12, 8, 11, 9, 0 };
			//const u8 g[8] = { 9, 11, 8, 12, 3, 13, 1, 15 };
			//const u8 g[8] = { 1, 7, 15, 10, 8, 2, 9, 0 };
			const u8 g[8] = { 1, 13, 3, 14, 4, 11, 6, 0 };
			//const u8 g[8] = { 1, 13, 7, 10, 12, 4, 6, 0 };
			//const u8 g[8] = { 1, 7, 3, 10, 4, 2, 6, 0 };

			//WRITE_D0to7_TO_BUS( c64CurRasterLine )
			c64CurRasterLine = c64CurCycleInFrame / c64CyclesPerRasterline;

			if ( visMode == 0 )
			{
				if ( c64CurRasterLine < 50 || c64CurRasterLine >= 242 )
				{
					WRITE_D0to7_TO_BUS( 0 )
					OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
					return;
				} else
				{
					int bin = 47 - ( ( c64CurRasterLine - 50 ) / 4 );
					int line = ( c64CurRasterLine - 50 ) & 7;
					int v = colorbar[ bin >> 1 ];
					if ( line == 0 || line == 7 ) v += 2;
					if ( line == 1 || line == 6 ) v ++;
					if ( v > 7 ) v = 7;
					WRITE_D0to7_TO_BUS( g[ v ] )
					OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
					return;
				}
			}
			if ( c64CurRasterLine == c64LastRasterLine )
			{
				WRITE_D0to7_TO_BUS( c64LastColor )
				OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
				return;
			}
			int d = ( 128 - c64CurRasterLine );
			if ( d < 0 ) d = -d;

			if ( peakLevel == 0 )
			{
				WRITE_D0to7_TO_BUS( 0 )
				OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
				return;
			}

			d = (d << 21) / peakLevel;
			static u32 seed = 123456789;
			seed = (1664525*seed+1013904223);
			s32 ditherNoise = (s32)( seed >> 16 ) - 32768;
			static s32 triangleState = 0;
			d += ditherNoise - triangleState;
			d >>= 16;
			if ( d < 0 ) d = 0;
			if ( d > 7 ) d = 7;
			WRITE_D0to7_TO_BUS( g[ d ] )
			triangleState = ditherNoise;
			c64LastRasterLine = c64CurRasterLine;
			c64LastColor = g[ d ];
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		}

		if ( IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x62 ) //&& c64CycleCount > cycleFirstKeyHandling )
		{
			READ_D0to7_FROM_BUS( D )

			//if ( D == 123 )
			{
				//resetFromCodeState = 2;
				fadeOut = 0;
				OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
				return;
			}
		}

		if ( IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x63 ) // C64 writes here on rasterline $f9
		{
			c64CurCycleInFrame = 0xf9 * 63;
			//READ_D0to7_FROM_BUS( D )
			//c64CurRasterLine = D;
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		}

		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x61 ) //&& c64CycleCount > cycleFirstKeyHandling )
		{
			if ( playFileType == 0 )
			{
				if ( context.pattern < context.length - 1 )
					modJumpTo = context.pattern + 1;
			} else
			{
				modJumpTo = 1;
			}
		} else
		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x60 ) //&& c64CycleCount > cycleFirstKeyHandling )
		{
			if ( playFileType == 0 )
			{
				if ( context.pattern > 0 )
					modJumpTo = context.pattern - 1;
			} else
			{
				modJumpTo = 2;
			}
		} else
		if ( IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x57 && c64CycleCount > cycleFirstKeyHandling )
		{
			// additional key presses:
			// CIA return value for PA1, see https://retrocomputing.stackexchange.com/questions/6421/reading-both-keyboard-and-joystick-with-non-kernal-code-on-c64
			READ_D0to7_FROM_BUS( D )

			if ( D == 0b11111110 ) // '3'
			{
				visMode = 0;
			} else
			if ( D == 0b11110111 ) // '4'
			{
				visMode = 1;
			}
		}

	#if 1

		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x55 )
		{
		#if 1
			WRITE_D0to7_TO_BUS( nextSample )

			if ( !outputViaHDMI )
			{
				u8 s = ringbuf[ (rbRead + 1) & ( ringbufSize - 1 ) ];
				if ( modJumpTo > -1 ) s = 128;
				nextSample = mahoneyLUTSID2[ s ];
			}
		#endif
		}
		if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x56 )
		{
		#if 1
			WRITE_D0to7_TO_BUS( nextSample )

			if ( !outputViaHDMI )
			{
				wavPosition = (unsigned long long)c64CycleCount * (unsigned long long)MOD_sampleRate / c64ClockSpeed;
				if ( wavPosition < 0 ) wavPosition = 0;
				rbRead = wavPosition * 2;
				CACHE_PRELOADL1STRM( &ringbuf[ rbRead & ( ringbufSize - 1 ) ] );

				u8 s = ringbuf[ rbRead & ( ringbufSize - 1 ) ];
				if ( modJumpTo > -1 ) s = 128;
				nextSample = mahoneyLUTSID1[ s ];
			}
		#endif
		}

		#ifdef HDMI_SOUND_MODPLAY
		if ( outputViaHDMI )
		{
			if ( lastWavPositionHDMI != wavPosition )
			{
				if ( firstHDMIOut )
				{
					firstHDMIOut = 0;
				}

				//if ( hdmiSoundDevice->IsWritable() )
				if ( !( read32( RegMaiControl ) & BitMaiControlFull ) )
				{
					write32( RegMaiData, ringbufHDMI[ rbRead & ( ringbufSize - 1 ) ] );
					write32( RegMaiData, ringbufHDMI[ (rbRead+1) & ( ringbufSize - 1 ) ] );
				}

				lastWavPositionHDMI = wavPosition;
			}

			wavPosition = (unsigned long long)c64CycleCount * (unsigned long long)MOD_sampleRate / c64ClockSpeed;
			rbRead = wavPosition * 2;
			CACHE_PRELOADL1STRM( &ringbufHDMI[ rbRead & ( ringbufSize - 1 ) ] );
		}
		#endif

	#endif
	}

#endif

	if ( !disableCart && CPU_WRITES_TO_BUS && IO2_ACCESS ) // writing #123 to $df00 (IO2) will disable the cartridge
	{
		READ_D0to7_FROM_BUS( D )

		if ( GET_IO12_ADDRESS == 0 && D == 123 )
		{
			disableCart = 1;

			//samplesPlayed = 0;
			exitPlayer = 0;
			c64CycleCount = 0;

			SET_GPIO( bGAME | bEXROM | bNMI );
			FINISH_BUS_HANDLING
			return;
		}
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}




static u8 buffer4[ 4 ];
static u8 buffer2[ 2 ];

static void convertWAV2RAW_inplace( u8 *_data )
{
#if 1
	u8 *data = _data;
	u8 *rawOut = data;

	#define FREAD( dst, s ) { memcpy( (u8*)dst, data, s ); data += s; }

	FREAD( header.riff, sizeof( header.riff ) );

	FREAD( buffer4, sizeof( buffer4 ) );
	header.filesize = buffer4[ 0 ] | ( buffer4[ 1 ] << 8 ) | ( buffer4[ 2 ] << 16 ) | ( buffer4[ 3 ] << 24 );

	FREAD( header.wave, sizeof( header.wave ) );

	FREAD( header.fmtChunkMarker, sizeof( header.fmtChunkMarker ) );

	FREAD( buffer4, sizeof( buffer4 ) );
	header.fmtLength = buffer4[ 0 ] | ( buffer4[ 1 ] << 8 ) | ( buffer4[ 2 ] << 16 ) | ( buffer4[ 3 ] << 24 );

	FREAD( buffer2, sizeof( buffer2 ) ); 
	header.fmtType = buffer2[ 0 ] | ( buffer2[ 1 ] << 8 );

	FREAD( buffer2, sizeof( buffer2 ) );
	header.nChannels = buffer2[ 0 ] | ( buffer2[ 1 ] << 8 );

	FREAD( buffer4, sizeof( buffer4 ) );
	header.sampleRate = buffer4[ 0 ] | ( buffer4[ 1 ] << 8 ) | ( buffer4[ 2 ] << 16 ) | ( buffer4[ 3 ] << 24 );

	MOD_sampleRate = header.sampleRate;

	FREAD( buffer4, sizeof( buffer4 ) );
	header.byteRate = buffer4[ 0 ] | ( buffer4[ 1 ] << 8 ) | ( buffer4[ 2 ] << 16 ) | ( buffer4[ 3 ] << 24 );

	FREAD( buffer2, sizeof( buffer2 ) );

	header.blockAlign = buffer2[ 0 ] | ( buffer2[ 1 ] << 8 );

	FREAD( buffer2, sizeof( buffer2 ) );
	header.bpp = buffer2[ 0 ] | ( buffer2[ 1 ] << 8 );

	FREAD( header.dataChunkHeader, sizeof( header.dataChunkHeader ) );

	FREAD( buffer4, sizeof( buffer4 ) );
	header.dataSize = buffer4[ 0 ] | ( buffer4[ 1 ] << 8 ) | ( buffer4[ 2 ] << 16 ) | ( buffer4[ 3 ] << 24 );

	long num_samples = ( 8 * header.dataSize ) / ( header.nChannels * header.bpp );

	long size_of_each_sample = ( header.nChannels * header.bpp ) / 8;

	// duration in secs: (float)header.filesize / header.byteRate;

	nWAVSamples = 0;

	if ( header.fmtType == 1 ) // PCM
	{ 
		char data_buffer[ size_of_each_sample ];

		long bytes_in_each_channel = ( size_of_each_sample / header.nChannels );

		if ( ( bytes_in_each_channel  * header.nChannels ) == size_of_each_sample ) // size if correct?
		{
			for ( u32 i = 1; i <= num_samples; i++ ) {
				FREAD( data_buffer, sizeof( data_buffer ) );

				unsigned int  xnChannels = 0;
				int data_in_channel = 0;
				int offset = 0; // move the offset for every iteration in the loop below
					
				for ( xnChannels = 0; xnChannels < header.nChannels; xnChannels++ ) 
				{
					if ( bytes_in_each_channel == 4 ) 
					{
						data_in_channel = ( data_buffer[ offset ] & 0x00ff ) | ( ( data_buffer[ offset + 1 ] & 0x00ff ) << 8 ) | ( ( data_buffer[ offset + 2 ] & 0x00ff ) << 16 ) | ( data_buffer[ offset + 3 ] << 24 );
						data_in_channel += 2147483648;
						data_in_channel >>= 24;
					} else 
					if ( bytes_in_each_channel == 2 ) 
					{
						data_in_channel = ( data_buffer[ offset ] & 0x00ff ) | ( data_buffer[ offset + 1 ] << 8 );
						data_in_channel += 32768;
						data_in_channel >>= 8;
					} else 
					if ( bytes_in_each_channel == 1 ) 
					{
						data_in_channel = data_buffer[ offset ] & 0x00ff; // 8 bit unsigned
					}

					if ( header.nChannels == 1 )
					{
						rawOut[ 0 ] = rawOut[ 1 ] = (u8)data_in_channel;
						rawOut += 2;
						nWAVSamples ++;
					} else
					{
						if ( xnChannels < 2 )
						{
							*rawOut = (u8)data_in_channel;
							rawOut ++;
							if ( xnChannels == 1 )
								nWAVSamples ++;
						}
					}

					offset += bytes_in_each_channel;
				} 
			} 
		} 
	} 
#endif
}
