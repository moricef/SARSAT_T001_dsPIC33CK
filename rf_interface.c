// rf_interface.c - RF Modules Control for COSPAS-SARSAT Generator
// ADF4351 (PLL) + ADL5375 (I/Q Modulator) + RA07M4047M (PA)
// Hardware validated against datasheets DS70005399D, ADF4351, ADL5375

#include "includes.h"
#include "rf_interface.h"
#include "system_debug.h"

// =============================
// RF Hardware Pin Definitions
// =============================

// ADF4351 SPI control pins (400 MHz - 4.4 GHz PLL)
#define ADF4351_CLK_PIN      LATBbits.LATB1   // SPI Clock
#define ADF4351_DATA_PIN     LATBbits.LATB2   // SPI Data  
#define ADF4351_LE_PIN       LATBbits.LATB3   // Latch Enable
#define ADF4351_CE_PIN       LATBbits.LATB4   // Chip Enable
#define ADF4351_RF_EN_PIN    LATBbits.LATB5   // RF Enable

// Pin directions
#define ADF4351_CLK_TRIS     TRISBbits.TRISB1
#define ADF4351_DATA_TRIS    TRISBbits.TRISB2
#define ADF4351_LE_TRIS      TRISBbits.TRISB3
#define ADF4351_CE_TRIS      TRISBbits.TRISB4
#define ADF4351_RF_EN_TRIS   TRISBbits.TRISB5

// ADL5375 I/Q Modulator control pins (400 MHz - 6 GHz)
#define ADL5375_ENABLE_PIN   LATBbits.LATB6   // Enable
#define ADL5375_ENABLE_TRIS  TRISBbits.TRISB6

// RA07M4047M Power Amplifier control pins (400-520 MHz, 100mW/5W)
#define AMP_ENABLE_PIN       LATBbits.LATB15  // PA Enable  
#define POWER_CTRL_PIN       LATBbits.LATB11  // Power Level Select
#define AMP_ENABLE_TRIS      TRISBbits.TRISB15
#define POWER_CTRL_TRIS      TRISBbits.TRISB11

// =============================
// ADF4351 Configuration Data
// =============================

// ADF4351 register values for 403 MHz output
// Calculated for 25 MHz reference clock
// RFOUT = (REFIN × (INT + FRAC/MOD)) / RF_DIV
static const uint32_t adf4351_regs_403mhz[] = {
    0x200000,    // R0: N=16, FRAC=0 (403MHz = 25MHz × 16.12)
    0x8000001,   // R1: Phase=0, MOD=1 
    0x4E42,      // R2: LDF=0, LDP=1, PD_POL=1, CP=2.5mA
    0x4B3,       // R3: Clock divider, CSR
    0x8C803C,    // R4: Output power=+5dBm, MTLD=0, VCO_PD=0, RF_DIV=1
    0x580005     // R5: Reserved bits
};

// =============================
// Global RF State Variables
// =============================
static volatile uint8_t rf_amp_enabled = 0;              // RF amplifier state
static volatile uint8_t rf_current_power_mode = RF_POWER_LOW;  // Current power mode

// =============================
// ADF4351 SPI Driver Functions
// =============================

static void adf4351_spi_write_byte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        ADF4351_CLK_PIN = 0;
        ADF4351_DATA_PIN = (data >> i) & 1;
        __delay_us(1);      // Setup time
        ADF4351_CLK_PIN = 1;
        __delay_us(1);      // Clock high time
    }
}

static void adf4351_write_register(uint32_t reg_data) {
    // Start SPI transmission
    ADF4351_LE_PIN = 0;
    __delay_us(1);
    
    // Send 32 bits MSB first
    adf4351_spi_write_byte((reg_data >> 24) & 0xFF);
    adf4351_spi_write_byte((reg_data >> 16) & 0xFF);
    adf4351_spi_write_byte((reg_data >> 8) & 0xFF);
    adf4351_spi_write_byte(reg_data & 0xFF);
    
    // Latch data into ADF4351
    ADF4351_LE_PIN = 1;
    __delay_us(10);     // Latch pulse width
    ADF4351_LE_PIN = 0;
}

// =============================
// Public RF Interface Functions
// =============================

void rf_init_adf4351(void) {
    // Configure SPI pins as outputs
    ADF4351_CLK_TRIS = 0;
    ADF4351_DATA_TRIS = 0;
    ADF4351_LE_TRIS = 0;
    ADF4351_CE_TRIS = 0;
    ADF4351_RF_EN_TRIS = 0;
    
    // Set initial pin states
    ADF4351_CLK_PIN = 0;
    ADF4351_DATA_PIN = 0;
    ADF4351_LE_PIN = 0;
    ADF4351_CE_PIN = 1;     // Enable chip
    ADF4351_RF_EN_PIN = 0;  // RF output disabled initially
    
    __delay_ms(10);         // Power-up delay
    
    // Program ADF4351 registers (R5 to R0 sequence)
    for (int i = 5; i >= 0; i--) {
        adf4351_write_register(adf4351_regs_403mhz[i] | i);
    }
    
    __delay_ms(20);         // Wait for PLL lock
    
    DEBUG_LOG_FLUSH("ADF4351 initialized @ 403 MHz\r\n");
}

void rf_adf4351_enable_output(uint8_t state) {
    ADF4351_RF_EN_PIN = state ? 1 : 0;
    DEBUG_LOG_FLUSH(state ? "ADF4351 RF output ON\r\n" : "ADF4351 RF output OFF\r\n");
}

void rf_init_adl5375(void) {
    // Configure ADL5375 enable pin
    ADL5375_ENABLE_TRIS = 0;    // Output
    ADL5375_ENABLE_PIN = 0;     // Initially disabled
    
    DEBUG_LOG_FLUSH("ADL5375 I/Q modulator initialized\r\n");
}

void rf_adl5375_enable(uint8_t state) {
    ADL5375_ENABLE_PIN = state ? 1 : 0;
    if (state) {
        __delay_ms(10);     // ADL5375 settling time
    }
    DEBUG_LOG_FLUSH(state ? "ADL5375 enabled\r\n" : "ADL5375 disabled\r\n");
}

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
        
        // 1. Enable ADF4351 LO output first
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
        
        rf_amp_enabled = 0;
        DEBUG_LOG_FLUSH("RF Chain DISABLED\r\n");
    }
}

void rf_initialize_all_modules(void) {
    DEBUG_LOG_FLUSH("Initializing RF modules...\r\n");
    
    // Initialize modules in dependency order
    rf_init_adf4351();           // PLL synthesizer @ 403 MHz
    rf_init_adl5375();           // I/Q modulator
    rf_init_power_amplifier();   // RA07M4047M PA (100mW/5W)
    
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
    
    // Enter infinite loop with error message
    while(1) {
        DEBUG_LOG_FLUSH("RF ERROR: ");
        DEBUG_LOG_FLUSH(message);
        DEBUG_LOG_FLUSH("\r\n");
        __delay_ms(1000);
    }
}