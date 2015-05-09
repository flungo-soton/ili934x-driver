/* The MIT License (MIT)
 *
 * Copyright (c) 2015, Steve Gunn
 * Copyright (c) 2015, Fabrizio Lungo <fab@lungo.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lcd.h"
#include "colour/basic.h"

lcd_display lcd;

void lcd_init() {
  /* Enable extended memory interface with 10 bit addressing */
  XMCRB = _BV(XMM2) | _BV(XMM1);
  XMCRA = _BV(SRE);
  /* Set ILI934X reset pin to output */
  DDRC |= _BV(ILI934X_RESET_PIN);
  /* Set backlight pin to output */
  DDRB |= _BV(ILI934X_BLC_PIN);
  _delay_ms(1);
  /* Backlight off initially */
  PORTB &= ~_BV(ILI934X_BLC_PIN);
  /* Reset the display to synchronise initialisation */
  PORTC &= ~_BV(ILI934X_RESET_PIN);
  _delay_ms(20);
  PORTC |= _BV(ILI934X_RESET_PIN);
  _delay_ms(120);
  /* Disable the display while settings are configured */
  ili934x_displayOff();
  /* Leave sleep and allow for wake up to to complete */
  ili934x_sleepOut();
  _delay_ms(60);
  /* Set GVDD level to 4.75V */
  ili934x_powerControl1(0x26);
  /* Set VCOMH to 4.025V & VCOML to -1.925V */
  ili934x_vcomControl1(0x35, 0x2E);
  /* Set VCOMH to VMH – 11 and VCOML to VMH – 11 */
  /* TODO: Does this do anything without setting nVM? */
  ili934x_vcomControl2(0, 0x35);
  /* Set non-overlap timing control */
  ili934x_driverTimingCtrlA(1,0,0,0);
  /* Set RGB Interface Format (DPI) to 16 bits / pixel */
  /* Set MCU Interface Format (DBI) to 16 bits / pixel */
  ili934x_pixelFormatSet(5, 5);     /* 16bit/pixel */
  /* Init lcd object */
  lcd.width = LCD_WIDTH;
  lcd.height = LCD_HEIGHT;
  lcd.orient = North;
  lcd.background = BLACK;
  lcd.foreground = WHITE;
  /* Clear contents of display */
  lcd_clear();
  /* Enable Tearing Effect Line */
  ili934x_tearingEffectOn(0);
  /* Enable falling edge detect on External Interrupt Control Register B ISC6 */
  EICRB |= _BV(ISC61);
}

void lcd_displayOn() {
  /* Enable display */
  ili934x_displayOn();
  _delay_ms(30);
  /* Enable the LCD backlight */
  PORTB |= _BV(ILI934X_BLC_PIN);
}

void lcd_displayOff() {
  /* Disable the LCD backlight */
  PORTB &= ~_BV(ILI934X_BLC_PIN);
  /* Disable Display */
  ili934x_displayOff();
}

void lcd_selectRegion(lcd_region r) {
  ili934x_columnAddrSet(r.left, r.right);
  ili934x_pageAddrSet(r.top, r.bottom);
  lcd.selection = r;
}

void lcd_setColour(lcd_colour16 colour) {
  /*uint16_t i;
  ili934x_initMemoryWrite();
  for (i = 0; i < lcd.selection; i++) {
    ili934x_write_data16(colour);
  }*/
  uint16_t x, y;
  ili934x_write_cmd(ILI934X_RAMWR);
  for(x=lcd.selection.left; x<=lcd.selection.right; x++)
    for(y=lcd.selection.top; y<=lcd.selection.bottom; y++)
          ili934x_write_data16(colour);
}

void lcd_setPixel(lcd_point p, lcd_colour16 colour) {
  /* Select the point */
  lcd_region r = {p.x, p.x, p.y, p.y};
  lcd_selectRegion(r);
  /* Write the colour */
  ili934x_initMemoryWrite();
  ili934x_write_data16(colour);
}

void lcd_setPixels(lcd_point *p, lcd_colour16 *colour, uint16_t pixels) {
  uint16_t i;
  /* For each pixel, set pixel */
  for (i = 0; i < pixels; i++) {
    lcd_setPixel(*p++, *colour++);
  }
}

void lcd_setRegion(lcd_region region, lcd_colour16 colour) {
  lcd_selectRegion(region);
  lcd_setColour(colour);
}

void lcd_setRegions(lcd_region *region, lcd_colour16 *colour, uint16_t regions) {
  uint16_t i;
  /* For each pixel, set pixel */
  for (i = 0; i < regions; i++) {
    lcd_setRegion(*region++, *colour++);
  }
}

void lcd_setBitmap(lcd_region region, lcd_colour16 *colour) {
  uint16_t i;
  /* Select the region */
  lcd_selectRegion(region);
  /* Initialise write process */
  ili934x_initMemoryWrite();
  /* Write to all pixels of region */
  uint16_t x, y;
  for(x=lcd.selection.left; x<=lcd.selection.right; x++) {
      for(y=lcd.selection.top; y<=lcd.selection.bottom; y++) {
      ili934x_write_data16(*colour++);
    }
  }
}

void lcd_setBitmap8bit(lcd_region region, lcd_colour8 *colour) {
  uint16_t i;
  /* Select the region */
  lcd_selectRegion(region);
  /* Initialise write process */
  ili934x_initMemoryWrite();
  /* Write to all pixels of region */
  uint16_t x, y;
  for(x=lcd.selection.left; x<=lcd.selection.right; x++) {
      for(y=lcd.selection.top; y<=lcd.selection.bottom; y++) {
      ili934x_write_data16(colour_8to16(*colour++));
    }
  }
}

void lcd_setBitmapMono(lcd_region region, uint8_t* data, uint8_t bpc) {
  uint16_t x = 0, y = 0;
  uint16_t i, h, w;
  uint8_t b = 0;
  h = region.bottom - region.top;
  w = region.right - region.left;
  /* Select the region */
  lcd_selectRegion(region);
  ili934x_initMemoryWrite();
  uint16_t xv, yv;
  for(xv=lcd.selection.left; xv<=lcd.selection.right; xv++) {
      for(yv=lcd.selection.top; yv<=lcd.selection.bottom; yv++) {
      if (*data++ & (1 << b)) {
        ili934x_write_data16(lcd.foreground)
      } else {
        ili934x_write_data16(lcd.background)
      }
      b = (b + 1) % 8;
      if (y % bpc == 0 || ++y >= h) {
        if (++x >= w) {
          return;
        }
      }
    }
  }
}

void lcd_setRegionFunction(lcd_region region, lcd_colour16 (*f)(uint16_t x, uint16_t), bool relative) {
  uint16_t x, y;
  uint16_t w, h;
  /* Select the region */
  lcd_selectRegion(region);
  /* Determine the width of the area to write to.
   * Starting point will be 0 if relative and top-left otherwise
   */
  if (relative) {
    h = region.bottom - region.top;
    w = region.right - region.left;
  } else {
    h = region.bottom;
    w = region.right;
  }
  for (x = relative ? 0 : region.left; x < w; x++) {
    for (y = relative ? 0 : region.top; y < h; y++) {
      /* TODO: Check x and y are correct way round */
      ili934x_write_data16(f(x,y));
    }
  }
}

void lcd_clear() {
  lcd_region display = {0, LCD_WIDTH - 1, 0, LCD_HEIGHT - 1};
  lcd_setRegion(display, 0);
}
