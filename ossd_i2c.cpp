/* OLED display support for ATmega32L or similar

   Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
   https://opensource.org/licenses/MIT
*/
/**
    Limited set of functions for SSD1306 compatible OLED 128x64 displays
    in text mode to minimize memory footprint if used on Atmel AVRs chips
    with low memory.
*/

#include <stdio.h>

#include "ossd_i2c.h"

#if (OSSD_TARGET == OSSD_AVR) 
	#include <avr/io.h>
	#include <avr/pgmspace.h>
	#include <util/atomic.h>
	#include "i2cmaster.h"
#else 
	#define PROGMEM
	#define pgm_read_byte(x) (*((uint8_t *)x))
	#include <alloca.h>
	#include <memory.h>
#if (OSSD_TARGET == OSSD_RPI)
	#include "pi2c.h"
#endif
#endif

#include "bmfont.h"

#define OSSD_CMD  0x00
#define OSSD_DATA 0x40

#define OSSD_SET_ADDR_MODE   0x20
#define OSSD_ADDR_MODE_PAGE  0x10
#define OSSD_ADDR_MODE_HOR   0x00
#define OSSD_ADDR_MODE_VER   0x01

#define OSSD_SET_COL_ADDR    0x21
#define OSSD_SET_PAGE_ADDR   0x22

#define OSSD_SET_START_LINE  0x40

#define OSSD_SET_START_PAGE  0xB0
#define OSSD_SET_START_LCOL  0x00
#define OSSD_SET_START_HCOL  0x10

#define OSSD_SET_MUX_RATIO   0xA8
#define OSSD_SET_DISP_OFFSET 0xD3

#define OSSD_SET_CONTRAST	 0x81

#define OSSD_SET_SEG_REMAP   0xA0
#define OSSD_SEG_REMAP_R2L   0x01

#define OSSD_SET_COM_DIR     0xC0
#define OSSD_COM_DIR_UPDOWN  0x08

#define OSSD_SET_COM_CONFIG  0xDA
#define OSSD_COM_ALT         0x12
#define OSSD_COM_LR_REMAP    0x22

#define OSSD_SET_SLEEP_ON	 0xAE
#define OSSD_SET_SLEEP_OFF	 0xAF

#define OSSD_SET_INVERSE_ON	 0xA7
#define OSSD_SET_INVERSE_OFF 0xA6

#define OSSD_SET_OUTPUT_RAM	 0xA4
#define OSSD_SET_OUTPUT_ON	 0xA5

#define OSSD_SET_DISP_CLOCK  0xD5

#define OSSD_SET_PRECHARGE   0xD9

#define OSSD_SET_VCOMH_LEVEL 0xDB
#define OSSD_VCOMH_L065      0x00
#define OSSD_VCOMH_L077      0x20
#define OSSD_VCOMH_L083      0x30

#define OSSD_SET_CHARGE_PUMP 0x8D
#define OSSD_CHARGE_PUMP_ON  0x14
#define OSSD_CHARGE_PUMP_OFF 0x10

#define I2C_TIMEOUT 200
static uint8_t _mode;

#if (OSSD_TARGET == OSSD_AVR)
static int8_t ossd_send_byte(uint8_t dc, uint8_t data)
{
	if (i2c_start_ex(I2C_OSSD | I2C_WRITE, I2C_TIMEOUT))
		return -1;

	i2c_write(dc);
	i2c_write(data);
	i2c_stop();
	return 0;
}

static int8_t ossd_cmd_arg(uint8_t cmd, uint8_t arg)
{
	if (i2c_start_ex(I2C_OSSD | I2C_WRITE, I2C_TIMEOUT))
		return -1;
	i2c_write(OSSD_CMD);
	i2c_write(cmd);
	i2c_write(arg);
	i2c_stop();
	return 0;
}

static void ossd_cmd_arg2(uint8_t cmd, uint8_t arg1, uint8_t arg2)
{
	if (i2c_start_ex(I2C_OSSD | I2C_WRITE, I2C_TIMEOUT))
		return;
	i2c_write(OSSD_CMD);
	i2c_write(cmd);
	i2c_write(arg1);
	i2c_write(arg2);
	i2c_stop();
}

static void ossd_fill_line(uint8_t data, uint8_t num)
{
	if (i2c_start_ex(I2C_OSSD | I2C_WRITE, I2C_TIMEOUT))
		return;
	i2c_write(OSSD_DATA);
	for(uint8_t i = 0; i < num; i++)
		i2c_write(data);
	i2c_stop();
}

#else

static int8_t ossd_send_byte(uint8_t dc, uint8_t data)
{
	uint8_t buf[2];
	buf[0] = dc;
	buf[1] = data;

	pi2c_write(PI2C_BUS, buf, 2);
	return 0;
}

static int8_t ossd_cmd_arg(uint8_t cmd, uint8_t arg)
{
	uint8_t data[3];
	data[0] = OSSD_CMD;
	data[1] = cmd;
	data[2] = arg;
	pi2c_write(PI2C_BUS, data, 3);
	return 0;
}

static void ossd_cmd_arg2(uint8_t cmd, uint8_t arg1, uint8_t arg2)
{
	uint8_t data[4];
	data[0] = OSSD_CMD;
	data[1] = cmd;
	data[2] = arg1;
	data[3] = arg2;
	pi2c_write(PI2C_BUS, data, 4);
}

static void ossd_fill_line(uint8_t data, uint8_t num)
{
	uint8_t *buf = (uint8_t *)alloca(num+1);
	memset(buf, data, num+1);
	buf[0] = OSSD_DATA;
	pi2c_write(PI2C_BUS, buf, num+1);
}

#endif

static inline int8_t ossd_cmd(uint8_t cmd)
{
	return ossd_send_byte(OSSD_CMD, cmd);
}

int8_t ossd_write(uint8_t data)
{
	return ossd_send_byte(OSSD_DATA, data);
}

static uint8_t ossd_set_addr_mode(uint8_t set_mode)
{
	uint8_t ret = _mode;
	if (_mode != set_mode) {
		ossd_cmd_arg(OSSD_SET_ADDR_MODE, set_mode);
		// if switching back to page mode
		// set full screen as output region
		if (set_mode == OSSD_ADDR_MODE_PAGE) {
			ossd_cmd_arg2(OSSD_SET_PAGE_ADDR, 0, 7);
			ossd_cmd_arg2(OSSD_SET_COL_ADDR, 0, 127);
		}
		_mode = set_mode;
	}
	return ret;
}

void ossd_goto(uint8_t line, uint8_t x)
{
	if (_mode == OSSD_ADDR_MODE_PAGE) {
		ossd_cmd(OSSD_SET_START_PAGE | (line & 0x07));
		ossd_cmd(OSSD_SET_START_LCOL | (x & 0x0F));
		ossd_cmd(OSSD_SET_START_HCOL | (x >> 4));
	}
	else {
		uint8_t gw = bmfont_get()->gw;
		// in OSSD_ADDR_MODE_HOR/VER mode we set output region (gw x 16)
		ossd_cmd_arg2(OSSD_SET_COL_ADDR, x, x + gw - 1);
		ossd_cmd_arg2(OSSD_SET_PAGE_ADDR, line, line+1);
	}
}

void ossd_fill_screen(uint8_t data)
{
	// fill full screen line by line
	for(uint8_t line = 0; line < 8; line++) {
		ossd_goto(line, 0);
		ossd_fill_line(data, 128);
	}
} 

void ossd_sleep(uint8_t on_off)
{
	if (on_off)
		ossd_cmd(OSSD_SET_SLEEP_ON);
	else
		ossd_cmd(OSSD_SET_SLEEP_OFF);
}

void ossd_set_contrast(uint8_t val)
{
	ossd_cmd_arg(OSSD_SET_CONTRAST, val);
}

static void ossd_put_centre(uint8_t line, const char *str, uint8_t atr)
{
	uint16_t len;
	uint8_t x = 0;
	bmfont_t *font = bmfont_get();
	uint8_t gw = font->gw;
	uint8_t gh = font->gh;
	for(len = 0; str[x]; len+=gw, x++);
	if (len > 128)
		x = 0;
	else
		x = (128 - len) / 2;

	// in case if new text is shorter than previous one
	// we clean line up to x position
	if (x) {
		for(uint8_t i = 0; i < (gh+7)/8; i++) {
			ossd_goto(line + i, 0);
			ossd_fill_line(0, x);
		}
	}

	// recursive call of ossd_putlx()
	ossd_putlx(line, x, str, atr);

	// in case if new text is shorter than previous one
	// we clean to the end of the line
	if (x) {
		for(uint8_t i = 0; i < (gh+7)/8; i++) {
			ossd_goto(line + i, x + len);
			ossd_fill_line(0, x);
		}
	}
}

void ossd_putlx(uint8_t line, int8_t x, const char *str, uint8_t atr)
{
	line &= 0x07;

	// try to put this text in the middle of the line:
	// ossd_put_centre() will calculate proper x coordinate
	if (x < 0) {
		ossd_put_centre(line, str, atr);
		return;
	}

	uint8_t rev = 0;
	uint8_t over = 0;
	uint8_t under = 0;
	if (atr & TEXT_REVERSE)
		rev = ~rev;
	if (atr & TEXT_OVERLINE)
		over = 0x01;
	if (atr & TEXT_UNDERLINE)
		under = 0x80;

	bmfont_t *pfont = bmfont_get();
	uint8_t gw = pfont->gw;
	uint8_t gh = pfont->gh;
	uint8_t go = pfont->go;
	uint8_t gb = gw*(gh / 8); // bytes per glyph
	const uint8_t *font = pfont->font;
	uint8_t cmode = ossd_set_addr_mode(OSSD_ADDR_MODE_HOR);
	for(; *str != '\0'; str++, x += gw) {
		uint16_t idx = (*str - go) * gb;
		if ((uint8_t)x > (128 - gw)) {
			x = 0;
			line = (line + (gh+7)/8) & 0x07;
		}
		ossd_goto(line, x);    
		for(uint8_t i = 0; i < gb; i++) {
			uint8_t d = pgm_read_byte(&font[idx+i]);
			d ^= rev;
			if (under && (gh == 8 || i > (gw - 1)))
				d ^= under;
			if (i < gw)
				d ^= over;
			ossd_write(d);
		}
	}
	ossd_set_addr_mode(cmode);
}

void ossd_putcx(uint8_t line, int8_t x, uint8_t ch, uint8_t atr)
{
	uint8_t buf[2];
	buf[0] = ch;
	buf[1] = 0;
	ossd_putlx(line, x, (const char *)buf, atr);
}

int8_t ossd_init(uint8_t orientation)
{
	_mode = 0xFF;
	// set all default values
	if (ossd_cmd(OSSD_SET_SLEEP_ON) != 0)
		return -1;
	ossd_cmd_arg(OSSD_SET_MUX_RATIO, 63);	
	ossd_cmd_arg(OSSD_SET_DISP_OFFSET, 0);
	ossd_cmd(OSSD_SET_START_LINE | 0);
	ossd_cmd(OSSD_SET_SEG_REMAP | (orientation & OSSD_SEG_REMAP_R2L));
	ossd_cmd(OSSD_SET_COM_DIR | (orientation & OSSD_COM_DIR_UPDOWN));
	ossd_cmd_arg(OSSD_SET_COM_CONFIG, OSSD_COM_ALT);
	ossd_cmd_arg(OSSD_SET_CONTRAST, 64);
	ossd_cmd(OSSD_SET_OUTPUT_RAM);
	ossd_cmd_arg(OSSD_SET_DISP_CLOCK, 0x80);
	ossd_cmd_arg(OSSD_SET_PRECHARGE, 0x22);
	ossd_cmd_arg(OSSD_SET_VCOMH_LEVEL, OSSD_VCOMH_L077);
	ossd_cmd(OSSD_SET_INVERSE_OFF);
	ossd_cmd_arg(OSSD_SET_CHARGE_PUMP, OSSD_CHARGE_PUMP_ON);
	ossd_set_addr_mode(OSSD_ADDR_MODE_PAGE);
	ossd_fill_screen(0);
	ossd_cmd(OSSD_SET_SLEEP_OFF);
	ossd_goto(0, 0);
	return 0;
}
