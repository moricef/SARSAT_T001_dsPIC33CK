#include "gps_nmea.h"
#include "system_debug.h"
#include "protocol_data.h"
#include <xc.h>
#include <string.h>
#include <stdlib.h>

// Build timestamp for GPS module
const char gps_build_time[] = __TIME__;
const char gps_build_date[] = __DATE__;

// =============================
// Global Variables
// =============================
gps_data_t gps_data = {0};
volatile uint8_t gps_rx_buffer[GPS_BUFFER_SIZE];
volatile uint8_t gps_rx_head = 0;
volatile uint8_t gps_rx_tail = 0;

static char nmea_sentence[GPS_NMEA_MAX_LENGTH];
static uint8_t nmea_index = 0;

// GPS debug mode
volatile uint8_t gps_debug_raw = 1;  // 0=off, 1=print raw NMEA sentences (AUTO ON)
volatile uint16_t gps_rx_count = 0;  // Count of chars received from GPS

// =============================
// UART3 Initialization (GPS)
// =============================
void gps_init(void) {
    // Disable UART3
    U3MODEbits.UARTEN = 0;
    U3MODEbits.URXEN = 0;

    // Reset registers
    U3MODE = 0;
    U3STAH = 0;

    // Configure GPIO pins for UART3 GPS
    // RC5 (RP53) = U3RX (input from GPS TX)
    // RC4 (RP52) = U3TX (output to GPS RX)
    TRISCbits.TRISC5 = 1;       // RC5 as input (U3RX)
    TRISCbits.TRISC4 = 0;       // RC4 as output (U3TX)
    // Note: RC4 and RC5 are digital-only pins, no ANSEL configuration needed

    // Configure PPS (Peripheral Pin Select) for UART3
    __builtin_write_OSCCONL(OSCCONL | 0x40);    // Unlock PPS
    _U3RXR = 53;                                 // Map U3RX to RP53 (RC5)
    _RP52R = 0x0003;                            // Map RP52 (RC4) to U3TX (function 1)
    __builtin_write_OSCCONL(OSCCONL & ~0x40);   // Lock PPS

    // Configure UART3: 9600 baud, 8N1
    // Baud rate calculation: BRG = (Fcy / (16 * BaudRate)) - 1
    // For Fcy = 100 MHz, BaudRate = 9600: BRG = 651
    U3BRG = 651;  // Adjust based on your Fcy

    // UART Mode: 8-bit data, no parity, 1 stop bit
    U3MODEbits.MOD = 0;     // Asynchronous 8-bit UART
    U3MODEbits.BRGH = 0;    // Standard speed mode

    // Configure RX interrupt mode
    U3STAHbits.URXISEL = 0; // Interrupt when any character received

    // Enable UART3 - CRITICAL: UARTEN first, then UTXEN/URXEN (like U1)
    U3MODEbits.UARTEN = 1;
    U3MODEbits.UTXEN = 1;
    U3MODEbits.URXEN = 1;

    // Configure interruptions AFTER enabling UART (like U1)
    IFS3bits.U3RXIF = 0;
    IEC3bits.U3RXIE = 1;

    // Double activation like U1
    U3MODEbits.UARTEN = 1;
    __builtin_nop();
    __builtin_nop();

    // Initialize GPS data structure
    gps_data.position_valid = 0;
    gps_data.fix_quality = GPS_FIX_INVALID;
    gps_data.satellites = 0;
    gps_data.last_update_ms = 0;

    DEBUG_LOG_FLUSH("GPS: UART3 initialized at 9600 baud [Build: ");
    DEBUG_LOG_FLUSH(gps_build_time);
    DEBUG_LOG_FLUSH(" ");
    DEBUG_LOG_FLUSH(gps_build_date);
    DEBUG_LOG_FLUSH("]\r\n");
    DEBUG_LOG_FLUSH("GPS: U3MODE=");
    debug_print_hex((U3MODE >> 12) & 0xF);
    debug_print_hex((U3MODE >> 8) & 0xF);
    debug_print_hex((U3MODE >> 4) & 0xF);
    debug_print_hex(U3MODE & 0xF);
    DEBUG_LOG_FLUSH(" U3MODEH=");
    debug_print_hex((U3MODEH >> 12) & 0xF);
    debug_print_hex((U3MODEH >> 8) & 0xF);
    debug_print_hex((U3MODEH >> 4) & 0xF);
    debug_print_hex(U3MODEH & 0xF);
    DEBUG_LOG_FLUSH(" IEC3=");
    debug_print_hex((IEC3 >> 12) & 0xF);
    debug_print_hex((IEC3 >> 8) & 0xF);
    debug_print_hex((IEC3 >> 4) & 0xF);
    debug_print_hex(IEC3 & 0xF);
    DEBUG_LOG_FLUSH("\r\n");
}

// =============================
// UART3 RX Interrupt Handler
// =============================
void __attribute__((interrupt, auto_psv)) _U3RXInterrupt(void) {
    // Clear interrupt flag
    IFS3bits.U3RXIF = 0;

    // Read all available data
    while (U3STAHbits.URXBE == 0) {  // While RX buffer not empty
        uint8_t data = U3RXREG & 0xFF;

        gps_rx_count++;  // Count received chars

        // Store in circular buffer
        uint8_t next_head = (gps_rx_head + 1) % GPS_BUFFER_SIZE;
        if (next_head != gps_rx_tail) {
            gps_rx_buffer[gps_rx_head] = data;
            gps_rx_head = next_head;
        }
        // else: buffer overflow, drop data
    }
}

// =============================
// NMEA Checksum Validation
// =============================
uint8_t gps_validate_checksum(const char *sentence) {
    if (!sentence || sentence[0] != '$') {
        return 0;
    }

    // Find checksum delimiter
    const char *star = strchr(sentence, '*');
    if (!star || strlen(star) < 3) {
        return 0;
    }

    // Calculate checksum (XOR between $ and *)
    uint8_t calc_checksum = 0;
    for (const char *p = sentence + 1; p < star; p++) {
        calc_checksum ^= (uint8_t)*p;
    }

    // Parse received checksum (2 hex digits)
    char hex[3] = {star[1], star[2], '\0'};
    uint8_t recv_checksum = (uint8_t)strtol(hex, NULL, 16);

    return (calc_checksum == recv_checksum);
}

// =============================
// Helper: Parse coordinate
// =============================
static double parse_coordinate(const char *coord, const char *dir) {
    if (!coord || !dir || strlen(coord) == 0) {
        return 0.0;
    }

    // Format: DDMM.MMMM or DDDMM.MMMM
    double raw = atof(coord);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    double decimal = degrees + (minutes / 60.0);

    // Apply direction
    if (dir[0] == 'S' || dir[0] == 'W') {
        decimal = -decimal;
    }

    return decimal;
}

// =============================
// Parse NMEA GGA Sentence
// =============================
void gps_parse_gga(const char *sentence) {
    if (!sentence || !gps_validate_checksum(sentence)) {
        return;
    }

    // Tokenize sentence
    char buffer[GPS_NMEA_MAX_LENGTH];
    strncpy(buffer, sentence, GPS_NMEA_MAX_LENGTH - 1);
    buffer[GPS_NMEA_MAX_LENGTH - 1] = '\0';

    char *fields[15];
    uint8_t field_count = 0;

    char *token = strtok(buffer, ",");
    while (token && field_count < 15) {
        fields[field_count++] = token;
        token = strtok(NULL, ",");
    }

    if (field_count < 10) {
        return;  // Incomplete sentence
    }

    // Parse fields
    // $GPGGA,time,lat,N/S,lon,E/W,quality,numSV,HDOP,alt,M,...
    uint8_t quality = atoi(fields[6]);

    if (quality > 0) {
        double latitude = parse_coordinate(fields[2], fields[3]);
        double longitude = parse_coordinate(fields[4], fields[5]);
        double altitude = atof(fields[9]);
        uint8_t satellites = atoi(fields[7]);
        uint16_t hdop_x10 = (uint16_t)(atof(fields[8]) * 10.0);

        // Update GPS data structure
        __builtin_disable_interrupts();
        gps_data.latitude = latitude;
        gps_data.longitude = longitude;
        gps_data.altitude = altitude;
        gps_data.fix_quality = quality;
        gps_data.satellites = satellites;
        gps_data.hdop_x10 = hdop_x10;
        gps_data.position_valid = 1;
        gps_data.last_update_ms = millis_counter;

        // Update global GPS variables for protocol_data.c
        set_gps_position(latitude, longitude, altitude);
        __builtin_enable_interrupts();
    }
}

// =============================
// Parse NMEA RMC Sentence
// =============================
void gps_parse_rmc(const char *sentence) {
    if (!sentence || !gps_validate_checksum(sentence)) {
        return;
    }

    // Tokenize sentence
    char buffer[GPS_NMEA_MAX_LENGTH];
    strncpy(buffer, sentence, GPS_NMEA_MAX_LENGTH - 1);
    buffer[GPS_NMEA_MAX_LENGTH - 1] = '\0';

    char *fields[13];
    uint8_t field_count = 0;

    char *token = strtok(buffer, ",");
    while (token && field_count < 13) {
        fields[field_count++] = token;
        token = strtok(NULL, ",");
    }

    if (field_count < 8) {
        return;  // Incomplete sentence
    }

    // Parse fields
    // $GPRMC,time,status,lat,N/S,lon,E/W,speed,course,date,...
    char status = fields[2][0];

    if (status == 'A') {  // Active (valid position)
        double latitude = parse_coordinate(fields[3], fields[4]);
        double longitude = parse_coordinate(fields[5], fields[6]);

        // Update GPS data structure
        __builtin_disable_interrupts();
        gps_data.latitude = latitude;
        gps_data.longitude = longitude;
        gps_data.position_valid = 1;
        gps_data.last_update_ms = millis_counter;

        // Update global GPS variables
        set_gps_position(latitude, longitude, gps_data.altitude);
        __builtin_enable_interrupts();
    }
}

// =============================
// Process GPS Data (Main Loop)
// =============================
uint8_t gps_update(void) {
    uint8_t new_data = 0;

    // Process all available data in RX buffer
    while (gps_rx_tail != gps_rx_head) {
        char c = gps_rx_buffer[gps_rx_tail];
        gps_rx_tail = (gps_rx_tail + 1) % GPS_BUFFER_SIZE;

        // Build NMEA sentence
        if (c == '$') {
            nmea_index = 0;
            nmea_sentence[nmea_index++] = c;
        } else if (nmea_index > 0 && nmea_index < GPS_NMEA_MAX_LENGTH - 1) {
            nmea_sentence[nmea_index++] = c;

            // End of sentence
            if (c == '\n') {
                nmea_sentence[nmea_index] = '\0';

                // Debug: print raw NMEA sentence
                if (gps_debug_raw) {
                    DEBUG_LOG_FLUSH("NMEA: ");
                    DEBUG_LOG_FLUSH(nmea_sentence);
                    DEBUG_LOG_FLUSH("\r\n");
                }

                // Parse sentence
                if (strstr(nmea_sentence, "$GPGGA") || strstr(nmea_sentence, "$GNGGA")) {
                    gps_parse_gga(nmea_sentence);
                    new_data = 1;
                } else if (strstr(nmea_sentence, "$GPRMC") || strstr(nmea_sentence, "$GNRMC")) {
                    gps_parse_rmc(nmea_sentence);
                    new_data = 1;
                }

                nmea_index = 0;
            }
        } else {
            nmea_index = 0;  // Reset on overflow
        }
    }

    return new_data;
}

// =============================
// GPS Status Functions
// =============================
const gps_data_t* gps_get_data(void) {
    return &gps_data;
}

uint8_t gps_has_fix(void) {
    return (gps_data.position_valid && gps_data.fix_quality > 0);
}

void gps_print_status(void) {
    DEBUG_LOG_FLUSH("\r\n=== GPS Status ===\r\n");

    DEBUG_LOG_FLUSH("Fix: ");
    if (gps_data.position_valid) {
        DEBUG_LOG_FLUSH("VALID (quality: ");
        debug_print_uint16(gps_data.fix_quality);
        DEBUG_LOG_FLUSH(")\r\n");
    } else {
        DEBUG_LOG_FLUSH("INVALID\r\n");
    }

    DEBUG_LOG_FLUSH("Satellites: ");
    debug_print_uint16(gps_data.satellites);
    DEBUG_LOG_FLUSH("\r\n");

    DEBUG_LOG_FLUSH("Position: ");
    debug_print_float(gps_data.latitude, 6);
    DEBUG_LOG_FLUSH(", ");
    debug_print_float(gps_data.longitude, 6);
    DEBUG_LOG_FLUSH("\r\n");

    DEBUG_LOG_FLUSH("Altitude: ");
    debug_print_float(gps_data.altitude, 1);
    DEBUG_LOG_FLUSH(" m\r\n");

    DEBUG_LOG_FLUSH("HDOP: ");
    debug_print_uint16(gps_data.hdop_x10 / 10);
    DEBUG_LOG_FLUSH(".");
    debug_print_uint16(gps_data.hdop_x10 % 10);
    DEBUG_LOG_FLUSH("\r\n");

    uint32_t current_time;
    __builtin_disable_interrupts();
    current_time = millis_counter;
    __builtin_enable_interrupts();

    uint32_t age_ms = current_time - gps_data.last_update_ms;
    DEBUG_LOG_FLUSH("Last update: ");
    debug_print_uint32(age_ms);
    DEBUG_LOG_FLUSH(" ms ago\r\n");

    DEBUG_LOG_FLUSH("==================\r\n\r\n");
}
