#ifndef PROTOCOL_DATA_H
#define PROTOCOL_DATA_H

#include "system_comms.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef __bool_true_false_are_defined
#define bool _Bool
#define true 1
#define false 0
#endif

// =============================
// Configuration de la transmission
// =============================
#define PREAMBLE_DURATION_MS 160
#define MODULATED_DURATION_MS 360
#define POSTAMBLE_DURATION_MS 320
#define TOTAL_TX_DURATION_MS                                                   \
  (PREAMBLE_DURATION_MS + MODULATED_DURATION_MS + POSTAMBLE_DURATION_MS)
#define MAX_DUTY_CYCLE 0.1
#define CARRIER_FREQ_HZ 40000
#define SYMBOL_RATE_HZ 400
#define SAMPLE_RATE_HZ 200000
#define SAMPLES_PER_SYMBOL (SAMPLE_RATE_HZ / SYMBOL_RATE_HZ)
#define SAMPLES_PER_HALF_BIT (SAMPLES_PER_SYMBOL / 2)
#define PREAMBLE_SAMPLES (PREAMBLE_DURATION_MS * SAMPLE_RATE_HZ / 1000)
#define POSTAMBLE_SAMPLES (POSTAMBLE_DURATION_MS * SAMPLE_RATE_HZ / 1000)

// =============================
// Configuration du protocole CS-T001
// =============================
#define MESSAGE_BITS 144
#define TEST_LATITUDE 42.95463
#define TEST_LONGITUDE 1.364479
#define TEST_ALTITUDE 1080

// BCH Polynomials (CS-T001 compliant)
#define BCH1_POLY       0x26D9E3  // 22-bit (X^21 + ... + 1)
#define BCH1_POLY_MASK  0x3FFFFF  // Masque 22 bits conforme
//#define BCH1_POLY_MASK  0x1FFFFF  // 21-bit mask
#define BCH1_DEGREE     21
#define BCH1_DATA_BITS  61

#define BCH2_POLY       0x1539    // 13-bit (X^12 + ... + 1)
#define BCH2_POLY_MASK  0x0FFF    // 12-bit mask
#define BCH2_DEGREE     12
#define BCH2_DATA_BITS  26

#define PROTOCOL_ELT_DT 0x9 // 1001 binary

// Frame sync patterns
#define SYNC_NORMAL_LONG 0x017  // 000101111 (9 bits MSB-first)
#define SYNC_SELF_TEST   0x0D0  // 011010000 (9 bits MSB-first)
//#define SYNC_NORMAL_LONG 0x2F // Normal long message
//#define SYNC_SELF_TEST 0x1A0  // Self-test message

// Country code (France)
#define COUNTRY_CODE_FRANCE 227

// Modes de balise
#define BEACON_MODE_NORMAL 0
#define BEACON_MODE_TEST 1

// Validation des polynômes BCH
#ifndef BCH_POLY_VALIDATED
#define BCH_POLY_VALIDATED

#if BCH1_POLY != 0x26D9E3
#error "BCH1 POLYNOMIAL INVALID! Must be 0x26D9E3 (CS-T001 standard)"
#endif

#if BCH2_POLY != 0x1539
#error "BCH2 POLYNOMIAL INVALID! Must be 0x1539 (CS-T001 standard)"
#endif

#endif

// =============================
// CS-T001 Bit Position Macros
// =============================
#define CS_BIT(bit_num) (bit_num - 1)  // Bit 1 = index 0, bit 144 = index 143
//#define CS_BIT(bit_num) (MESSAGE_BITS - (bit_num))
#define CS_BIT_RANGE(start_bit, end_bit)                                       \
  CS_BIT(start_bit), ((end_bit) - (start_bit) + 1)

// =============================
// Data Structures
// =============================
typedef struct __attribute__((packed)) {
  uint8_t gps_encoding_printed : 1;
  uint8_t frame_info_printed : 1;
  uint8_t build_msg_printed : 1;
  uint8_t test_frame_msg_printed : 1;
  uint8_t validation_printed : 1;
  uint8_t transmission_printed : 1;
  uint8_t frame_build_printed : 1;
  uint8_t power_mode_printed : 1;
  uint8_t power_printed : 1;
  uint8_t interval_adjusted_printed : 1; 
  uint8_t reserved : 1;             // Ajustement pour le packing
} debug_flags_t;

extern volatile debug_flags_t debug_flags;

// Complete GPS position structure for CS-T001 compliance
typedef struct {
  uint64_t full_position_40bit;   // Complete 40-bit encoding
  uint32_t coarse_position_21bit; // Truncated for short messages
  uint32_t fine_position_19bit;   // 30-minute resolution (PDF-1)
  uint32_t offset_position_18bit; // 4-second resolution offset
} cs_gps_position_t;

// Test vector structure
typedef struct {
  const char *name;
  uint64_t input_data;
  uint32_t expected_bch1;
  uint16_t expected_bch2;
  uint8_t data_bits;
} cs_test_vector_t;

// Mode utilisé pour le test
extern uint8_t beacon_mode;

// =============================
// BCH Functions
// =============================
uint32_t compute_bch(uint64_t data, int num_bits, uint32_t poly, int poly_degree, uint32_t poly_mask);
uint32_t compute_bch1(uint64_t data);
uint16_t compute_bch2(uint32_t data);
uint64_t bits_to_uint64(volatile uint8_t *bits, int start, int len);
uint32_t bits_to_uint32(volatile uint8_t *bits, int start, int len);
uint16_t bits_to_uint16(volatile uint8_t *bits, int start, int len);

// =============================
// GPS Functions - Updated for Compliance
// =============================
void set_gps_position(double lat, double lon, double alt);
void parse_nmea_gga(const char *line);

// PRIORITY 1: Fixed GPS encoding functions
cs_gps_position_t encode_gps_position_complete(double lat, double lon);
uint32_t encode_gps_position(double lat, double lon); // Backward compatibility
uint32_t compute_30min_position(double lat, double lon);
uint32_t compute_4sec_offset(double lat, double lon, uint32_t position_30min);
uint8_t altitude_to_code(double altitude);

// =============================
// PRIORITY 2: Standardized Bit Operations
// =============================
void set_bit_field(uint8_t *frame, uint16_t cs_start_bit, uint8_t length,
                   uint64_t value);
uint64_t get_bit_field(const uint8_t *frame, uint16_t cs_start_bit,
                       uint8_t length);
uint64_t get_bit_field_volatile(volatile const uint8_t *frame,
                                uint16_t cs_start_bit, uint8_t length);

// =============================
// Frame Construction - Updated
// =============================
void build_test_frame(void);      // Original function
void build_exercice_frame(void);  // Original function
void build_compliant_frame(void); // PRIORITY 2: New compliant version

// =============================
// PRIORITY 3: Comprehensive Testing
// =============================
void validate_cs_t001_comprehensive(void);
void validate_position_encoding(void);
void test_bch(void);
void test_bch_norm(void);
void test_cs_t001_vectors(void);
void cs_t001_full_compliance_check(void); // Master check function

// =============================
// PRIORITY 4: Improved Debug Output
// =============================
void debug_print_complete_frame_info(uint8_t include_hex);
void debug_print_frame_hex(
    volatile const uint8_t *frame); // Version paramétrée
void debug_print_beacon_frame_hex(
    void); // Version utilisant beacon_frame global
void debug_print_frame_bits(const uint8_t *frame, int len);
void debug_print_frame_hex_condensed(const uint8_t *frame, size_t nbits);
void debug_print_frame_analysis(volatile const uint8_t *frame);
void debug_print_field(const char *name, uint8_t *bits, int start, int length);

// Debug output functions for different data sizes
void debug_print_hex24(uint32_t value);
void debug_print_hex16(uint16_t value);
void debug_print_hex32(uint32_t value);
void debug_print_hex64(uint64_t value);

// Utility functions
void frame_to_bytes(const uint8_t *frame, int len, uint8_t *out_bytes);
void print_frame_hex(const uint8_t *frame_bits);
float get_freq_deviation(void); // Retourne la déviation de fréquence en Hz

// =============================
// NOUVEAU: Fonction de reinitialisation des flags anti-doublons
// =============================
void reset_debug_flags(void);
void initialize_debug_system(void);
uint8_t validate_frame_hardware(void);
void set_debug_flag_atomic(uint8_t flag_bit);
void full_error_diagnostic(void);
void transmit_beacon_frame(void);
void debug_print_int16(int16_t value);
void debug_print_hex24(uint32_t value);

// =============================
// CORRECTION 7: dsPIC33CK frame validation macro
// =============================
#define VALIDATE_CS_T001_FRAME()                                               \
  do {                                                                         \
    uint64_t pdf1 = get_bit_field_volatile(beacon_frame, 25, 61);              \
    uint32_t bch1_calc = compute_bch1(pdf1);                                   \
    uint32_t bch1_recv =                                                       \
        (uint32_t)get_bit_field_volatile(beacon_frame, 86, 21);                \
    uint32_t pdf2 = (uint32_t)get_bit_field_volatile(beacon_frame, 107, 26);   \
    uint16_t bch2_calc = compute_bch2(pdf2);                                   \
    uint16_t bch2_recv =                                                       \
        (uint16_t)get_bit_field_volatile(beacon_frame, 133, 12);               \
    if ((bch1_calc != bch1_recv) || (bch2_calc != bch2_recv)) {                \
      debug_print_str("FRAME VALIDATION ERROR\r\n");                           \
      return 0;                                                                \
    }                                                                          \
    debug_flags.validation_printed = 1; /* SET FLAG AFTER VALIDATION */        \
  } while (0)

// D�finitions pour acc�s atomique
#define DEBUG_FLAG_GPS_ENCODING 0
#define DEBUG_FLAG_FRAME_INFO 1
#define DEBUG_FLAG_BUILD_MSG 2
#define DEBUG_FLAG_TEST_FRAME 3
#define DEBUG_FLAG_VALIDATION 4
#define DEBUG_FLAG_TRANSMISSION 5

#define MAX_DUTY_CYCLE_PERCENT 0.1 // 10%
#define MIN_TX_INTERVAL_MS (TOTAL_TX_DURATION_MS / MAX_DUTY_CYCLE_PERCENT)

// =============================
// Variables globales partagees
// =============================
extern uint8_t frame[MESSAGE_BITS];
extern volatile uint8_t gps_updated;
extern double current_latitude;
extern double current_longitude;
extern double current_altitude;
extern volatile uint8_t beacon_frame[MESSAGE_BITS];

// =============================
// Convenience Macros for Frame Fields
// =============================
#define FRAME_PREAMBLE_START 1
#define FRAME_PREAMBLE_LENGTH 15
#define FRAME_SYNC_START 16
#define FRAME_SYNC_LENGTH 9
#define FRAME_FORMAT_FLAG_BIT 25
#define FRAME_PROTOCOL_FLAG_BIT 26
#define FRAME_COUNTRY_START 27
#define FRAME_COUNTRY_LENGTH 10
#define FRAME_PROTOCOL_START 37
#define FRAME_PROTOCOL_LENGTH 4
#define FRAME_BEACON_ID_START 41
#define FRAME_BEACON_ID_LENGTH 26
#define FRAME_POSITION_START 67
#define FRAME_POSITION_LENGTH 19
#define FRAME_BCH1_START 86
#define FRAME_BCH1_LENGTH 21
#define FRAME_ACTIVATION_START 107
#define FRAME_ACTIVATION_LENGTH 2
#define FRAME_ALTITUDE_START 109
#define FRAME_ALTITUDE_LENGTH 4
#define FRAME_FRESHNESS_START 113
#define FRAME_FRESHNESS_LENGTH 2
#define FRAME_OFFSET_START 115
#define FRAME_OFFSET_LENGTH 18
#define FRAME_BCH2_START 133
#define FRAME_BCH2_LENGTH 12

#endif // PROTOCOL_DATA_H
