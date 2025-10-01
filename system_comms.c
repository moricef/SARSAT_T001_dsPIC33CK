// system_comms.c - Native SARSAT transmission - no postamble concept
#include "includes.h"
#include "system_definitions.h"
#include "system_comms.h"
#include "system_debug.h"
#include "protocol_data.h"
#include "rf_interface.h"
#include "signal_processor.h"
#include "drivers/mcp4922_driver.h"

// RF control function declarations
extern void rf_start_transmission(void);
extern void rf_stop_transmission(void);
extern void control_rf_amplifier(uint8_t enable);

// =================
// Global Variables 
// =================
volatile uint32_t millis_counter = 0;
volatile uint32_t last_tx_time = 0;
volatile uint32_t tx_interval_ms = 5000;           // 5 second default interval
volatile tx_phase_t tx_phase = IDLE_STATE;
volatile uint16_t bit_index = 0;
volatile uint16_t sample_count = 0;
volatile uint8_t beacon_frame[MESSAGE_BITS] = {0};
volatile uint8_t transmission_complete_flag = 0;

// RF timing - empirically calibrated for PLL stability
volatile uint16_t rf_startup_samples = RF_STARTUP_SAMPLES;      // 320 samples (~50ms)
volatile uint16_t rf_shutdown_samples = RF_SHUTDOWN_SAMPLES;    // 640 samples (~100ms)

// Modulation timing
volatile uint16_t modulation_counter = 0;

// Legacy variables for debug compatibility
volatile uint8_t carrier_phase = 0;
volatile float envelope_gain = 0.0f;

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
    while(OSCCONbits.OSWEN);

    // Configure PLL for 100 MHz operation (FCY = 100 MHz)
    CLKDIVbits.PLLPRE = 1;     // N1 = 1 (Input divider)
    PLLFBD = 125;              // M = 125 (Feedback divider)
    PLLDIVbits.POST1DIV = 5;   // N2 = 5 (Post divider 1)
    PLLDIVbits.POST2DIV = 1;   // N3 = 1 (Post divider 2)

    // Activate FRCPLL
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while(OSCCONbits.OSWEN != 0);

    // Wait for PLL to lock
    while(OSCCONbits.LOCK != 1);

    if(!OSCCONbits.LOCK) {
        system_halt("PLL lock failed");
    }

    DEBUG_LOG_FLUSH("System clock initialized at 100 MHz\r\n");
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
    TRISAbits.TRISA3 = 0;
    LATAbits.LATA3 = 0;
    ANSELAbits.ANSELA3 = 1;   // Analog mode for DAC

    // RF control pins
    TRISBbits.TRISB10 = 0;    // Amplifier enable
    LATBbits.LATB10 = 0;
    TRISBbits.TRISB11 = 0;    // Power level select
    LATBbits.LATB11 = 0;

    // Transmission LED (RD10)
    ANSELDbits.ANSELD10 = 0;
    TRISDbits.TRISD10 = 0;
    LATDbits.LATD10 = 0;

    // Reset button (RD13)
    ANSELDbits.ANSELD13 = 0;
    TRISDbits.TRISD13 = 1;
    CNPUDbits.CNPUD13 = 1;

    // Configuration switches
    TRISBbits.TRISB2 = 1;     // Frame type select
    CNPDBbits.CNPDB2 = 1;
    TRISBbits.TRISB1 = 1;     // Frequency select
    CNPDBbits.CNPDB1 = 1;

    // Debug pin (RB0)
    TRISBbits.TRISB0 = 0;
    ANSELBbits.ANSELB0 = 0;
    LATBbits.LATB0 = 0;

    DEBUG_LOG_FLUSH("GPIO initialized\r\n");
}

// =============================
// DAC Initialization
// =============================
void init_dac(void) {
    DAC1CONH = 0x0000;
    DAC1CONL = 0x8700;
    DAC1CONLbits.FLTREN = 1;  // Enable digital filter

    DACCTRL1L = 0x0040;
    DACCTRL2H = 0x008A;
    DACCTRL2L = 0x0055;

    DAC1CONLbits.DACEN = 1;
    DAC1CONLbits.DACOEN = 1;

    // Set initial idle level
    DAC1DATH = calculate_idle_dac_value();

    DACCTRL1Lbits.DACON = 1;

    DEBUG_LOG_FLUSH("DAC initialized\r\n");
}

// =============================
// Timer1 Initialization
// =============================
void init_timer1(void) {
    T1CON = 0;
    TMR1 = 0;

    // Calculate period for 6400 Hz sample rate
    PR1 = (FCY / SAMPLE_RATE_HZ) - 1;

    T1CONbits.TCKPS = 0;    // No prescaler
    T1CONbits.TCS = 0;      // Internal clock
    IPC0bits.T1IP = 7;      // Highest priority
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
    T1CONbits.TON = 1;

    DEBUG_LOG_FLUSH("Timer1 initialized at 6400 Hz\r\n");
}

// =============================
// Signal Processing Functions - Native Implementation
// =============================
uint16_t calculate_idle_dac_value(void) {
    // Idle state: 0V output (power saving)
    return 0;
}

uint16_t calculate_carrier_dac_value(void) {
    // Carrier state: 500mV bias for ADL5375
    return (uint16_t)((0.5f * DAC_RESOLUTION) / VOLTAGE_REF_3V3);
}

uint16_t calculate_bpsk_dac_value(uint8_t bit_value, uint16_t sample_index) {
    // Use existing signal processor for Biphase-L modulation
    return signal_processor_get_biphase_l_value(bit_value, sample_index, SAMPLES_PER_SYMBOL);
}

// =============================
// Native Transmission ISR - Clean State Machine
// =============================
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    static uint8_t debug_pin_state = 0;

    // Toggle debug pin during active transmission phases only
    if (tx_phase != IDLE_STATE) {
        LATBbits.LATB0 = debug_pin_state = !debug_pin_state;
    }

    // Update millisecond counter (6400 Hz = 6.4 samples per ms)
    static uint16_t ms_accumulator = 0;
    ms_accumulator += 1000;
    if (ms_accumulator >= SAMPLE_RATE_HZ) {
        millis_counter++;
        ms_accumulator -= SAMPLE_RATE_HZ;
    }

    // Main transmission state machine
    if (++modulation_counter >= MODULATION_INTERVAL) {
        modulation_counter = 0;
        uint16_t dac_value = calculate_idle_dac_value();

        switch(tx_phase) {
            case IDLE_STATE:
                // Idle: DAC at 0V for power saving
                dac_value = calculate_idle_dac_value();
                break;

            case RF_STARTUP:
                // RF chain stabilization
                dac_value = calculate_idle_dac_value();
                break;

            case CARRIER_TX:
                // Unmodulated carrier transmission
                dac_value = calculate_carrier_dac_value();
                envelope_gain = 1.0f;  // Full power during carrier
                if (++sample_count >= CARRIER_SAMPLES) {
                    DEBUG_LOG_FLUSH("Carrier phase complete [");
                    debug_print_uint32(millis_counter);
                    DEBUG_LOG_FLUSH("ms]\r\n");
                    tx_phase = DATA_TX;
                    sample_count = 0;
                    bit_index = 0;
                }
                break;

            case DATA_TX:
                // Modulated data transmission
                envelope_gain = 1.0f;  // Full power during data
                if (bit_index < MESSAGE_BITS) {
                    uint8_t current_bit = beacon_frame[bit_index];
                    dac_value = calculate_bpsk_dac_value(current_bit, sample_count);

                    if (++sample_count >= SAMPLES_PER_SYMBOL) {
                        sample_count = 0;
                        bit_index++;
                    }
                } else {
                    // All data transmitted - begin shutdown
                    DEBUG_LOG_FLUSH("Data transmission complete [");
                    debug_print_uint32(millis_counter);
                    DEBUG_LOG_FLUSH("ms]\r\n");
                    tx_phase = RF_SHUTDOWN;
                    sample_count = 0;
                }
                break;

            case RF_SHUTDOWN:
                if (sample_count < (rf_shutdown_samples / 2)) {
                    dac_value = calculate_carrier_dac_value();
                    envelope_gain = 1.0f;
                    sample_count++;
                } else if (sample_count < rf_shutdown_samples) {
                    uint16_t bias_dac = calculate_carrier_dac_value();
                    uint16_t step2_samples = sample_count - (rf_shutdown_samples / 2);
                    float reduction = (float)step2_samples / (rf_shutdown_samples / 2);
                    dac_value = (uint16_t)(bias_dac * (1.0f - reduction));
                    envelope_gain = 1.0f - reduction;
                    sample_count++;
                } else {
                    DEBUG_LOG_FLUSH("RF shutdown complete\r\n");
                    control_rf_amplifier(0);
                    rf_stop_transmission();
                    LED_TX_PIN = 1;
                    tx_phase = IDLE_STATE;
                    sample_count = 0;
                    transmission_complete_flag = 1;
                    envelope_gain = 0.0f;
                    dac_value = calculate_idle_dac_value();
                }
                break;
        }

        // Update DAC output
        DAC1DATH = dac_value & 0x0FFF;
    }

    // Clear interrupt flag
    IFS0bits.T1IF = 0;
}

// =============================
// RF Timing Calibration
// =============================
void calibrate_rf_timing(void) {
    // Empirical calibration based on hardware behavior
    // These values are determined through testing to prevent PLL unlock

    DEBUG_LOG_FLUSH("RF startup time: ");
    debug_print_uint16(rf_startup_samples);
    DEBUG_LOG_FLUSH(" samples (");
    debug_print_uint16((rf_startup_samples * 1000) / SAMPLE_RATE_HZ);
    DEBUG_LOG_FLUSH(" ms)\r\n");

    DEBUG_LOG_FLUSH("RF shutdown time: ");
    debug_print_uint16(rf_shutdown_samples);
    DEBUG_LOG_FLUSH(" samples (");
    debug_print_uint16((rf_shutdown_samples * 1000) / SAMPLE_RATE_HZ);
    DEBUG_LOG_FLUSH(" ms)\r\n");
}

// =============================
// Transmission Control
// =============================
void start_transmission(volatile const uint8_t* data) {
    static uint8_t first_run = 1;
    if(first_run) {
        calibrate_rf_timing();
        first_run = 0;
    }

    // Update transmission timestamp
    last_tx_time = millis_counter;

    // Copy message data
    __builtin_disable_interrupts();
    for (uint16_t i = 0; i < MESSAGE_BITS; i++) {
        beacon_frame[i] = data[i];
    }
    __builtin_enable_interrupts();

    // Reset state machine
    sample_count = 0;
    bit_index = 0;
    transmission_complete_flag = 0;

    // Start RF chain
    DEBUG_LOG_FLUSH("Starting transmission sequence\r\n");
    rf_start_transmission();
    __delay_ms(5);  // Brief RF stabilization
    LED_TX_PIN = 0;  // Turn on TX LED

    // Begin transmission state machine - prepare signal BEFORE RF activation
    tx_phase = CARRIER_TX;             // Signal DAC → 500mV first
    __delay_us(2);                    // Let DAC stabilize at 500mV

    rf_control_amplifier_chain(1);     // THEN activate RF chain with stable signal
    DEBUG_LOG_FLUSH("RF carrier ON - ready for modulation [");
    debug_print_uint32(millis_counter);
    DEBUG_LOG_FLUSH("ms]\r\n");
}

void set_tx_interval(uint32_t interval_ms) {
    __builtin_disable_interrupts();
    tx_interval_ms = interval_ms;
    __builtin_enable_interrupts();
}

// =============================
// System Initialization
// =============================
void system_init(void) {
    init_clock();
    init_gpio();
    init_dac();
    init_comm_uart();
    
    system_debug_init();
    mcp4922_init();              // Initialize MCP4922 DAC
    init_timer1();
    signal_processor_init();

    // Initialize RF modules
    rf_initialize_all_modules();

    // Initialize timing variables
    last_tx_time = 0;
    rf_startup_samples = RF_STARTUP_SAMPLES;
    rf_shutdown_samples = RF_SHUTDOWN_SAMPLES;

    // Clear message buffer
    memset((void*)beacon_frame, 0, MESSAGE_BITS);

    DEBUG_LOG_FLUSH("Native SARSAT system initialized\r\n");
}
