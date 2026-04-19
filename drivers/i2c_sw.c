/*
 * i2c_sw.c - Software (bit-bang) I2C master driver
 */

#include "i2c_sw.h"
#include "../system_debug.h"

void i2c_sw_init(void) {
    // Analog mode already disabled globally in init_gpio() (ANSELD = 0)
    // Start in released (high) state
    I2C_SCL_HIGH();
    I2C_SDA_HIGH();
    __delay_us(I2C_SW_DELAY_US);
}

// Returns 0 on success, 1 if bus is busy
uint8_t i2c_sw_start(void) {
    // SDA high, SCL high → SDA falls while SCL high = START
    I2C_SDA_HIGH();
    I2C_SCL_HIGH();
    __delay_us(I2C_SW_DELAY_US);

    if (!I2C_SDA_READ()) {
        return 1;  // Bus busy
    }

    I2C_SDA_LOW();
    __delay_us(I2C_SW_DELAY_US);
    I2C_SCL_LOW();
    __delay_us(I2C_SW_DELAY_US);
    return 0;
}

void i2c_sw_stop(void) {
    // SDA low, SCL rises, then SDA rises = STOP
    I2C_SDA_LOW();
    __delay_us(I2C_SW_DELAY_US);
    I2C_SCL_HIGH();
    __delay_us(I2C_SW_DELAY_US);
    I2C_SDA_HIGH();
    __delay_us(I2C_SW_DELAY_US);
}

// Returns 0 = ACK, 1 = NACK
uint8_t i2c_sw_write_byte(uint8_t data) {
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8; i++) {
        if (data & 0x80) {
            I2C_SDA_HIGH();
        } else {
            I2C_SDA_LOW();
        }
        data <<= 1;
        __delay_us(I2C_SW_DELAY_US);
        I2C_SCL_HIGH();
        __delay_us(I2C_SW_DELAY_US);
        I2C_SCL_LOW();
    }

    // Release SDA for ACK
    I2C_SDA_HIGH();
    __delay_us(I2C_SW_DELAY_US);
    I2C_SCL_HIGH();
    __delay_us(I2C_SW_DELAY_US);
    ack = I2C_SDA_READ() ? 1 : 0;  // 0 = ACK, 1 = NACK
    I2C_SCL_LOW();
    __delay_us(I2C_SW_DELAY_US);

    return ack;
}

// ack=1: send ACK (more bytes), ack=0: send NACK (last byte)
uint8_t i2c_sw_read_byte(uint8_t ack) {
    uint8_t i;
    uint8_t data = 0;

    I2C_SDA_HIGH();  // Release SDA for reading

    for (i = 0; i < 8; i++) {
        data <<= 1;
        __delay_us(I2C_SW_DELAY_US);
        I2C_SCL_HIGH();
        __delay_us(I2C_SW_DELAY_US);
        if (I2C_SDA_READ()) {
            data |= 0x01;
        }
        I2C_SCL_LOW();
    }

    // Send ACK or NACK
    if (ack) {
        I2C_SDA_LOW();
    } else {
        I2C_SDA_HIGH();
    }
    __delay_us(I2C_SW_DELAY_US);
    I2C_SCL_HIGH();
    __delay_us(I2C_SW_DELAY_US);
    I2C_SCL_LOW();
    __delay_us(I2C_SW_DELAY_US);

    return data;
}

// Returns 0 on success
uint8_t i2c_sw_write(uint8_t addr7, const uint8_t *buf, uint8_t len) {
    uint8_t i;

    if (i2c_sw_start()) {
        return 1;  // Bus busy
    }

    // Address byte: addr7 << 1 | 0 (write)
    if (i2c_sw_write_byte((uint8_t)(addr7 << 1))) {
        i2c_sw_stop();
        return 1;  // No ACK from device
    }

    for (i = 0; i < len; i++) {
        if (i2c_sw_write_byte(buf[i])) {
            i2c_sw_stop();
            return 1;
        }
    }

    i2c_sw_stop();
    return 0;
}

uint8_t i2c_sw_probe(uint8_t addr7) {
    uint8_t nack;
    if (i2c_sw_start()) return 1;
    nack = i2c_sw_write_byte((uint8_t)(addr7 << 1));
    i2c_sw_stop();
    return nack;
}

void i2c_sw_scan(void) {
    uint8_t addr;
    uint8_t found = 0;
    DEBUG_LOG_FLUSH("I2C scan...\r\n");
    for (addr = 0x08; addr <= 0x77; addr++) {
        if (!i2c_sw_probe(addr)) {
            char hi = (char)((addr >> 4) & 0x0F);
            char lo = (char)(addr & 0x0F);
            hi = (hi < 10) ? (char)('0' + hi) : (char)('A' + hi - 10);
            lo = (lo < 10) ? (char)('0' + lo) : (char)('A' + lo - 10);
            DEBUG_LOG_FLUSH("I2C device at 0x");
            debug_print_char(hi);
            debug_print_char(lo);
            DEBUG_LOG_FLUSH("\r\n");
            found++;
        }
    }
    if (!found) {
        DEBUG_LOG_FLUSH("I2C scan: no device found\r\n");
    }
}
