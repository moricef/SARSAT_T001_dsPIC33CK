# GPS NMEA Integration for dsPIC33CK T.001 Beacon

✅ **STATUS: VALIDATED WITH REAL GPS HARDWARE** ✅
This implementation has been fully tested and validated with Ublox NEO-6M GPS receiver. Real-time GPS coordinate transmission is operational.

## 📡 Overview

This document describes the GPS NMEA integration for the dsPIC33CK-based COSPAS-SARSAT T.001 beacon transmitter. The system uses UART3 to receive real-time GPS data from an NMEA-compatible GPS module and automatically updates the beacon frame with current position data.

## 🔧 Hardware Configuration

### UART Assignments
- **UART1**: RB3/RB4 - Communication UART (configured but currently unused)
- **UART2**: RC10/RC11 - Debug output
- **UART3**: RC4/RC5 - GPS NMEA receiver (9600 baud, 8N1) ✅

### GPS Module Connection
Connect an NMEA-compatible GPS module to UART3:
- **GPS TX** → **RC5 (RP53)** - dsPIC33CK U3RX ✅ **VALIDATED**
- **GPS RX** → **RC4 (RP52)** - dsPIC33CK U3TX (optional, not used for receive-only)
- **GPS VCC** → **3.3V or 5V** (depending on module)
- **GPS GND** → **GND**

**Note**: Pin mapping was corrected from initial implementation (RC4/RC5 were swapped).

### Pin Configuration (Already Configured in gps_nmea.c)
The GPS module automatically configures the PPS registers:

```c
// GPIO Configuration (CORRECTED)
TRISCbits.TRISC5 = 1;       // RC5 as input (U3RX)
TRISCbits.TRISC4 = 0;       // RC4 as output (U3TX)
// Note: RC4 and RC5 are digital-only pins, no ANSEL configuration needed

// PPS Configuration (configured centrally in init_all_pps())
// U3RX mapped to RP53 (RC5)
// U3TX mapped to RP52 (RC4)
```

**No additional configuration needed** - everything is handled by `gps_init()`.

## 📂 Files Added

### New Files
- **gps_nmea.h**: GPS module header with data structures and function prototypes
- **gps_nmea.c**: GPS implementation with UART3 driver and NMEA parser
- **GPS_INTEGRATION.md**: This documentation file

### Modified Files
- **system_comms.c**: Added `gps_init()` call in `system_init()`
- **main.c**: Added `gps_update()` in main loop and GPS status display
- **system_debug.c**: Added "GPS" command for status display

## 🚀 Features

### NMEA Sentence Support
- ✅ **$GPGGA**: Position, altitude, fix quality, satellites, HDOP
- ✅ **$GPRMC**: Position, velocity, course
- ✅ **$GNGGA**: Multi-constellation (GPS+GLONASS) position
- ✅ **$GNRMC**: Multi-constellation RMC
- ✅ **Checksum validation**: All sentences are validated before parsing

### Real-Time Operation
- **Interrupt-driven UART3 reception**: Circular buffer (96 bytes)
- **Non-blocking parser**: Called from main loop via `gps_update()`
- **Automatic frame update**: GPS position automatically updates `current_latitude`, `current_longitude`, `current_altitude`
- **Fix validation**: Only valid GPS fixes update the position

### GPS Data Structure
```c
typedef struct {
    double latitude;            // Decimal degrees (-90 to +90)
    double longitude;           // Decimal degrees (-180 to +180)
    double altitude;            // Meters above sea level
    uint8_t fix_quality;        // 0=invalid, 1=GPS, 2=DGPS
    uint8_t satellites;         // Number of satellites in use
    uint8_t position_valid;     // 1 if position is valid
    uint32_t last_update_ms;    // Timestamp of last GPS update
    uint16_t hdop_x10;          // HDOP * 10 (e.g., 12 = 1.2)
} gps_data_t;
```

## 💻 Usage

### Initialization
GPS is automatically initialized at system startup:
```c
void system_init(void) {
    // ...
    gps_init();  // Initialize UART3 at 9600 baud
    // ...
}
```

### Main Loop Integration
The main loop automatically processes GPS data:
```c
while(1) {
    // Process GPS data (non-blocking)
    if (gps_update()) {
        // New GPS data received - frame will be rebuilt at next transmission
    }

    // Periodic beacon transmission
    if (should_transmit_beacon()) {
        if (gps_has_fix()) {
            DEBUG_LOG_FLUSH("GPS Fix: ... satellites, Pos: lat, lon\r\n");
        }
        start_beacon_frame(current_frame_type);  // Frame uses latest GPS data
    }
}
```

### UART Commands
Send commands via UART1 (communication port):

| Command | Description |
|---------|-------------|
| `GPS` | Display current GPS status |
| `LOG ALL` | Enable all debug logs |
| `LOG SYSTEM` | Enable system logs only |
| `LOG ISR` | Enable ISR logs only |
| `LOG NONE` | Disable all logs |

### GPS Status Output Example
```
=== GPS Status ===
Fix: VALID (quality: 1)
Satellites: 8
Position: 48.856600, 2.352200
Altitude: 35.0 m
HDOP: 1.2
Last update: 127 ms ago
==================
```

## 🔍 API Reference

### Core Functions

#### `void gps_init(void)`
Initialize UART3 for GPS reception at 9600 baud.
- Configures UART3 hardware
- Enables RX interrupt
- Initializes circular buffer
- Resets GPS data structure

#### `uint8_t gps_update(void)`
Process GPS data from RX buffer (call from main loop).
- **Returns**: 1 if new valid GPS data received, 0 otherwise
- **Non-blocking**: Processes all available data in buffer
- **Thread-safe**: Uses interrupts for data reception

#### `const gps_data_t* gps_get_data(void)`
Get pointer to current GPS data structure.
- **Returns**: Pointer to `gps_data_t` structure
- **Read-only**: Do not modify returned structure

#### `uint8_t gps_has_fix(void)`
Check if GPS has valid fix.
- **Returns**: 1 if position is valid and fix quality > 0, 0 otherwise

#### `void gps_print_status(void)`
Print GPS status to debug UART2.
- Displays fix status, satellites, position, altitude, HDOP
- Shows time since last GPS update

### Parser Functions

#### `void gps_parse_gga(const char *sentence)`
Parse NMEA GGA sentence.
- Validates checksum
- Extracts position, altitude, fix quality, satellites, HDOP
- Updates global GPS data structure and protocol variables

#### `void gps_parse_rmc(const char *sentence)`
Parse NMEA RMC sentence.
- Validates checksum
- Extracts position and status
- Updates global GPS data structure

#### `uint8_t gps_validate_checksum(const char *sentence)`
Validate NMEA sentence checksum.
- **Returns**: 1 if checksum valid, 0 otherwise

## ⚙️ Configuration

### Baud Rate
Default: **9600 baud** (standard for most GPS modules)

To change baud rate, modify `gps_init()`:
```c
// For 9600 baud with Fcy = 100 MHz:
U3BRG = 651;  // BRG = (Fcy / (16 * BaudRate)) - 1
```

### Buffer Size
GPS RX buffer size: **96 bytes** (configurable in `gps_nmea.h`)

### Timeout
GPS fix timeout: **2000 ms** (configurable in `gps_nmea.h`)

## 🧪 Testing

### Without GPS Module
The system will continue to work with default position values:
- Latitude: 42.95463
- Longitude: 1.364479
- Altitude: 1080 m

### With GPS Module
1. Connect GPS module to UART3
2. Power on the beacon
3. Wait for GPS fix (cold start: 30-60 seconds)
4. Send "GPS" command via UART1 to check status
5. Observe position updates in periodic transmissions

### Verification
- Check debug output for "GPS Fix: X sats" messages
- Use "GPS" command to verify position data
- Decode beacon transmission to verify GPS data in T.001 frame

## 📊 Integration with T.001 Protocol

The GPS module automatically updates the global position variables used by `protocol_data.c`:
- `current_latitude`
- `current_longitude`
- `current_altitude`

When `start_beacon_frame()` is called, it reads these variables and encodes them into the T.001 frame:
- **PDF-1 Position** (19 bits): 30-minute resolution
- **PDF-2 Offset** (18 bits): 4-second resolution
- **Altitude Code** (4 bits): Encoded altitude range

## 🐛 Troubleshooting

### No GPS Data Received
- Check GPS TX is connected to **RC4 (U3RX/RP52)**
- Check GPS RX is connected to **RC5 (U3TX/RP53)** if using bidirectional
- Verify GPS module baud rate (9600 baud)
- Check GPS module power supply (3.3V or 5V)
- Verify UART3 PPS configuration is correct (done automatically by `gps_init()`)

### Invalid GPS Position
- Wait longer for GPS fix (cold start can take 60+ seconds)
- Check GPS antenna connection
- Verify clear sky view (GPS needs satellites in view)
- Use "GPS" command to check fix quality

### Checksum Errors
- Check for electrical noise on GPS TX line
- Verify ground connection between GPS and dsPIC
- Check baud rate match between GPS and UART3

## 📚 References
- **NMEA 0183 Standard**: GPS sentence format specification
- **COSPAS-SARSAT T.001**: Beacon protocol specification
- **dsPIC33CK Datasheet**: UART and PPS configuration

## ⚠️ Critical Issues Resolved

### BCH2_POLY_MASK Bug (2025-11-25)
**Problem**: System validation failed with real GPS coordinates, causing infinite transmission retry loop.

**Diagnostic Process**:
1. Traced execution with TRACE_IMMEDIATE logs
2. Discovered validation always failed at `validate_frame_hardware()`
3. Added BCH value logging: `BCH2_calc=0x1E77` vs `BCH2_recv=0x0E77`
4. Identified bit 12 truncation due to incorrect mask

**Root Cause**: `BCH2_POLY_MASK = 0x0FFF` (12-bit) should have been `0x1FFF` (13-bit). BCH degree 12 requires 13-bit mask to accommodate values 0 to 2^13-1.

**Why Hidden in Testing**: TEST coordinates (42.95463, 1.364479) produced BCH2 values with bit 12=0, so masking had no effect. Real GPS coordinates (42.960594, 1.371029) produced BCH2=0x1E77 (bit 12=1), revealing the bug.

**Impact**: Geographic location-dependent bug. Beacons would fail only at specific positions where BCH2 calculation sets bit 12.

**Fix**: Single-line change in `protocol_data.h`:
```c
#define BCH2_POLY_MASK  0x1FFF    // Was 0x0FFF
```

### GPS Race Condition (2025-11-23)
**Problem**: Non-deterministic frame validation failures when GPS updated position during transmission.

**Root Cause**: GPS ISR (UART3) modified `current_latitude/longitude/altitude` (volatile double, 64-bit) while `build_compliant_frame()` read them. On dsPIC33CK, doubles require 4 memory accesses, creating TOCTOU vulnerability.

**Solution Implemented**:
1. **Atomic GPS snapshot** in `build_compliant_frame()`:
```c
__builtin_disable_interrupts();
lat_snapshot = current_latitude;
lon_snapshot = current_longitude;
alt_snapshot = current_altitude;
__builtin_enable_interrupts();
```

2. **Extended GPS ISR protection** in `start_beacon_frame()`:
```c
if (frame_type == BEACON_EXERCISE_FRAME) {
    IEC3bits.U3RXIE = 0;  // Disable GPS ISR
    build_exercise_frame();
    transmit_beacon_frame();  // Includes validation
    IEC3bits.U3RXIE = 1;  // Re-enable GPS ISR
}
```

3. **Atomic beacon_frame[] write**:
```c
__builtin_disable_interrupts();
memcpy((void*)beacon_frame, frame, MESSAGE_BITS);
__builtin_enable_interrupts();
```

See `ISR_CONFLICTS.md` for complete analysis of ISR interactions.

## 🔒 Notes
- GPS position updates are atomic (interrupt-safe) with multi-layer protection
- System operates normally even without GPS fix (uses default position)
- GPS module must output standard NMEA sentences (GGA or RMC)
- Maximum NMEA sentence length: 82 characters
- GPS ISR temporarily disabled during EXERCISE frame TX (~2ms, FIFO=4 chars, no data loss)
- Transmission validated with real GPS coordinates at multiple geographic locations
