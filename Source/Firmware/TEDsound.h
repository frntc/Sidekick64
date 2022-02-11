/* 
This TED sound emulation is taken from YapeSDL (https://github.com/calmopyrin/yapesdl), only one minor change has been made (always retrieving a single sample value).
The following lists the license that applies to Yape:

README.SDL

This is the README for the SDL port of Yape/SDL (Yet Another Plus/4 Emulator)

Current version is 0.70.2

LEGAL
=====

  Yape for SDL is 
  (C) 2000,2001,2004,2007,2008,2015-2017 Attila Grósz (gyros@freemail.hu)
  http://yape.plus4.net

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not see <http://www.gnu.org/licenses/>.
*/

#define TED_CLOCK (312*114*500)
#define TED_REAL_CLOCK_M10 17734475
#define TED_SOUND_CLOCK (TED_CLOCK / 10 )
#define TED_REAL_SOUND_CLOCK (TED_REAL_CLOCK_M10 / 10 / 8)

#define PRECISION 4
#define OSCRELOADVAL (0x3FF << PRECISION)

static int             Volume;
static int             channelStatus[2];
static int             SndNoiseStatus;
static int             DAStatus;
static unsigned short  Freq[2];
static int             NoiseCounter;
static int             FlipFlop;
static int             oscCount[2];
static int             OscReload[2];
static int             oscStep;
static unsigned char    noise[256]; // 0-8
static unsigned int		MixingFreq;
static unsigned int		originalFreq;
static int				volumeTable[64];
static int				cachedDigiSample;
static int				cachedSoundSample[2];

void setClockStep( unsigned int originalFreq, unsigned int samplingFreq )
{
	oscStep = (int)( ( originalFreq * (double)( 1 << PRECISION ) ) / (double)(samplingFreq)+0.5 );
}

void setFrequency( unsigned int frequency )
{
	originalFreq = frequency;
	setClockStep( frequency, MixingFreq );
}

void setSampleRate(unsigned int sampleRate)
{
	MixingFreq = sampleRate;
	setClockStep(originalFreq, sampleRate);
}

void tedSoundInit( unsigned int mixingFreq )
{
	originalFreq = TED_SOUND_CLOCK / 8;
	MixingFreq = mixingFreq;
	setClockStep( originalFreq, MixingFreq );
	FlipFlop = 0;
	oscCount[ 0 ] = oscCount[ 1 ] = 0;
	NoiseCounter = 0;
	Freq[ 0 ] = Freq[ 1 ] = 0;
	DAStatus = cachedDigiSample = 0;
	cachedSoundSample[ 0 ] = cachedSoundSample[ 1 ] = 0;

	/* initialise im with 0xa8 */
	int im = 0xa8;
	for ( int i = 0; i < 256; i++ ) {
		noise[ i ] = ( im & 1 ) * 0x20;
		im = ( im << 1 ) + ( 1 ^ ( ( im >> 7 ) & 1 ) ^ ( ( im >> 5 ) & 1 ) ^ ( ( im >> 4 ) & 1 ) ^ ( ( im >> 1 ) & 1 ) );
	}
	for ( int i = 0; i < 64; i++ ) {
		//		volumeTable[i] = (i & 0x0F && i & 0x30) ? (586 + (((i & 0x0F) < 9 ? (i & 0x0F) : 8) - 1) * 1024) << (((i & 0x30) == 0x30) ? 1 : 0) : 0;
		volumeTable[ i ] = ( i & 0x0F && i & 0x30 ) ? ( ( 586 + ( ( ( i & 0x0F ) < 9 ? ( i & 0x0F ) : 8 ) - 1 ) * 1024 ) << ( ( ( i & 0x30 ) == 0x30 ) ? 1 : 0 ) ) / -2 :
			( ( 586 + ( ( ( i & 0x0F ) < 9 ? ( i & 0x0F ) : 8 ) - 1 ) * 1024 ) << ( ( ( i & 0x30 ) == 0x30 ) ? 1 : 0 ) ) / 2;
	}
}


short TEDcalcNextSample()
{
	if ( DAStatus )
	{
		return cachedDigiSample;
	} else
	{
		// Channel 1
		if ( ( oscCount[ 0 ] += oscStep ) >= OSCRELOADVAL )
		{
			if ( OscReload[ 0 ] != ( 0x3FF << PRECISION ) )
			{
				FlipFlop ^= 0x10;
				cachedSoundSample[ 0 ] = volumeTable[ Volume | ( FlipFlop & channelStatus[ 0 ] ) ];
			}
			oscCount[ 0 ] = OscReload[ 0 ] + ( oscCount[ 0 ] - OSCRELOADVAL );
		}
		// Channel 2
		if ( ( oscCount[ 1 ] += oscStep ) >= OSCRELOADVAL )
		{
			if ( OscReload[ 1 ] != ( 0x3FF << PRECISION ) )
			{
				FlipFlop ^= 0x20;
				if ( ++NoiseCounter == 256 )
					NoiseCounter = 0;
				cachedSoundSample[ 1 ] = volumeTable[ Volume | ( FlipFlop & channelStatus[ 1 ] ) | ( noise[ NoiseCounter ] & SndNoiseStatus ) ];
			}
			oscCount[ 1 ] = OscReload[ 1 ] + ( oscCount[ 1 ] - OSCRELOADVAL );
		}
		return cachedSoundSample[ 0 ] + cachedSoundSample[ 1 ];
	}
}

inline void setFreq( unsigned int channel, int freq )
{
	if ( freq == 0x3FE ) {
		FlipFlop |= 0x10 << channel;
		if ( !channel )
			cachedSoundSample[ 0 ] = volumeTable[ Volume | ( channelStatus[ 0 ] ) ];
		else
			cachedSoundSample[ 1 ] = volumeTable[ Volume | ( channelStatus[ 1 ] | ( SndNoiseStatus >> 1 ) ) ];
	}
	OscReload[ channel ] = ( ( freq + 1 ) & 0x3FF ) << PRECISION;
}

void writeSoundReg( unsigned int reg, unsigned char value )
{
	switch ( reg ) {
	case 0:
		Freq[ 0 ] = ( Freq[ 0 ] & 0x300 ) | value;
		setFreq( 0, Freq[ 0 ] );
		break;
	case 1:
		Freq[ 1 ] = ( Freq[ 1 ] & 0x300 ) | value;
		setFreq( 1, Freq[ 1 ] );
		break;
	case 2:
		Freq[ 1 ] = ( Freq[ 1 ] & 0xFF ) | ( value << 8 );
		setFreq( 1, Freq[ 1 ] );
		break;
	case 3:
		if ( ( DAStatus = ( value & 0x80 ) ) ) {
			FlipFlop = 0x30;
			oscCount[ 0 ] = OscReload[ 0 ];
			oscCount[ 1 ] = OscReload[ 1 ];
			NoiseCounter = 0xFF;
			cachedDigiSample = volumeTable[ value & 0x3F ];
		}
		Volume = value & 0x0F;
		channelStatus[ 0 ] = value & 0x10;
		channelStatus[ 1 ] = value & 0x20;
		SndNoiseStatus = ( ( value & 0x40 ) >> 1 ) & ( channelStatus[ 1 ] ^ 0x20 );
		cachedSoundSample[ 0 ] = volumeTable[ Volume | ( FlipFlop & channelStatus[ 0 ] ) ];
		cachedSoundSample[ 1 ] = volumeTable[ Volume | ( FlipFlop & channelStatus[ 1 ] ) | ( noise[ NoiseCounter ] & SndNoiseStatus ) ];
		break;
	case 4:
		Freq[ 0 ] = ( Freq[ 0 ] & 0xFF ) | ( value << 8 );
		setFreq( 0, Freq[ 0 ] );
		break;
	}
}
 