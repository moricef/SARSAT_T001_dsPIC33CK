#ifndef RF_INTERFACE_H
#define RF_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// =============================
// RF Hardware Pin Definitions
// =============================

// ADF4351 SPI control pins (400 MHz - 4.4 GHz PLL)
#define ADF4351_CLK_PIN      LATCbits.LATC2    // SPI Clock (hardware SPI)
#define ADF4351_DATA_PIN     LATCbits.LATC0    // SPI Data (hardware SPI)  
#define ADF4351_LE_PIN       LATCbits.LATC3    // Latch Enable
#define ADF4351_CE_PIN       LATCbits.LATC9    // Chip Enable
#define ADF4351_RF_EN_PIN    LATCbits.LATC8    // RF Enable
#define ADF4351_LD_PIN       PORTCbits.RC1     // Lock Detect (input)
#define ADF4351_LOCK_LED_PIN LATBbits.LATB15   // ADF4351 lock LED
#define PA_LED_PIN           LATBbits.LATB13   // RA07M4047M PA LED

// Pin directions
#define ADF4351_CLK_TRIS     TRISCbits.TRISC2
#define ADF4351_DATA_TRIS    TRISCbits.TRISC0
#define ADF4351_LE_TRIS      TRISCbits.TRISC3
#define ADF4351_CE_TRIS      TRISCbits.TRISC9
#define ADF4351_RF_EN_TRIS   TRISCbits.TRISC8
#define ADF4351_LD_TRIS      TRISCbits.TRISC1
#define ADF4351_LOCK_LED_TRIS TRISBbits.TRISB15
#define PA_LED_TRIS          TRISBbits.TRISB13

// ADL5375 I/Q Modulator control pins (400 MHz - 6 GHz)
#define ADL5375_ENABLE_PIN   LATBbits.LATB9   // Enable
#define ADL5375_ENABLE_TRIS  TRISBbits.TRISB9

// RA07M4047M Power Amplifier control pins (400-520 MHz, 100mW/5W)
#define AMP_ENABLE_PIN       LATBbits.LATB10  // PA Enable  
#define POWER_CTRL_PIN       LATBbits.LATB11  // Power Level Select
#define AMP_ENABLE_TRIS      TRISBbits.TRISB10
#define POWER_CTRL_TRIS      TRISBbits.TRISB11

// =============================
// RF Power Level Definitions
// =============================
#define RF_POWER_LOW  0     // 100mW mode for local tests
#define RF_POWER_HIGH 1     // 5W mode for exercises

// =============================
// ADF4351 PLL Synthesizer Functions
// =============================
void rf_init_adf4351(void);                    // Initialize ADF4351 @ 403 MHz
void rf_adf4351_enable_output(uint8_t state);  // Enable/disable RF output
uint8_t adf4351_verify_lock_status(void);      // Verify PLL lock with multiple readings
void adf4351_write_register(uint32_t reg_data); // Write 32-bit register to ADF4351
extern const uint32_t adf4351_regs_403mhz[6];   // ADF4351 register values for 403MHz

// =============================
// ADL5375 I/Q Modulator Functions
// =============================
void rf_init_adl5375(void);                    // Initialize ADL5375 I/Q modulator
void rf_adl5375_enable(uint8_t state);         // Enable/disable modulator

// =============================
// RA07M4047M Power Amplifier Functions
// =============================
void rf_init_power_amplifier(void);            // Initialize PA control
void rf_set_power_level(uint8_t mode);         // Set power level (LOW/HIGH)
void rf_control_amplifier_chain(uint8_t state);// Enable/disable complete RF chain

// =============================
// Master RF Control Functions
// =============================
void rf_initialize_all_modules(void);          // Initialize complete RF chain
uint8_t rf_get_amplifier_state(void);          // Get current amplifier state
uint8_t rf_get_power_mode(void);               // Get current power mode

// =============================
// Error Handling
// =============================
void rf_system_halt(const char* message);      // RF error handler with safe shutdown

// =============================
// Legacy compatibility (mapped to new functions)
// =============================
#define init_adf4351()                 rf_init_adf4351()
#define adf4351_enable_rf(state)       rf_adf4351_enable_output(state)
#define init_adl5375()                 rf_init_adl5375()
#define adl5375_enable(state)          rf_adl5375_enable(state)
#define init_rf_amplifier()            rf_init_power_amplifier()
#define set_rf_power_level(mode)       rf_set_power_level(mode)
#define control_rf_amplifier(state)    rf_control_amplifier_chain(state)

#endif // RF_INTERFACE_H
