/*
 * keypad_matrix.h - 4x4 matrix keypad driver
 *
 * Wiring convention:
 *   Row pins (output): driven LOW one at a time for scanning.
 *   Col pins (input, internal pull-up): read to detect key press.
 *
 * Default pin assignment (modify defines below):
 *   Rows: RA0, RA1, RA2, RA4  (output, active low)
 *   Cols: RC6, RC7, RC12, RC13 (input, internal pull-up)
 *
 * Key map (standard 4x4):
 *   1  2  3  A
 *   4  5  6  B
 *   7  8  9  C
 *   *  0  #  D
 */

#ifndef KEYPAD_MATRIX_H
#define KEYPAD_MATRIX_H

#include "../includes.h"

// =============================
// Configuration
// =============================
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

// Debounce: number of consecutive identical reads required
#define KEYPAD_DEBOUNCE_COUNT 3

// Row pins: RA0, RA1, RA2, RA4 (output, driven low for scan)
// Note: analog mode already cleared by init_gpio() (ANSELA = 0 except RA3)
#define KP_ROW0_TRIS   TRISAbits.TRISA0
#define KP_ROW0_LAT    LATAbits.LATA0

#define KP_ROW1_TRIS   TRISAbits.TRISA1
#define KP_ROW1_LAT    LATAbits.LATA1

#define KP_ROW2_TRIS   TRISAbits.TRISA2
#define KP_ROW2_LAT    LATAbits.LATA2

#define KP_ROW3_TRIS   TRISAbits.TRISA4
#define KP_ROW3_LAT    LATAbits.LATA4

// Col pins: RC6, RC7, RC12, RC13 (input with internal pull-up)
#define KP_COL0_TRIS   TRISCbits.TRISC6
#define KP_COL0_PORT   PORTCbits.RC6
#define KP_COL0_CNPU   CNPUCbits.CNPUC6

#define KP_COL1_TRIS   TRISCbits.TRISC7
#define KP_COL1_PORT   PORTCbits.RC7
#define KP_COL1_CNPU   CNPUCbits.CNPUC7

#define KP_COL2_TRIS   TRISCbits.TRISC12
#define KP_COL2_PORT   PORTCbits.RC12
#define KP_COL2_CNPU   CNPUCbits.CNPUC12

#define KP_COL3_TRIS   TRISCbits.TRISC13
#define KP_COL3_PORT   PORTCbits.RC13
#define KP_COL3_CNPU   CNPUCbits.CNPUC13

// Return value when no key is pressed
#define KEYPAD_NO_KEY   '\0'

// =============================
// API
// =============================

// Initialize GPIO for keypad (call once at startup).
void keypad_init(void);

// Scan keypad and return the pressed key character, or KEYPAD_NO_KEY.
// Includes debounce. Call periodically (every ~10 ms) from main loop.
char keypad_get_key(void);

// Raw scan without debounce (returns key or KEYPAD_NO_KEY).
char keypad_scan_raw(void);

#endif /* KEYPAD_MATRIX_H */
