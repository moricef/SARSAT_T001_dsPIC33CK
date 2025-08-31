#ifndef SYSTEM_COMMS_H
#define SYSTEM_COMMS_H

#include "system_definitions.h"

// =============================
// ELT Standard Configuration
// =============================
#define SYMBOL_RATE_HZ          400     // Mandatory 400 baud symbol rate
#define OVERSAMPLING            16      // 16 samples per symbol
#define ACTUAL_SAMPLE_RATE_HZ   (SYMBOL_RATE_HZ * OVERSAMPLING)  // 6400 Hz

// Transmission segment durations (ms)
#define PREAMBLE_DURATION_MS    160     // Unmodulated carrier
#define MODULATED_DURATION_MS   360     // Biphase-L modulated data
#define POSTAMBLE_DURATION_MS   320     // Silence after transmission
#define MODULATION_INTERVAL 1
#define SYMMETRY_THRESHOLD 0.05f
#define RISE_TIME_MIN_US 50
#define RISE_TIME_MAX_US 150

// Sample calculations
#define PREAMBLE_SAMPLES        ((uint32_t)PREAMBLE_DURATION_MS * ACTUAL_SAMPLE_RATE_HZ / 1000)   // 1024
#define POSTAMBLE_SAMPLES       ((uint32_t)POSTAMBLE_DURATION_MS * ACTUAL_SAMPLE_RATE_HZ / 1000)  // 2048

// Message structure
#define SYNC_BITS               15      // Synchronization bits
#define FRAME_SYNC_BITS         9       // Frame synchronization bits
#define DATA_BITS               120     // Payload data bits (144 - 15 - 9)
#define MESSAGE_BITS            144     // Total bits (SYNC + FRAME_SYNC + DATA)

// RF Parameters
#define RAMP_SAMPLES_DEFAULT    10      // ~1.56ms @6400Hz (<2ms required)
#define PHASE_SHIFT_RADIANS     1.1f    // �1.1 rad (C/S T.001 standard)

// =============================
// Hardware Configuration
// =============================
#define LED_TX_PIN              LATDbits.LATD10  // Transmission indicator LED

// DAC Configuration  
#define DAC_OFFSET              2048             // 1.65V mid-point
#define DAC_RESOLUTION          4096             // 12-bit DAC
#define SAMPLE_FREQ_HZ          6400             // 6.4 kHz sample rate

// ADL5375-05 Interface Configuration
#define ADL5375_BIAS_MV         500              // 500mV bias level for ADL5375-05
#define ADL5375_SWING_MV        500              // 500mV p-p per pin (1V p-p differential)
#define ADL5375_MIN_VOLTAGE     0.0f             // Minimum voltage (bias - swing/2)
#define ADL5375_MAX_VOLTAGE     1.0f             // Maximum voltage (bias + swing/2)
#define VOLTAGE_REF_3V3         3.3f             // dsPIC33CK supply voltage

// =============================
// Transmission States
// =============================
typedef enum {
    IDLE_STATE,             // System idle
    PRE_AMPLI_RAMP_UP,      // RF power ramp up
    PREAMBLE_PHASE,         // Unmodulated carrier
    DATA_PHASE,             // Biphase-L modulated data
    POSTAMBLE_PHASE,        // Post-transmission silence
    POST_AMPLI_RAMP_DOWN    // RF power ramp down
} tx_phase_t;

// =============================
// Function Prototypes
// =============================

// Initialization functions
void init_clock(void);       // System clock configuration
void init_gpio(void);        // GPIO pin initialization
void init_dac(void);         // DAC module setup
void init_timer1(void);      // Timer1 for sample generation
void system_init(void);      // Main system initialization

// RF Control functions - moved to rf_interface.h

// Transmission management
void calibrate_rise_fall_times(void);    // RF ramp timing calibration
void start_transmission(volatile const uint8_t* data);  // Start TX sequence
uint16_t calculate_modulated_value(float phase_shift, uint8_t carrier_phase, uint8_t apply_envelope);  // BPSK modulation

// ADL5375-05 Interface functions
uint16_t adapt_dac_for_adl5375(uint16_t dac_value);     // Convert DAC levels for ADL5375-05
uint16_t calculate_adl5375_q_channel(float phase_shift, uint8_t apply_envelope);  // Q channel for ADL5375-05

void set_tx_interval(uint32_t interval_ms); 

// Interrupt Service Routines
void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt(void);  // Timer1 ISR

// =============================
// Shared Global Variables
// =============================
extern volatile uint32_t millis_counter;          // System time in milliseconds
extern volatile tx_phase_t tx_phase;              // Current transmission phase
extern volatile uint32_t last_tx_time;            // Last transmission timestamp
extern volatile uint32_t tx_interval_ms;          // Transmission interval
extern volatile uint8_t carrier_phase;            // Current carrier phase
extern volatile uint32_t phase_sample_count;      // Phase sample counter
extern volatile uint16_t bit_index;               // Current bit index in frame
extern volatile uint8_t beacon_frame[MESSAGE_BITS]; // Transmission frame data
extern volatile float envelope_gain;              // RF envelope gain (0.0-1.0)
extern volatile uint16_t ramp_samples;            // RF ramp duration in samples
extern volatile uint16_t current_ramp_count;      // Current ramp position
extern volatile uint16_t modulation_counter;      // Modulation sample counter
extern volatile uint16_t sample_count;            // General sample counter
extern volatile uint8_t amp_enabled;              // RF amplifier state
extern volatile uint8_t current_power_mode;       // Current power mode
extern volatile uint8_t transmission_complete_flag; // TX completion flag

#endif