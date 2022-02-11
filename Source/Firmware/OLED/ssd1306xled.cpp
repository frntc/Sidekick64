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

 modified for use with RasPIC64 by Carsten Dachsbacher

 */

// ============================================================================

#include <stdlib.h>
//#include <avr/io.h>
//#include <avr/pgmspace.h>

#include "ssd1306xled.h"
#include "font6x8.h"
#include "num2str.h"
#include "../latch.h"

// ----------------------------------------------------------------------------


// Convenience definitions for PORTB

//#define DIGITAL_WRITE_HIGH(PORT) PORTB |= (1 << PORT)
//#define DIGITAL_WRITE_LOW(PORT) PORTB &= ~(1 << PORT)
//#define DIGITAL_WRITE_HIGH(PORT) 
//#define DIGITAL_WRITE_LOW(PORT) 

static u32 lastSDA = 255;

// PORT = SSD1306_SDA or SSD1306_SCL
void DIGITAL_WRITE_HIGH( u32 PORT )
{
	putI2CCommand( (PORT << 1) | 1 );
	lastSDA = 1;
}

void DIGITAL_WRITE_LOW( u32 PORT )
{
	putI2CCommand( (PORT << 1) | 0 );
	lastSDA = 0;
}

// ----------------------------------------------------------------------------

// Some code based on "IIC_wtihout_ACK" by http://www.14blog.com/archives/1358

const uint8_t ssd1306_init_sequence [] PROGMEM = {	// Initialization Sequence
#if 1
	0xAE,			// Display OFF (sleep mode)
	0x20, 0b00,		// Set Memory Addressing Mode
					// 00=Horizontal Addressing Mode; 01=Vertical Addressing Mode;
					// 10=Page Addressing Mode (RESET); 11=Invalid
	0xB0,			// Set Page Start Address for Page Addressing Mode, 0-7
	0xC8,			// Set COM Output Scan Direction
	0x00,			// ---set low column address
	0x10,			// ---set high column address
	0x40,			// --set start line address
	0x81, 0x3F,		// Set contrast control register
	0xA1,			// Set Segment Re-map. A0=address mapped; A1=address 127 mapped. 
	0xA6,			// Set display mode. A6=Normal; A7=Inverse
	0xA8, 0x3F,		// Set multiplex ratio(1 to 64)
	0xA4,			// Output RAM to Display
					// 0xA4=Output follows RAM content; 0xA5,Output ignores RAM content
	0xD3, 0x00,		// Set display offset. 00 = no offset
	0xD5,			// --set display clock divide ratio/oscillator frequency
	0xF0,			// --set divide ratio
	0xD9, 0x22,		// Set pre-charge period
	0xDA, 0x12,		// Set com pins hardware configuration		
	0xDB,			// --set vcomh
	//0x00,			// 0x20,0.65xVcc
	//0x30,			// 0x20,0.83xVcc
	0x20,			// 0x20,0.77xVcc
	0x8D, 0x14,		// Set DC-DC enable
	0xAF			// Display ON in normal mode
#endif

#if 0
	// some modifications to the original init sequence above (seemed to work better with some SSD1306)
	0xae,			// display off
	0xa8, 63,		// multiplex ratio
	0xd3, 0x00,		// display offset
	0x40 | 0x0,		// startline
	0xA1, 0xC8,
	
	0xda, 0x12,		// <- 128x64, for 128x32 it would be 0x02

	0xa4,			// test display off
	0xa6,			// normal display
	0xd9, 0xff,		// precharge period
	0xdb, 0x30,		// VCOMH deselct
	0xd5, 0x80,		// clock devide
	0x8d, 0x14,		// enable charge pump

	0x20, 0x0,		// addressing mode
	0x22, 0b000, 0b111,
		
	0x2d,			// deactivate scroll
	0xaf			// display on

#endif
};

// ----------------------------------------------------------------------------

// These function should become separate library for handling I2C simplified output.

void ssd1306_xfer_start(void)
{
	DIGITAL_WRITE_HIGH(SSD1306_SCL);	// Set to HIGH
	DIGITAL_WRITE_HIGH(SSD1306_SDA);	// Set to HIGH
	DIGITAL_WRITE_LOW(SSD1306_SDA);		// Set to LOW
	DIGITAL_WRITE_LOW(SSD1306_SCL);		// Set to LOW
}

void ssd1306_xfer_stop(void)
{
	DIGITAL_WRITE_LOW(SSD1306_SCL);		// Set to LOW
	DIGITAL_WRITE_LOW(SSD1306_SDA);		// Set to LOW
	DIGITAL_WRITE_HIGH(SSD1306_SCL);	// Set to HIGH
	DIGITAL_WRITE_HIGH(SSD1306_SDA);	// Set to HIGH
}

void ssd1306_send_byte(uint8_t byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		if ((byte << i) & 0x80)
			DIGITAL_WRITE_HIGH(SSD1306_SDA);
		else
			DIGITAL_WRITE_LOW(SSD1306_SDA);
		
		DIGITAL_WRITE_HIGH(SSD1306_SCL);
		DIGITAL_WRITE_LOW(SSD1306_SCL);
	}
	DIGITAL_WRITE_HIGH(SSD1306_SDA);
	DIGITAL_WRITE_HIGH(SSD1306_SCL);
	DIGITAL_WRITE_LOW(SSD1306_SCL);
}

void ssd1306_send_command_start(void) {
	ssd1306_xfer_start();
	ssd1306_send_byte(SSD1306_SA);  // Slave address, SA0=0
	ssd1306_send_byte(0x00);	// write command
}

void ssd1306_send_command_stop(void) {
	ssd1306_xfer_stop();
}

void ssd1306_send_command(uint8_t command)
{
	ssd1306_send_command_start();
	ssd1306_send_byte(command);
	ssd1306_send_command_stop();
}

void ssd1306_send_data_start(void)
{
	ssd1306_xfer_start();
	ssd1306_send_byte(SSD1306_SA);
	ssd1306_send_byte(0x40);	//write data
}

void ssd1306_send_data_stop(void)
{
	ssd1306_xfer_stop();
}

/*
void ssd1306_send_data(uint8_t byte)
{
	ssd1306_send_data_start();
	ssd1306_send_byte(byte);
	ssd1306_send_data_stop();
}
*/

// ----------------------------------------------------------------------------


void ssd1306_init(void)
{
	//DDRB |= (1 << SSD1306_SDA);	// Set port as output
	//DDRB |= (1 << SSD1306_SCL);	// Set port as output
	
	ssd1306_send_command_start();
	for (uint8_t i = 0; i < sizeof (ssd1306_init_sequence); i++) {
		//ssd1306_send_command(ssd1306_init_sequence[i]);
		ssd1306_send_byte(ssd1306_init_sequence[i]);

		flushI2CBuffer();
	}
	ssd1306_send_command_stop();
		flushI2CBuffer();
}


void ssd1306_setpos(uint8_t x, uint8_t y)
{
	ssd1306_send_command_start();
	ssd1306_send_byte(0xb0 + y);
	ssd1306_send_byte(((x & 0xf0) >> 4) | 0x10); // | 0x10
/* TODO: Verify correctness */	ssd1306_send_byte((x & 0x0f)); // | 0x01
	ssd1306_send_command_stop();
}

void ssd1306_fill4(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
	ssd1306_setpos(0, 0);
	ssd1306_send_data_start();
	for (uint16_t i = 0; i < 128 * 8 / 4; i++) {
		ssd1306_send_byte(p1);
		ssd1306_send_byte(p2);
		ssd1306_send_byte(p3);
		ssd1306_send_byte(p4);
	}
	ssd1306_send_data_stop();
}

void ssd1306_fill2(uint8_t p1, uint8_t p2) {
	ssd1306_fill4(p1, p2, p1, p2);
}

void ssd1306_fill(uint8_t p) {
	ssd1306_fill4(p, p, p, p);
}

// ----------------------------------------------------------------------------

void ssd1306_char_font6x8(char ch) {
	uint8_t c = ch - 32;
	ssd1306_send_data_start();
	for (uint8_t i = 0; i < 6; i++)
	{
		ssd1306_send_byte(ssd1306xled_font6x8[c * 6 + i]);
	}
	ssd1306_send_data_stop();
}

void ssd1306_string_font6x8(char *s) {
	while (*s) {
		ssd1306_char_font6x8(*s++);
	}
}

char ssd1306_numdec_buffer[USINT2DECASCII_MAX_DIGITS + 1];

void ssd1306_numdec_font6x8(uint16_t num) {
	ssd1306_numdec_buffer[USINT2DECASCII_MAX_DIGITS] = '\0';   // Terminate the string.
	uint8_t digits = usint2decascii(num, ssd1306_numdec_buffer);
	ssd1306_string_font6x8(ssd1306_numdec_buffer + digits);
}

void ssd1306_numdecp_font6x8(uint16_t num) {
	ssd1306_numdec_buffer[USINT2DECASCII_MAX_DIGITS] = '\0';   // Terminate the string.
	usint2decascii(num, ssd1306_numdec_buffer);
	ssd1306_string_font6x8(ssd1306_numdec_buffer);
}

// ----------------------------------------------------------------------------

void ssd1306_draw_bmp(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t bitmap[])
{
	uint16_t j = 0;
	uint8_t y;
	if (y1 % 8 == 0) y = y1 / 8;
	else y = y1 / 8 + 1;
	for (y = y0; y < y1; y++)
	{
		ssd1306_setpos(x0,y);
		ssd1306_send_data_start();
		for (uint8_t x = x0; x < x1; x++)
		{
			ssd1306_send_byte(bitmap[j++]);
		}
		ssd1306_send_data_stop();
	}
}


// ============================================================================
