#ifndef GPS_NMEA_H
#define GPS_NMEA_H

#include <stdint.h>
#include "system_definitions.h"

// =============================
// GPS NMEA Configuration
// =============================
// Hardware: UART3 on RC4 (U3RX/RP52) and RC5 (U3TX/RP53)
// Baud rate: 9600 baud (standard NMEA)
#define GPS_NMEA_MAX_LENGTH     82      // Maximum NMEA sentence length
#define GPS_BUFFER_SIZE         96      // RX buffer size
#define GPS_FIX_TIMEOUT_MS      2000    // GPS update timeout

// GPS Fix Quality
#define GPS_FIX_INVALID         0
#define GPS_FIX_GPS             1
#define GPS_FIX_DGPS            2

// =============================
// GPS Data Structure
// =============================
typedef struct {
    // Position
    double latitude;            // Decimal degrees (-90 to +90)
    double longitude;           // Decimal degrees (-180 to +180)
    double altitude;            // Meters above sea level

    // Fix quality
    uint8_t fix_quality;        // 0=invalid, 1=GPS, 2=DGPS
    uint8_t satellites;         // Number of satellites in use

    // Status
    uint8_t position_valid;     // 1 if position is valid
    uint32_t last_update_ms;    // Timestamp of last GPS update

    // HDOP
    uint16_t hdop_x10;          // HDOP * 10 (ex: 12 = 1.2)
} gps_data_t;

// =============================
// Function Prototypes
// =============================

/**
 * Initialize GPS UART3 at 9600 baud
 */
void gps_init(void);

/**
 * Process GPS data (call periodically from main loop)
 * Returns 1 if new valid data received
 */
uint8_t gps_update(void);

/**
 * Get current GPS data
 */
const gps_data_t* gps_get_data(void);

/**
 * Check if GPS has valid fix
 */
uint8_t gps_has_fix(void);

/**
 * Print GPS status to debug UART
 */
void gps_print_status(void);

/**
 * Parse NMEA GGA sentence (enhanced version)
 */
void gps_parse_gga(const char *sentence);

/**
 * Parse NMEA RMC sentence
 */
void gps_parse_rmc(const char *sentence);

/**
 * Validate NMEA checksum
 */
uint8_t gps_validate_checksum(const char *sentence);

// =============================
// Global Variables
// =============================
extern gps_data_t gps_data;
extern volatile uint8_t gps_rx_buffer[GPS_BUFFER_SIZE];
extern volatile uint8_t gps_rx_head;
extern volatile uint8_t gps_rx_tail;
extern volatile uint8_t gps_debug_raw;  // 0=off, 1=print raw NMEA sentences

#endif // GPS_NMEA_H
