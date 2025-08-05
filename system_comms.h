#ifndef SYSTEM_COMMS_H
#define SYSTEM_COMMS_H

#include <stdint.h>
#include <xc.h>
#include <inttypes.h>
// =============================
// Configuration matérielle
// =============================
#define DEBUG_UART_TX_PIN  _RB9
#define DEBUG_BAUD_RATE    115200
#define UART_BUFFER_SIZE   512
#define DAC_OFFSET         2048
#define AMP_ENABLE_PIN     LATBbits.LATB15
#define POWER_CTRL_PIN     LATBbits.LATB11
#define POWER_LOW          0
#define POWER_HIGH         1

// =============================
// Prototypes
// =============================

// Initialisation
void init_clock(void);
void init_gpio(void);
void init_dac(void);
void init_timer1(void);
void init_rf_amplifier(void);
void UART1_Initialize(void);
void init_debug_uart(void);

// RF Control
void set_rf_power_level(uint8_t mode);
void control_rf_amplifier(uint8_t state);

// UART Functions
uint8_t uart_get_line(char *buffer, uint16_t max_len);
void debug_print_char(char c);
void debug_print_int(int value);  // Type unifié
void debug_print_str(const char *str);
void debug_print_float(double value, int prec);  // Ajouté
void debug_print_int32(int32_t value);  // Ajouté
void debug_print_uint32(uint32_t value); 

// Timer Related
void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt(void);

// // Déclaration des fonctions de debug
void debug_print_int(int value);      // Déclaration pour cohérence
void debug_print_str(const char *str);
void debug_print_hex(uint8_t value);  // Uniformiser sur uint8_t
void debug_print_char(char c);
void debug_print_float(double f, int decimals);
void debug_print_hex64(uint64_t value);
void debug_print_hex32(uint32_t value);

// =============================
// Variables globales partagées
// =============================
extern volatile char rxQueue[UART_BUFFER_SIZE];
extern volatile uint16_t rxHead;
extern volatile uint16_t rxTail;
extern volatile uint8_t rxOverflowed;
extern volatile uint32_t millis_counter;
extern volatile uint8_t tx_phase;
extern volatile uint32_t last_tx_time;
extern volatile uint32_t tx_interval_ms;
extern volatile uint8_t carrier_phase;
extern volatile uint32_t phase_sample_count;
extern volatile uint16_t bit_index;


#endif