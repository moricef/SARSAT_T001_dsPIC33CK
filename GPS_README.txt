================================================================================
GPS NMEA INTEGRATION - QUICK START
================================================================================

HARDWARE CONNECTION:
  GPS Module          dsPIC33CK
  ----------          ---------
  GPS TX      --->    RC4 (U3RX/RP52)
  GPS RX      <---    RC5 (U3TX/RP53) [optional]
  VCC         --->    3.3V or 5V
  GND         --->    GND

CONFIGURATION:
  - UART3: 9600 baud, 8N1
  - Interrupt-driven reception
  - Automatic PPS configuration in gps_init()

UART ASSIGNMENTS:
  - UART1 (RB3/RB4): Communication (currently unused)
  - UART2 (RC10/RC11): Debug output
  - UART3 (RC4/RC5): GPS NMEA ✓

FILES:
  - gps_nmea.h/c: GPS driver and NMEA parser
  - GPS_INTEGRATION.md: Complete documentation
  - PIN_ASSIGNMENT.md: Full pin mapping reference

USAGE:
  Send "GPS" command via UART1 to display GPS status

SUPPORTED NMEA SENTENCES:
  - $GPGGA: Position, altitude, fix quality, satellites, HDOP
  - $GPRMC: Position, velocity, course
  - $GNGGA/$GNRMC: Multi-constellation variants

================================================================================
