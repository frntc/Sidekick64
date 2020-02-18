//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 2010  Dag Lem <resid@nimrod.no>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  ---------------------------------------------------------------------------

#ifndef RESID_SIDDEFS_H
#define RESID_SIDDEFS_H

#include <circle/memory.h>
#include <circle/types.h>

// Compilation configuration.
//#define RESID_INLINING @RESID_INLINING@
//#define RESID_INLINE @RESID_INLINE@
//#define RESID_BRANCH_HINTS @RESID_BRANCH_HINTS@

#define RESID_INLINING 1
//#define RESID_INLINE __attribute__((always_inline))
#define RESID_INLINE inline
#define RESID_BRANCH_HINTS 0

//#define NEW_8580_FILTER @NEW_8580_FILTER@
#define NEW_8580_FILTER 0

// Compiler specifics.
#define HAVE_BOOL 1
#define HAVE_BUILTIN_EXPECT 0
#define HAVE_LOG1P 0

//#define HAVE_BOOL @HAVE_BOOL@
//#define HAVE_BUILTIN_EXPECT @HAVE_BUILTIN_EXPECT@
//#define HAVE_LOG1P @HAVE_LOG1P@

// Define bool, true, and false for C++ compilers that lack these keywords.
#ifndef HAVE_BOOL
typedef int bool;
const bool true = 1;
const bool false = 0;
#endif

#if HAVE_LOG1P
#define HAS_LOG1P
#endif

// Branch prediction macros, lifted off the Linux kernel.
#if RESID_BRANCH_HINTS && HAVE_BUILTIN_EXPECT
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#ifndef likely
#define likely(x)      (x)
#endif
#ifndef unlikely
#define unlikely(x)    (x)
#endif
#endif

namespace reSID {

// We could have used the smallest possible data type for each SID register,
// however this would give a slower engine because of data type conversions.
// An int is assumed to be at least 32 bits (necessary in the types reg24
// and cycle_count). GNU does not support 16-bit machines
// (GNU Coding Standards: Portability between CPUs), so this should be
// a valid assumption.

typedef unsigned int reg4;
typedef unsigned int reg8;
typedef unsigned int reg12;
typedef unsigned int reg16;
typedef unsigned int reg24;

typedef int cycle_count;
typedef short short_point[2];
typedef double double_point[2];

enum chip_model { MOS6581, MOS8580 };

enum sampling_method {
    SAMPLE_FAST, 
    SAMPLE_INTERPOLATE,
    SAMPLE_RESAMPLE, 
    SAMPLE_RESAMPLE_FASTMEM 
};

} // namespace reSID

extern "C"
{
#ifndef RESID_VERSION_CC
extern const char* resid_version_string;
#else
const char* resid_version_string = "resid 1.0";
#endif
}

#endif // not RESID_SIDDEFS_H
