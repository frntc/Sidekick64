/*
  _________.__    .___      __   .__        __            _____                       
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __       /     \   ____   ____  __ __ 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /      /  \ /  \_/ __ \ /    \|  |  \
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /    Y    \  ___/|   |  \  |  /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \____|__  /\___  >___|  /____/ 
        \/         \/    \/     \/       \/     \/             \/     \/     \/       
 
 kernel_menu264.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Menu: ugly glue code to expose some functionality in one menu with browser
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
#ifndef _kernel_h
#define _kernel_h

// use the OLED connected to the latch
#define USE_OLED

// 0 = SSD1306 OLED 128x64
// 1 = ST7789 RGB TFT 240x240
extern int screenType;

//#if defined(USE_OLED) && !defined(USE_LATCH_OUTPUT)
#define USE_LATCH_OUTPUT
//#endif

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

#include <stdio.h>

#ifdef USE_VCHIQ_SOUND
#include <vc4/vchiq/vchiqdevice.h>
#include <vc4/sound/vchiqsoundbasedevice.h>
#endif

#include "lowlevel_arm64.h"
#include "gpio_defs.h"
#include "latch.h"
#include "helpers.h"
#include "helpers264.h"
#include "crt.h"
#include "mygpiopinfiq.h"

#include "oled.h"
#include "splash_sidekick264.h"
#include "tft_st7789.h"

extern CLogger *logger;

void startInjectCode();
void injectPOKE( u32 a, u8 d );

class CKernelMenu
{
public:
	CKernelMenu( void )
		: m_CPUThrottle( CPUSpeedMaximum ),
	#ifdef USE_HDMI_VIDEO
		m_Screen( m_Options.GetWidth(), m_Options.GetHeight() ),
	#endif
		m_Timer( &m_Interrupt ),
		m_Logger( m_Options.GetLogLevel(), &m_Timer ),
#ifdef COMPILE_MENU_WITH_SOUND
	#ifdef USE_VCHIQ_SOUND
		m_VCHIQ( &m_Memory, &m_Interrupt ),
	#endif
#endif
		m_InputPin( PHI2, GPIOModeInput, &m_Interrupt ),
		m_EMMC( &m_Interrupt, &m_Timer, 0 )
	{
	}

	~CKernelMenu( void )
	{
	}

	boolean Initialize( void );

	void Run( void );

private:
	static void FIQHandler( void *pParam );

public:
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
#ifdef COMPILE_MENU_WITH_SOUND
	#ifdef USE_VCHIQ_SOUND
	CVCHIQDevice		m_VCHIQ;
	#endif
	CSoundBaseDevice	*m_pSound;
#endif
	CGPIOPinFIQ2		m_InputPin;
	CEMMCDevice			m_EMMC;
};

#endif
