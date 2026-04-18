/*
 * keypad_matrix.c - 4x4 matrix keypad driver
 */

#include "keypad_matrix.h"

// Key map: [row][col]
static const char keypad_map[KEYPAD_ROWS][KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

// =============================
// Row/Col helpers
// =============================

static void set_row(uint8_t row, uint8_t level) {
    switch (row) {
        case 0: KP_ROW0_LAT = level; break;
        case 1: KP_ROW1_LAT = level; break;
        case 2: KP_ROW2_LAT = level; break;
        case 3: KP_ROW3_LAT = level; break;
    }
}

static uint8_t read_col(uint8_t col) {
    switch (col) {
        case 0: return KP_COL0_PORT;
        case 1: return KP_COL1_PORT;
        case 2: return KP_COL2_PORT;
        case 3: return KP_COL3_PORT;
    }
    return 1;
}

// =============================
// API implementation
// =============================

void keypad_init(void) {
    // Analog mode already cleared globally in init_gpio() for PORTA.
    // PORTC pins here are digital-only (no ANSELC bits on RC6/7/12/13).

    // Row pins: digital output, drive high (inactive)
    KP_ROW0_TRIS = 0; KP_ROW0_LAT = 1;
    KP_ROW1_TRIS = 0; KP_ROW1_LAT = 1;
    KP_ROW2_TRIS = 0; KP_ROW2_LAT = 1;
    KP_ROW3_TRIS = 0; KP_ROW3_LAT = 1;

    // Col pins: digital input with internal pull-up
    KP_COL0_TRIS = 1; KP_COL0_CNPU = 1;
    KP_COL1_TRIS = 1; KP_COL1_CNPU = 1;
    KP_COL2_TRIS = 1; KP_COL2_CNPU = 1;
    KP_COL3_TRIS = 1; KP_COL3_CNPU = 1;
}

char keypad_scan_raw(void) {
    uint8_t row, col;

    for (row = 0; row < KEYPAD_ROWS; row++) {
        // Drive current row low
        set_row(row, 0);
        __delay_us(10);  // Settle time

        for (col = 0; col < KEYPAD_COLS; col++) {
            if (!read_col(col)) {
                // Key pressed: restore row and return key
                set_row(row, 1);
                return keypad_map[row][col];
            }
        }

        // Restore row to high
        set_row(row, 1);
    }

    return KEYPAD_NO_KEY;
}

char keypad_get_key(void) {
    static char last_key = KEYPAD_NO_KEY;
    static uint8_t count = 0;

    char key = keypad_scan_raw();

    if (key == last_key) {
        if (key != KEYPAD_NO_KEY) {
            count++;
            if (count == KEYPAD_DEBOUNCE_COUNT) {
                // Confirmed press: return key only once
                return key;
            }
        } else {
            count = 0;
        }
    } else {
        last_key = key;
        count = 1;
    }

    return KEYPAD_NO_KEY;
}
