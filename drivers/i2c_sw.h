/*
 * i2c_sw.h - Software (bit-bang) I2C master driver
 *
 * Open-drain emulation: TRIS=1 (input) = high, TRIS=0 + LAT=0 = low.
 * Requires external pull-up resistors on SCL and SDA (typically 4.7k to 3.3V).
 *
 * Default pins: SCL=RD8, SDA=RD1 (modify defines below to change).
 */

#ifndef I2C_SW_H
#define I2C_SW_H

#include "../includes.h"

// =============================
// Pin Configuration
// =============================
#define I2C_SW_SCL_TRIS   TRISDbits.TRISD8
#define I2C_SW_SCL_LAT    LATDbits.LATD8
#define I2C_SW_SCL_PORT   PORTDbits.RD8
#define I2C_SW_SCL_ANSEL  ANSELDbits.ANSELD8

#define I2C_SW_SDA_TRIS   TRISDbits.TRISD1
#define I2C_SW_SDA_LAT    LATDbits.LATD1
#define I2C_SW_SDA_PORT   PORTDbits.RD1
#define I2C_SW_SDA_ANSEL  ANSELDbits.ANSELD1

// Half-period delay in µs (5 µs → ~100 kHz at FCY=50 MHz)
#define I2C_SW_DELAY_US   5

// =============================
// Open-drain helpers
// =============================
#define I2C_SCL_HIGH()  do { I2C_SW_SCL_LAT = 0; I2C_SW_SCL_TRIS = 1; } while(0)
#define I2C_SCL_LOW()   do { I2C_SW_SCL_LAT = 0; I2C_SW_SCL_TRIS = 0; } while(0)
#define I2C_SDA_HIGH()  do { I2C_SW_SDA_LAT = 0; I2C_SW_SDA_TRIS = 1; } while(0)
#define I2C_SDA_LOW()   do { I2C_SW_SDA_LAT = 0; I2C_SW_SDA_TRIS = 0; } while(0)
#define I2C_SDA_READ()  (I2C_SW_SDA_PORT)

// =============================
// API
// =============================

// Initialize GPIO pins for I2C (call once at startup)
void i2c_sw_init(void);

// Send START condition. Returns 0 on success.
uint8_t i2c_sw_start(void);

// Send STOP condition.
void i2c_sw_stop(void);

// Write one byte. Returns 0 if ACK received, 1 if NACK.
uint8_t i2c_sw_write_byte(uint8_t data);

// Read one byte. ack=1 to ACK (more bytes follow), ack=0 to NACK (last byte).
uint8_t i2c_sw_read_byte(uint8_t ack);

// Convenience: write a buffer to a 7-bit address. Returns 0 on success.
uint8_t i2c_sw_write(uint8_t addr7, const uint8_t *buf, uint8_t len);

#endif /* I2C_SW_H */
