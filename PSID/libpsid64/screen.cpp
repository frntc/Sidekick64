/*
    $Id$

    psid64 - create a C64 executable from a PSID file
    Copyright (C) 2001-2003  Roland Hermans <rolandh@users.sourceforge.net>

	this code has been modified for integration into the Sidekick64 software
    (the modifications are mostly removing everything that does not compile with vanilla Circle,
    I really feel sorry for having disfigured this code, please refer to the original repository 
    if you want to get the real and decent psid64 version)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////

/*#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstring>*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screen.h"


//////////////////////////////////////////////////////////////////////////////
//                           G L O B A L   D A T A
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                     L O C A L   D E F I N I T I O N S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                            L O C A L   D A T A
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                       L O C A L   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                      G L O B A L   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////

// constructor

Screen::Screen() :
    m_x(0),
    m_y(0)
{
    clear();
    home();
}


// destructor

Screen::~Screen()
{
}


// public member functions

void Screen::clear()
{
    uint_least8_t c = iso2scr (' ');
    for (unsigned int i = 0; i < m_screenSize; ++i)
    {
	m_screen[i] = c;
    }
}


void Screen::home()
{
    m_x = 0;
    m_y = 0;
}


void Screen::move(unsigned int x, unsigned int y)
{
    if ((x < m_width) && (y < m_height))
    {
	m_x = x;
	m_y = y;
    }
}


void Screen::putchar(char c)
{
    if (c == '\n')
    {
	m_x = 0;
	moveDown();
    }
    else
    {
	unsigned int offs = offset(m_x, m_y);
	m_screen[offs] = iso2scr ((uint_least8_t) c);
	moveRight();
    }
}


void Screen::write(const char *str)
{
    while (*str)
    {
	putchar(*str);
	++str;
    }
}


void Screen::poke(unsigned int x, unsigned int y, uint_least8_t value)
{
    if ((x < m_width) && (y < m_height))
    {
	unsigned int offs = offset (x, y);
	m_screen[offs] = value;
    }
}


void Screen::poke(unsigned int offs, uint_least8_t value)
{
    if (offs < m_screenSize)
    {
	m_screen[offs] = value;
    }
}
