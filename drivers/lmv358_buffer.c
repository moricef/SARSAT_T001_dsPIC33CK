// lmv358_buffer.c - LMV358 Rail-to-Rail Buffer Implementation

#include "lmv358_buffer.h"
#include "mcp4922_driver.h"

static uint8_t lmv358_enabled = 0;

void lmv358_init(void) {
    LMV358_ENABLE_TRIS = 0;
    LMV358_ENABLE_LAT = 0;
    lmv358_enabled = 0;
}

void lmv358_enable(uint8_t enable) {
    LMV358_ENABLE_LAT = enable ? 1 : 0;
    lmv358_enabled = enable;
    if (enable) {
        __delay_ms(10);
    }
}

uint8_t lmv358_is_enabled(void) {
    return lmv358_enabled;
}

float lmv358_scale_voltage_for_adl5375(float input_voltage) {
    if (input_voltage < LMV358_INPUT_MIN) input_voltage = LMV358_INPUT_MIN;
    if (input_voltage > LMV358_INPUT_MAX) input_voltage = LMV358_INPUT_MAX;

    float output_voltage = input_voltage * VOLTAGE_SCALE_FACTOR;

    if (output_voltage > ADL5375_INPUT_MAX) output_voltage = ADL5375_INPUT_MAX;

    return output_voltage;
}

uint16_t lmv358_convert_dac_value(uint16_t mcp4922_value) {
    float input_voltage = ((float)mcp4922_value * LMV358_INPUT_MAX) / (float)MCP4922_RESOLUTION;
    float output_voltage = lmv358_scale_voltage_for_adl5375(input_voltage);
    uint16_t output_dac = (uint16_t)((output_voltage * (float)MCP4922_RESOLUTION) / LMV358_INPUT_MAX);
    return output_dac;
}

void lmv358_set_i_channel(float voltage) {
    float scaled_voltage = lmv358_scale_voltage_for_adl5375(voltage);
    uint16_t dac_value = (uint16_t)((scaled_voltage * (float)MCP4922_RESOLUTION) / VOLTAGE_SCALE_FACTOR);
    mcp4922_write_dac_a(dac_value);
}

void lmv358_set_q_channel(float voltage) {
    float scaled_voltage = lmv358_scale_voltage_for_adl5375(voltage);
    uint16_t dac_value = (uint16_t)((scaled_voltage * (float)MCP4922_RESOLUTION) / VOLTAGE_SCALE_FACTOR);
    mcp4922_write_dac_b(dac_value);
}

void lmv358_set_iq_channels(float i_voltage, float q_voltage) {
    float i_scaled = i_voltage * (1.0f / 3.3f);
    float q_scaled = q_voltage * (1.0f / 3.3f);
    uint16_t i_dac = (uint16_t)((i_scaled * 4096.0f) / (1.0f / 3.3f));
    uint16_t q_dac = (uint16_t)((q_scaled * 4096.0f) / (1.0f / 3.3f));
    mcp4922_write_both(i_dac, q_dac);
}

void lmv358_test_buffers(void) {
    if (!lmv358_enabled) {
        return;
    }

    lmv358_set_iq_channels(ADL5375_BIAS_LEVEL, ADL5375_BIAS_LEVEL);
    __delay_ms(100);

    lmv358_set_i_channel(ADL5375_INPUT_MIN);
    __delay_ms(50);
    lmv358_set_i_channel(ADL5375_INPUT_MAX);
    __delay_ms(50);
    lmv358_set_i_channel(ADL5375_BIAS_LEVEL);

    lmv358_set_q_channel(ADL5375_INPUT_MIN);
    __delay_ms(50);
    lmv358_set_q_channel(ADL5375_INPUT_MAX);
    __delay_ms(50);
    lmv358_set_q_channel(ADL5375_BIAS_LEVEL);

    lmv358_set_iq_channels(ADL5375_BIAS_LEVEL, ADL5375_BIAS_LEVEL);
}

void lmv358_calibrate_offset(void) {
    // Future: Measure actual output voltages and compensate for offsets
}