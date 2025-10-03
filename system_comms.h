#ifndef SYSTEM_COMMS_H
#define SYSTEM_COMMS_H

#include "system_definitions.h"

// =============================
// Hardware Configuration
// =============================
// DAC Configuration
#define DAC_RESOLUTION          4096            // 12-bit DAC
#define DAC_BIAS_LEVEL          2048            // 1.65V mid-point (500mV for ADL5375)
#define VOLTAGE_REF_3V3         3.3f            // dsPIC33CK supply voltage

// ADL5375 I/Q Modulator Configuration with LMV358 filter stages (5V supply)
#define ADL5375_BIAS_MV         1650            // 1650mV bias level (3.3V/2)
#define ADL5375_SWING_MV        1000            // 1000mV peak-to-peak swing (±500mV)
#define ADL5375_MIN_VOLTAGE     1.15f           // Minimum output voltage (1.65V - 0.5V)
#define ADL5375_MAX_VOLTAGE     2.15f           // Maximum output voltage (1.65V + 0.5V)

// BPSK Modulation Parameters
#define PHASE_SHIFT_RADIANS     1.1f            // ±1.1 rad (SARSAT T.001 standard)

// Timing
#define MODULATION_INTERVAL     1               // Process every sample

// =============================
// Transmission State Machine - Native Design
// =============================
typedef enum {
    IDLE_STATE,                 // System idle - no transmission
    RF_STARTUP,                 // RF chain initialization and stabilization
    CARRIER_TX,                 // Unmodulated carrier transmission
    DATA_TX,                    // Modulated data transmission
    RF_SHUTDOWN                 // RF chain clean shutdown
} tx_phase_t;

// =============================
// Function Prototypes
// =============================

// System initialization
void init_clock(void);
void init_gpio(void);
void init_dac(void);
void init_timer1(void);
void system_init(void);

// Transmission control
void start_transmission(volatile const uint8_t* data);
void set_tx_interval(uint32_t interval_ms);

// Signal processing
uint16_t calculate_bpsk_dac_value(uint8_t bit_value, uint16_t sample_index);
uint16_t calculate_carrier_dac_value(void);
uint16_t calculate_idle_dac_value(void);

// RF timing calibration
void calibrate_rf_timing(void);

// Interrupt Service Routines
void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt(void);

// =============================
// Global Variables
// =============================
extern volatile uint32_t millis_counter;           // System time in milliseconds
extern volatile tx_phase_t tx_phase;               // Current transmission phase
extern volatile uint32_t last_tx_time;             // Last transmission timestamp
extern volatile uint32_t tx_interval_ms;           // Transmission interval
extern volatile uint16_t bit_index;                // Current bit index in message
extern volatile uint16_t sample_count;             // Sample counter within current phase
extern volatile uint8_t beacon_frame[MESSAGE_BITS]; // Message data buffer
extern volatile uint8_t transmission_complete_flag; // Transmission completion flag

// RF timing variables
extern volatile uint16_t rf_startup_samples;       // RF startup time in samples
extern volatile uint16_t rf_shutdown_samples;      // RF shutdown time in samples

// Legacy variables for debug compatibility
extern volatile uint16_t modulation_counter;       // Modulation timing counter
extern volatile uint8_t carrier_phase;             // Carrier phase (legacy)
extern volatile float envelope_gain;               // RF envelope gain

#endif