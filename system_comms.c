#include "system_comms.h"
#include "protocol_data.h"
#include "system_definitions.h"
#include "includes.h"

// =============================
// Variables globales
// =============================
volatile char rxQueue[UART_BUFFER_SIZE];
volatile uint16_t rxHead = 0;
volatile uint16_t rxTail = 0;
volatile uint8_t rxOverflowed = 0;
volatile uint32_t millis_counter = 0;
volatile uint32_t last_tx_time = 0;
volatile uint32_t tx_interval_ms = 5000;
volatile uint8_t carrier_phase = 0;
volatile uint32_t phase_sample_count = 0;
volatile uint16_t bit_index = 0;
volatile uint8_t tx_phase = IDLE_STATE;
volatile uint8_t beacon_frame[MESSAGE_BITS] = {0};  // Initialisation

// Tables DAC
#define COS_1P1_Q15  14865
#define SIN_1P1_Q15  29197
const uint16_t dac_table_plus[5] = {
    DAC_OFFSET + (int16_t)((32767L * COS_1P1_Q15 - 0L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((10126L * COS_1P1_Q15 - 31163L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((-26510L * COS_1P1_Q15 - 19260L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((-26510L * COS_1P1_Q15 + 19260L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((10126L * COS_1P1_Q15 + 31163L * SIN_1P1_Q15) >> 18)
};

const uint16_t dac_table_minus[5] = {
    DAC_OFFSET + (int16_t)((32767L * COS_1P1_Q15 + 0L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((10126L * COS_1P1_Q15 + 31163L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((-26510L * COS_1P1_Q15 + 19260L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((-26510L * COS_1P1_Q15 - 19260L * SIN_1P1_Q15) >> 18),
    DAC_OFFSET + (int16_t)((10126L * COS_1P1_Q15 - 31163L * SIN_1P1_Q15) >> 18)
};

// =============================
// Fonctions d'initialisation
// =============================
void init_clock(void) {
    // Unlock sequence
    __builtin_write_OSCCONH(0x78);
    __builtin_write_OSCCONL(OSCCONL | 0x01);
    
    OSCTUN = 0;
    
    // PLL configuration for 100 MHz
    CLKDIVbits.PLLPRE = 0;
    PLLFBD = 48;
    
    // Switch to FRCPLL
    __builtin_write_OSCCONH(0x03);
    __builtin_write_OSCCONL(OSCCONL | 0x01);
    
    // Wait for stabilization
    while(OSCCONbits.COSC != 0b011);
    while(!OSCCONbits.LOCK);
    
    // Lock sequence
    __builtin_write_OSCCONH(0x00);
    __builtin_write_OSCCONL(OSCCONL & ~0x01);
}

void init_gpio(void) {
    // Configure switches
    TRISBbits.TRISB12 = 1;   // S1: Frame type (Test/Exercice)
    TRISBbits.TRISB13 = 1;   // S2: Frequency (5s/50s)
    CNPDBbits.CNPDB12 = 1;
    CNPDBbits.CNPDB13 = 1;
    
    // Configure TX LED
    TRISBbits.TRISB14 = 0;
    LATBbits.LATB14 = 0;
    
    // Configure RF Amplifier Control
    TRISBbits.TRISB15 = 0;   // Amplifier enable
    TRISBbits.TRISB11 = 0;   // Power level control
    AMP_ENABLE_PIN = 0;
    POWER_CTRL_PIN = 0;
}

void init_dac(void) {
    ANSELBbits.ANSELB0 = 1;
    TRISBbits.TRISB0 = 0;
    
    DAC1CONL = 0x0000;
    DAC1CONH = 0x8000;
    
    uint16_t init_val = DAC_OFFSET << 4;
    DAC1DATL = init_val & 0xFF;
    DAC1DATH = (init_val >> 8) & 0x0F;
}

void init_timer1(void) {
    T1CON = 0;
    TMR1 = 0;
    PR1 = (100000000UL / 200000) - 1; // SAMPLE_RATE_HZ = 200000
    IFS0bits.T1IF = 0;
    IPC0bits.T1IP = 5;
    IEC0bits.T1IE = 1;
    T1CONbits.TCKPS = 0;
    T1CONbits.TON = 1;
}

void init_rf_amplifier(void) {
    set_rf_power_level(POWER_LOW);
}

// =============================
// Contrôle RF
// =============================
void set_rf_power_level(uint8_t mode) {
    POWER_CTRL_PIN = (mode == POWER_HIGH) ? 1 : 0;
}

void control_rf_amplifier(uint8_t state) {
    if (state) {
        __delay_us(100);
        AMP_ENABLE_PIN = 1;
        __delay_us(500);
    } else {
        AMP_ENABLE_PIN = 0;
        __delay_us(100);
    }
}

// =============================
// Fonctions UART
// =============================
void UART1_Initialize(void) {
    _RP34R = 0x0003;
    _U1RXR = 32;
    
    IEC0bits.U1TXIE = 0;
    IEC0bits.U1RXIE = 0;
    IEC11bits.U1EVTIE = 0;

    U1MODE = 0x0000;
    U1MODEH = 0x0800;
    U1STA = 0x0080;
    U1STAH = 0x002E;
    
    U1BRG = (100000000UL / (16UL * 9600UL)) - 1;
    U1BRGH = 0x0000;

    rxHead = 0;
    rxTail = 0;
    rxOverflowed = 0;

    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;
    IPC2bits.U1RXIP = 4;

    U1MODEbits.UARTEN = 1;
    U1MODEbits.UTXEN = 1;
    U1MODEbits.URXEN = 1;
}

void __attribute__((__interrupt__, __auto_psv__)) _U1RXInterrupt(void) {
    volatile uint16_t nextTail = (rxTail + 1) % UART_BUFFER_SIZE;
    
    if (nextTail == rxHead) {
        rxOverflowed = 1;
    } else {
        rxQueue[rxTail] = U1RXREG;
        rxTail = nextTail;
    }
    IFS0bits.U1RXIF = 0;
}

uint8_t uart_get_line(char *buffer, uint16_t max_len) {
    uint16_t len = 0;
    uint16_t localTail = rxTail;
    
    if (rxOverflowed) {
        buffer[0] = '\0';
        rxOverflowed = 0;
        return 0;
    }
    
    while (localTail != rxHead && len < max_len - 1) {
        char c = rxQueue[localTail];
        buffer[len++] = c;
        
        if (++localTail >= UART_BUFFER_SIZE) {
            localTail = 0;
        }
        
        if (c == '\n') break;
    }
    
    if (len > 0) {
        rxTail = localTail;
        buffer[len] = '\0';
        return 1;
    }
    return 0;
}

void init_debug_uart(void) {
    U2MODE = 0x0000;
    U2MODEH = 0x0800;
    U2STAH = 0x0000;
    
    U2BRG = (100000000UL / (4 * DEBUG_BAUD_RATE)) - 1;
    U2MODEbits.BRGH = 1;
    
    U2MODEbits.UARTEN = 1;
    U2MODEbits.UTXEN = 1;
    
    ANSELBbits.ANSELB9 = 0;
    TRISBbits.TRISB9 = 0;
}

// =============================
// Fonctions debug_print 
// =============================

void debug_print_char(char c) {
    while (U2STAHbits.UTXBF);
    U2TXREG = c;
}

void debug_print_str(const char *str) {
    while (*str) debug_print_char(*str++);
}

void debug_print_hex(uint8_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    debug_print_char(hex_chars[(value >> 4) & 0x0F]);
    debug_print_char(hex_chars[value & 0x0F]);
}

// Formatage portable pour entiers
void debug_print_int32(int32_t value) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%ld", (long)value);  // Formatage 32-bit signé
    debug_print_str(buffer);
}

void debug_print_uint32(uint32_t value) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);  // Formatage 32-bit non signé
    debug_print_str(buffer);
}

void debug_print_float(double value, int prec) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%.*f", prec, value);
    debug_print_str(buffer);
}

void debug_print_hex24(uint32_t value) {
    debug_print_hex((value >> 16) & 0xFF);
    debug_print_hex((value >> 8) & 0xFF);
    debug_print_hex(value & 0xFF);
}

void debug_print_hex16(uint16_t value) {
    debug_print_hex((value >> 8) & 0xFF);
    debug_print_hex(value & 0xFF);
}

void debug_print_hex64(uint64_t value) {
    debug_print_hex((value >> 56) & 0xFF);
    debug_print_hex((value >> 48) & 0xFF);
    debug_print_hex((value >> 40) & 0xFF);
    debug_print_hex((value >> 32) & 0xFF);
    debug_print_hex((value >> 24) & 0xFF);
    debug_print_hex((value >> 16) & 0xFF);
    debug_print_hex((value >> 8) & 0xFF);
    debug_print_hex(value & 0xFF);
}

void debug_print_hex32(uint32_t value) {
    // Affiche les 32 bits en deux parties de 16 bits
    debug_print_hex16((uint16_t)(value >> 16));
    debug_print_hex16((uint16_t)(value & 0xFFFF));
}

// =============================
// Interruption Timer1
// =============================
void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt(void) {
    static uint16_t sample_count = 0;
    static uint32_t amp_start_time = 0;
    
    sample_count++;
    if (sample_count >= 200) {
        sample_count = 0;
        millis_counter++;
    }
    
    uint16_t dac_value = DAC_OFFSET;
    
    	if (tx_phase == PREAMBLE_PHASE && phase_sample_count == 0) {
        DEBUG_BREAKPOINT();
    }
    
    switch(tx_phase) {
        case PRE_AMPLI:
            if (phase_sample_count == 0) {
                control_rf_amplifier(1);
                LATBbits.LATB14 = 1;
                amp_start_time = millis_counter;
                phase_sample_count = 1;
            } else {
                if ((millis_counter - amp_start_time) >= 1) {
                    tx_phase = PREAMBLE_PHASE;
                    phase_sample_count = 0;
                    carrier_phase = 0;
                }
            }
            break;
            
        case PREAMBLE_PHASE:
            dac_value = dac_table_plus[carrier_phase];
            carrier_phase = (carrier_phase + 1) % 5;
            
            if (++phase_sample_count >= PREAMBLE_SAMPLES) {
                tx_phase = DATA_PHASE;
                phase_sample_count = 0;
                bit_index = 0;
                carrier_phase = 0;
            }
            break;
            
        case DATA_PHASE:
            if (bit_index < MESSAGE_BITS) {
                uint8_t current_bit = beacon_frame[bit_index];
                uint16_t half_bit = phase_sample_count / SAMPLES_PER_HALF_BIT;
                
                if (half_bit == 0) {
                    dac_value = (current_bit == 1) ? 
                        dac_table_plus[carrier_phase] : 
                        dac_table_minus[carrier_phase];
                } else {
                    dac_value = (current_bit == 1) ? 
                        dac_table_minus[carrier_phase] : 
                        dac_table_plus[carrier_phase];
                }
                
                carrier_phase = (carrier_phase + 1) % 5;
                
                if (++phase_sample_count >= SAMPLES_PER_SYMBOL) {
                    phase_sample_count = 0;
                    bit_index++;
                    
                    if (bit_index >= MESSAGE_BITS) {
                        tx_phase = POSTAMBLE_PHASE;
                    }
                }
            }
            break;
            
        case POSTAMBLE_PHASE:
            dac_value = DAC_OFFSET;
            
            if (++phase_sample_count >= POSTAMBLE_SAMPLES) {
                tx_phase = POST_AMPLI;
                phase_sample_count = 0;
            }
            break;
            
        case POST_AMPLI:
            control_rf_amplifier(0);
            LATBbits.LATB14 = 0;
            tx_phase = IDLE_STATE;
            break;
    }
    
    uint16_t dac_val_aligned = dac_value << 4;
    DAC1DATL = dac_val_aligned & 0xFF;
    DAC1DATH = (dac_val_aligned >> 8) & 0x0F;
  
    IFS0bits.T1IF = 0;
}
