// spi2_test.c - Test compatibilité logicielle SPI2 sans matériel
// Test d'impact sur ADF4351 PLL LOCK

#include "includes.h"
#include "rf_interface.h"
#include "system_debug.h"

// Variables de test
static volatile uint8_t spi2_test_active = 0;
static volatile uint32_t pll_lock_checks = 0;
static volatile uint32_t pll_lock_failures = 0;

// Déclarations externes pour fonctions debug
extern void debug_print_uint16(uint16_t value);
extern void debug_print_uint32(uint32_t value);
extern void debug_print_char(char c);

// =============================
// Configuration SPI2 pour test logiciel uniquement
// =============================
void spi2_test_init(void) {
    DEBUG_LOG_FLUSH("SPI2 Test Init with PPS (RB7-9)...\r\n");

    TRISBbits.TRISB7 = 0;   // SCK2 - RP39
    TRISBbits.TRISB8 = 0;   // SDO2 - RP40
    TRISBbits.TRISB9 = 0;   // CS - RP41

    LATBbits.LATB7 = 0;     // SCK2 low
    LATBbits.LATB8 = 0;     // SDO2 low
    LATBbits.LATB9 = 1;     // CS high

    // Configuration PPS pour SPI2
    __builtin_write_RPCON(0x0000);     // Unlock PPS
    RPOR3bits.RP39R = 8;               // SCK2 sur RB7/RP39
    RPOR4bits.RP40R = 7;               // SDO2 sur RB8/RP40
    __builtin_write_RPCON(0x0800);     // Lock PPS

    // Configuration SPI2 (Mode 0, 16-bit pour MCP4922)
    SPI2CON1L = 0x0000;     // Reset first
    SPI2CON1Lbits.MSTEN = 1;  // Master mode
    SPI2CON1Lbits.CKE = 1;    // CPHA=0 for MCP4922
    SPI2CON1Lbits.CKP = 0;    // CPOL=0
    SPI2CON1Lbits.MODE16 = 1; // 16-bit for MCP4922
    SPI2CON1H = 0x0000;
    SPI2CON2L = 0x000F;       // 16-bit mode
    SPI2BRGL = 24;            // Same as SPI1 (1MHz) for test


    DEBUG_LOG_FLUSH("SPI2 configured with PPS routing\r\n");

    // Verify PPS configuration
    DEBUG_LOG_FLUSH("PPS Check - RP39R: ");
    debug_print_uint16(RPOR3bits.RP39R);
    DEBUG_LOG_FLUSH(" (should be 8 for SCK2)\r\n");
    DEBUG_LOG_FLUSH("PPS Check - RP40R: ");
    debug_print_uint16(RPOR4bits.RP40R);
    DEBUG_LOG_FLUSH(" (should be 7 for SDO2)\r\n");
}

void spi2_test_enable(void) {
    DEBUG_LOG_FLUSH("Enabling SPI2 (software test)...\r\n");

    // Check PLL LOCK before
    if (!adf4351_verify_lock_status()) {
        DEBUG_LOG_FLUSH("WARNING: PLL not locked before SPI2 enable\r\n");
        pll_lock_failures++;
    }

    // Enable SPI2
    SPI2CON1Lbits.SPIEN = 1;
    spi2_test_active = 1;

    __delay_us(10);

    // Check PLL LOCK after
    pll_lock_checks++;
    if (!adf4351_verify_lock_status()) {
        DEBUG_LOG_FLUSH("ERROR: PLL LOST LOCK after SPI2 enable!\r\n");
        pll_lock_failures++;
    } else {
        DEBUG_LOG_FLUSH("OK: PLL still locked after SPI2 enable\r\n");
    }
}

void spi2_test_transactions(uint16_t count) {
    if (!spi2_test_active) {
        DEBUG_LOG_FLUSH("SPI2 not active for test\r\n");
        return;
    }

    DEBUG_LOG_FLUSH("SPI2 Test: ");
    debug_print_uint16(count);
    DEBUG_LOG_FLUSH(" dummy transactions...\r\n");

    for (uint16_t i = 0; i < count; i++) {
        // Check PLL every 10 transactions
        if (i % 10 == 0) {
            pll_lock_checks++;
            if (!adf4351_verify_lock_status()) {
                DEBUG_LOG_FLUSH("ERROR: PLL lost during SPI2 transaction ");
                debug_print_uint16(i);
                DEBUG_LOG_FLUSH("\r\n");
                pll_lock_failures++;
                break;
            }
        }

        LATBbits.LATB9 = 0;  // CS low
        __delay_us(1);

        // Dummy 16-bit transaction (MCP4922 format)
        uint16_t dummy_data = 0x3000 | (i & 0x0FFF);  // Channel A + 12-bit data
        SPI2BUFL = dummy_data;
        while(!SPI2STATLbits.SPIRBF);
        (void)SPI2BUFL;  // Clear receive buffer

        LATBbits.LATB9 = 1;  // CS high
        __delay_us(1);

        // Small delay between transactions
        __delay_us(5);
    }

    DEBUG_LOG_FLUSH("SPI2 test transactions completed\r\n");
}

void spi2_test_disable(void) {
    if (!spi2_test_active) return;

    DEBUG_LOG_FLUSH("Disabling SPI2...\r\n");

    // Check PLL before disable
    pll_lock_checks++;
    if (!adf4351_verify_lock_status()) {
        DEBUG_LOG_FLUSH("WARNING: PLL not locked before SPI2 disable\r\n");
        pll_lock_failures++;
    }

    // Disable SPI2
    SPI2CON1Lbits.SPIEN = 0;
    spi2_test_active = 0;

    __delay_us(10);

    // Check PLL after disable
    pll_lock_checks++;
    if (!adf4351_verify_lock_status()) {
        DEBUG_LOG_FLUSH("ERROR: PLL still not locked after SPI2 disable\r\n");
        pll_lock_failures++;
    } else {
        DEBUG_LOG_FLUSH("OK: PLL locked after SPI2 disable\r\n");
    }
}

void spi2_test_report(void) {
    DEBUG_LOG_FLUSH("\r\n=== SPI2 Compatibility Test Report ===\r\n");
    DEBUG_LOG_FLUSH("PLL Lock checks: ");
    debug_print_uint32(pll_lock_checks);
    DEBUG_LOG_FLUSH("\r\nPLL Lock failures: ");
    debug_print_uint32(pll_lock_failures);
    DEBUG_LOG_FLUSH("\r\nSuccess rate: ");

    if (pll_lock_checks > 0) {
        uint32_t success_rate = ((pll_lock_checks - pll_lock_failures) * 100) / pll_lock_checks;
        debug_print_uint32(success_rate);
        DEBUG_LOG_FLUSH("%\r\n");

        if (pll_lock_failures == 0) {
            DEBUG_LOG_FLUSH("RESULT: SPI2 is COMPATIBLE (no PLL impact)\r\n");
        } else if (pll_lock_failures < (pll_lock_checks / 10)) {
            DEBUG_LOG_FLUSH("RESULT: SPI2 has MINOR impact on PLL\r\n");
        } else {
            DEBUG_LOG_FLUSH("RESULT: SPI2 has MAJOR impact on PLL - NOT RECOMMENDED\r\n");
        }
    }
    DEBUG_LOG_FLUSH("=====================================\r\n\r\n");
}

// =============================
// Séquence de test complète
// =============================
void run_spi2_compatibility_test(void) {
    DEBUG_LOG_FLUSH("\r\n*** Starting SPI2 Compatibility Test ***\r\n");

    // Reset counters
    pll_lock_checks = 0;
    pll_lock_failures = 0;

    // Test 1: Configuration
    spi2_test_init();
    __delay_ms(100);

    // Test 2: Enable/Disable cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        DEBUG_LOG_FLUSH("Test cycle ");
        debug_print_uint16(cycle + 1);
        DEBUG_LOG_FLUSH("/5\r\n");

        spi2_test_enable();
        __delay_ms(50);

        spi2_test_transactions(50);  // 50 dummy transactions
        __delay_ms(50);

        spi2_test_disable();
        __delay_ms(100);
    }

    // Test 3: Stress test - continuous operation
    DEBUG_LOG_FLUSH("Stress test: continuous SPI2 operation...\r\n");
    spi2_test_enable();
    spi2_test_transactions(500);  // 500 transactions
    spi2_test_disable();

    // Report final results
    spi2_test_report();

    DEBUG_LOG_FLUSH("*** SPI2 Compatibility Test Complete ***\r\n\r\n");
}