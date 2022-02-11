/*
    $Id$

    psid64 - PlaySID player for a real C64 environment
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

#ifndef RELOC65_H
#define RELOC65_H

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////

//#include <map>
//#include <string>


//////////////////////////////////////////////////////////////////////////////
//                  F O R W A R D   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                     D A T A   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////

//typedef std::map<std::string,int> globals_t;

#define uint_least32_t unsigned int

typedef struct
{
	uint_least32_t song;
	uint_least32_t player;
	uint_least32_t stopvec;
	uint_least32_t screen;
	uint_least32_t barsprptr;
	uint_least32_t dd00;
	uint_least32_t d018;
	uint_least32_t screen_songnum;
	uint_least32_t sid2base;
	uint_least32_t sid3base;
	uint_least32_t stil;
	uint_least32_t songlengths_min;
	uint_least32_t songlengths_sec;
	uint_least32_t songtpi_lo;
	uint_least32_t songtpi_hi;
}GLOBALS_BLOCK;


//////////////////////////////////////////////////////////////////////////////
//                  F U N C T I O N   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////

extern int              reloc65 (char **buf, int *fsize, int addr, GLOBALS_BLOCK *globals);

#endif // RELOC65_H
