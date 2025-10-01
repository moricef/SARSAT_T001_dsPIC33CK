// mcp4922_driver.h - MCP4922 Dual 12-bit DAC Driver for OQPSK I/Q

#ifndef MCP4922_DRIVER_H
#define MCP4922_DRIVER_H

#include <stdint.h>

// MCP4922 SPI Commands
#define MCP4922_DAC_A_CMD       0x7000  // DAC A, buffered, gain=1, active
#define MCP4922_DAC_B_CMD       0xF000  // DAC B, buffered, gain=1, active
#define MCP4922_SHUTDOWN_A      0x6000  // Shutdown DAC A
#define MCP4922_SHUTDOWN_B      0xE000  // Shutdown DAC B

// MCP4922 Configuration
#define MCP4922_RESOLUTION      4096    // 12-bit DAC
#define MCP4922_OFFSET          2048    // Mid-scale (1.65V with 3.3V ref)

// Pin assignments for SPI2 (RB7, RB8, RB9)
#define MCP4922_CS_TRIS         TRISBbits.TRISB9
#define MCP4922_CS_LAT          LATBbits.LATB9

// Function prototypes
void mcp4922_init(void);
void mcp4922_write_dac_a(uint16_t value);
void mcp4922_write_dac_b(uint16_t value);
void mcp4922_write_both(uint16_t i_value, uint16_t q_value);
void mcp4922_shutdown(void);
void mcp4922_debug_spi2(void);
void mcp4922_test_pattern(void);

// I/Q specific functions
void mcp4922_set_iq_outputs(float i_amplitude, float q_amplitude);
void mcp4922_output_oqpsk_symbol(uint8_t symbol_data);

#endif /* MCP4922_DRIVER_H */