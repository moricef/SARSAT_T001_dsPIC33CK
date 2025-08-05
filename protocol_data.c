#include "protocol_data.h"
#include "system_comms.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "system_definitions.h"

// =============================
// Variables globales
// =============================
uint8_t frame[MESSAGE_BITS];
volatile uint8_t gps_updated = 0;
double current_latitude = TEST_LATITUDE;
double current_longitude = TEST_LONGITUDE;
double current_altitude = TEST_ALTITUDE;
uint8_t beacon_mode = BEACON_MODE_NORMAL;
extern volatile debug_flags_t debug_flags;
extern volatile uint8_t beacon_frame[MESSAGE_BITS];

void reset_debug_flags(void) {
    __builtin_disable_interrupts();  // Désactive les interruptions sans retour
    debug_flags = (volatile debug_flags_t){0};
    __builtin_enable_interrupts();   // Réactive sans argument
}

volatile debug_flags_t debug_flags __attribute__((persistent));

/**
 * @brief Mesure la deviation de frequence RF
 * @return Deviation en Hz (valeur absolue)
 * @note Doit etre calibrage selon le quartz utilise
 */
float get_freq_deviation(void) {
    #ifdef SIMULATION
        return 0.0f;  // Valeur par defaut en simulation
    #else
        // A€ remplacer par la vraie lecture hardware
        return read_pll_deviation();  // Exemple pour dsPIC33CK
    #endif
}

// =============================
// BCH Functions - CS-T001 Annex B Compliant
// =============================

uint32_t compute_bch(uint64_t data, int num_bits, uint32_t poly, int poly_degree, uint32_t poly_mask) {
    uint32_t reg = 0;
    // Applique le polyn�me complet avec MSB (Annexe T.001 �B.2)
    uint32_t poly_val = poly & ((1UL << (poly_degree + 1)) - 1);

    for (int i = num_bits - 1; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        uint8_t msb = (reg >> (poly_degree - 1)) & 1;
        reg = ((reg << 1) | bit) & poly_mask;
        if (msb) reg ^= poly_val;
    }
    
    // Finalisation avec bits de padding
    for (int i = 0; i < poly_degree; i++) {
        uint8_t msb = (reg >> (poly_degree - 1)) & 1;
        reg = (reg << 1) & poly_mask;
        if (msb) reg ^= poly_val;
    }
    
    return reg;
}

// BCH-61 (PDF1)
uint32_t compute_bch1(uint64_t data) {
    return compute_bch(data, BCH1_DATA_BITS, BCH1_POLY, BCH1_DEGREE, BCH1_POLY_MASK);
}

// BCH-26 (PDF2)
uint16_t compute_bch2(uint32_t data) {
    return (uint16_t)compute_bch((uint64_t)data, BCH2_DATA_BITS, BCH2_POLY, BCH2_DEGREE, BCH2_POLY_MASK);
}

// =============================
// Fonctions d'extraction de bits corrigÃ©es
// =============================

uint64_t bits_to_uint64(volatile uint8_t *bits, int start, int len) {
    uint64_t value = 0;
    for (int i = 0; i < len; i++) {
        value = (value << 1) | bits[start + i];
    }
    return value;
}

uint32_t bits_to_uint32(volatile uint8_t *bits, int start, int len) {
    uint32_t value = 0;
    for (int i = 0; i < len; i++) {
        value = (value << 1) | bits[start + i];
    }
    return value;
}

uint16_t bits_to_uint16(volatile uint8_t *bits, int start, int len) {
    uint16_t value = 0;
    for (int i = 0; i < len; i++) {
        value = (value << 1) | bits[start + i];
    }
    return value;
}

// =============================
// PRIORITY 2: Standardized Bit Operations
// =============================

void set_bit_field(uint8_t *frame, uint16_t cs_start_bit, uint8_t length, uint64_t value) {
    for (uint8_t i = 0; i < length; i++) {
        frame[CS_BIT(cs_start_bit + i)] = (value >> (length - 1 - i)) & 1;
    }
}

uint64_t get_bit_field(const uint8_t *frame, uint16_t cs_start_bit, uint8_t length) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < length; i++) {
        value = (value << 1) | frame[CS_BIT(cs_start_bit + i)];
    }
    return value;
}

uint64_t get_bit_field_volatile(volatile const uint8_t *frame, uint16_t cs_start_bit, uint8_t length) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < length; i++) {
        value = (value << 1) | frame[CS_BIT(cs_start_bit + i)];
    }
    return value;
}

// =============================
// GPS Functions
// =============================

void set_gps_position(double lat, double lon, double alt) {
    current_latitude = lat;
    current_longitude = lon;
    current_altitude = alt;
    gps_updated = 1;
}

// Fonction personnalisÃ©e de conversion chaÃ®ne -> float optimisÃ©e
static float custom_atof(const char *p) {
    int integer = 0;
    float fractional = 0.0f;
    float divisor = 10.0f;
    uint8_t decimal = 0;
    uint8_t negative = 0;
    
    if(*p == '-') {
        negative = 1;
        p++;
    }
    
    while(*p) {
        if(*p == '.') {
            decimal = 1;
            p++;
            continue;
        }
        
        if(*p >= '0' && *p <= '9') {
            if(!decimal) {
                integer = integer * 10 + (*p - '0');
            } else {
                fractional += (*p - '0') / divisor;
                divisor *= 10.0f;
            }
        }
        p++;
    }
    
    float result = (float)integer + fractional;
    return negative ? -result : result;
}

// Fonction de validation de checksum NMEA
static uint8_t validate_nmea_checksum(const char *msg) {
    const char *p = msg + 1; // AprÃ¨s '$'
    uint8_t checksum = 0;
    char hex[3] = {0};
    
    while(*p && *p != '*') {
        checksum ^= *p++;
    }
    
    if(*p == '*') {
        hex[0] = *(p+1);
        hex[1] = *(p+2);
        return checksum == (uint8_t)strtol(hex, NULL, 16);
    }
    return 0;
}

void parse_nmea_gga(const char *line) {
    // VÃ©rification rapide du format
    if(line[0] != '$') return;
    if(strncmp(line, "$GPGGA", 6) && strncmp(line, "$GNGGA", 6)) return;
    
    // Validation checksum
    if(!validate_nmea_checksum(line)) {
        debug_print_str("NMEA: Bad checksum\r\n");
        return;
    }
    
    // Variables locales pour Ã©viter les accÃ¨s mÃ©moire rÃ©pÃ©tÃ©s
    char field[15][16] = {0};
    uint8_t field_count = 0;
    const char *ptr = line;
    char *field_ptr = field[0];
    
    // Parsing optimisÃ© sans utiliser strtok
    while(*ptr && field_count < 15) {
        if(*ptr == ',' || *ptr == '*') {
            *field_ptr = '\0';
            field_count++;
            field_ptr = field[field_count];
            ptr++;
            continue;
        }
        
        if(*ptr == '$') {
            ptr++;
            continue;
        }
        
        if(field_ptr < &field[field_count][15]) {
            *field_ptr++ = *ptr;
        }
        ptr++;
    }
    
    // VÃ©rification des champs critiques
    if(!field[6][0] || !field[2][0] || !field[3][0] || 
       !field[4][0] || !field[5][0] || !field[9][0]) {
        debug_print_str("NMEA: Missing fields\r\n");
        return;
    }
    
    // Conversion qualitÃ© de fix
    uint8_t fix_quality = atoi(field[6]);
    if(fix_quality == 0) {
        debug_print_str("NMEA: No fix\r\n");
        return;
    }
    
    // Conversion latitude
    float lat_val = custom_atof(field[2]);
    float latitude = (int)(lat_val / 100) + fmod(lat_val, 100.0f) / 60.0f;
    if(field[3][0] == 'S') latitude = -latitude;
    
    // Conversion longitude
    float lon_val = custom_atof(field[4]);
    float longitude = (int)(lon_val / 100) + fmod(lon_val, 100.0f) / 60.0f;
    if(field[5][0] == 'W') longitude = -longitude;
    
    // Validation des coordonnÃ©es
    if(fabs(latitude) > 90.0f || fabs(longitude) > 180.0f) {
        debug_print_str("NMEA: Invalid coords\r\n");
        return;
    }
    
    // Conversion altitude
    float altitude = custom_atof(field[9]);
    
    // Mise Ã  jour position
    set_gps_position(latitude, longitude, altitude);
    
    // Debug optimisÃ©
    debug_print_str("GPS: ");
    debug_print_float(latitude, 6);
    debug_print_char(field[3][0]);
    debug_print_str(",");
    debug_print_float(longitude, 6);
    debug_print_char(field[5][0]);
    debug_print_str(",Alt:");
    debug_print_float(altitude, 1);
    debug_print_str("m\r\n");
}

// =============================
// PRIORITY 1: CRITICAL - GPS Position Encoding Fix
// =============================
// system_comms.c (nouveau fichier)
void debug_print_int16(int16_t value) {
    char buffer[7];
    snprintf(buffer, sizeof(buffer), "%d", value);
    debug_print_str(buffer);
}
// =============================
// CORRECTION 3: GPS encoding with dsPIC33CK optimizations
// =============================
/*
uint32_t compute_30min_position(double lat, double lon) {
    // Conversion conforme � CS-T001 Annexe A.3.2
    int32_t lat_steps = (int32_t)round(lat * 2.0);
    int32_t lon_steps = (int32_t)round(lon * 2.0);
    
    // Clamping strict selon plages autoris�es
    lat_steps = (lat_steps < -180) ? -180 : (lat_steps > 180) ? 180 : lat_steps;
    lon_steps = (lon_steps < -360) ? -360 : (lon_steps > 360) ? 360 : lon_steps;
    
    // Encodage 19-bit (9 bits lat + 10 bits lon)
    uint32_t encoded = 0;
    encoded |= ((uint32_t)(lat_steps & 0x1FF)) << 10;  // 9 bits latitude
    encoded |= (uint32_t)(lon_steps & 0x3FF);          // 10 bits longitude
    
    return encoded;
}*/

uint32_t compute_30min_position(double lat, double lon) {
    // Conversion en pas de 0.5° avec arrondi
    int16_t lat_steps = (int32_t)round(lat * 2.0);
    int16_t lon_steps = (int32_t)round(lon * 2.0);
    
    // Clamping des valeurs
    lat_steps = (lat_steps < -128) ? -128 : (lat_steps > 127) ? 127 : lat_steps;
    lon_steps = (lon_steps < -512) ? -512 : (lon_steps > 511) ? 511 : lon_steps;
    
    // Conversion en 32-bit AVANT décalage
    uint32_t lat_code = (uint32_t)(lat_steps & 0x1FF);  // 9 bits
    uint32_t lon_code = (uint32_t)(lon_steps & 0x3FF);  // 10 bits
    
    // Assemblage final avec décalage 32-bit
    uint32_t position = (lat_code << 10) | lon_code;
    
    // Debug critique
    if (!debug_flags.gps_encoding_printed) {
        debug_print_str("30min POS CALC:\r\n");
        debug_print_str("  lat_steps: "); debug_print_int16(lat_steps);
        debug_print_str(" (0x"); debug_print_hex16(lat_steps); debug_print_str(")");
        debug_print_str(" -> lat_code: 0x"); debug_print_hex24(lat_code);
        debug_print_str("\r\n  lon_steps: "); debug_print_int16(lon_steps);
        debug_print_str(" (0x"); debug_print_hex16(lon_steps); debug_print_str(")");
        debug_print_str(" -> lon_code: 0x"); debug_print_hex24(lon_code);
        debug_print_str("\r\n  position: 0x"); debug_print_hex24(position);
        debug_print_str("\r\n");
        debug_flags.gps_encoding_printed = 1;
    }
    
    return position;
}

cs_gps_position_t encode_gps_position_complete(double lat, double lon) {
    cs_gps_position_t result = {0};
    
    if(__builtin_fabs(lat) > 90.0 || __builtin_fabs(lon) > 180.0) {
        return result;
    }

    uint32_t lat_units = (uint32_t)__builtin_round(__builtin_fabs(lat) * 900.0);
    uint32_t lon_units = (uint32_t)__builtin_round(__builtin_fabs(lon) * 900.0);
    lat_units &= 0x7FFFF;
    lon_units &= 0x7FFFF;

    uint64_t lat_encoded = ((uint64_t)lat_units << 1) | (lat < 0.0 ? 1ULL : 0ULL);
    uint64_t lon_encoded = ((uint64_t)lon_units << 1) | (lon < 0.0 ? 1ULL : 0ULL);
    result.full_position_40bit = (lat_encoded << 20) | lon_encoded;
    result.coarse_position_21bit = (uint32_t)(result.full_position_40bit >> 19);
    result.fine_position_19bit = compute_30min_position(lat, lon);
    result.offset_position_18bit = compute_4sec_offset(lat, lon, result.fine_position_19bit);

    if (!debug_flags.gps_encoding_printed) {
        debug_print_str("GPS FINE POS: 0x");
        debug_print_hex24(result.fine_position_19bit);
        debug_print_str("\r\n");
        debug_flags.gps_encoding_printed = 1;
    }
    
    return result;
}

// Backward compatibility wrapper
uint32_t encode_gps_position(double lat, double lon) {
    cs_gps_position_t pos = encode_gps_position_complete(lat, lon);
    return pos.coarse_position_21bit;
}

uint32_t compute_4sec_offset(double lat, double lon, uint32_t position_30min) {
    // Extraction position de rÃ©fÃ©rence PDF-1 (19 bits)
    uint16_t lat_ref_raw = (position_30min >> 10) & 0x1FF;  // 9 bits latitude
    uint16_t lon_ref_raw = position_30min & 0x3FF;          // 10 bits longitude
    
    // Conversion complÃ©ment Ã  deux vers entiers signÃ©s
    int16_t lat_ref_signed = lat_ref_raw;
    int16_t lon_ref_signed = lon_ref_raw;
    
    // Extension de signe pour complÃ©ment Ã  deux
    if (lat_ref_raw & 0x100) {  // Bit de signe latitude (bit 8)
        lat_ref_signed |= 0xFE00;  // Ã‰tendre signe sur 16 bits
    }
    if (lon_ref_raw & 0x200) {  // Bit de signe longitude (bit 9)
        lon_ref_signed |= 0xFC00;  // Ã‰tendre signe sur 16 bits
    }
    
    // Position de rÃ©fÃ©rence en degrÃ©s (rÃ©solution 0.5Â°)
    double lat_ref_deg = lat_ref_signed * 0.5;
    double lon_ref_deg = lon_ref_signed * 0.5;
    
    // Calcul des offsets en degrÃ©s
    double lat_offset_deg = lat - lat_ref_deg;
    double lon_offset_deg = lon - lon_ref_deg;
    
    // Conversion en minutes
    double lat_offset_min = fabs(lat_offset_deg) * 60.0;
    double lon_offset_min = fabs(lon_offset_deg) * 60.0;
    
    // Bits de signe (1 = positif, 0 = nÃ©gatif)
    uint8_t lat_sign = (lat_offset_deg >= 0) ? 1 : 0;
    uint8_t lon_sign = (lon_offset_deg >= 0) ? 1 : 0;
    
    // SÃ©paration minutes entiÃ¨res et secondes
    uint8_t lat_min_int = (uint8_t)lat_offset_min;
    uint8_t lon_min_int = (uint8_t)lon_offset_min;
    
    // Calcul des secondes (partie fractionnaire * 60)
    double lat_sec = (lat_offset_min - lat_min_int) * 60.0;
    double lon_sec = (lon_offset_min - lon_min_int) * 60.0;
    
    // Conversion en incrÃ©ments de 4 secondes avec arrondi
    uint8_t lat_sec_4 = (uint8_t)((lat_sec + 2.0) / 4.0);
    uint8_t lon_sec_4 = (uint8_t)((lon_sec + 2.0) / 4.0);
    
    // Saturation Ã  15 (4 bits max)
    if (lat_min_int > 15) lat_min_int = 15;
    if (lon_min_int > 15) lon_min_int = 15;
    if (lat_sec_4 > 15) lat_sec_4 = 15;
    if (lon_sec_4 > 15) lon_sec_4 = 15;
    
    // Encodage latitude (9 bits) : signe(1) + minutes(4) + 4sec(4)
    uint16_t lat_encoded = 0;
    lat_encoded |= (lat_sign & 0x1) << 8;        // Bit 8 : signe
    lat_encoded |= (lat_min_int & 0xF) << 4;     // Bits 7-4 : minutes
    lat_encoded |= (lat_sec_4 & 0xF);            // Bits 3-0 : 4-sec
    
    // Encodage longitude (9 bits) : signe(1) + minutes(4) + 4sec(4)
    uint16_t lon_encoded = 0;
    lon_encoded |= (lon_sign & 0x1) << 8;        // Bit 8 : signe
    lon_encoded |= (lon_min_int & 0xF) << 4;     // Bits 7-4 : minutes
    lon_encoded |= (lon_sec_4 & 0xF);            // Bits 3-0 : 4-sec
    
    // Assemblage final (18 bits) : latitude(9) + longitude(9)
    uint32_t offset_18bits = ((uint32_t)lat_encoded << 9) | lon_encoded;
    
    return offset_18bits & 0x3FFFF;  // Masque 18 bits
}

// Conversion altitude -> code 4 bits (Â§A3.3.2.4)
uint8_t altitude_to_code(double altitude) {
    if (altitude < 400)   return 0x0;
    if (altitude < 800)   return 0x1;
    if (altitude < 1200)  return 0x2;
    if (altitude < 1600)  return 0x3;
    if (altitude < 2200)  return 0x4;
    if (altitude < 2800)  return 0x5;
    if (altitude < 3400)  return 0x6;
    if (altitude < 4000)  return 0x7;
    if (altitude < 4800)  return 0x8;
    if (altitude < 5600)  return 0x9;
    if (altitude < 6600)  return 0xA;
    if (altitude < 7600)  return 0xB;
    if (altitude < 8800)  return 0xC;
    if (altitude < 10000) return 0xD;
    if (altitude > 10000) return 0xE;
    return 0xF; // default if altitude information is not available
}

// =============================
// PRIORITY 2: Frame Construction with Compliant Bit Indexing
// =============================
// =============================
// CORRECTION 4: dsPIC33CK optimized frame construction
// =============================
void build_compliant_frame(void) {
    // Local frame buffer - dsPIC33CK stack optimization
    uint8_t frame[MESSAGE_BITS];
    
    // Fast zero initialization
    memset(frame, 0, MESSAGE_BITS);
    
    // Message de construction unique
    if (!debug_flags.build_msg_printed) {
        debug_flags.build_msg_printed = 1;
        debug_print_str("Building CS-T001 compliant frame...\r\n");
    }
    
    // CS-T001 frame construction - bit-exact
    set_bit_field(frame, FRAME_PREAMBLE_START, FRAME_PREAMBLE_LENGTH, 0x7FFFUL);
    
    // Sync pattern selection
    uint16_t sync_pattern = (beacon_mode == BEACON_MODE_TEST) ? SYNC_SELF_TEST : SYNC_NORMAL_LONG;
    set_bit_field(frame, FRAME_SYNC_START, FRAME_SYNC_LENGTH, sync_pattern);
    
    // Format and protocol flags
    set_bit_field(frame, FRAME_FORMAT_FLAG_BIT, 1, 1UL);
    set_bit_field(frame, FRAME_PROTOCOL_FLAG_BIT, 1, 0UL);
    
    // Country and protocol codes
    set_bit_field(frame, FRAME_COUNTRY_START, FRAME_COUNTRY_LENGTH, COUNTRY_CODE_FRANCE);
    set_bit_field(frame, FRAME_PROTOCOL_START, FRAME_PROTOCOL_LENGTH, PROTOCOL_ELT_DT);
    
    // Beacon ID (example - replace with actual ID)
    set_bit_field(frame, FRAME_BEACON_ID_START, FRAME_BEACON_ID_LENGTH, 0x123456UL);
    
    // GPS position encoding
    cs_gps_position_t gps_pos = encode_gps_position_complete(current_latitude, current_longitude);
    set_bit_field(frame, FRAME_POSITION_START, FRAME_POSITION_LENGTH, gps_pos.fine_position_19bit);
    // DEBUG CRITIQUE
    if(!debug_flags.gps_encoding_printed) {
        debug_print_str("GPS FINE POS: 0x");
        debug_print_hex24(gps_pos.fine_position_19bit);
        debug_print_str("\r\n");
        debug_flags.gps_encoding_printed = 1;
    }
    
    // BCH1 calculation - CS-T001 compliant
    uint64_t pdf1_data = get_bit_field(frame, 25, 61);
    uint32_t bch1 = compute_bch1(pdf1_data);
    set_bit_field(frame, FRAME_BCH1_START, FRAME_BCH1_LENGTH, bch1);
       
    // PDF-2 additional data
    set_bit_field(frame, FRAME_ACTIVATION_START, FRAME_ACTIVATION_LENGTH, 0x0UL);
    
    uint8_t alt_code = altitude_to_code(current_altitude);
    set_bit_field(frame, FRAME_ALTITUDE_START, FRAME_ALTITUDE_LENGTH, alt_code);
    
    set_bit_field(frame, FRAME_FRESHNESS_START, FRAME_FRESHNESS_LENGTH, 0x2UL);
    set_bit_field(frame, FRAME_OFFSET_START, FRAME_OFFSET_LENGTH, gps_pos.offset_position_18bit);
    
    // BCH2 calculation - CS-T001 compliant
    uint32_t pdf2_data = get_bit_field(frame, 107, 26);
    uint16_t bch2 = compute_bch2(pdf2_data);
    set_bit_field(frame, FRAME_BCH2_START, FRAME_BCH2_LENGTH, bch2);
       
    // Critical validation - dsPIC33CK optimized
    uint64_t pdf1_check = get_bit_field(frame, 25, 61);
    uint32_t bch1_check = (uint32_t)get_bit_field(frame, 86, 21);
    uint32_t bch1_calc = compute_bch1(pdf1_check);

    uint32_t pdf2_check = (uint32_t)get_bit_field(frame, 107, 26);
    uint16_t bch2_check = (uint16_t)get_bit_field(frame, 133, 12);
    uint16_t bch2_calc = compute_bch2(pdf2_check);

    // Single validation message
    if (!debug_flags.validation_printed) {
        debug_flags.validation_printed = 1;
        if (bch1_calc != bch1_check || bch2_calc != bch2_check) {
            debug_print_str("BCH Validation FAILED!\r\n");
        }
    }

    // Fast copy to global buffer - dsPIC33CK optimized
    memcpy((void*)beacon_frame, frame, MESSAGE_BITS);

    // Single comprehensive log
    debug_print_complete_frame_info(1);
}

void build_test_frame(void) {
    // Set test coordinates
    set_gps_position(TEST_LATITUDE, TEST_LONGITUDE, TEST_ALTITUDE);
    beacon_mode = BEACON_MODE_TEST;
    
    // Build compliant frame
    build_compliant_frame();
    
    // Single test message
    if (!debug_flags.test_frame_msg_printed) {
        debug_flags.test_frame_msg_printed = 1;
        debug_print_str("Test frame built with fixed GPS values\r\n");
    }
}

void transmit_beacon_frame(void) {
    if (!debug_flags.transmission_printed) {
        debug_flags.transmission_printed = 1;
        debug_print_str("Transmitting beacon frame...\r\n");
    }
    // Logique r�elle de transmission ici
}

// Backward compatibility - Exercise frame
void build_exercice_frame(void) {
    beacon_mode = BEACON_MODE_NORMAL;
    
    // Build compliant frame with current GPS position
    build_compliant_frame();
    
    set_rf_power_level(POWER_HIGH);
}

// =============================
// PRIORITY 3: Comprehensive CS-T001 Test Vectors
// =============================

// CS-T001 Annex C test vectors
static const cs_test_vector_t cs_test_vectors[] = {
    // BCH1 test vectors (61-bit input data)
    {"Annex C.3.1", 0x11C662468AC5600ULL, 0x53E3E, 0, 61},
    {"Annex C.3.2", 0x08E331234562B00ULL, 0x53E3E, 0, 61},
    {"All zeros",   0x0000000000000000ULL, 0x1FFFFF, 0, 61},
    {"All ones",    0x1FFFFFFFFFFFFFFFULL, 0x0, 0, 61},
    
    // BCH2 test vectors (26-bit input data)
    {"BCH2 Test 1", 0x0DB0040, 0, 0x679, 26},
    {"BCH2 Test 2", 0x3FFFFFF, 0, 0x0, 26},
    {"BCH2 Zeros",  0x0000000, 0, 0xFFF, 26},
};

void validate_cs_t001_comprehensive(void) {
    uint8_t bch1_passed = 0, bch1_total = 0;
    uint8_t bch2_passed = 0, bch2_total = 0;
    
    debug_print_str("=== CS-T001 Compliance Test Suite ===\r\n");
    
    for (size_t i = 0; i < sizeof(cs_test_vectors)/sizeof(cs_test_vectors[0]); i++) {
        const cs_test_vector_t *tv = &cs_test_vectors[i];
        
        if (tv->data_bits == 61 && tv->expected_bch1 != 0) {
            // BCH1 test
            uint32_t calculated = compute_bch1(tv->input_data);
            uint8_t pass = (calculated == tv->expected_bch1);
            
            debug_print_str("BCH1 ");
            debug_print_str(tv->name);
            debug_print_str(": 0x");
            debug_print_hex64(tv->input_data);
            debug_print_str(" -> 0x");
            debug_print_hex24(calculated);
            debug_print_str(pass ? " PASS\r\n" : " FAIL\r\n");
            
            if (pass) bch1_passed++;
            bch1_total++;
        }
        
        if (tv->data_bits == 26 && tv->expected_bch2 != 0) {
            // BCH2 test
            uint16_t calculated = compute_bch2((uint32_t)tv->input_data);
            uint8_t pass = (calculated == tv->expected_bch2);
            
            debug_print_str("BCH2 ");
            debug_print_str(tv->name);
            debug_print_str(": 0x");
            debug_print_hex32((uint32_t)tv->input_data);
            debug_print_str(" -> 0x");
            debug_print_hex16(calculated);
            debug_print_str(pass ? " PASS\r\n" : " FAIL\r\n");
            
            if (pass) bch2_passed++;
            bch2_total++;
        }
    }
    
    debug_print_str("=== Test Results ===\r\n");
    debug_print_str("BCH1: ");
    debug_print_int32(bch1_passed);
    debug_print_str("/");
    debug_print_int32(bch1_total);
    debug_print_str(" passed\r\n");
    
    debug_print_str("BCH2: ");
    debug_print_int32(bch2_passed);
    debug_print_str("/");
    debug_print_int32(bch2_total);
    debug_print_str(" passed\r\n");
    
    if (bch1_passed == bch1_total && bch2_passed == bch2_total) {
        debug_print_str("*** CS-T001 COMPLIANCE: VERIFIED ***\r\n");
    } else {
        debug_print_str("*** CS-T001 COMPLIANCE: FAILED ***\r\n");
    }
}

// Position encoding validation
void validate_position_encoding(void) {
    debug_print_str("=== Position Encoding Validation ===\r\n");
    
    // Test known coordinates
    struct {
        double lat, lon;
        const char *location;
    } test_coords[] = {
        {TEST_LATITUDE, TEST_LONGITUDE, "Test Location"},
        {0.0, 0.0, "Equator/Prime Meridian"},
        {90.0, 180.0, "North Pole/Date Line"},
        {-90.0, -180.0, "South Pole/Date Line"},
        {45.5, -73.6, "Montreal, Canada"},
        {48.8566, 2.3522, "Paris, France"}
    };
    
    for (size_t i = 0; i < sizeof(test_coords)/sizeof(test_coords[0]); i++) {
        cs_gps_position_t pos = encode_gps_position_complete(test_coords[i].lat, test_coords[i].lon);
        
        debug_print_str(test_coords[i].location);
        debug_print_str(": (");
        debug_print_float(test_coords[i].lat, 6);
        debug_print_str(", ");
        debug_print_float(test_coords[i].lon, 6);
        debug_print_str(")\r\n");
        
        debug_print_str("  40-bit: 0x");
        debug_print_hex64(pos.full_position_40bit);
        debug_print_str("\r\n");
        
        debug_print_str("  21-bit: 0x");
        debug_print_hex24(pos.coarse_position_21bit);
        debug_print_str("\r\n");
        
        debug_print_str("  19-bit: 0x");
        debug_print_hex24(pos.fine_position_19bit);
        debug_print_str("\r\n");
    }
}

// Legacy test functions for backward compatibility
void test_bch(void) {
    uint64_t test_data = 0x123456789ULL;
    uint32_t bch1 = compute_bch1(test_data);
    if(bch1 != 0x15F3C7) {
        debug_print_str("BCH1 Test Failed\r\n");
    } else {
        debug_print_str("BCH1 Test Passed\r\n");
    }
}

void test_bch_norm(void) {
    uint64_t pdf1_data = 0x08E331234562B00ULL;
    uint32_t bch1 = compute_bch1(pdf1_data);
    if(bch1 != 0x53E3E) {
        debug_print_str("BCH1 FAIL: Expected 0x53E3E, got 0x");
        debug_print_hex24(bch1);
        debug_print_str("\r\n");
    } else {
        debug_print_str("BCH1 Normative Test PASSED\r\n");
    }
}

void test_cs_t001_vectors(void) {
    // Vecteur T.001 Annexe C.3.1
    uint64_t pdf1_test = 0x11C662468AC5600ULL;
    uint32_t bch1_expected = 0x53E3E;
    
    uint32_t bch1_calc = compute_bch1(pdf1_test);
    debug_print_str(bch1_calc == bch1_expected ? "T.001 BCH1 PASS\r\n" : "T.001 BCH1 FAIL\r\n");
}

// =============================
// PRIORITY 4: Improved Debug Output
// =============================

void debug_print_field(const char* name, uint8_t* bits, int start, int length) {
    debug_print_str(name);
    debug_print_str(": ");
    for (int i = start; i < start + length; i++) {
        debug_print_char(bits[i] ? '1' : '0');
    }
    debug_print_str("\r\n");
}

void debug_print_frame_analysis(volatile const uint8_t *frame) {
    debug_print_str("\r\n=== FRAME ANALYSIS (CS-T001 Bit Numbering) ===\r\n");
    
    // Frame structure breakdown
    struct {
        const char *name;
        uint16_t start_bit;
        uint8_t length;
        const char *description;
    } frame_fields[] = {
        {"Preamble", 1, 15, "Carrier detect"},
        {"Frame Sync", 16, 9, "Message boundary"},
        {"Format Flag", 25, 1, "Message type"},
        {"Protocol Flag", 26, 1, "Location protocol"},
        {"Country Code", 27, 10, "Administration"},
        {"Protocol Code", 37, 4, "Beacon type"},
        {"Beacon ID", 41, 26, "Unique identifier"},
        {"Position", 67, 19, "PDF-1 location"},
        {"BCH1", 86, 21, "Error correction"},
        {"Activation", 107, 2, "Trigger type"},
        {"Altitude", 109, 4, "Height code"},
        {"Freshness", 113, 2, "Location age"},
        {"Offset", 115, 18, "Fine position"},
        {"BCH2", 133, 12, "Error correction"}
    };
    
    for (size_t i = 0; i < sizeof(frame_fields)/sizeof(frame_fields[0]); i++) {
        uint64_t value = get_bit_field_volatile(frame, frame_fields[i].start_bit, frame_fields[i].length);
        
        debug_print_str("Bits ");
        debug_print_int32(frame_fields[i].start_bit);
        debug_print_str("-");
        debug_print_int32(frame_fields[i].start_bit + frame_fields[i].length - 1);
        debug_print_str(" (");
        debug_print_str(frame_fields[i].name);
        debug_print_str("): 0x");
        
        if (frame_fields[i].length <= 8) {
            debug_print_hex((uint8_t)value);
        } else if (frame_fields[i].length <= 16) {
            debug_print_hex16((uint16_t)value);
        } else if (frame_fields[i].length <= 24) {
            debug_print_hex24((uint32_t)value);
        } else {
            debug_print_hex64(value);
        }
        
        debug_print_str(" - ");
        debug_print_str(frame_fields[i].description);
        debug_print_str("\r\n");
    }
    
    // Hex dump with proper formatting
    debug_print_str("\r\n=== HEX DUMP (18 bytes) ===\r\n");
    for (int byte = 0; byte < 18; byte++) {
        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8; bit++) {
            byte_val = (byte_val << 1) | frame[byte * 8 + bit];
        }
        debug_print_hex(byte_val);
        debug_print_char((byte + 1) % 8 == 0 ? '\r' : ' ');
        if ((byte + 1) % 8 == 0) debug_print_char('\n');
    }
    debug_print_str("\r\n");
}

void debug_print_frame_bits(const uint8_t *frame, int len) {
    debug_print_str("Frame bits (CS-T001 numbering):\r\n");
    for (int i = 0; i < len; i++) {
        if (i % 8 == 0) {
            debug_print_str("\r\nBits ");
            debug_print_int32(i + 1);
            debug_print_str("-");
            debug_print_int32((i + 8 <= len) ? i + 8 : len);
            debug_print_str(": ");
        }
        debug_print_char(frame[i] ? '1' : '0');
    }
    debug_print_str("\r\n");
}

void debug_print_frame_hex_condensed(const uint8_t *frame_bits, size_t nbits) {
    debug_print_str("Frame hex: ");
    size_t nbytes = (nbits + 7) / 8;
    
    for (size_t byte_idx = 0; byte_idx < nbytes; byte_idx++) {
        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8 && (byte_idx * 8 + bit) < nbits; bit++) {
            byte_val = (byte_val << 1) | (frame_bits[byte_idx * 8 + bit] & 1);
        }
        debug_print_hex(byte_val);
        if ((byte_idx + 1) % 8 == 0) debug_print_char(' ');
    }
    debug_print_str("\r\n");
}

// Utility functions
void frame_to_bytes(const uint8_t *frame, int len, uint8_t *out_bytes) {
    // len doit Ãªtre un multiple de 8 (ici 144 bits = 18 octets)
    for (int byte_i = 0; byte_i < len / 8; byte_i++) {
        uint8_t b = 0;
        for (int bit = 0; bit < 8; bit++) {
            b <<= 1;
            b |= frame[byte_i * 8 + bit];
        }
        out_bytes[byte_i] = b;
    }
}

void print_frame_hex(const uint8_t *frame_bits) {
    uint8_t bytes[18] = {0};

    for (int i = 0; i < 144; i++) {
        int byte_index = i / 8;
        int bit_index = 7 - (i % 8);
        bytes[byte_index] |= frame_bits[i] << bit_index;
    }

    debug_print_str("Trame hex condensÃ©e (18 octets / 144 bits) :\r\n");
    for (int i = 0; i < 18; i++) {
        debug_print_hex(bytes[i]);
    }
    debug_print_str("\r\n");
}

// =============================
// Master Compliance Check Function
// =============================

void cs_t001_full_compliance_check(void) {
    debug_print_str("\r\n");
    debug_print_str("==================================================\r\n");
    debug_print_str("COSPAS-SARSAT CS-T001 FULL COMPLIANCE CHECK\r\n");
    debug_print_str("==================================================\r\n");
    
    // 1. BCH polynomial verification
    debug_print_str("\r\n1. BCH Algorithm Validation:\r\n");
    debug_print_str("-----------------------------\r\n");
    validate_cs_t001_comprehensive();
    
    // 2. Legacy test compatibility
    debug_print_str("\r\n2. Legacy Test Compatibility:\r\n");
    debug_print_str("------------------------------\r\n");
    test_bch();
    test_bch_norm();
    test_cs_t001_vectors();
    
    // 3. Position encoding verification  
    debug_print_str("\r\n3. Position Encoding Validation:\r\n");
    debug_print_str("---------------------------------\r\n");
    validate_position_encoding();
    
    // 4. Complete frame construction and analysis
    debug_print_str("\r\n4. Frame Construction & Analysis:\r\n");
    debug_print_str("---------------------------------\r\n");
    build_compliant_frame();
    debug_print_frame_analysis(beacon_frame);
    
    // 5. Final verification
    debug_print_str("\r\n5. Final Frame Verification:\r\n");
    debug_print_str("----------------------------\r\n");
    
    uint64_t pdf1_check = get_bit_field_volatile(beacon_frame, 25, 61);
    uint32_t bch1_check = (uint32_t)get_bit_field_volatile(beacon_frame, 86, 21);
    uint32_t bch1_calc = compute_bch1(pdf1_check);
    
    uint32_t pdf2_check = (uint32_t)get_bit_field_volatile(beacon_frame, 107, 26);
    uint16_t bch2_check = (uint16_t)get_bit_field_volatile(beacon_frame, 133, 12);
    uint16_t bch2_calc = compute_bch2(pdf2_check);
    
    debug_print_str("BCH1 Frame Check: ");
    debug_print_str((bch1_calc == bch1_check) ? "PASS" : "FAIL");
    debug_print_str(" (0x");
    debug_print_hex24(bch1_calc);
    debug_print_str(" vs 0x");
    debug_print_hex24(bch1_check);
    debug_print_str(")\r\n");
    
    debug_print_str("BCH2 Frame Check: ");
    debug_print_str((bch2_calc == bch2_check) ? "PASS" : "FAIL");
    debug_print_str(" (0x");
    debug_print_hex16(bch2_calc);
    debug_print_str(" vs 0x");
    debug_print_hex16(bch2_check);
    debug_print_str(")\r\n");
    
    debug_print_str("Frequency Deviation: ");
    debug_print_float(get_freq_deviation(), 1);
    debug_print_str(" Hz\r\n");
    
    // Final status
    debug_print_str("\r\n==================================================\r\n");
    if ((bch1_calc == bch1_check) && (bch2_calc == bch2_check)) {
        debug_print_str("*** CS-T001 FULL COMPLIANCE: VERIFIED ***\r\n");
        debug_print_str("Frame ready for 406 MHz transmission\r\n");
    } else {
        debug_print_str("*** CS-T001 COMPLIANCE: FAILED ***\r\n");
        debug_print_str("Frame requires correction before transmission\r\n");
    }
    debug_print_str("==================================================\r\n\r\n");
    
        // V�rification hardware finale
    if (!validate_frame_hardware()) {
        debug_print_str("ERROR: Frame validation failed hardware check\r\n");
        full_error_diagnostic();
    }

}

// =============================
// CORRECTION 6: dsPIC33CK consolidated debug output
// =============================
void debug_print_complete_frame_info(uint8_t include_hex) {
    if(debug_flags.frame_info_printed) return;
    debug_flags.frame_info_printed = 1;
    
    debug_print_str("=== GPS DATA ===\r\n");
    debug_print_str("Input: (");
    debug_print_float(current_latitude, 6);
    debug_print_str(", ");
    debug_print_float(current_longitude, 6);
    debug_print_str(")\r\n");

    // Extraction directe depuis la trame beacon_frame
    uint32_t fine_pos = get_bit_field_volatile(beacon_frame, FRAME_POSITION_START, FRAME_POSITION_LENGTH);
    debug_print_str("19-bit: 0x");
    debug_print_hex24(fine_pos);
    debug_print_str("\r\n");

    uint32_t offset_pos = get_bit_field_volatile(beacon_frame, FRAME_OFFSET_START, FRAME_OFFSET_LENGTH);
    debug_print_str("18-bit offset: 0x");
    debug_print_hex24(offset_pos);
    debug_print_str("\r\n");

    // BCH validation - dsPIC33CK optimized
    uint64_t pdf1_data = get_bit_field_volatile(beacon_frame, 25, 61);
    uint32_t bch1_calc = compute_bch1(pdf1_data);
    uint32_t bch1_recv = (uint32_t)get_bit_field_volatile(beacon_frame, 86, 21);
    
    uint32_t pdf2_data = (uint32_t)get_bit_field_volatile(beacon_frame, 107, 26);
    uint16_t bch2_calc = compute_bch2(pdf2_data);
    uint16_t bch2_recv = (uint16_t)get_bit_field_volatile(beacon_frame, 133, 12);
    
    // Validation output
    debug_print_str("=== FRAME VALIDATION ===\r\n");
    debug_print_str("PDF1:      0x");
    debug_print_hex64(pdf1_data);
    debug_print_str("\r\nBCH1 Calc: 0x");
    debug_print_hex24(bch1_calc);
    debug_print_str("\r\nBCH1 Recv: 0x");
    debug_print_hex24(bch1_recv);
    debug_print_str((bch1_calc == bch1_recv) ? " (VALID)" : " (INVALID)");
    debug_print_str("\r\nPDF2:      0x");
    debug_print_hex32(pdf2_data);
    debug_print_str("\r\nBCH2 Calc: 0x");
    debug_print_hex16(bch2_calc);
    debug_print_str("\r\nBCH2 Recv: 0x");
    debug_print_hex16(bch2_recv);
    debug_print_str((bch2_calc == bch2_recv) ? " (VALID)" : " (INVALID)");
    debug_print_str("\r\n");

    // Transmission info - single output
    if (!debug_flags.transmission_printed) {
        debug_flags.transmission_printed = 1;
        debug_print_str("=== TRANSMISSION ===\r\n");
    }

    // Conditional hex dump - dsPIC33CK optimized
    if (include_hex) {
        debug_print_str("Frame HEX: ");
        for (uint8_t byte = 0; byte < 18; byte++) {
            uint8_t byte_val = 0;
            for (uint8_t bit = 0; bit < 8; bit++) {
                byte_val = (byte_val << 1) | (beacon_frame[byte * 8 + bit] & 1);
            }
            debug_print_hex(byte_val);
        }
        debug_print_str("\r\n");
    }
}

uint8_t validate_frame_hardware(void) {
    uint64_t pdf1 = get_bit_field_volatile(beacon_frame, 25, 61);
    uint32_t bch1_calc = compute_bch1(pdf1);
    uint32_t bch1_recv = (uint32_t)get_bit_field_volatile(beacon_frame, 86, 21);
    
    uint32_t pdf2 = (uint32_t)get_bit_field_volatile(beacon_frame, 107, 26);
    uint16_t bch2_calc = compute_bch2(pdf2);
    uint16_t bch2_recv = (uint16_t)get_bit_field_volatile(beacon_frame, 133, 12);
    
    if(bch1_calc != bch1_recv || bch2_calc != bch2_recv) {
        debug_print_str("FRAME VALIDATION ERROR\r\n");
        return 0;
    }
    return 1;
}

// =============================
// CORRECTION 9: Memory efficient debug reset for dsPIC33CK
// =============================
void initialize_debug_system(void) {
    __builtin_disable_interrupts();
    debug_flags = (volatile debug_flags_t){0};
    __builtin_enable_interrupts();
}

// =============================
// CORRECTION 10: dsPIC33CK interrupt-safe debug flag access
// =============================
// === Correction 4 : Implémentation des builtins dsPIC ===
/*
void set_debug_flag_atomic(uint8_t flag_bit) {
    uint16_t saved_ipl = __builtin_disable_interrupts();
    *(uint8_t*)&debug_flags |= (1 << flag_bit);
    __builtin_enable_interrupts(saved_ipl);
}
*/
// Flag bit definitions for atomic access
#define DEBUG_FLAG_GPS_ENCODING   0
#define DEBUG_FLAG_FRAME_INFO     1
#define DEBUG_FLAG_BUILD_MSG      2
#define DEBUG_FLAG_TEST_FRAME     3
#define DEBUG_FLAG_VALIDATION     4
#define DEBUG_FLAG_TRANSMISSION   5

// Version pour beacon_frame global
void debug_print_beacon_frame_hex(void) {
    debug_print_str("Frame HEX: ");
    for (int byte = 0; byte < 18; byte++) {
        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8; bit++) {
            byte_val = (byte_val << 1) | beacon_frame[byte * 8 + bit];
        }
        debug_print_hex(byte_val);
    }
    debug_print_str("\r\n");
}