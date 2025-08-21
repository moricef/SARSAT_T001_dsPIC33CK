// system_comms.c
#include "includes.h"
#include "system_definitions.h"
#include "system_comms.h"
#include "system_debug.h"
#include "protocol_data.h"
#include "rf_interface.h"

// =============================
// Global Variables
// =============================
volatile uint32_t millis_counter = 0;              // System time in milliseconds
volatile uint32_t last_tx_time = 0;                // Timestamp of last transmission
volatile uint32_t tx_interval_ms = 5000;           // Transmission interval (5s default)
volatile uint8_t carrier_phase = 0;                // Current carrier phase position
volatile uint32_t phase_sample_count = 0;           // Phase sample counter
volatile uint16_t bit_index = 0;                   // Current bit position in frame
volatile tx_phase_t tx_phase = IDLE_STATE;          // Current transmission state
volatile uint8_t beacon_frame[MESSAGE_BITS] = {0};  // Transmission frame buffer
volatile uint8_t transmission_complete_flag = 0;    // Transmission completion flag

// RF envelope control
volatile float envelope_gain = 0.0f;               // RF envelope gain (0.0-1.0)
volatile uint16_t ramp_samples = RAMP_SAMPLES_DEFAULT;  // RF ramp duration in samples
volatile uint16_t current_ramp_count = 0;          // Current position in ramp

// Single-timer architecture variables
volatile uint16_t modulation_counter = 0;           // Modulation sample counter
volatile uint16_t sample_count = 0;                 // General purpose sample counter

// RF control variables
volatile uint8_t amp_enabled = 0;                  // RF amplifier state (0=off)
volatile uint8_t current_power_mode = POWER_LOW;    // Current power mode

// I/Q Modulation for ADL5375 with single DAC (dsPIC33CK64MC105)
// RF modules managed by rf_interface.c

// BPSK modulation values for Q channel (sin component)
const uint16_t q_channel_plus_1_1_rad = 2048 + 1823;   // +1.1 rad: sin(1.1) * 2047 ≈ 1823
const uint16_t q_channel_minus_1_1_rad = 2048 - 1823;  // -1.1 rad: sin(-1.1) * 2047 ≈ -1823
const uint16_t i_channel_constant = 2048;              // Constant level for reference

void system_halt(const char* message) {
    while(1) {
        DEBUG_LOG_FLUSH(message);
        __delay_ms(1000);
    }
}

// =============================
// System Clock Initialization
// =============================
void init_clock(void) {
    // Select FRC as primary clock source
    __builtin_write_OSCCONH(0x0000);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while(OSCCONbits.OSWEN);  // Wait for clock switch completion
    
    // Configure PLL for 100 MHz operation (FCY = 100 MHz)
    // FRC = 8 MHz, Target FCY = 100 MHz → FOSC = 200 MHz
    // FOSC = FIN × M / (N1 × N2 × N3) = 8 × 200 / (2 × 4 × 1) = 200 MHz
    CLKDIVbits.PLLPRE = 1;     // N1 = 2 (Pre-diviseur)
    PLLFBD = 199;              // M = 200 (Multiplicateur, register = M-1)
    PLLDIVbits.POST1DIV = 3;   // N2 = 4 (Post-diviseur 1)
    PLLDIVbits.POST2DIV = 0;   // N3 = 1 (Post-diviseur 2)

    // Activate FRCPLL
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    
    // Wait for PLL lock
    while(OSCCONbits.OSWEN);
    while(!OSCCONbits.LOCK);
    
    // Handle PLL lock failure
    if(!OSCCONbits.LOCK) {
        system_halt("PLL lock failed");
    }
}

// =============================
// GPIO Initialization
// =============================
void init_gpio(void) {
    // Disable analog functionality
    ANSELA = 0x0000; 
    ANSELB = 0x0000;
	ANSELD = 0x000;	
    
    // DAC output pin (RA3)
    TRISAbits.TRISA3 = 0;     // Output
    LATAbits.LATA3 = 0;       // Initial low state
    
    // RF control pins
    TRISBbits.TRISB15 = 0;    // Amplifier enable (output)
    LATBbits.LATB15 = 0;      // Initially disabled
    TRISBbits.TRISB11 = 0;    // Power level select (output)
    LATBbits.LATB11 = 0;      // Initial low power mode
    
    // Transmission indicator LED (RD10)
	ANSELDbits.ANSELD10 = 0;  // Digital mode
    TRISDbits.TRISD10 = 0;    // Output
    LATDbits.LATD10 = 0;      // Initially off
    
    // Configuration switches
    TRISBbits.TRISB12 = 1;    // Frame type select (input)
    CNPDBbits.CNPDB12 = 1;    // Enable pull-down
    TRISBbits.TRISB13 = 1;    // Frequency select (input)
    CNPDBbits.CNPDB13 = 1;    // Enable pull-down
    
    // DAC output configuration
    ANSELAbits.ANSELA3 = 1;   // Analog mode
    TRISAbits.TRISA3 = 0;     // Output
    
    // Debug pin (RB0)
    TRISBbits.TRISB0 = 0;     // Output
    ANSELBbits.ANSELB0 = 0;   // Digital mode
    LATBbits.LATB0 = 0;       // Initial low state
}

// =============================
// DAC Initialization
// =============================
void init_dac(void) {
    // Configure DAC control registers
    DAC1CONH = 0x0;
    DAC1CONL = 0x8200; 
    
    // Timing configuration
    DACCTRL1L = 0x0040;      
    DACCTRL2H = 0x008A;      
    DACCTRL2L = 0x0055;      
    
    // Enable DAC
    DAC1CONLbits.DACEN = 1;   
    DAC1CONLbits.DACOEN = 1;  
    
    // Initial value (1.65V midpoint)
    DAC1DATH = DAC_OFFSET;
    
    // Final activation
    DACCTRL1Lbits.DACON = 1;  
    
    DEBUG_LOG_FLUSH("DAC initialized\r\n");
}

// =============================
// Timer1 Initialization
// =============================
void init_timer1(void) {
    T1CON = 0;        // Clear configuration
    TMR1 = 0;         // Reset timer counter
    
    // Calculate period for 6400 Hz sample rate
    PR1 = (FCY / ACTUAL_SAMPLE_RATE_HZ) - 1;  // 100MHz/6.4kHz-1 = 15624
    
    // Timer configuration
    T1CONbits.TCKPS = 0;    // No prescaler (1:1)
    T1CONbits.TCS = 0;      // Internal clock (Fcy)
    IPC0bits.T1IP = 7;      // Highest priority
    IFS0bits.T1IF = 0;      // Clear interrupt flag
    IEC0bits.T1IE = 1;      // Enable interrupt
    T1CONbits.TON = 1;      // Start timer
    
    DEBUG_LOG_FLUSH("Timer1: PR1=");
    debug_print_uint16(PR1);
    DEBUG_LOG_FLUSH(", Freq=");
    debug_print_uint32(FCY/(PR1+1));
    DEBUG_LOG_FLUSH("Hz (expected 6400Hz)\r\n");
}

// =============================
// Unified ISR for Envelope and Modulation
// =============================
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    static uint16_t debug_counter = 0;
    static uint8_t pin_state = 0;
    // Phase continuity handled in Biphase-L encoding below
    
    // Toggle debug pin (RB0) for timing analysis
    LATBbits.LATB0 = pin_state = !pin_state;
    
    // Millisecond counter management
    // Timer1 = 6400 Hz → 1ms = 6.4 échantillons
    #define MS_PER_INCREMENT 6  // Approximation 6400/6 = 1066 Hz (trop rapide!)
    // CORRECTION: 6400 Hz / 6.4 = 1000 Hz (1ms exact)
    static uint16_t sub_ms_counter = 0;
    static uint8_t fractional_counter = 0;
    
    sub_ms_counter++;
    // Simulation de 6.4 échantillons par ms avec 6 + 0.4 fractionnaire
    if (sub_ms_counter >= 6) {
        sub_ms_counter = 0;
        fractional_counter += 4;  // Accumulation 0.4
        if (fractional_counter >= 10) {
            fractional_counter -= 10;
            // Skip this increment (6.4 au lieu de 6)
        } else {
            millis_counter++;
        }
    }
    
    // Biphase-L modulation handling
    if (++modulation_counter >= MODULATION_INTERVAL) {
        modulation_counter = 0;
        uint16_t dac_value = DAC_OFFSET;

        switch(tx_phase) {
            // RF ramp-up phase: no modulation
            case PRE_AMPLI_RAMP_UP:
                dac_value = DAC_OFFSET;  // Midpoint voltage
                break;
                
            // RF ramp-down phase: no modulation
            case POST_AMPLI_RAMP_DOWN:
                dac_value = DAC_OFFSET;  // Midpoint voltage
                break;
                
            // Unmodulated carrier phase  
            case PREAMBLE_PHASE:
                // For ADL5375-05: unmodulated = Q at bias level (500mV)
                dac_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);  // 500mV bias level
                
                // Check if preamble duration completed
                if (++sample_count >= PREAMBLE_SAMPLES) {
                    sample_count = 0;
                    tx_phase = DATA_PHASE;
                    // Phase reset for data transmission
                }
                break;

            // Data transmission phase
            case DATA_PHASE:
                if (bit_index < MESSAGE_BITS) {
                    uint8_t current_bit = beacon_frame[bit_index];
                    float phase_shift;
                    
                    // Biphase-L encoding: bit 1 = +/-, bit 0 = -/+
                    if (current_bit == 1) {
                        phase_shift = (sample_count < OVERSAMPLING/2) ? 
                            +PHASE_SHIFT_RADIANS : -PHASE_SHIFT_RADIANS;
                    } else {
                        phase_shift = (sample_count < OVERSAMPLING/2) ? 
                            -PHASE_SHIFT_RADIANS : +PHASE_SHIFT_RADIANS;
                    }
                    
                    // Phase continuity maintained automatically in I/Q modulation
                    
                    // Calculate ADL5375-05 Q channel value with current envelope
                    dac_value = calculate_adl5375_q_channel(phase_shift, 1);
                    
                    // Debug logging (sample every 500 cycles)
                    static uint16_t log_counter = 0;
                    if (++log_counter >= 500) {
                        log_counter = 0;
                        ISR_LOG_PHASE(sample_count & 0x0F, envelope_gain, dac_value);
                    } 

                    // End of symbol handling
                    if (++sample_count >= OVERSAMPLING) {
                        sample_count = 0;
                        bit_index++;
                        if (bit_index >= MESSAGE_BITS) {
                            tx_phase = POSTAMBLE_PHASE;
                        }
                    }
                }
                break;

            // Post-transmission silence
            case POSTAMBLE_PHASE:
                dac_value = DAC_OFFSET;  // Midpoint voltage
                if (++sample_count >= POSTAMBLE_SAMPLES) {
                    tx_phase = POST_AMPLI_RAMP_DOWN;
                    current_ramp_count = 0;
                    sample_count = 0;
                }
                break;
                
            // System idle state
            case IDLE_STATE:
                dac_value = DAC_OFFSET;  // Midpoint voltage
                if (++debug_counter >= 1000) {
                    debug_counter = 0;
                    ISR_LOG_PHASE(carrier_phase & 0x0F, envelope_gain, dac_value);
                }
                break;
        }
        
        // Update DAC output register
        DAC1DATH = dac_value & 0x0FFF;
        
        // Transmission completion detection
        static tx_phase_t previous_phase = IDLE_STATE;
        if (tx_phase == IDLE_STATE && previous_phase != IDLE_STATE) {
            transmission_complete_flag = 1;
        }
        previous_phase = tx_phase;
    }

    // RF envelope management (independent of modulation timing)
    switch(tx_phase) {
        case PRE_AMPLI_RAMP_UP:
            if(current_ramp_count < ramp_samples) {
                envelope_gain = (float)current_ramp_count / ramp_samples;
                current_ramp_count++;
            } else {
                envelope_gain = 1.0f;
                tx_phase = PREAMBLE_PHASE;
                sample_count = 0;  // Reset for preamble
            }
            break;

        case POST_AMPLI_RAMP_DOWN:
            if(current_ramp_count < ramp_samples) {
                envelope_gain = 1.0f - (float)current_ramp_count / ramp_samples;
                current_ramp_count++;
            } else {
                envelope_gain = 0.0f;
                control_rf_amplifier(0);
                LED_TX_PIN = 1;  // Turn off TX LED (inverted logic)
                tx_phase = IDLE_STATE;
            }
            break;

        default:
            break;
    }

    // Clear Timer1 interrupt flag
    IFS0bits.T1IF = 0;
}

// =============================
// ADL5375-05 Interface Functions
// =============================

// Convert DAC value to ADL5375-05 compatible range (0-1V + 500mV bias)
uint16_t adapt_dac_for_adl5375(uint16_t dac_value) {
    // Input: dac_value 0-4095 (0-3.3V from dsPIC33CK)
    // Output: DAC value for 0-1V range suitable for ADL5375-05
    
    // Convert DAC units to voltage
    float voltage = ((float)dac_value * VOLTAGE_REF_3V3) / (float)DAC_RESOLUTION;
    
    // Map to ADL5375-05 range: 0-1V (bias±swing handled externally)
    voltage = voltage * (ADL5375_MAX_VOLTAGE / VOLTAGE_REF_3V3);
    
    // Clamp to valid range
    if (voltage < ADL5375_MIN_VOLTAGE) voltage = ADL5375_MIN_VOLTAGE;
    if (voltage > ADL5375_MAX_VOLTAGE) voltage = ADL5375_MAX_VOLTAGE;
    
    // Convert back to DAC units
    return (uint16_t)((voltage * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
}

// Calculate Q channel value for ADL5375-05 BPSK modulation
uint16_t calculate_adl5375_q_channel(float phase_shift, uint8_t apply_envelope) {
    // ADL5375-05: 500mV bias ± 500mV swing = 0-1V range
    // Q channel modulation: bias + sin(±1.1 rad) * swing/2
    
    float q_voltage = (float)ADL5375_BIAS_MV / 1000.0f;  // 0.5V bias
    
    // Add BPSK modulation based on phase shift
    if (phase_shift >= 0) {
        q_voltage += (sinf(PHASE_SHIFT_RADIANS) * (float)ADL5375_SWING_MV / 2000.0f);  // +modulation
    } else {
        q_voltage += (sinf(-PHASE_SHIFT_RADIANS) * (float)ADL5375_SWING_MV / 2000.0f); // -modulation  
    }
    
    // Apply envelope gain for RF ramp up/down
    if (apply_envelope) {
        float bias_voltage = (float)ADL5375_BIAS_MV / 1000.0f;
        q_voltage = bias_voltage + (q_voltage - bias_voltage) * envelope_gain;
    }
    
    // Clamp to ADL5375-05 range [0, 1V]
    if (q_voltage < ADL5375_MIN_VOLTAGE) q_voltage = ADL5375_MIN_VOLTAGE;
    if (q_voltage > ADL5375_MAX_VOLTAGE) q_voltage = ADL5375_MAX_VOLTAGE;
    
    // Convert to DAC units (0-4095 for 0-3.3V, scaled to 0-1V range)
    return (uint16_t)((q_voltage * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
}

// Legacy function for backward compatibility
uint16_t calculate_modulated_value(float phase_shift, uint8_t carrier_phase, uint8_t apply_envelope) {
    // Use new ADL5375-05 optimized function
    return calculate_adl5375_q_channel(phase_shift, apply_envelope);
}

// =============================
// RF Rise/Fall Time Calibration
// =============================
void calibrate_rise_fall_times(void) {
    // Placeholder for actual measurement
    float actual_rise_time = 125.0f;
    float actual_fall_time = 115.0f; 
    
    // Calculate symmetry
    float symmetry = fabsf(actual_rise_time - actual_fall_time) / 
                    fabsf(actual_rise_time + actual_fall_time);
    
    // Symmetry check
    if (symmetry > SYMMETRY_THRESHOLD) {
        DEBUG_LOG_FLUSH("Symmetry error: ");
        debug_print_float(symmetry, 3);
        DEBUG_LOG_FLUSH("\r\n");
    }
    
    // Adjust ramp samples based on timing
    if (actual_rise_time < RISE_TIME_MIN_US) 
        ramp_samples = (uint16_t)(ramp_samples * 1.1f);
    else if (actual_rise_time > RISE_TIME_MAX_US) 
        ramp_samples = (uint16_t)(ramp_samples * 0.9f);
    
    DEBUG_LOG_FLUSH("Ramp samples: ");
    debug_print_uint32(ramp_samples);
    DEBUG_LOG_FLUSH("\r\n");
}

// =============================
// Transmission Start Sequence
// =============================
void start_transmission(volatile const uint8_t* data) {
    static uint8_t first_run = 1;
    if(first_run) {
        calibrate_rise_fall_times();
        first_run = 0;
    }
    
	last_tx_time = millis_counter;
	
    // Copy data to frame buffer atomically
    __builtin_disable_interrupts();
    for (uint16_t i = 0; i < MESSAGE_BITS; i++) {
        beacon_frame[i] = data[i];
    }
    __builtin_enable_interrupts();
    
    // Reset transmission state
    sample_count = 0;
    bit_index = 0;
    current_ramp_count = 0;
    envelope_gain = 0.0f;
    
    // Activate RF systems
    control_rf_amplifier(1);       // Enable RF amplifier
    LED_TX_PIN = 0;                // Turn on transmission LED
    tx_phase = PRE_AMPLI_RAMP_UP;  // Start transmission sequence
}

// RF functions moved to rf_interface.c
//============================
//
//============================
void set_tx_interval(uint32_t interval_ms) {
    __builtin_disable_interrupts();
    tx_interval_ms = interval_ms;
    __builtin_enable_interrupts();
}

// =============================
// System Initialization
// =============================
void system_init(void) {
    init_clock();              // Configure system clock
    init_gpio();               // Initialize GPIO pins
    init_dac();                // Set up DAC module
    system_debug_init();       // Initialize debug system
    init_comm_uart();          // Set up communication UART
    init_timer1();             // Configure sample timer
    
    // Initialize RF modules (managed by rf_interface.c)
    rf_initialize_all_modules(); // Initialize complete RF chain
    
    // Initialize time management variables
    last_tx_time = 0;
    //tx_interval_ms = 5000;
    ramp_samples = RAMP_SAMPLES_DEFAULT;
    envelope_gain = 0.0f;
    
    // Clear frame buffer
    memset((void*)beacon_frame, 0, MESSAGE_BITS);
}