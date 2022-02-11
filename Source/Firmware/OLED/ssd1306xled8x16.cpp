/**
 * SSD1306xLED - Drivers for SSD1306 controlled dot matrix OLED/PLED 128x64 displays
 *
 * @created: 2014-08-12
 * @author: Neven Boyanov
 *
 * This is part of the Tinusaur/SSD1306xLED project.
 *
 * Copyright (c) 2016 Neven Boyanov, Tinusaur Team. All Rights Reserved.
 * Distributed as open source software under MIT License, see LICENSE.txt file.
 * Please, as a favor, retain the link http://tinusaur.org to The Tinusaur Project.
 *
 * Source code available at: https://bitbucket.org/tinusaur/ssd1306xled
 *
 */

// ============================================================================

#include <stdlib.h>
//#include <avr/io.h>

//#include <avr/pgmspace.h>

#include "ssd1306xled.h"
#include "ssd1306xled8x16.h"
#include "font8x16.h"

// ----------------------------------------------------------------------------

/*

// NOTE: DOES NOT WORK
void ssd1306_char_font8x16(char ch) {
	uint8_t c = ch - 32;
	ssd1306_send_data_start();
	for (uint8_t i = 0; i < 8; i++)
	{
		ssd1306_send_byte(pgm_read_byte(&ssd1306xled_font8x16[c * 16 + i]));
		ssd1306_send_byte(pgm_read_byte(&ssd1306xled_font8x16[c * 16 + i + 8]));
	}
	ssd1306_send_data_stop();
}

// NOTE: DOES NOT WORK
void ssd1306_string_font8x16(char *s) {
	while (*s) {
		ssd1306_char_font8x16(*s++);
	}
}

*/

void ssd1306_string_font8x16xy(uint8_t x, uint8_t y, const char s[]) {
	uint8_t ch, j = 0;
	while (s[j] != '\0') {
		ch = s[j] - 32;
		if (x > 120) {
			x = 0;
			y++;
		}
		ssd1306_setpos(x, y);
		ssd1306_send_data_start();
		for (uint8_t i = 0; i < 8; i++) {
			ssd1306_send_byte(ssd1306xled_font8x16[ch * 16 + i]);
		}
		ssd1306_send_data_stop();
		ssd1306_setpos(x, y + 1);
		ssd1306_send_data_start();
		for (uint8_t i = 0; i < 8; i++) {
			ssd1306_send_byte(ssd1306xled_font8x16[ch * 16 + i + 8]);
		}
		ssd1306_send_data_stop();
		x += 8;
		j++;
	}
}

// ============================================================================
