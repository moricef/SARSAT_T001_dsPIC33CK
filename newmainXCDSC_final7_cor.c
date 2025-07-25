#include "includes.h"

// Modulation parameters
#define CARRIER_FREQ_HZ    40000   // 40 kHz carrier
#define SYMBOL_RATE_HZ     400     // 400 baud
#define SAMPLE_RATE_HZ     200000  // 200 kHz sampling
#define SAMPLES_PER_SYMBOL (SAMPLE_RATE_HZ / SYMBOL_RATE_HZ) // 500
#define DAC_OFFSET         2048    // Mid-scale for 12-bit DAC

// Fixed-point constants (Q15 format)
#define COS_1P1_Q15  14865    // cos(1.1 rad) in Q15
#define SIN_1P1_Q15  29197    // sin(1.1 rad) in Q15

// Carrier wave lookup table (5 samples @ 200 kHz = 40 kHz)
const int16_t cos_table[5] = {32767, 10126, -26510, -26510, 10126};
const int16_t sin_table[5] = {0, 31163, 19260, -19260, -31163};

// Beacon frame parameters
#define PREAMBLE_DURATION_MS   160     // 160ms of carrier
#define MODULATED_DURATION_MS  360     // 360ms of modulated signal

// Dual-Phase State Machine
#define PREAMBLE_PHASE 0
#define DATA_PHASE     1
volatile uint8_t tx_phase = PREAMBLE_PHASE;

// Phase Management
volatile uint8_t carrier_phase = 0;  // Cycles 0-4 at 40kHz

// Frame Timing Control
#define PREAMBLE_SAMPLES (PREAMBLE_DURATION_MS * SAMPLE_RATE_HZ / 1000)  // 32,000
volatile uint32_t preamble_count = 0;
volatile uint16_t idle_count = 0;
#define IDLE_SYMBOLS 2  // 5ms guard interval

// Frame composition (corrected sizes)
#define SYNC_BITS      15      // 15 bits of 1's
#define FRAME_SYNC_BITS 9      // 9-bit frame sync
#define COUNTRY_BITS   10      // Country code
#define AIRCRAFT_BITS  24      // Aircraft ID
#define POSITION_BITS  21      // Position data
#define OFFSET_BITS    20      // Position offset
#define BCH_POS_BITS   10      // BCH(31,21) parity
#define BCH_ID_BITS    12      // BCH(12,12) parity
#define MESSAGE_BITS   (SYNC_BITS + FRAME_SYNC_BITS + COUNTRY_BITS + \
                       AIRCRAFT_BITS + POSITION_BITS + OFFSET_BITS + \
                       BCH_POS_BITS + BCH_ID_BITS)  // 121 bits

// BCH Parameters (BCH(31,21) + BCH(12,12))
#define BCH_N1 31   // Codeword length for data
#define BCH_K1 21   // Data length for position
#define BCH_N2 12   // Parity bits length
#define BCH_POLY 0x3B3  // Generator polynomial for BCH(31,21)

// Global variables
volatile uint32_t sample_count = 0;
volatile size_t symbol_index = 0;
uint8_t beacon_frame[MESSAGE_BITS];  // Full beacon frame
volatile uint16_t debug_dac_value = 0;  // Debug probe

// =============================
// BCH Encoder Functions (Optimized)
// =============================

// BCH(31,21) encoder for position data
uint16_t bch_encode_31_21(uint32_t data) {
    uint32_t reg = 0;
    data &= 0x1FFFFF;  // Ensure 21-bit input
    
    for (int i = 20; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        uint8_t msb = (reg >> 9) & 1;
        reg = (reg << 1) | bit;
        if (msb ^ bit) reg ^= BCH_POLY;
    }
    return reg & 0x3FF;  // Return 10-bit parity
}

// BCH(12,12) encoder - identity function
uint16_t bch_encode_12_12(uint16_t data) {
    return data;
}

// =============================
// Beacon Frame Construction (Optimized)
// =============================

void build_beacon_frame() {
    int bit_index = 0;
    
    // Sync bits (15 bits of 1)
    memset(beacon_frame, 1, SYNC_BITS);
    bit_index += SYNC_BITS;
    
    // Frame sync bits (0x1AC)
    uint16_t frame_sync = 0x1AC;
    for (int i = 8; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (frame_sync >> i) & 1;
    }
    
    // Message content
    uint16_t country_code = 0x2A5;         // France
    uint32_t aircraft_id = 0x00A5F3C;      // Example ID
    uint32_t position = 0x1A5F3;           // 42.25°N, 2.75°E
    uint32_t position_offset = 0x0A5F3;    // 150m offset
    
    // Country code
    for (int i = 9; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (country_code >> i) & 1;
    }
    
    // Aircraft ID
    for (int i = 23; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (aircraft_id >> i) & 1;
    }
    
    // Position
    for (int i = 20; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (position >> i) & 1;
    }
    
    // Position offset
    for (int i = 19; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (position_offset >> i) & 1;
    }
    
    // BCH encoding
    uint16_t position_parity = bch_encode_31_21(position);
    for (int i = 9; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (position_parity >> i) & 1;
    }
    
    uint16_t id_parity = bch_encode_12_12(aircraft_id & 0xFFF);
    for (int i = 11; i >= 0; i--, bit_index++) {
        beacon_frame[bit_index] = (id_parity >> i) & 1;
    }
}

// =============================
// Hardware Initialization (dsPIC33CK specific)
// =============================

// Initialize system clock to 100 MHz
void init_clock(void) {
    // Unlock clock registers
    __builtin_write_OSCCONH(0x78);
    __builtin_write_OSCCONL(0x01);
    
    // Configure PLL: 8MHz FRC -> 100MHz
    CLKDIVbits.PLLPRE = 0;      // N1 = 2
    PLLFBD = 98;                // M = 100
    CLKDIVbits.PLLPOST = 0;     // N2 = 2
    
    // Switch to FRCPLL
    __builtin_write_OSCCONH(0x03);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while(OSCCONbits.COSC != 0b11);
    while(!OSCCONbits.LOCK);
    
    // Lock clock registers
    __builtin_write_OSCCONH(0x00);
    __builtin_write_OSCCONL(0x00);
}

// Initialize DAC with left-justified format
void init_dac(void) {
    ANSELBbits.ANSB0 = 1;     // Analog mode for RB0
    TRISBbits.TRISB0 = 0;      // Output
    
    DAC1CONL = 0;              // Clear register
    DAC1CONLbits.DACEN = 1;    // Enable DAC
    DAC1CONLbits.DACOEN = 1;   // Enable output
    DAC1CONLbits.DACFM = 1;    // Left-justified format
    
    // Set initial value
    uint16_t init_val = DAC_OFFSET << 4;  // Convert to left-justified
    DAC1DAT = init_val;        // Direct write to combined register
}

// Initialize Timer1 for 200 kHz interrupts
void init_timer1(void) {
    T1CON = 0;                  // Stop timer
    TMR1 = 0;                   // Clear
    PR1 = (50000000UL / SAMPLE_RATE_HZ) - 1;  // 50MHz FCY, 200kHz rate
    IFS0bits.T1IF = 0;          // Clear flag
    IPC0bits.T1IP = 5;          // Priority
    IEC0bits.T1IE = 1;          // Enable interrupt
    T1CONbits.TCKPS = 0;        // 1:1 prescaler
    T1CONbits.TON = 1;          // Start
}

// Precomputed DAC values in left-justified format
const uint16_t precomputed_dac[5] = {
    (DAC_OFFSET + ((32767L * COS_1P1_Q15) >> 18)) << 4,
    (DAC_OFFSET + ((10126L * COS_1P1_Q15) >> 18)) << 4,
    (DAC_OFFSET + ((-26510L * COS_1P1_Q15) >> 18)) << 4,
    (DAC_OFFSET + ((-26510L * COS_1P1_Q15) >> 18)) << 4,
    (DAC_OFFSET + ((10126L * COS_1P1_Q15) >> 18)) << 4
};

const uint16_t precomputed_symbol_dac[2][5] = {
    { // Symbol 0
        (DAC_OFFSET + ((32767L * COS_1P1_Q15 - 0L * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((10126L * COS_1P1_Q15 - 31163L * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((-26510L * COS_1P1_Q15 - 19260L * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((-26510L * COS_1P1_Q15 - (-19260L) * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((10126L * COS_1P1_Q15 - (-31163L) * SIN_1P1_Q15) >> 18)) << 4
    },
    { // Symbol 1
        (DAC_OFFSET + ((32767L * COS_1P1_Q15 + 0L * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((10126L * COS_1P1_Q15 + 31163L * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((-26510L * COS_1P1_Q15 + 19260L * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((-26510L * COS_1P1_Q15 + (-19260L) * SIN_1P1_Q15) >> 18)) << 4,
        (DAC_OFFSET + ((10126L * COS_1P1_Q15 + (-31163L) * SIN_1P1_Q15) >> 18)) << 4
    }
};

// =============================
// Optimized Timer1 ISR (200 kHz)
// =============================
void __attribute__((interrupt, no_auto_psv, shadow)) _T1Interrupt(void) {
    if (tx_phase == PREAMBLE_PHASE) {
        // Output DAC value
        DAC1DAT = precomputed_dac[carrier_phase];
        
        // Update carrier phase
        carrier_phase = (carrier_phase < 4) ? carrier_phase + 1 : 0;
        
        // Check if preamble completed
        if (++preamble_count >= PREAMBLE_SAMPLES) {
            tx_phase = DATA_PHASE;
            preamble_count = 0;
            symbol_index = 0;
            sample_count = 0;
        }
    } 
    else {
        // Get current symbol
        uint8_t current_symbol = (symbol_index < MESSAGE_BITS) ? 
                                beacon_frame[symbol_index] : 0;
        
        // Output DAC value
        DAC1DAT = precomputed_symbol_dac[current_symbol][carrier_phase];
        
        // Update carrier phase
        carrier_phase = (carrier_phase < 4) ? carrier_phase + 1 : 0;
        
        // Handle symbol transitions
        if (++sample_count >= SAMPLES_PER_SYMBOL) {
            sample_count = 0;
            
            if (symbol_index < MESSAGE_BITS) {
                symbol_index++;
            } else if (++idle_count >= IDLE_SYMBOLS) {
                tx_phase = PREAMBLE_PHASE;
                idle_count = 0;
            }
        }
    }
    
    IFS0bits.T1IF = 0;  // Clear interrupt flag
}

// =============================
// Main Application
// =============================
int main(void) {
    // Disable watchdog
    WDTCONLbits.ON = 0;
    
    // Build beacon frame
    build_beacon_frame();
    
    // Initialize hardware
    init_clock();
    init_dac();
    init_timer1();
    
    // Enable interrupts
    __builtin_enable_interrupts();
    
    // Main loop
    while(1) {
        __builtin_nop();  // Sleep between interrupts
    }
    return 0;
}