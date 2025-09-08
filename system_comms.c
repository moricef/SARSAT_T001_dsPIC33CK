// system_comms.c
#include "includes.h"
#include "system_definitions.h"
#include "system_comms.h"
#include "system_debug.h"
#include "protocol_data.h"
#include "rf_interface.h"
#include "signal_processor.h"

// Declarations for new RF control functions
extern void rf_start_transmission(void);
extern void rf_stop_transmission(void);
void transmission_complete_callback(void);
void dac_cleanup(void);

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

// I/Q Modulation for ADL5375 with single DAC (dsPIC33CK64MC105)
// RF modules managed by rf_interface.c

// BPSK modulation values for Q channel (sin component)
const uint16_t q_channel_plus_1_1_rad = 2048 + 1823;   // +1.1 rad: sin(1.1) * 2047 ≈ 1823
const uint16_t q_channel_minus_1_1_rad = 2048 - 1823;  // -1.1 rad: sin(-1.1) * 2047 ≈ -1823
// Augmentation de 10% : 1823 × 1.10 ≈ 2005
//const uint16_t q_channel_plus_1_1_rad = 2048 + 2005;
//const uint16_t q_channel_minus_1_1_rad = 2048 - 2005;
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
    
    // Configure PLL for 100 MHz operation (FCY = 100 MHz) - Based on Example 9-2
    // FRC = 8 MHz, Target FCY = 100 MHz → FOSC = 200 MHz  
    // FOSC = FIN × M / (N1 × N2 × N3) = 8 × 125 / (1 × 5 × 1) = 200 MHz FOSC
    CLKDIVbits.PLLPRE = 1;     // N1 = 1 (Input divider)
    PLLFBD = 125;              // M = 125 (Feedback divider)
    PLLDIVbits.POST1DIV = 5;   // N2 = 5 (Post divider 1)
    PLLDIVbits.POST2DIV = 1;   // N3 = 1 (Post divider 2)

    // Activate FRCPLL (NOSC=0b001)
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    
    // Wait for Clock switch to occur
    while(OSCCONbits.OSWEN != 0);
    
    // Debug: Check current oscillator source
    DEBUG_LOG_FLUSH("COSC=");
    debug_print_hex(OSCCONbits.COSC);
    DEBUG_LOG_FLUSH(" NOSC=");
    debug_print_hex(OSCCONbits.NOSC);
    DEBUG_LOG_FLUSH("\r\n");
    
    // Wait for PLL to lock
    while(OSCCONbits.LOCK != 1);
    
    // Debug: Confirm PLL lock
    DEBUG_LOG_FLUSH("PLL LOCK=");
    debug_print_hex(OSCCONbits.LOCK);
    DEBUG_LOG_FLUSH("\r\n");
    
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
    TRISBbits.TRISB10 = 0;    // Amplifier enable (output)
    LATBbits.LATB10 = 0;      // Initially disabled
    TRISBbits.TRISB11 = 0;    // Power level select (output)
    LATBbits.LATB11 = 0;      // Initial low power mode
    
    // Transmission indicator LED (RD10)
	ANSELDbits.ANSELD10 = 0;  // Digital mode
    TRISDbits.TRISD10 = 0;    // Output
    LATDbits.LATD10 = 0;      // Initially off
    
    // Reset button (RD13)
    ANSELDbits.ANSELD13 = 0;  // Digital mode  
    TRISDbits.TRISD13 = 1;    // Input
    CNPUDbits.CNPUD13 = 1;    // Enable pull-up
    
    // Configuration switches
    TRISBbits.TRISB2 = 1;    // Frame type select (input)
    CNPDBbits.CNPDB2 = 1;    // Enable pull-down
    TRISBbits.TRISB1 = 1;    // Frequency select (input)
    CNPDBbits.CNPDB1 = 1;    // Enable pull-down
    
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
    DAC1CONH = 0x0000;  // Clear high register
    
    // Configure low register with FILTER ENABLED
    DAC1CONL = 0x8700;  // Valeur de base
    DAC1CONLbits.FLTREN = 1;  // ACTIVATION FILTRE NUMÉRIQUE
    
    
    // Timing configuration
    DACCTRL1L = 0x0040;      
    DACCTRL2H = 0x008A;      
    DACCTRL2L = 0x0055;      
    
    // Enable DAC
    DAC1CONLbits.DACEN = 1;   
    DAC1CONLbits.DACOEN = 1;  
    
    // Initial value (0.5V ADL5375 bias)
    DAC1DATH = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
    
    // Final activation
    DACCTRL1Lbits.DACON = 1;  
    
    DEBUG_LOG_FLUSH("DAC initialized with digital filter (FLTREN=1)\r\n");
}

// =============================
// Timer1 Initialization
// =============================
void init_timer1(void) {
    T1CON = 0;        // Clear configuration
    TMR1 = 0;         // Reset timer counter
    
    // Calculate period for 6400 Hz sample rate
    PR1 = (FCY / ACTUAL_SAMPLE_RATE_HZ) - 1;
    
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
// Transmission Complete Callback
// =============================
void transmission_complete_callback(void) {
    // 1. FIRST: Clean up DAC progressively  
    dac_cleanup();
    
    // 2. THEN: Allow DAC to stabilize
    __delay_ms(2);
    
    // 3. FINALLY: Disable RF chain
    rf_stop_transmission();
    DEBUG_LOG_FLUSH("RF transmission complete - carrier disabled\r\n");
}

// =============================
// DAC Cleanup Function
// =============================
void dac_cleanup(void) {
    // Transition progressive vers 0.5V ADL5375 bias sur 2ms
    uint16_t current_value = DAC1DATH;
    uint16_t target_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
    
    for (int i = 0; i < 20; i++) {
        DAC1DATH = current_value + ((target_value - current_value) * i) / 20;
        __delay_us(100);  // 100μs × 20 = 2ms
    }
    DAC1DATH = target_value;
}

// =============================
// Unified ISR for Envelope and Modulation
// =============================
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    static uint8_t pin_state = 0;
    // Phase continuity handled in Biphase-L encoding below
    
    // Toggle debug pin (RB0) only during transmission phases
    if (tx_phase == DATA_PHASE || tx_phase == PRE_AMPLI_RAMP_UP || tx_phase == POST_AMPLI_RAMP_DOWN || tx_phase == PREAMBLE_PHASE || tx_phase == POSTAMBLE_PHASE) {
        LATBbits.LATB0 = pin_state = !pin_state;
    }
    
    // Timer1 = 6400 Hz, 1ms = 6.4 samples exact
    static uint16_t sample_accumulator = 0;
    sample_accumulator += 1000;
    if (sample_accumulator >= 6400) {
        millis_counter++;
        sample_accumulator -= 6400;
    }
    
    // Biphase-L modulation handling
    if (++modulation_counter >= MODULATION_INTERVAL) {
        modulation_counter = 0;
        uint16_t dac_value = DAC_OFFSET;

        switch(tx_phase) {
            // RF ramp-up phase: no modulation, fixed DC level
            case PRE_AMPLI_RAMP_UP:
                dac_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);  // 500mV bias level (quiet state)
                tx_phase = PREAMBLE_PHASE;  // Immediate transition to preamble
                sample_count = 0;  // Reset counter for preamble
                break;
                
            // RF ramp-down phase: no modulation
            case POST_AMPLI_RAMP_DOWN:
                dac_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);  // 500mV bias level (quiet state)
                break;
                
            // Unmodulated carrier phase  
            case PREAMBLE_PHASE:
                dac_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
                
                // Check if preamble duration completed
                if (++sample_count >= PREAMBLE_SAMPLES) {
                    sample_count = 0;
                    tx_phase = DATA_PHASE;
                }
                break;

            // Data transmission phase - normal modulation
            case DATA_PHASE:
                if (bit_index < MESSAGE_BITS) {
                    uint8_t current_bit = beacon_frame[bit_index];
                    
                    dac_value = signal_processor_get_biphase_l_value(current_bit, sample_count, OVERSAMPLING);                    
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
                dac_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
                if (++sample_count >= POSTAMBLE_SAMPLES) {
                    tx_phase = POST_AMPLI_RAMP_DOWN;
                    current_ramp_count = 0;
                    sample_count = 0;
                }
                break;
                
            // System idle state
            case IDLE_STATE:
                dac_value = (uint16_t)((0.5f * (float)DAC_RESOLUTION) / VOLTAGE_REF_3V3);
                break;
        }
        
        // Update DAC output register
        DAC1DATH = dac_value & 0x0FFF;
        
        // Transmission completion detection
        static tx_phase_t previous_phase = IDLE_STATE;
        if (tx_phase == IDLE_STATE && previous_phase != IDLE_STATE) {
            transmission_complete_flag = 1;
            transmission_complete_callback();
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
                sample_count = 0;
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
    // Facteur d'augmentation d'amplitude (10%)
    const float amplitude_scale = 1.00f;
    // ADL5375-05: 500mV bias ± 500mV swing = 0-1V range
    // Q channel modulation: bias + sin(±1.1 rad) * swing/2
    
    float q_voltage = (float)ADL5375_BIAS_MV / 1000.0f;  // 0.5V bias
    
    // Add BPSK modulation based on phase shift
    if (phase_shift >= 0) {
        q_voltage += (sinf(PHASE_SHIFT_RADIANS) * (float)ADL5375_SWING_MV / 2000.0f) * amplitude_scale;  // +modulation
    } else {
        q_voltage += (sinf(-PHASE_SHIFT_RADIANS) * (float)ADL5375_SWING_MV / 2000.0f) * amplitude_scale; // -modulation  
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
    rf_start_transmission();        // Activate complete RF chain (ADF4351 + ADL5375-05 + PA)
    __delay_ms(5);                  // Stabilization delay of carrier
    control_rf_amplifier(1);       // Enable RF amplifier
    LED_TX_PIN = 0;                // Turn on transmission LED
    tx_phase = PRE_AMPLI_RAMP_UP;  // Start transmission sequence
}

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
    signal_processor_init();   // Initialize modulation LUT
    
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
