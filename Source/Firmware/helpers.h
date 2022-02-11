/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 helpers.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - misc code and lots of macros
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
#ifndef _helpers_h
#define _helpers_h

#include <SDCard/emmc.h>
#include <fatfs/ff.h>

extern int readFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 *size );
extern int getFileSize( CLogger *logger, const char *DRIVE, const char *FILENAME, u32 *size );
extern int writeFile( CLogger *logger, const char *DRIVE, const char *FILENAME, u8 *data, u32 size );

#define START_AND_READ_ADDR0to7_RW_RESET_CS	\
	register u32 g2, g3;					\
	BEGIN_CYCLE_COUNTER						\
	WAIT_UP_TO_CYCLE( WAIT_FOR_SIGNALS );	\
	g2 = read32( ARM_GPIO_GPLEV0 );			\
	write32( ARM_GPIO_GPSET0, bCTRL257 );	

#define START_AND_READ_ADDR0to7_RW_RESET_CS_NO_MULTIPLEX \
	register u32 g2, g3;					\
	BEGIN_CYCLE_COUNTER						\
	WAIT_UP_TO_CYCLE( WAIT_FOR_SIGNALS );	\
	g2 = read32( ARM_GPIO_GPLEV0 );			

#define READ_ADDR0to7_RW_RESET_CS_AND_MULTIPLEX	\
	g2 = read32( ARM_GPIO_GPLEV0 );			\
	write32( ARM_GPIO_GPSET0, bCTRL257 );	


#define WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA	\
	WAIT_UP_TO_CYCLE( WAIT_CYCLE_MULTIPLEXER ); \
	g3 = read32( ARM_GPIO_GPLEV0 );				

#define WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA_VIC2	\
	WAIT_UP_TO_CYCLE( WAIT_CYCLE_MULTIPLEXER_VIC2 ); \
	g3 = read32( ARM_GPIO_GPLEV0 );				

#define READ_ADDR8to12_ROMLH_IO12_BA	\
	g3 = read32( ARM_GPIO_GPLEV0 );				

#define READ_D0to7_FROM_BUS( D )	{ GET_DATA_FROM_BUS_AND_CLEAR257( D ) }
#define WRITE_D0to7_TO_BUS( D )		{ PUT_DATA_ON_BUS_AND_CLEAR257( D ) }
#define WRITE_D0to7_TO_BUS_VIC( D )	{ PUT_DATA_ON_BUS_AND_CLEAR257_VIC2( D ) }
#define WRITE_D0to7_TO_BUS_BADLINE( D )	{ PUT_DATA_ON_BUS_AND_CLEAR257_BADLINE( D ) }

#define FINISH_BUS_HANDLING						\
	write32( ARM_GPIO_GPCLR0, bCTRL257 );		\
	RESET_CPU_CYCLE_COUNTER					

#define OUTPUT_LATCH_AND_FINISH_BUS_HANDLING	\
	write32( ARM_GPIO_GPCLR0, bCTRL257 );		\
	outputLatch();								\
	RESET_CPU_CYCLE_COUNTER					

#define NO_IO12_ACCESS		((g3 & bIO1) && (g3 & bIO2))
#define IO1_ACCESS			(!(g3 & bIO1))
#define IO2_ACCESS			(!(g3 & bIO2))
#define IO1_OR_IO2_ACCESS	(IO1_ACCESS||IO2_ACCESS)
#define ROML_ACCESS			(!(g3 & bROML))
#define ROMH_ACCESS			(!(g3 & bROMH))
#define ROML_OR_ROMH_ACCESS (ROML_ACCESS||ROMH_ACCESS)

#define GET_IO12_ADDRESS	 ((g2>>A0)&255)
#define GET_ADDRESS0to7		 ((g2>>A0)&255)
#define GET_ADDRESS8to12	 ((g3>>A8)&31)
#define GET_ADDRESS			 (GET_ADDRESS0to7|(GET_ADDRESS8to12<<8))
#define GET_ADDRESS0to13	 (GET_ADDRESS0to7|(GET_ADDRESS8to12<<8)|(((g2>>A13)&1)<<13))
#define GET_ADDRESS_CACHEOPT ((GET_ADDRESS0to7<<5)|GET_ADDRESS8to12)

#define ACCESS(SIGNAL)		(!(g3&SIGNAL))
#define KERNAL_ACCESS		(!(g3&bCS))
#define SID_ACCESS			(!(g2&bCS))

#define CPU_RESET			(!(g2&bRESET)) 
#define BUTTON_PRESSED		(!(g3&(1<<BUTTON)))

#define CPU_READS_FROM_BUS	(g2 & bRW)
#define CPU_WRITES_TO_BUS	(!(g2 & bRW))

#define VIC_HALF_CYCLE		(!(g2 & bPHI))
#define VIC_BADLINE			(!(g3 & bBA) && (g2 & bRW))

#define ADDR_LINEAR2CACHE(l) ((((l)&255)<<5)|(((l)>>8)&31))

#define CLEAR_LEDS_EVERY_8K_CYCLES		\
	static u32 cycleCount = 0;			\
	if ( !((++cycleCount)&8191) )		\
		clrLatchFIQ( LATCH_LED_ALL );

#define STANDARD_SETUP_TIMER_INTERRUPT_CYCLECOUNTER_GPIO										\
	boolean bOK = TRUE;																			\
	m_CPUThrottle.SetSpeed( CPUSpeedMaximum );													\
	if ( bOK ) bOK = m_Screen.Initialize();														\
	if ( bOK ) { 																				\
		CDevice *pTarget = m_DeviceNameService.GetDevice( m_Options.GetLogDevice(), FALSE );	\
		if ( pTarget == 0 )	pTarget = &m_Screen;												\
		bOK = m_Logger.Initialize( pTarget ); 													\
	}																							\
	if ( bOK ) bOK = m_Interrupt.Initialize(); 													\
	if ( bOK ) bOK = m_Timer.Initialize();														\
	/* initialize ARM cycle counters (for accurate timing) */ 									\
	initCycleCounter(); 																		\
	/* initialize GPIOs */ 																		\
	gpioInit(); 																				\
	/* initialize latch and software I2C buffer */												\
	initLatch();



#define TEST_FOR_JUMP_TO_MAINMENU( c64CycleCount, resetCounter )		\
		if ( c64CycleCount > 2000000 && resetCounter > 500000 ) {		\
			EnableIRQs();												\
			m_InputPin.DisableInterrupt();								\
			m_InputPin.DisconnectInterrupt();							\
			return;														\
		}

#define TEST_FOR_JUMP_TO_MAINMENU_CB( c64CycleCount, resetCounter, CB )	\
		if ( c64CycleCount > 2000000 && resetCounter > 500000 ) {		\
			CB;															\
			EnableIRQs();												\
			m_InputPin.DisableInterrupt();								\
			m_InputPin.DisconnectInterrupt();							\
			return;														\
		}

#define TEST_FOR_JUMP_TO_MAINMENU2FIQs( c64CycleCount, resetCounter )	\
		if ( c64CycleCount > 2000000 && resetCounter > 500000 ) {		\
			EnableIRQs();												\
			m_InputPin.DisableInterrupt2();								\
			m_InputPin.DisableInterrupt();								\
			m_InputPin.DisconnectInterrupt();							\
			return;														\
		}

#define TEST_FOR_JUMP_TO_MAINMENU2FIQs_CB( c64CycleCount, resetCounter, CB ) \
		if ( c64CycleCount > 2000000 && resetCounter > 500000 ) {		\
			CB;															\
			EnableIRQs();												\
			if ( irqFallingEdge ) m_InputPin.DisableInterrupt2();		\
			m_InputPin.DisableInterrupt();								\
			m_InputPin.DisconnectInterrupt();							\
			return;														\
		}

#define MAINLOOP( cbReset, c64CycleCount, resetCounter, cyclesSinceReset, resetReleased )			\
waitFor64:																							\
	u32 firstBoot = 3;																				\
	u32 lastCycleCount = c64CycleCount;																\
	/**/																							\
	while ( lastCycleCount == c64CycleCount ) { /* true as long as the C64 is not running */		\
		static u32 curLED = LATCH_LED0;																\
		clrLatchFIQ( LATCH_LED0 | LATCH_LED1 | LATCH_LED2 | LATCH_LED3 ); setLatchFIQ( curLED ); 	\
		curLED <<= 1; if ( curLED > LATCH_LED3 ) curLED = LATCH_LED0; outputLatch();				\
		for ( int i = 0; i < (1<<24); i++ ) {														\
			asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" );	asm volatile( "nop" ); \
		}																							\
	};																								\
	/**/																							\
	clrLatchFIQ( LATCH_LED_ALL );																	\
	outputLatch();																					\
	/**/																							\
	while ( true )																					\
	{																								\
		if ( firstBoot == 1 && c64CycleCount > lastCycleCount + 500000 ) {							\
			firstBoot = 2; lastCycleCount = c64CycleCount;											\
			clrLatchFIQ( LATCH_RESET ); setLatchFIQ( LATCH_LEDR ); outputLatch();					\
		}																							\
		if ( firstBoot == 2 && c64CycleCount > lastCycleCount + 5000 ) {							\
			firstBoot = 0;																			\
			clrLatchFIQ( LATCH_LED1to3 ); setLatchFIQ( LATCH_RESET | LATCH_LED0 ); outputLatch();	\
			goto C64IsRunning;																		\
		}																							\
		if ( firstBoot == 4 && cyclesSinceReset > 50 ) {											\
			firstBoot = 1; lastCycleCount = c64CycleCount;											\
			setLatchFIQ( LATCH_LEDG ); outputLatch();												\
		}																							\
		/**/																						\
		if ( resetCounter > 30 ) {																	\
			if ( firstBoot == 3 ) {																	\
				firstBoot = 4; lastCycleCount = c64CycleCount;										\
			}																						\
			cbReset();																				\
		}																							\
		/**/																						\
		asm volatile ("wfi");																		\
	}																								\
C64IsRunning:																						\
	u32 check = 0;																					\
	resetCounter = 0;																				\
	lastCycleCount = c64CycleCount;																	\
	while ( true ) {																				\
		if ( resetCounter > 1000000 ) {																\
			setLatchFIQ( LATCH_LEDB ); outputLatch();												\
		}																							\
		if ( ++check >= 1024 * 2048 ) { 															\
			if ( lastCycleCount == c64CycleCount ) goto waitFor64;									\
			lastCycleCount = c64CycleCount;	check = 0;												\
		}																							\
		/**/																						\
		if ( resetCounter > 30 && resetReleased )												\
			cbReset();																				\
		/**/																						\
		asm volatile ("wfi");																		\
	}

#define	UPDATE_COUNTERS( c64CycleCount, resetCounter, resetPressed, resetReleased, cyclesSinceReset )	\
	cyclesSinceReset ++; c64CycleCount ++;																\
	if ( !( g2 & bRESET ) ) {																			\
		resetReleased = 0; resetPressed = 1; resetCounter ++;											\
	} else {																							\
		if ( resetPressed )	resetReleased = 1;															\
		resetPressed = 0;																				\
	}

#define	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )	\
	c64CycleCount ++;																					\
	if ( !( g2 & bRESET ) ) {																			\
		resetCounter ++;																				\
	} else {																							\
		resetCounter = 0;																				\
	}

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#endif
