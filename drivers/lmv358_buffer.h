// lmv358_buffer.h - LMV358 Rail-to-Rail Buffer Interface

#ifndef LMV358_BUFFER_H
#define LMV358_BUFFER_H

#include <stdint.h>

// Pin assignments for buffer control/monitoring
#define LMV358_ENABLE_TRIS      TRISBbits.TRISB3
#define LMV358_ENABLE_LAT       LATBbits.LATB3

// Buffer voltage levels
#define LMV358_INPUT_MIN        0.0f        // 0V (rail-to-rail)
#define LMV358_INPUT_MAX        3.3f        // 3.3V (rail-to-rail)
#define LMV358_OUTPUT_MIN       0.0f        // 0V output
#define LMV358_OUTPUT_MAX       3.3f        // 3.3V output
#define LMV358_BIAS_VOLTAGE     1.65f       // Mid-rail bias

// ADL5375 interface levels
#define ADL5375_BIAS_LEVEL      0.5f        // 500mV bias required
#define ADL5375_SWING_MAX       0.5f        // 500mV max swing
#define ADL5375_INPUT_MIN       0.0f        // 0V minimum
#define ADL5375_INPUT_MAX       1.0f        // 1V maximum

// Buffer gain and scaling
#define LMV358_GAIN             1.0f        // Unity gain buffers
#define VOLTAGE_SCALE_FACTOR    (ADL5375_INPUT_MAX / LMV358_INPUT_MAX)  // 1/3.3

// Function prototypes
void lmv358_init(void);
void lmv358_enable(uint8_t enable);
uint8_t lmv358_is_enabled(void);

// Voltage conversion functions
float lmv358_scale_voltage_for_adl5375(float input_voltage);
uint16_t lmv358_convert_dac_value(uint16_t mcp4922_value);

// I/Q buffer functions
void lmv358_set_i_channel(float voltage);
void lmv358_set_q_channel(float voltage);
void lmv358_set_iq_channels(float i_voltage, float q_voltage);

// Test and calibration
void lmv358_test_buffers(void);
void lmv358_calibrate_offset(void);

#endif /* LMV358_BUFFER_H */