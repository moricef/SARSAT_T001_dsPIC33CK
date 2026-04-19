/*
 * lcd1604_i2c.c - LCD 16x4 HD44780 driver via PCF8574 I2C backpack
 */

#include "lcd1604_i2c.h"
#include "i2c_sw.h"
#include "../system_debug.h"
#include <stdio.h>

static volatile uint16_t lcd_nack_count = 0;

// Row start addresses for 20x4 LCD (HD44780 DDRAM offsets)
static const uint8_t lcd_row_offsets[4] = { 0x00, 0x40, 0x14, 0x54 };

static uint8_t lcd_backlight = LCD_BIT_BL;  // Backlight on by default

// =============================
// Low-level PCF8574 write
// =============================
static void lcd_pcf8574_write(uint8_t data) {
    uint8_t byte = data | lcd_backlight;
    if (i2c_sw_write(LCD_I2C_ADDR, &byte, 1)) {
        lcd_nack_count++;
    }
}

// Pulse the Enable pin to latch 4-bit nibble
static void lcd_pulse_enable(uint8_t data) {
    lcd_pcf8574_write(data | LCD_BIT_E);
    __delay_us(1);
    lcd_pcf8574_write(data & ~LCD_BIT_E);
    __delay_us(50);
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
    uint16_t n0, n1, n2, n3, n4, n5;
    __delay_ms(50);

    lcd_nack_count = 0;
    lcd_pcf8574_write(0x00);
    __delay_ms(20);
    n0 = lcd_nack_count;

    lcd_write_nibble(0x03, 0);
    __delay_ms(5);
    lcd_write_nibble(0x03, 0);
    __delay_us(150);
    lcd_write_nibble(0x03, 0);
    __delay_us(150);
    n1 = lcd_nack_count;

    lcd_write_nibble(0x02, 0);
    __delay_us(150);
    n2 = lcd_nack_count;

    lcd_command(0x28);
    __delay_us(50);
    n3 = lcd_nack_count;

    lcd_command(0x08);
    __delay_us(50);

    lcd_command(0x01);
    __delay_ms(2);
    n4 = lcd_nack_count;

    lcd_command(0x06);
    __delay_us(50);

    lcd_command(0x0C);
    __delay_us(50);
    n5 = lcd_nack_count;

    DEBUG_LOG_FLUSH("LCD init NACKs: n0=");
    debug_print_uint16(n0);
    DEBUG_LOG_FLUSH(" n1=");
    debug_print_uint16(n1);
    DEBUG_LOG_FLUSH(" n2=");
    debug_print_uint16(n2);
    DEBUG_LOG_FLUSH(" n3=");
    debug_print_uint16(n3);
    DEBUG_LOG_FLUSH(" n4=");
    debug_print_uint16(n4);
    DEBUG_LOG_FLUSH(" n5=");
    debug_print_uint16(n5);
    DEBUG_LOG_FLUSH("\r\n");
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
