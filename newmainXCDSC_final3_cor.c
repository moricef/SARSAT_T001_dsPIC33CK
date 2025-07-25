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
#define TOTAL_SYMBOLS          (PREAMBLE_DURATION_MS * SYMBOL_RATE_HZ / 1000 + \
                               MODULATED_DURATION_MS * SYMBOL_RATE_HZ / 1000)

// =============================================================================
// =============================================================================

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

// =============================================================================
// =============================================================================

// Frame composition
#define SYNC_BITS      15      // 38ms
#define FRAME_SYNC_BITS 9      // 38ms
#define MESSAGE_BITS   144     // Total message bits

// BCH Parameters (BCH(31,21) + BCH(12,12) as per COPAS-SARSAT)
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
// BCH Encoder Functions
// =============================

// BCH(31,21) encoder for position data
uint16_t bch_encode_31_21(uint32_t data) {
    uint16_t parity = 0;
    data <<= 10;  // Shift data to make room for 10-bit parity
    
    for (int i = 20; i >= 0; i--) {
        if (data & (1UL << (i + 10))) {
            data ^= (uint32_t)BCH_POLY << i;
            parity ^= BCH_POLY;
        }
    }
    return parity & 0x3FF;  // Return 10-bit parity
}

// BCH(12,12) encoder - simple parity check
uint16_t bch_encode_12_12(uint16_t data) {
    return data;  // No parity added for this demonstration
}

// =============================
// Beacon Frame Construction
// =============================

void build_beacon_frame() {
    int bit_index = 0;
    
    // 1. Add sync bits (15 bits of 1)
    for (int i = 0; i < SYNC_BITS; i++) {
        beacon_frame[bit_index++] = 1;
    }
    
    // 2. Add frame sync bits (9 bits: 0x1AC = 0b110101100)
    const uint16_t frame_sync = 0x1AC;
    for (int i = 8; i >= 0; i--) {
        beacon_frame[bit_index++] = (frame_sync >> i) & 1;
    }
    
    // 3. Add message content
    //    a) Country code (10 bits)
    uint16_t country_code = 0x2A5; // Example: France
    for (int i = 9; i >= 0; i--) {
        beacon_frame[bit_index++] = (country_code >> i) & 1;
    }
    
    //    b) Aircraft ID (24 bits)
    uint32_t aircraft_id = 0x00A5F3C; // Example ID
    for (int i = 23; i >= 0; i--) {
        beacon_frame[bit_index++] = (aircraft_id >> i) & 1;
    }
    
    //    c) Position (21 bits, 0.25° accuracy = ±34km)
    uint32_t position = 0x1A5F3; // Example: 42.25°N, 2.75°E
    for (int i = 20; i >= 0; i--) {
        beacon_frame[bit_index++] = (position >> i) & 1;
    }
    
    //    d) Position offset (20 bits, ±150m accuracy)
    uint32_t position_offset = 0x0A5F3; // Example: 150m offset
    for (int i = 19; i >= 0; i--) {
        beacon_frame[bit_index++] = (position_offset >> i) & 1;
    }
    
    // 4. Apply BCH encoding
    //uint32_t position_data = (position << 10) | position_offset;
    uint32_t position_data = (position << 20) | position_offset;  // 41 bits concaténés
    uint16_t position_parity = bch_encode_31_21(position_data);
    for (int i = 9; i >= 0; i--) {
        beacon_frame[bit_index++] = (position_parity >> i) & 1;
    }
    
    uint16_t id_parity = bch_encode_12_12(aircraft_id & 0xFFF);
    for (int i = 11; i >= 0; i--) {
        beacon_frame[bit_index++] = (id_parity >> i) & 1;
    }
}

// =============================
// Hardware Initialization
// =============================

// Initialize system clock to 100 MHz
void init_clock(void) {
    CLKDIVbits.PLLPRE = 0;      // N1 = 2
    PLLFBD = 98;                // M = 100
    __builtin_write_OSCCONH(0x01);  // Initiate clock switch
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while(OSCCONbits.COSC != 0b01);
    while(!OSCCONbits.LOCK);
}

// Initialize DAC (Output on RB0)
void init_dac(void) {
    // Configure RB0 as analog output
    ANSELB |= 0x0001;        // Set bit 0 of ANSELB (RB0 analog)
    TRISB &= ~0x0001;        // Set RB0 as output (bit 0 = 0)
    
    // Configure DAC registers
    DAC1CONL = 0x8000;       // DACEN=1 (bit 15)
    DAC1CONL |= 0x2000;      // DACOEN=1 (bit 13)
    DAC1CONH = 0x0000;       // Right-justified format (DACFM=00)
    
    // Set default output value
    DAC1DATH = (DAC_OFFSET >> 8) & 0x0F; // High 4 bits
    DAC1DATL = DAC_OFFSET & 0xFF;        // Low 8 bits
}

// Initialize Timer1 for 200 kHz interrupts
void init_timer1(void) {
    T1CON = 0;                  // Stop timer
    TMR1 = 0;                   // Clear timer
    PR1 = (FCY / SAMPLE_RATE_HZ) - 1;  // Set period
    IFS0bits.T1IF = 0;          // Clear interrupt flag
    IPC0bits.T1IP = 4;          // Set priority level (1-7)
    IEC0bits.T1IE = 1;          // Enable interrupt
    T1CONbits.TCKPS = 0;        // 1:1 prescaler
    T1CONbits.TON = 1;          // Start timer
}

// =============================
// Optimized Timer1 ISR (200 kHz)
// =============================
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void)
{
    // Phase 1: Preamble (160ms unmodulated carrier)
    if (tx_phase == PREAMBLE_PHASE) 
    {
// Correction pour la phase préambule
static const uint16_t precomputed_dac[5] = {
    [0] = (uint16_t)(2048 + ((((int32_t)32767 * COS_1P1_Q15) >> 15) >> 3)),
    [1] = (uint16_t)(2048 + ((((int32_t)10126 * COS_1P1_Q15) >> 15) >> 3)),
    [2] = (uint16_t)(2048 + ((((int32_t)(-26510) * COS_1P1_Q15) >> 15) >> 3)),
    [3] = (uint16_t)(2048 + ((((int32_t)(-26510) * COS_1P1_Q15) >> 15) >> 3)),
    [4] = (uint16_t)(2048 + ((((int32_t)10126 * COS_1P1_Q15) >> 15) >> 3))
};
        uint16_t dac_val = precomputed_dac[carrier_phase];
        DAC1DATH = (dac_val >> 8) & 0x0F;
        DAC1DATL = dac_val & 0xFF;
        debug_dac_value = dac_val;
        
        // Update carrier phase (0-4)
        carrier_phase = (carrier_phase < 4) ? carrier_phase + 1 : 0;
        
        // Check preamble completion
        //if (++preamble_count >= PREAMBLE_SAMPLES) {
        if (preamble_count++ >= PREAMBLE_SAMPLES - 1) {
            // Break at end of transmission
                if (idle_count >= IDLE_SYMBOLS) {
                    // Breakpoint 3: End of transmission
                    Nop();  // Placeholder for breakpoint
                }
            tx_phase = DATA_PHASE;
            preamble_count = 0;
            symbol_index = 0;
            sample_count = 0;
            idle_count = 0;
        }
    }
    // Phase 2: Data Transmission
    else 
    {
        // Get current symbol
        uint8_t current_symbol = 0;
        if (symbol_index < MESSAGE_BITS) {
            current_symbol = beacon_frame[symbol_index];
        }
        
        // Precomputed DAC values for both symbols - CAST EN INT32_T
        //static const uint16_t symbol_dac[2][5] = {
       /* static const uint16_t precomputed_symbol_dac[2][5] = {
            { // Symbol 0 (+1.1 rad)
                (uint16_t)(2048 + (((int32_t)cos_table[0] * COS_1P1_Q15 - (int32_t)sin_table[0] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[1] * COS_1P1_Q15 - (int32_t)sin_table[1] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[2] * COS_1P1_Q15 - (int32_t)sin_table[2] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[3] * COS_1P1_Q15 - (int32_t)sin_table[3] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[4] * COS_1P1_Q15 - (int32_t)sin_table[4] * SIN_1P1_Q15) >> 15) >> 3))
            },
            { // Symbol 1 (-1.1 rad)
                (uint16_t)(2048 + (((int32_t)cos_table[0] * COS_1P1_Q15 + (int32_t)sin_table[0] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[1] * COS_1P1_Q15 + (int32_t)sin_table[1] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[2] * COS_1P1_Q15 + (int32_t)sin_table[2] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[3] * COS_1P1_Q15 + (int32_t)sin_table[3] * SIN_1P1_Q15) >> 15) >> 3)),
                (uint16_t)(2048 + (((int32_t)cos_table[4] * COS_1P1_Q15 + (int32_t)sin_table[4] * SIN_1P1_Q15) >> 15) >> 3))
              // Precomputed DAC values for both symbols
            }
        };*/
        static const uint16_t precomputed_symbol_dac[2][5] = {
    { // Symbol 0
        (uint16_t)(2048 + ((((int32_t)32767 * COS_1P1_Q15 - (int32_t)0 * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)10126 * COS_1P1_Q15 - (int32_t)31163 * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)(-26510) * COS_1P1_Q15 - (int32_t)19260 * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)(-26510) * COS_1P1_Q15 - (int32_t)(-19260) * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)10126 * COS_1P1_Q15 - (int32_t)(-31163) * SIN_1P1_Q15) >> 15) >> 3))
    },
    { // Symbol 1
        (uint16_t)(2048 + ((((int32_t)32767 * COS_1P1_Q15 + (int32_t)0 * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)10126 * COS_1P1_Q15 + (int32_t)31163 * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)(-26510) * COS_1P1_Q15 + (int32_t)19260 * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)(-26510) * COS_1P1_Q15 + (int32_t)(-19260) * SIN_1P1_Q15) >> 15) >> 3)),
        (uint16_t)(2048 + ((((int32_t)10126 * COS_1P1_Q15 + (int32_t)(-31163) * SIN_1P1_Q15) >> 15) >> 3))
    }
};
        
        // Output DAC value
        //uint16_t dac_val = symbol_dac[current_symbol][carrier_phase];
        uint16_t dac_val = precomputed_symbol_dac[current_symbol][carrier_phase];
        DAC1DATH = (dac_val >> 8) & 0x0F;
        DAC1DATL = dac_val & 0xFF;
        debug_dac_value = dac_val;
        
        // Update carrier phase (0-4)
        carrier_phase = (carrier_phase < 4) ? carrier_phase + 1 : 0;
        
        // Symbol transition handling
        if (++sample_count >= SAMPLES_PER_SYMBOL) {
            sample_count = 0;
            
            if (symbol_index < MESSAGE_BITS) {
                // Break at start of new symbol
                if (sample_count == 0 && symbol_index < MESSAGE_BITS) {
                    // Breakpoint 2: Start of new symbol
                    Nop();  // Placeholder for breakpoint
                }
                symbol_index++;
            }
            else if (++idle_count >= IDLE_SYMBOLS) {
                // Break at end of transmission
                if (idle_count >= IDLE_SYMBOLS) {
                    // Breakpoint 3: End of transmission
                    Nop();  // Placeholder for breakpoint
                }
                tx_phase = PREAMBLE_PHASE;
                symbol_index = 0;
                idle_count = 0;
            }
        }
    }
    
    // Clear interrupt flag
    IFS0bits.T1IF = 0;
}

int main(void) {
    // Disable watchdog timer
    WDTCONLbits.ON = 0;
    
    // Build the beacon frame
    build_beacon_frame();
    
    init_clock();       // 100 MHz system clock
    init_dac();         // Configure DAC
    init_timer1();      // Configure Timer1
    
    // Enable global interrupts
    __builtin_enable_interrupts();
    
    // Ajouter dans main() avant la boucle
for(int i=0; i<4096; i++) {
    DAC1DATH = (i >> 8) & 0x0F;
    DAC1DATL = i & 0xFF;
    //__delay_us(10);
    __delay32(FCY/100000);  // Exactly 10?s at 50 MIPS
    
}
    
    while(1) {
        // Main loop - all processing in ISR
    }
    return 0;
}