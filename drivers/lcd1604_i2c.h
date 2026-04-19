/*
 * lcd1604_i2c.h - LCD 16x4 HD44780 driver via PCF8574 I2C backpack
 *
 * Standard PCF8574 wiring (most common modules):
 *   P7=D7  P6=D6  P5=D5  P4=D4
 *   P3=BL  P2=E   P1=RW  P0=RS
 *
 * Default I2C address: 0x27 (A0=A1=A2=1).
 * Alternative: 0x3F for modules with all address bits high.
 *
 * Call order: lcd_init() → lcd_set_cursor() → lcd_print_str() / lcd_print_char()
 */

#ifndef LCD1604_I2C_H
#define LCD1604_I2C_H

#include "../includes.h"

// =============================
// Configuration
// =============================
#define LCD_I2C_ADDR    0x27   // 7-bit address (PCF8574, A0..A2=1)
#define LCD_COLS        20
#define LCD_ROWS        4

// PCF8574 bit mapping
#define LCD_BIT_RS      0x01
#define LCD_BIT_RW      0x02
#define LCD_BIT_E       0x04
#define LCD_BIT_BL      0x08   // Backlight
#define LCD_DATA_SHIFT  4      // D4-D7 on P4-P7

// =============================
// API
// =============================

// Initialize LCD. Must call i2c_sw_init() first.
void lcd_init(void);

// Clear display and return cursor to home.
void lcd_clear(void);

// Return cursor to home (row 0, col 0).
void lcd_home(void);

// Set cursor position (row 0..3, col 0..15).
void lcd_set_cursor(uint8_t row, uint8_t col);

// Print a single character at current cursor position.
void lcd_print_char(char c);

// Print a null-terminated string.
void lcd_print_str(const char *str);

// Print a signed 16-bit integer.
void lcd_print_int(int16_t value);

// Enable or disable backlight (1=on, 0=off).
void lcd_set_backlight(uint8_t on);

// Create a custom character (index 0..7, pattern 8 bytes of 5-bit rows).
void lcd_create_char(uint8_t index, const uint8_t *pattern);

#endif /* LCD1604_I2C_H */
