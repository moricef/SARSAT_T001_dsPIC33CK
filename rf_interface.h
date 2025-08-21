#ifndef RF_INTERFACE_H
#define RF_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// =============================
// RF Power Level Definitions
// =============================
#define RF_POWER_LOW  0     // 100mW mode for local tests
#define RF_POWER_HIGH 1     // 5W mode for exercises

// =============================
// ADF4351 PLL Synthesizer Functions
// =============================
void rf_init_adf4351(void);                    // Initialize ADF4351 @ 403 MHz
void rf_adf4351_enable_output(uint8_t state);  // Enable/disable RF output

// =============================
// ADL5375 I/Q Modulator Functions
// =============================
void rf_init_adl5375(void);                    // Initialize ADL5375 I/Q modulator
void rf_adl5375_enable(uint8_t state);         // Enable/disable modulator

// =============================
// RA07H4047M Power Amplifier Functions
// =============================
void rf_init_power_amplifier(void);            // Initialize PA control
void rf_set_power_level(uint8_t mode);         // Set power level (LOW/HIGH)
void rf_control_amplifier_chain(uint8_t state);// Enable/disable complete RF chain

// =============================
// Master RF Control Functions
// =============================
void rf_initialize_all_modules(void);          // Initialize complete RF chain
uint8_t rf_get_amplifier_state(void);          // Get current amplifier state
uint8_t rf_get_power_mode(void);               // Get current power mode

// =============================
// Error Handling
// =============================
void rf_system_halt(const char* message);      // RF error handler with safe shutdown

// =============================
// Legacy compatibility (mapped to new functions)
// =============================
#define init_adf4351()                 rf_init_adf4351()
#define adf4351_enable_rf(state)       rf_adf4351_enable_output(state)
#define init_adl5375()                 rf_init_adl5375()
#define adl5375_enable(state)          rf_adl5375_enable(state)
#define init_rf_amplifier()            rf_init_power_amplifier()
#define set_rf_power_level(mode)       rf_set_power_level(mode)
#define control_rf_amplifier(state)    rf_control_amplifier_chain(state)

#endif // RF_INTERFACE_H