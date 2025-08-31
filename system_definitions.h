#ifndef SYSTEM_DEFINITIONS_H
#define SYSTEM_DEFINITIONS_H

// =============================
// System Configuration Constants
// =============================
#define MIN_TX_INTERVAL_MS 5000      // Minimum 5 seconds between transmissions
#define TOTAL_TX_DURATION_MS 840     // Total transmission duration (160+360+320)
#define MAX_DUTY_CYCLE 0.06          // 10% maximum duty cycle
#define PREAMBLE_BITS 16
#define POSTAMBLE_BITS 8
#define OVERSAMPLING 16
#define TOTAL_SAMPLES_PER_BIT (OVERSAMPLING)
#define MESSAGE_BITS 144

// =============================
// Configuration de la transmission
// =============================
// TOTAL_TX_DURATION_MS, MAX_DUTY_CYCLE, MIN_TX_INTERVAL_MS already defined there

#define PREAMBLE_DURATION_MS 160
#define MODULATED_DURATION_MS 360
#define POSTAMBLE_DURATION_MS 320

#define CARRIER_FREQ_HZ 40000
#define SYMBOL_RATE_HZ 400
#define SAMPLE_RATE_HZ 200000
#define SAMPLES_PER_SYMBOL (SAMPLE_RATE_HZ / SYMBOL_RATE_HZ)
#define SAMPLES_PER_HALF_BIT (SAMPLES_PER_SYMBOL / 2)

// =============================
// Hardware Pin Definitions
// =============================
#define LED_TX_PIN           LATDbits.LATD10

// =============================
// Function Prototypes
// =============================
void system_init(void);
void full_error_diagnostic(void);
float read_pll_deviation(void);

extern volatile uint8_t beacon_frame[MESSAGE_BITS];

// =============================
// 
// =============================
typedef struct  __attribute__((packed)) {
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
    volatile uint8_t log_mode : 2;  // 2 bits pour stocker les 4 modes (log_mode_t)
} debug_flags_t;

#endif // SYSTEM_DEFINITIONS_H