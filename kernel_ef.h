/*
  _________.__    .___      __   .__        __       ___________.__                .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   \_   _____/|  | _____    _____|  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    |    __)  |  | \__  \  /  ___/  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     |     \   |  |__/ __ \_\___ \|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    \___  /   |____(____  /____  >___|  /
        \/         \/    \/     \/       \/     \/        \/              \/     \/     \/ 

 kernel_ef.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - Sidekick Flash: example how to implement an generic/magicdesk/easyflash-compatible cartridge
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
#ifndef _kernel_ef_h
#define _kernel_ef_h

// support output signals via the latch (if not used, you'd have to set EXROM and GAME manually!)
#define USE_LATCH_OUTPUT

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

#include <SDCard/emmc.h>
#include <fatfs/ff.h>

#include "lowlevel_arm64.h"
#include "gpio_defs.h"
#include "latch.h"
#include "helpers.h"

#ifdef USE_OLED
#include "oled.h"
#include "splash_flash.h"
#endif

#include "crt.h"
#ifdef COMPILE_MENU
#include "kernel_menu.h"
#define FIQ_HANDLER	KernelEFFIQHandler
#define FIQ_PARENT	kernelMenu
#else
CLogger	*logger;
//volatile u32 forceRead;
#define FIQ_HANDLER	(this->FIQHandler)
#define FIQ_PARENT	this
#endif

class CKernelEF
{
public:
	CKernelEF( void )
		: m_CPUThrottle( CPUSpeedMaximum ),
	#ifdef USE_HDMI_VIDEO
		m_Screen( m_Options.GetWidth(), m_Options.GetHeight() ),
	#endif
		m_Timer( &m_Interrupt ),
	#ifdef USE_HDMI_VIDEO
		m_Logger( m_Options.GetLogLevel(), &m_Timer ),
	#endif
		m_InputPin( PHI2, GPIOModeInput, &m_Interrupt ),
		m_EMMC( &m_Interrupt, &m_Timer, 0 )
	{
	}

	~CKernelEF( void )
	{
	}

	boolean Initialize( void )
	{
		STANDARD_SETUP_TIMER_INTERRUPT_CYCLECOUNTER_GPIO
		logger = &m_Logger;
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
#ifdef USE_HDMI_VIDEO
	CLogger				m_Logger;
#endif
	CScheduler			m_Scheduler;
	CGPIOPinFIQ			m_InputPin;
	CEMMCDevice			m_EMMC;
	FATFS				m_FileSystem;
};

#ifndef COMPILE_MENU
int main( void )
{
	CKernelEF kernel;
	if ( kernel.Initialize() )
		kernel.Run();

	halt();
	return EXIT_HALT;
}
#endif

#endif
