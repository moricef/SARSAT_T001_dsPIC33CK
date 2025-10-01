#ifndef SPI2_TEST_H
#define SPI2_TEST_H

#include <stdint.h>

// =============================
// SPI2 Compatibility Test Functions
// =============================

// Initialize SPI2 for software-only testing
void spi2_test_init(void);

// Enable SPI2 with PLL monitoring
void spi2_test_enable(void);

// Perform dummy SPI2 transactions
void spi2_test_transactions(uint16_t count);

// Disable SPI2 with PLL monitoring
void spi2_test_disable(void);

// Print test results
void spi2_test_report(void);

// Complete test sequence
void run_spi2_compatibility_test(void);

#endif // SPI2_TEST_H