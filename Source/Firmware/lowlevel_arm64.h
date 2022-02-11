/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 lowlevel_arm64.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - low-level ARM code 
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
#ifndef _lowlevel_arm_h
#define _lowlevel_arm_h

#include <circle/types.h>

// default timings
#define	AUTO_TIMING_RPI3PLUS_C64		1
#define	AUTO_TIMING_RPI3PLUS_C64C128	2
#define	AUTO_TIMING_RPI0_C64			3
#define	AUTO_TIMING_RPI0_C64C128		4

#define	AUTO_TIMING_RPI3_264			0
#define	AUTO_TIMING_RPI0_264			1

extern void setDefaultTimings( int mode );
extern void setDefaultTimings264( int mode );

#ifndef MACHINE_C264
extern u32 WAIT_FOR_SIGNALS;
extern u32 WAIT_CYCLE_MULTIPLEXER;
extern u32 WAIT_CYCLE_READ;
extern u32 WAIT_CYCLE_READ_BADLINE;
extern u32 WAIT_CYCLE_READ_VIC2;
extern u32 WAIT_CYCLE_WRITEDATA;
extern u32 WAIT_CYCLE_WRITEDATA_VIC2;
extern u32 WAIT_CYCLE_MULTIPLEXER_VIC2;
extern u32 WAIT_TRIGGER_DMA;
extern u32 WAIT_RELEASE_DMA;

extern u32 modeC128;
extern u32 modeVIC, modePALNTSC;
extern u32 hasSIDKick;
#else
extern u32 WAIT_CYCLE_READ;
extern u32 WAIT_CYCLE_WRITEDATA;
#endif

extern u32 machine264;



// #cycles the C64 is delayed for prefetching data (a little bit less should be OK)
#define NUM_DMA_CYCLES (10)		


#define PMCCFILTR_NSH_EN_BIT    27
#define PMCNTENSET_C_EN_BIT     31
#define PMCR_LC_EN_BIT          6
#define PMCR_C_RESET_BIT        2
#define PMCR_EN_BIT             0

#define AA __attribute__ ((aligned (64)))
#define AAA __attribute__ ((aligned (128)))

#define BEGIN_CYCLE_COUNTER \
						  		u64 armCycleCounter; \
								armCycleCounter = 0; \
								asm volatile( "MRS %0, PMCCNTR_EL0" : "=r" (armCycleCounter) );

#define RESTART_CYCLE_COUNTER \
								asm volatile( "MRS %0, PMCCNTR_EL0" : "=r" (armCycleCounter) );

#define READ_CYCLE_COUNTER( cc ) \
								asm volatile( "MRS %0, PMCCNTR_EL0" : "=r" (cc) );


#define WAIT_UP_TO_CYCLE( wc ) { \
								u64 cc2; \
								do { \
									asm volatile( "MRS %0, PMCCNTR_EL0" : "=r" (cc2) ); \
								} while ( (cc2) < (wc+armCycleCounter) ); }

#define WAIT_UP_TO_CYCLE_AFTER( wc, cc ) { \
								u64 cc2; \
								do { \
									asm volatile( "MRS %0, PMCCNTR_EL0" : "=r" (cc2) ); \
								} while ( (cc2-cc) < (wc) ); }

#define CACHE_PRELOADL1KEEP( ptr )	{ asm volatile ("prfm PLDL1KEEP, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADL1STRM( ptr )	{ asm volatile ("prfm PLDL1STRM, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADL1KEEPW( ptr ) { asm volatile ("prfm PSTL1KEEP, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADL1STRMW( ptr ) { asm volatile ("prfm PSTL1STRM, [%0]" :: "r" (ptr)); }

#define CACHE_PRELOADL2KEEP( ptr )	{ asm volatile ("prfm PLDL2KEEP, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADL2KEEPW( ptr )	{ asm volatile ("prfm PSTL2KEEP, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADL2STRM( ptr )	{ asm volatile ("prfm PLDL2STRM, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADL2STRMW( ptr )	{ asm volatile ("prfm PSTL2STRM, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADI( ptr )		{ asm volatile ("prfm PLIL1STRM, [%0]" :: "r" (ptr)); }
#define CACHE_PRELOADIKEEP( ptr )	{ asm volatile ("prfm PLIL1KEEP, [%0]" :: "r" (ptr)); }

#define CACHE_PRELOAD_INSTRUCTION_CACHE( p, size )			\
	{ u8 *ptr = (u8*)( p );									\
	for ( register u32 i = 0; i < (size+63) / 64; i++ )	{	\
		CACHE_PRELOADIKEEP( ptr );							\
		ptr += 64;											\
	} }

#define CACHE_PRELOAD_DATA_CACHE( p, size, FUNC )			\
	{ u8 *ptr = (u8*)( p );									\
	for ( register u32 i = 0; i < (size+63) / 64; i++ )	{	\
		FUNC( ptr );										\
		ptr += 64;											\
	} }

#define FORCE_READ_LINEAR( p, size ) {						\
		__attribute__((unused)) volatile u8 forceRead;		\
		for ( register u32 i = 0; i < size; i++ )			\
			forceRead = ((u8*)p)[ i ];						\
	}

#define FORCE_READ_LINEARa( p, size, acc ) {				\
		__attribute__((unused)) volatile u8 forceRead;		\
		for ( register u32 i = 0; i < acc; i++ )			\
			forceRead = ((u8*)p)[ i % size ];				\
	}

#define ADDR_LINEAR2CACHE_T(l) ((((l)&255)<<5)|(((l)>>8)&31))

#define FORCE_READ_LINEAR_CACHE( p, size, rounds ) {		\
		__attribute__((unused)) volatile u8 forceRead;		\
		for ( register u32 i = 0; i < size*rounds; i++ )	\
			forceRead = ((u8*)p)[ ADDR_LINEAR2CACHE_T(i%size) ]; \
	}

#define FORCE_READ_LINEAR32_SKIP( p, size ) {				\
		__attribute__((unused)) volatile u32 forceRead;		\
		for ( register u32 i = 0; i < size/4; i+=2 )		\
			forceRead = ((u32*)p)[ i ];						\
	}

#define FORCE_READ_LINEAR64( p, size ) {					\
		__attribute__((unused)) volatile u64 forceRead;		\
		for ( register u32 i = 0; i < size/8; i++ )			\
			forceRead = ((u64*)p)[ i ];						\
	}

#define FORCE_READ_LINEAR32( p, size ) {					\
		__attribute__((unused)) volatile u32 forceRead;		\
		for ( register u32 i = 0; i < size/4; i++ )			\
			forceRead = ((u32*)p)[ i ];						\
	}

#define FORCE_READ_LINEAR32a( p, size, acc ) {				\
		__attribute__((unused)) volatile u32 forceRead;		\
		for ( register u32 i = 0; i < acc; i++ )			\
			forceRead = ((u32*)p)[ i % (size/4) ];			\
	}

#define FORCE_READ_RANDOM( p, size, acc ) {					\
	__attribute__((unused)) volatile u32 forceRead;			\
	u32 seed = 123456789;	u32 *ptr32 = (u32*)p;			\
	for ( register u32 i = 0; i < acc; i++ ) {				\
		seed = (1103515245*seed+12345) & ((u32)(1<<31)-1 );	\
		forceRead = ptr32[ seed % ( size / 4 ) ];			\
	} }

#define _LDNP_2x32( addr, val1, val2 ) {						\
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val1), "=r" (val2) : "r" (addr) : "memory" ); }

#define _LDNP_1x32( addr, val ) { u32 tmp;					\
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val), "=r" (tmp) : "r" (addr) : "memory" ); }

#define _LDNP_1x16( addr, val ) { u32 tmp1, tmp2;					\
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (tmp1), "=r" (tmp2) : "r" (addr) : "memory" ); val = tmp1 & 65535; }

#define _LDNP_1x8( addr, val ) { u32 tmp1, tmp2;					\
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (tmp1), "=r" (tmp2) : "r" (addr) : "memory" ); val = tmp1 & 255; }

#define SET_GPIO( set )	write32( ARM_GPIO_GPSET0, (set) );
#define CLR_GPIO( clr )	write32( ARM_GPIO_GPCLR0, (clr) );

#define	SETCLR_GPIO( set, clr )	{SET_GPIO( set )	CLR_GPIO( clr )}

#define PUT_DATA_ON_BUS_AND_CLEAR257( D ) \
		register u32 DD = ( (D) & 255 ) << D0;											\
		write32( ARM_GPIO_GPSET0, DD  );												\
		write32( ARM_GPIO_GPCLR0, (D_FLAG & ( ~DD )) | (1 << GPIO_OE) | bCTRL257 );		\
		WAIT_UP_TO_CYCLE( WAIT_CYCLE_READ );											\
		write32( ARM_GPIO_GPSET0, (1 << GPIO_OE) );													


#define PUT_DATA_ON_BUS_AND_CLEAR257_NO_WAIT( D ) \
		register u32 DD = ( (D) & 255 ) << D0;											\
		write32( ARM_GPIO_GPSET0, DD  );												\
		write32( ARM_GPIO_GPCLR0, (D_FLAG & ( ~DD )) | (1 << GPIO_OE) | bCTRL257 );		\
		disableDataBus = 1;


#define PUT_DATA_ON_BUS_AND_CLEAR257_VIC2( D ) \
		register u32 DD = ( (D) & 255 ) << D0;											\
		write32( ARM_GPIO_GPSET0, DD  );												\
		write32( ARM_GPIO_GPCLR0, (D_FLAG & ( ~DD )) | (1 << GPIO_OE) | bCTRL257 );		\
		WAIT_UP_TO_CYCLE( WAIT_CYCLE_READ_VIC2 );										\
		write32( ARM_GPIO_GPSET0, (1 << GPIO_OE) );													

#define PUT_DATA_ON_BUS_AND_CLEAR257_BADLINE( D ) \
		register u32 DD = ( (D) & 255 ) << D0;											\
		write32( ARM_GPIO_GPSET0, DD  );												\
		write32( ARM_GPIO_GPCLR0, (D_FLAG & ( ~DD )) | (1 << GPIO_OE) | bCTRL257 );		\
		WAIT_UP_TO_CYCLE( WAIT_CYCLE_READ_BADLINE );									\
		write32( ARM_GPIO_GPSET0, (1 << GPIO_OE) );													

#define GET_DATA_FROM_BUS_AND_CLEAR257( D ) \
			SET_BANK2_INPUT															\
			write32( ARM_GPIO_GPCLR0, (1 << GPIO_OE) | bCTRL257 );					\
			WAIT_UP_TO_CYCLE( WAIT_CYCLE_WRITEDATA );								\
			D = ( read32( ARM_GPIO_GPLEV0 ) >> D0 ) & 255;							\
			write32( ARM_GPIO_GPSET0, 1 << GPIO_OE );								\
			SET_BANK2_OUTPUT														

extern void initCycleCounter();

#define RESET_CPU_CYCLE_COUNTER \
	asm volatile( "msr PMCR_EL0, %0" : : "r" ( ( 1 << PMCR_LC_EN_BIT ) | ( 1 << PMCR_C_RESET_BIT ) | ( 1 << PMCR_EN_BIT ) ) ); 

extern __attribute__( ( always_inline ) ) inline void LDNP_2x32( unsigned long addr, u32 &val1, u32 &val2 );
extern __attribute__( ( always_inline ) ) inline u32 LDNP_1x32( unsigned long addr );
extern __attribute__( ( always_inline ) ) inline u16 LDNP_1x16( unsigned long addr );
__attribute__( ( always_inline ) ) inline u8 LDNP_1x8( void *addr )
{
	u32 val1, val2;
    __asm__ __volatile__("ldnp %0, %1, [%2]\n\t" : "=r" (val1), "=r" (val2) : "r" ((unsigned long)addr) : "memory");
	return val1 & 255;
}


#endif

 