// mcp4922_driver.c - MCP4922 Dual 12-bit DAC Driver Implementation

#include "../includes.h"
#include "mcp4922_driver.h"
#include "../system_debug.h"
#include <math.h>
#include <stdio.h>

void mcp4922_init(void) {
    // Configure SPI2 pins as digital (RB7-9 are digital by default)
    ANSELBbits.ANSELB7 = 0;  // RB7 digital (SCK2)
    ANSELBbits.ANSELB8 = 0;  // RB8 digital (SDO2)
    ANSELBbits.ANSELB9 = 0;  // RB9 digital (CS)

    // Configure SPI2 pins as outputs
    TRISBbits.TRISB7 = 0;  // RB7 (SCK2) as output
    TRISBbits.TRISB8 = 0;  // RB8 (SDO2) as output
    MCP4922_CS_TRIS = 0;   // RB9 (CS) as output
    MCP4922_CS_LAT = 1;    // CS high (inactive)

    // Configure PPS for SPI2
    __builtin_write_RPCON(0x0000);
    RPOR3bits.RP39R = 8;    // SCK2 on RB7 (RP39)
    RPOR4bits.RP40R = 8;    // SDO2 on RB8 (RP40)
    __builtin_write_RPCON(0x0800);

    // SPI2 Configuration
    SPI2CON1L = 0;          // Clear configuration
    SPI2CON1Lbits.MSTEN = 1;    // Master mode
    SPI2CON1Lbits.CKP = 0;      // Clock polarity: idle low
    SPI2CON1Lbits.CKE = 1;      // Clock edge: transmit on active to idle
    SPI2CON2Lbits.WLENGTH = 15; // 16-bit word length for MCP4922
    SPI2BRGL = 6;               // 4MHz clock
    SPI2CON1Lbits.SPIEN = 1;    // Enable SPI2

    DEBUG_LOG_FLUSH("MCP4922: SPI2 initialized (SCK2=RB7, SDO2=RB8, CS=RB9)\r\n");

    // Initialize DACs to mid-scale
    mcp4922_write_dac_a(MCP4922_OFFSET);
    mcp4922_write_dac_b(MCP4922_OFFSET);
}

void mcp4922_write_dac_a(uint16_t value) {
    value &= 0x0FFF;
    uint16_t command = MCP4922_DAC_A_CMD | value;
    MCP4922_CS_LAT = 0;
    while(SPI2STATLbits.SPITBF);
    SPI2BUFL = command;  // Single 16-bit transaction
    while(!SPI2STATLbits.SPIRBF);
    (void)SPI2BUFL;
    MCP4922_CS_LAT = 1;
}

void mcp4922_write_dac_b(uint16_t value) {
    value &= 0x0FFF;
    uint16_t command = MCP4922_DAC_B_CMD | value;
    MCP4922_CS_LAT = 0;
    while(SPI2STATLbits.SPITBF);
    SPI2BUFL = command;  // Single 16-bit transaction
    while(!SPI2STATLbits.SPIRBF);
    (void)SPI2BUFL;
    MCP4922_CS_LAT = 1;
}

void mcp4922_write_both(uint16_t i_value, uint16_t q_value) {
    mcp4922_write_dac_a(i_value);
    mcp4922_write_dac_b(q_value);
}

void mcp4922_shutdown(void) {
    MCP4922_CS_LAT = 0;
    while(SPI2STATLbits.SPITBF);
    SPI2BUFL = MCP4922_SHUTDOWN_A;  // Single 16-bit transaction
    while(!SPI2STATLbits.SPIRBF);
    (void)SPI2BUFL;
    MCP4922_CS_LAT = 1;
    __delay_us(1);
    MCP4922_CS_LAT = 0;
    while(SPI2STATLbits.SPITBF);
    SPI2BUFL = MCP4922_SHUTDOWN_B;  // Single 16-bit transaction
    while(!SPI2STATLbits.SPIRBF);
    (void)SPI2BUFL;
    MCP4922_CS_LAT = 1;
}

void mcp4922_set_iq_outputs(float i_amplitude, float q_amplitude) {
    uint16_t i_dac = (uint16_t)(MCP4922_OFFSET + i_amplitude * 2047);
    uint16_t q_dac = (uint16_t)(MCP4922_OFFSET + q_amplitude * 2047);
    if(i_dac > 4095) i_dac = 4095;
    if(q_dac > 4095) q_dac = 4095;
    mcp4922_write_both(i_dac, q_dac);
}

void mcp4922_output_oqpsk_symbol(uint8_t symbol_data) {
    float i_val = ((symbol_data & 0x02) ? -1.0 : +1.0);
    float q_val = ((symbol_data & 0x01) ? -1.0 : +1.0);
    mcp4922_set_iq_outputs(i_val, q_val);
}

void mcp4922_debug_spi2(void) {
    char debug_buf[128];
    DEBUG_LOG_FLUSH("SPI2 Status Debug:\r\n");
    sprintf(debug_buf, "SPI2CON1L=0x%04X SPIEN=%d MSTEN=%d\r\n",
            SPI2CON1L, SPI2CON1Lbits.SPIEN, SPI2CON1Lbits.MSTEN);
    DEBUG_LOG_FLUSH(debug_buf);
    sprintf(debug_buf, "SPI2STATL=0x%04X SPITBF=%d SPIRBF=%d\r\n",
            SPI2STATL, SPI2STATLbits.SPITBF, SPI2STATLbits.SPIRBF);
    DEBUG_LOG_FLUSH(debug_buf);
    sprintf(debug_buf, "SPI2BRGL=0x%04X\r\n", SPI2BRGL);
    DEBUG_LOG_FLUSH(debug_buf);
}

void mcp4922_test_pattern(void) {
    mcp4922_debug_spi2();

    for(int i = 0; i < 360; i += 10) {
        float angle = i * 3.14159 / 180.0;
        uint16_t i_val = (uint16_t)(MCP4922_OFFSET + 1000 * sin(angle));
        uint16_t q_val = (uint16_t)(MCP4922_OFFSET + 1000 * cos(angle));
        mcp4922_write_both(i_val, q_val);
        __delay_ms(1000);  // 200ms delay for oscilloscope capture
    }
    mcp4922_write_both(MCP4922_OFFSET, MCP4922_OFFSET);
}
