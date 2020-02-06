/**
 * NUM2STR - Functions to handle the conversion of numeric vales to strings.
 *
 * @created	2014-12-18
 * @author	Neven Boyanov
 * @version	2016-04-17 (last modified)
 *
 * This is part of the Tinusaur/TinyAVRLib project.
 *
 * Copyright (c) 2016 Neven Boyanov, Tinusaur Team. All Rights Reserved.
 * Distributed as open source software under MIT License, see LICENSE.txt file.
 * Please, as a favor, retain the link http://tinusaur.org to The Tinusaur Project.
 *
 * Source code available at: https://bitbucket.org/tinusaur/tinyavrlib
 *
 */

// ============================================================================

#ifndef NUM2STR_H
#define NUM2STR_H

// ----------------------------------------------------------------------------

#include <stdint.h>

// ----------------------------------------------------------------------------

#define USINT2DECASCII_MAX_DIGITS 5
#define USINT2BINASCII_MAX_DIGITS 16

// ----------------------------------------------------------------------------

uint8_t usint2decascii(uint16_t, char *);
uint8_t usint2binascii(uint16_t, char *);

// ----------------------------------------------------------------------------

#endif

// ============================================================================
