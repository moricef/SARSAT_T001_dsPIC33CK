/*
 * lcd1604_i2c.c - LCD 16x4 HD44780 driver via PCF8574 I2C backpack
 */

#include "lcd1604_i2c.h"
#include "i2c_sw.h"
#include <stdio.h>

// Row start addresses for 16x4 LCD (HD44780 DDRAM offsets)
static const uint8_t lcd_row_offsets[4] = { 0x00, 0x40, 0x10, 0x50 };

static uint8_t lcd_backlight = LCD_BIT_BL;  // Backlight on by default

// =============================
// Low-level PCF8574 write
// =============================
static void lcd_pcf8574_write(uint8_t data) {
    uint8_t byte = data | lcd_backlight;
    i2c_sw_write(LCD_I2C_ADDR, &byte, 1);
}

// Pulse the Enable pin to latch 4-bit nibble
static void lcd_pulse_enable(uint8_t data) {
    lcd_pcf8574_write(data | LCD_BIT_E);
    __delay_us(1);
    lcd_pcf8574_write(data & ~LCD_BIT_E);
    __delay_us(50);  // Command execution time (min 37 µs)
}

// Send a 4-bit nibble (data on P4-P7)
static void lcd_write_nibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = (uint8_t)((nibble << LCD_DATA_SHIFT) & 0xF0);
    if (rs) {
        data |= LCD_BIT_RS;
    }
    lcd_pulse_enable(data);
}

// Send a full byte (two nibbles, MSB first)
static void lcd_write_byte(uint8_t byte, uint8_t rs) {
    lcd_write_nibble(byte >> 4, rs);
    lcd_write_nibble(byte & 0x0F, rs);
}

// Send a command byte (RS=0)
static void lcd_command(uint8_t cmd) {
    lcd_write_byte(cmd, 0);
}

// Send a data byte (RS=1)
static void lcd_data(uint8_t data) {
    lcd_write_byte(data, 1);
}

// =============================
// API implementation
// =============================

void lcd_init(void) {
    __delay_ms(50);  // Wait for LCD power-up (>40 ms)

    // Initialize in 4-bit mode (HD44780 power-on sequence)
    lcd_pcf8574_write(0x00);
    __delay_ms(20);

    // Send 0x3 three times to reset (8-bit mode init)
    lcd_write_nibble(0x03, 0);
    __delay_ms(5);
    lcd_write_nibble(0x03, 0);
    __delay_us(150);
    lcd_write_nibble(0x03, 0);
    __delay_us(150);

    // Switch to 4-bit mode
    lcd_write_nibble(0x02, 0);
    __delay_us(150);

    // Function Set: 4-bit, 2 lines (covers 4 rows), 5x8 dots
    lcd_command(0x28);
    __delay_us(50);

    // Display OFF
    lcd_command(0x08);
    __delay_us(50);

    // Clear display
    lcd_command(0x01);
    __delay_ms(2);  // Clear needs >1.52 ms

    // Entry mode: increment cursor, no display shift
    lcd_command(0x06);
    __delay_us(50);

    // Display ON, cursor OFF, blink OFF
    lcd_command(0x0C);
    __delay_us(50);
}

void lcd_clear(void) {
    lcd_command(0x01);
    __delay_ms(2);
}

void lcd_home(void) {
    lcd_command(0x02);
    __delay_ms(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    lcd_command((uint8_t)(0x80 | (lcd_row_offsets[row] + col)));
}

void lcd_print_char(char c) {
    lcd_data((uint8_t)c);
}

void lcd_print_str(const char *str) {
    while (*str) {
        lcd_data((uint8_t)*str);
        str++;
    }
}

void lcd_print_int(int16_t value) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);
    lcd_print_str(buf);
}

void lcd_set_backlight(uint8_t on) {
    lcd_backlight = on ? LCD_BIT_BL : 0;
    lcd_pcf8574_write(0x00);  // Send a write to update backlight state
}

void lcd_create_char(uint8_t index, const uint8_t *pattern) {
    uint8_t i;
    index &= 0x07;  // Only 8 custom characters (0..7)
    lcd_command((uint8_t)(0x40 | (index << 3)));
    for (i = 0; i < 8; i++) {
        lcd_data(pattern[i]);
    }
    lcd_command(0x80);  // Return to DDRAM (position 0)
}
