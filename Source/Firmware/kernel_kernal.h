/*
  _________.__    .___      __   .__        __        ____  __.                         .__   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |/ _|___________  ____ _____  |  |  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |      <_/ __ \_  __ \/    \\__  \ |  |  
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |  \  ___/|  | \/   |  \/ __ \|  |__
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |____|__ \___  >__|  |___|  (____  /____/
        \/         \/    \/     \/       \/     \/           \/   \/           \/     \/      

 kernel_kernal.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Kernal: example how to implement a kernal cartridge, based on http://blog.worldofjani.com/?p=3736
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
#ifndef _kernel_kernal_h
#define _kernel_kernal_h

// use the OLED connected to the latch
#define USE_OLED

#if defined(USE_OLED) && !defined(USE_LATCH_OUTPUT)
#define USE_LATCH_OUTPUT
#endif

#define USE_HDMI_VIDEO

#include <circle/startup.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/memory.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/types.h>
#include <circle/gpioclock.h>
#include <circle/gpiopin.h>
#include <circle/gpiopinfiq.h>
#include <circle/gpiomanager.h>
#include <circle/util.h>

#include "lowlevel_arm64.h"
#include "gpio_defs.h"
#include "latch.h"
#include "helpers.h"

#ifdef USE_OLED
#include "oled.h"
#include "splash_kernal.h"
#endif

#ifndef COMPILE_MENU
CLogger	*logger;
#define FIQ_HANDLER	(this->FIQHandler)
#define FIQ_PARENT	this
#else
#include "kernel_menu.h"
void KernelKernalFIQHandler( void *pParam );
#define FIQ_HANDLER	KernelKernalFIQHandler
#define FIQ_PARENT	kernelMenu
#endif


class CKernelKernal
{
public:
	CKernelKernal( void )
		: m_CPUThrottle( CPUSpeedMaximum ),
	#ifdef USE_HDMI_VIDEO
		m_Screen( m_Options.GetWidth(), m_Options.GetHeight() ),
	#endif
		m_Timer( &m_Interrupt ),
		m_Logger( m_Options.GetLogLevel(), &m_Timer ),
		m_InputPin( PHI2, GPIOModeInput, &m_Interrupt ),
		m_EMMC( &m_Interrupt, &m_Timer, 0 )
	{
	}

	~CKernelKernal( void )
	{
	}

	boolean Initialize( void )
	{
		STANDARD_SETUP_TIMER_INTERRUPT_CYCLECOUNTER_GPIO
		#ifndef COMPILE_MENU
		logger = &m_Logger;
		#endif
		return bOK;
	}

	void Run( void );

private:
	static void FIQHandler( void *pParam );

	// do not change this order
	CMemorySystem		m_Memory;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CCPUThrottle		m_CPUThrottle;
#ifdef USE_HDMI_VIDEO
	CScreenDevice		m_Screen;
#endif
	CInterruptSystem	m_Interrupt;
	CTimer				m_Timer;
	CLogger				m_Logger;
	CScheduler			m_Scheduler;
	CGPIOPinFIQ			m_InputPin;
	CEMMCDevice			m_EMMC;
};

#ifdef COMPILE_MENU
//int mainKernal( void )
#else
int main( void )
{
	CKernelKernal kernel;
	if ( kernel.Initialize() )
		kernel.Run();

	halt();
	return EXIT_HALT;
}
#endif

#endif
