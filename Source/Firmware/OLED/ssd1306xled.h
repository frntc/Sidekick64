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

#ifndef SSD1306XLED_H
#define SSD1306XLED_H

// ============================================================================

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32; 

typedef u8		uint8_t;
typedef u16		uint16_t;

// -----(+)--------------->	// Vcc,	Pin 1 on SSD1306 Board
// -----(-)--------------->	// GND,	Pin 2 on SSD1306 Board
#ifndef SSD1306_SCL
//#define SSD1306_SCL		PB2	// SCL,	Pin 3 on SSD1306 Board
#define SSD1306_SCL		1	// SCL,	Pin 3 on SSD1306 Board
#endif
#ifndef SSD1306_SDA
//#define SSD1306_SDA		PB1	// SDA,	Pin 4 on SSD1306 Board
#define SSD1306_SDA		0	// SDA,	Pin 4 on SSD1306 Board
#endif
#ifndef SSD1306_SA
#define SSD1306_SA		0x78	// Slave address
#endif

// ----------------------------------------------------------------------------

// These functions are used only internally by the library
void ssd1306_xfer_start(void);
void ssd1306_xfer_stop(void);
void ssd1306_send_byte(uint8_t byte);
void ssd1306_send_command(uint8_t command);
void ssd1306_send_data_start(void);
void ssd1306_send_data_stop(void);

// ----------------------------------------------------------------------------

#define ssd1306_clear() ssd1306_fill(0);

#define ssd1306_char(c) ssd1306_char_font6x8(c)
#define ssd1306_string(s) ssd1306_string_font6x8(s)
#define ssd1306_numdec(n) ssd1306_numdec_font6x8(n)
#define ssd1306_numdecp(n) ssd1306_numdecp_font6x8(n)

// ----------------------------------------------------------------------------

 void ssd1306_init(void);
 void ssd1306_setpos(uint8_t x, uint8_t y);
 void ssd1306_fill4(uint8_t, uint8_t, uint8_t, uint8_t);
 void ssd1306_fill2(uint8_t p1, uint8_t p2);
 void ssd1306_fill(uint8_t p);
 void ssd1306_fillscreen(uint8_t fill);
 void ssd1306_char_font6x8(char ch);
 void ssd1306_string_font6x8(char *s);
 void ssd1306_numdec_font6x8(uint16_t num);
 void ssd1306_numdecp_font6x8(uint16_t num);
 void ssd1306_draw_bmp(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t bitmap[]);

// ============================================================================

#endif
