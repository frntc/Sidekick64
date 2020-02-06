/*
  _________.__    .___      __   .__        __       _________                __   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   ___ \_____ ________/  |_ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /    \  \/\__  \\_  __ \   __\
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    \     \____/ __ \|  | \/|  |  
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \______  (____  /__|   |__|  
        \/         \/    \/     \/       \/     \/           \/     \/             

 kernel_cart.h

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Cart: example how to implement a simple CBM80 cartridge
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
#ifndef _kernel_cart_h
#define _kernel_cart_h

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
#include "splash_cart.h"
#endif

#ifndef COMPILE_MENU
CLogger	*logger;
#define FIQ_HANDLER	(this->FIQHandler)
#define FIQ_PARENT	this
#else
#include "kernel_menu.h"
#define FIQ_HANDLER	KernelCartFIQHandler
#define FIQ_PARENT	kernelMenu
void KernelCartFIQHandler( void *pParam );
#endif


class CKernelCart
{
public:
	CKernelCart( void )
		: m_CPUThrottle( CPUSpeedMaximum ),
	#ifdef USE_HDMI_VIDEO
		m_Screen( m_Options.GetWidth(), m_Options.GetHeight() ),
	#endif
		m_Timer( &m_Interrupt ),
		m_Logger( m_Options.GetLogLevel(), &m_Timer ),
		m_InputPin( PHI2, GPIOModeInput, &m_Interrupt )
	{
	}

	~CKernelCart( void )
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
};

#endif
