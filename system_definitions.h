#ifndef SYSTEM_DEFINITIONS_H
#define SYSTEM_DEFINITIONS_H

#include <stdint.h>

// =============================
// SARSAT T.001 Native Specification
// =============================
#define MIN_TX_INTERVAL_MS      5000    // Minimum 5 seconds between transmissions
#define CARRIER_DURATION_MS     160     // Unmodulated carrier duration
#define DATA_DURATION_MS        360     // Modulated data duration
#define TOTAL_BURST_DURATION_MS 520     // Total burst time (160+360)
#define MAX_DUTY_CYCLE          0.06    // 6% maximum duty cycle

// RF timing - empirically determined for hardware stability
#define RF_STARTUP_TIME_MS      1      // Time needed for RF chain stabilization
#define RF_SHUTDOWN_TIME_MS     0      // Immediate RF shutdown with power-down mode
#define PLL_LOCK_TIMEOUT_MS     10      // Maximum time to wait for PLL lock

// Message structure
#define MESSAGE_BITS            144     // Total bits to transmit
#define SYMBOL_RATE_HZ          400     // 400 baud symbol rate (SARSAT standard)
#define SAMPLES_PER_SYMBOL      16      // Oversampling factor
#define SAMPLE_RATE_HZ          6400    // Derived sample rate (400 * 16)

// Hardware timing calculations
#define CARRIER_SAMPLES         ((uint32_t)CARRIER_DURATION_MS * SAMPLE_RATE_HZ / 1000)    // 1024
#define DATA_SAMPLES            ((uint32_t)MESSAGE_BITS * SAMPLES_PER_SYMBOL)              // 2304
#define RF_STARTUP_SAMPLES      ((uint32_t)RF_STARTUP_TIME_MS * SAMPLE_RATE_HZ / 1000)    // 320
#define RF_SHUTDOWN_SAMPLES     ((uint32_t)RF_SHUTDOWN_TIME_MS * SAMPLE_RATE_HZ / 1000)   // 64

// =============================
// Hardware Pin Definitions
// =============================
#define LED_TX_PIN              LATDbits.LATD10

// =============================
// Function Prototypes
// =============================
void init_all_pps(void);         // Centralized PPS configuration
void system_init(void);
void full_error_diagnostic(void);
float read_pll_deviation(void);

extern volatile uint8_t beacon_frame[MESSAGE_BITS];

// =============================
// Debug Flags Structure
// =============================
typedef struct __attribute__((packed)) {
    volatile uint8_t gps_encoding_printed : 1;
    volatile uint8_t frame_info_printed : 1;
    volatile uint8_t build_msg_printed : 1;
    volatile uint8_t test_frame_msg_printed : 1;
    volatile uint8_t validation_printed : 1;
    volatile uint8_t transmission_printed : 1;
    volatile uint8_t frame_build_printed : 1;
    volatile uint8_t power_mode_printed : 1;
    volatile uint8_t power_printed : 1;
    volatile uint8_t interval_adjusted_printed : 1;
    volatile uint8_t reserved : 1;
    volatile uint8_t diagnostic_printed : 1;
    volatile uint8_t led_test_printed : 1;
    volatile uint8_t isr_logging_enabled : 1;
    volatile uint8_t log_mode : 2;
} debug_flags_t;

#endif // SYSTEM_DEFINITIONS_H
