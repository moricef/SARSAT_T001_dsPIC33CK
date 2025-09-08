// rf_interface.c - RF Modules Control for COSPAS-SARSAT Generator
// ADF4351 (PLL) + ADL5375 (I/Q Modulator) + RA07M4047M (PA)

#include "includes.h"
#include "rf_interface.h"
#include "system_debug.h"

// Build timestamp for this specific file
const char rf_build_time[] = __TIME__;

// =============================
// Global RF State Variables
// =============================
static volatile uint8_t rf_amp_enabled = 0;              // RF amplifier state
static volatile uint8_t rf_current_power_mode = RF_POWER_LOW;  // Current power mode

// =============================
// ADF4351 Configuration Data
// =============================

// ADF4351 register values for 403 MHz output with 25 MHz reference
// INT = 128, FRAC = 96, MOD = 100 → 25 MHz × (128 + 96/100) = 3224 MHz / 8 = 403 MHz
const uint32_t adf4351_regs_403mhz[] = {
    0x00580005,  // R5
    0x00BC803C,  // R4  
    0x000004B3,  // R3
    0x18004E42,  // R2
    0x080087D1,  // R1
    0x00400798   // R0
};

// =============================
// ADF4351 Hardware SPI Driver Functions
// =============================

static void adf4351_init_hardware_spi(void) {
    // Configure pins as digital
    ANSELB = 0x0000;
    ANSELC = 0x0000;
    ANSELD = 0x0000;
    
    // Pin configuration
    TRISCbits.TRISC0 = 0;   // SDO output
    TRISCbits.TRISC2 = 0;   // SCK output
    TRISCbits.TRISC3 = 0;   // LE output
    TRISBbits.TRISB15 = 0;  // LED output
    TRISBbits.TRISB14 = 0;  // Lock LED output
    
    // PPS Configuration
    __builtin_write_RPCON(0x0000);
    RPOR8bits.RP48R = 5;   // SDO1 on RC0
    RPOR9bits.RP50R = 6;   // SCK1 on RC2
    __builtin_write_RPCON(0x0800);
    
    // SPI Configuration
    SPI1CON1L = 0x0020;     // MSTEN=1 only
    SPI1CON1Lbits.CKE = 1;  // CPHA=0 for ADF4351
    SPI1CON1Lbits.CKP = 0;  // CPOL=0
    SPI1CON1H = 0x0000;
    SPI1CON2L = 0x0007;     // 8-bit mode
    SPI1BRGL = 24;           // 1MHz 
    
    // Enable SPI
    SPI1CON1Lbits.SPIEN = 1;
}

void adf4351_write_register(uint32_t reg_data) {
    LATCbits.LATC3 = 0;  // LE low
    __delay_us(2);
    
    // Send 32 bits as 4 bytes (MSB first)
    SPI1BUFL = (reg_data >> 24) & 0xFF;
    while(!SPI1STATLbits.SPIRBF);
    (void)SPI1BUFL;
    
    SPI1BUFL = (reg_data >> 16) & 0xFF;
    while(!SPI1STATLbits.SPIRBF);
    (void)SPI1BUFL;
    
    SPI1BUFL = (reg_data >> 8) & 0xFF;
    while(!SPI1STATLbits.SPIRBF);
    (void)SPI1BUFL;
    
    SPI1BUFL = reg_data & 0xFF;
    while(!SPI1STATLbits.SPIRBF);
    (void)SPI1BUFL;
    
    LATCbits.LATC3 = 1;  // LE high to latch
    __delay_us(20);
}

// =============================
// ADF4351 Lock Detection with Timeout
// =============================

#define ADF4351_LOCK_TIMEOUT_MS 1000    // Maximum time to wait for lock (1 second)
#define ADF4351_LOCK_CHECK_INTERVAL_MS 10 // Check every 10ms
#define ADF4351_LOCK_RETRY_COUNT 3      // Number of lock verification attempts

uint8_t adf4351_verify_lock_status(void) {
    // Read Lock Detect pin multiple times to confirm stable lock
    for (int i = 0; i < ADF4351_LOCK_RETRY_COUNT; i++) {
        if (!ADF4351_LD_PIN) {
            return 0; // Lock not detected
        }
        __delay_ms(2); // Short delay between readings
    }
    return 1; // Stable lock detected
}

static uint8_t adf4351_wait_for_lock(void) {
    int timeout_ms = ADF4351_LOCK_TIMEOUT_MS;
    
    DEBUG_LOG_FLUSH("Waiting for PLL lock");
    
    while (timeout_ms > 0) {
        if (adf4351_verify_lock_status()) {
            DEBUG_LOG_FLUSH(" - LOCKED\r\n");
            return 1; // Lock detected and verified
        }
        
        __delay_ms(ADF4351_LOCK_CHECK_INTERVAL_MS);
        timeout_ms -= ADF4351_LOCK_CHECK_INTERVAL_MS;
        
        // Visual feedback for waiting
        if (timeout_ms % 100 == 0) {
            DEBUG_LOG_FLUSH(".");
        }
    }
    
    DEBUG_LOG_FLUSH(" - TIMEOUT\r\n");
    return 0; // Lock not detected within timeout
}

void rf_init_adf4351(void) {
    DEBUG_LOG_FLUSH("ADF4351 INIT START\r\n");
    
    // Initialize hardware SPI
    adf4351_init_hardware_spi();
    
    // Configure remaining ADF4351 pins
    ADF4351_CE_TRIS = 0;
    ADF4351_RF_EN_TRIS = 0;
    ADF4351_LD_TRIS = 1;  // Lock Detect as input
    CNPDCbits.CNPDC1 = 1;  // Enable pull-down on RC1
    ADF4351_LOCK_LED_TRIS = 0;  // Lock LED as output
    
    // Set initial pin states
    LATCbits.LATC3 = 1;     // LE high (inactive)
    ADF4351_CE_PIN = 1;     // Enable chip
    ADF4351_RF_EN_PIN = 0;  // RF output disabled initially
    ADF4351_LOCK_LED_PIN = 0;  // Lock LED off initially
    
    __delay_ms(10);         // Power-up delay
    
    // Program ADF4351 registers (R5 to R0)
    DEBUG_LOG_FLUSH("Programming ADF4351 registers...\r\n");
    for(int i = 0; i < 6; i++) {
        adf4351_write_register(adf4351_regs_403mhz[i]);
        __delay_ms(2);
    }
    
    // Wait for PLL lock with retry mechanism
    uint8_t lock_attempts = 0;
    const uint8_t max_attempts = 3;
    
    while (lock_attempts < max_attempts) {
        lock_attempts++;
        DEBUG_LOG_FLUSH("PLL lock attempt ");
        debug_print_uint16(lock_attempts);
        DEBUG_LOG_FLUSH("/");
        debug_print_uint16(max_attempts);
        DEBUG_LOG_FLUSH("\r\n");
        
        if (adf4351_wait_for_lock()) {
            DEBUG_LOG_FLUSH("ADF4351 initialized successfully at 403.040 MHz\r\n");
            return; // Success - exit function
        } else {
            DEBUG_LOG_FLUSH("PLL lock attempt failed\r\n");
            if (lock_attempts < max_attempts) {
                DEBUG_LOG_FLUSH("Reprogramming registers...\r\n");
                // Reprogram all registers
                for(int i = 0; i < 6; i++) {
                    adf4351_write_register(adf4351_regs_403mhz[i]);
                    __delay_ms(2);
                }
                __delay_ms(50); // Extra settling time
            }
        }
    }
    
    // System halt if all attempts failed - NO RETURN
    rf_system_halt("ADF4351 PLL LOCK FAILED AFTER 3 ATTEMPTS");
}

void rf_adf4351_enable_output(uint8_t state) {
    ADF4351_RF_EN_PIN = state ? 1 : 0;
    DEBUG_LOG_FLUSH(state ? "ADF4351 RF output ON\r\n" : "ADF4351 RF output OFF\r\n");
}

//============================
// ADL5375 RF MOdule
//============================
void rf_init_adl5375(void) {
    // Configure ADL5375 enable pin
    ADL5375_ENABLE_TRIS = 0;    // Output
    ADL5375_ENABLE_PIN = 0;     // Initially disabled
    
    DEBUG_LOG_FLUSH("ADL5375 I/Q modulator initialized\r\n");
}

// =============================
// ADF4351 Chip Enable Control (RC9)
// =============================

void rf_adf4351_enable_chip(uint8_t state) {
    ADF4351_CE_PIN = state ? 1 : 0;
    DEBUG_LOG_FLUSH(state ? "ADF4351 chip ENABLED\r\n" : "ADF4351 chip DISABLED\r\n");
    
    if (state) {
        __delay_ms(10); // Stabilization time after activation
    }
}

uint8_t rf_adf4351_wait_for_lock_with_timeout(void) {
    return adf4351_wait_for_lock();
}

void rf_adl5375_enable(uint8_t state) {
    ADL5375_ENABLE_PIN = state ? 1 : 0;
    if (state) {
        __delay_ms(10);     // ADL5375 settling time
    }
    DEBUG_LOG_FLUSH(state ? "ADL5375 enabled\r\n" : "ADL5375 disabled\r\n");
}

//============================
// RA07M4047M RF Power Amplifier
//============================
void rf_init_power_amplifier(void) {
    static uint8_t initialized = 0;
    if (initialized) return;
    initialized = 1;
    
    // Configure RA07M4047M control pins
    AMP_ENABLE_TRIS = 0;    // PA enable (output)
    POWER_CTRL_TRIS = 0;    // Power level select (output)
    
    // Set initial states
    AMP_ENABLE_PIN = 0;     // PA disabled
    POWER_CTRL_PIN = 0;     // Low power mode
    
    // Initialize state variables
    rf_current_power_mode = RF_POWER_LOW;
    rf_amp_enabled = 0;
    
    DEBUG_LOG_FLUSH("RA07M4047M PA initialized: Low power, OFF\r\n");
}

void rf_set_power_level(uint8_t mode) {
    // Validate input parameter
    if (mode != RF_POWER_LOW && mode != RF_POWER_HIGH) return;
    
    // Only change if different from current mode
    if (mode != rf_current_power_mode) {
        // Disable amplifier during power level transition
        if (rf_amp_enabled) {
            rf_control_amplifier_chain(0);
            __delay_us(50);
        }
        
        // Set new power level on RA07M4047M
        POWER_CTRL_PIN = (mode == RF_POWER_HIGH) ? 1 : 0;
        rf_current_power_mode = mode;
        
        // Re-enable amplifier if it was previously enabled
        if (rf_amp_enabled) {
            __delay_us(50);
            rf_control_amplifier_chain(1);
        }
    }
}

void rf_control_amplifier_chain(uint8_t state) {
    if (state) {
        // RF chain power-up sequence (proper order for stability)
        
        // 0. Enable ADF4351 chip first (RC9)
        rf_adf4351_enable_chip(1);
        __delay_ms(5); // Short delay after CE activation
        
        // 1. Wait for PLL lock before continuing
        if (!rf_adf4351_wait_for_lock_with_timeout()) {
            DEBUG_LOG_FLUSH("WARNING: PLL not locked, continuing anyway\r\n");
        }
                
        // 1. Enable ADF4351 LO output
        rf_adf4351_enable_output(1);
        __delay_ms(10);             // Wait for LO frequency stability
        
        // 2. Enable ADL5375 I/Q modulator
        rf_adl5375_enable(1);
        __delay_ms(5);              // ADL5375 enable settling time
        
        // 3. Enable RA07M4047M power amplifier (last in chain)
        AMP_ENABLE_PIN = 1;
        __delay_us(500);            // PA enable settling time
        rf_amp_enabled = 1;
        
        // Debug confirmation
        DEBUG_LOG_FLUSH("RF Chain ENABLED (");
        DEBUG_LOG_FLUSH((rf_current_power_mode == RF_POWER_HIGH) ? "HIGH" : "LOW");
        DEBUG_LOG_FLUSH(" power, 403 MHz)\r\n");
        
    } else {
        // RF chain power-down sequence (reverse order for safety)
        
        // 1. Disable power amplifier first (prevent overdrive)
        AMP_ENABLE_PIN = 0;
        __delay_us(100);
        
        // 2. Disable I/Q modulator
        rf_adl5375_enable(0);
        
        // 3. Disable LO output (keep PLL running, just RF off)
        rf_adf4351_enable_output(0);
        
        // 4. Disable ADF4351 chip (RC9) - POWER SAVING
        rf_adf4351_enable_chip(0);
        
        rf_amp_enabled = 0;
        DEBUG_LOG_FLUSH("RF Chain DISABLED\r\n");
    }
}

// =============================
// Controlled Transmission Functions
// =============================

void rf_start_transmission(void) {
    DEBUG_LOG_FLUSH("Starting transmission sequence...\r\n");
    
    // Enable complete RF chain
    rf_control_amplifier_chain(1);
    
    // Add delay for complete stabilization
    __delay_ms(2);
    
    DEBUG_LOG_FLUSH("RF carrier ON - ready for modulation\r\n");
}

void rf_stop_transmission(void) {
    DEBUG_LOG_FLUSH("Stopping transmission sequence...\r\n");
    
    // Disable complete RF chain
    rf_control_amplifier_chain(0);
    
    DEBUG_LOG_FLUSH("RF carrier OFF\r\n");
}

void rf_initialize_all_modules(void) {
    DEBUG_LOG_FLUSH("*** RF INIT START ***\r\n");
    DEBUG_LOG_FLUSH("Initializing RF modules...\r\n");
    
    // Initialize modules in dependency order
    DEBUG_LOG_FLUSH("About to call rf_init_adf4351...\r\n");
    rf_init_adf4351();           // PLL synthesizer @ 403 MHz
    DEBUG_LOG_FLUSH("rf_init_adf4351 completed\r\n");
    
    DEBUG_LOG_FLUSH("About to call rf_init_adl5375...\r\n");
    rf_init_adl5375();           // I/Q modulator
    DEBUG_LOG_FLUSH("rf_init_adl5375 completed\r\n");
    
    DEBUG_LOG_FLUSH("About to call rf_init_power_amplifier...\r\n");
    rf_init_power_amplifier();   // RA07M4047M PA (100mW/5W)
    DEBUG_LOG_FLUSH("rf_init_power_amplifier completed\r\n");
    
    DEBUG_LOG_FLUSH("RF modules initialization complete\r\n");
}

// =============================
// RF Status and Debug Functions
// =============================

uint8_t rf_get_amplifier_state(void) {
    return rf_amp_enabled;
}

uint8_t rf_get_power_mode(void) {
    return rf_current_power_mode;
}

void rf_system_halt(const char* message) {
    // Disable all RF outputs for safety
    rf_control_amplifier_chain(0);
    rf_adf4351_enable_chip(0); // Ensure CE is disabled
    
    // Enter infinite loop with SOS error signal
    while(1) {
        // Check for reset button press (RD13 low = button pressed)
        if (!PORTDbits.RD13) {
            __delay_ms(50);  // Debounce
            if (!PORTDbits.RD13) {
                DEBUG_LOG_FLUSH("RESET BUTTON PRESSED - RESTARTING\r\n");
                __delay_ms(100);
                asm("reset");  // Software reset
            }
        }
        
        // Check for spontaneous PLL recovery (escape route from critical error)
        if (ADF4351_LD_PIN) {
            DEBUG_LOG_FLUSH("SPONTANEOUS PLL RECOVERY DETECTED - EXITING CRITICAL MODE\r\n");
            ADF4351_LOCK_LED_PIN = 1;  // Turn on lock LED
            return;  // Exit rf_system_halt and return to main loop
        }
        
        DEBUG_LOG_FLUSH("CRITICAL ERROR: ");
        DEBUG_LOG_FLUSH(message);
        DEBUG_LOG_FLUSH("\r\n");
        
        LATDbits.LATD10 = !LATDbits.LATD10;
        __delay_ms(150);
        if (!PORTDbits.RD13 || ADF4351_LD_PIN) continue;
    }
}
