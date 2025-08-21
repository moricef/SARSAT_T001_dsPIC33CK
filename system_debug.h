/* system_debug.h */
#ifndef SYSTEM_DEBUG_H
#define	SYSTEM_DEBUG_H

#include "system_definitions.h"
#include "system_comms.h"

// =============================
// Configuration materielle
// =============================
#define DEBUG_UART_TX_PIN  _RC10
#define DEBUG_UART_RX_PIN _RC11
#define UART1_BAUD_RATE    9600
#define DEBUG_BAUD_RATE    115200
// Tampons debogages
#define DEBUG_BUF_SIZE     256
#define UART_BUFFER_SIZE   128
#define ISR_LOG_BUF_SIZE 2048  // Taille spécifique pour les logs ISR

// =============================
// Macros pour les logs
// =============================
// Macro pour logs avec flush automatique
#define DEBUG_LOG_FLUSH(str) do { \
    debug_print_str(str); \
    debug_full_flush(); \
} while(0)

// Macro pour logs ISR
#define ISR_LOG_PHASE(phase, env, dac) do { \
    isr_log_push_char('P'); \
    isr_log_push_char(':'); \
    isr_log_push_hex_nibble(phase); \
    isr_log_push_char(' '); \
    isr_log_push_char('E'); \
    isr_log_push_char(':'); \
    isr_log_push_uint16((uint16_t)(env * 100)); \
    isr_log_push_char(' '); \
    isr_log_push_char('D'); \
    isr_log_push_char(':'); \
    isr_log_push_uint16(dac); \
    isr_log_push_str("\r\n"); \
} while(0)
	
// =============================
// Structure debug_flags_t
// =============================

// Définition des modes de log
typedef enum {
    LOG_MODE_NONE = 0,
    LOG_MODE_ISR,
    LOG_MODE_SYSTEM,
    LOG_MODE_ALL
} log_mode_t;

extern volatile debug_flags_t debug_flags;

// =============================
// Prototypes
// =============================
void system_debug_init(void);
void init_debug_uart(void);
void init_comm_uart(void);

// UART Functions
void debug_print_char(char c);
void debug_print_str(const char *str);
void debug_print_float(double value, int precision);
void debug_print_uint16(uint16_t value);
void debug_print_int32(int32_t value);
void debug_print_uint32(uint32_t value); 
void debug_print_hex(uint8_t value);
void debug_print_hex16(uint16_t value);
void debug_print_hex24(uint32_t value);
void debug_print_hex32(uint32_t value);
void debug_print_hex64(uint64_t value);
void debug_print_int(int value);
void debug_push_str(const char *str); 
void debug_flush(void);
void debug_full_flush(void);
void debug_system_status(void);
void debug_push_char(char c);
uint8_t uart_get_line(char *buffer, uint16_t max_len);
void isr_log_push_char(char c);
void isr_log_push_str(const char *str);
void isr_log_push_hex_nibble(uint8_t value);
void isr_log_push_uint16(uint16_t value);
uint8_t uart_data_available(void);
void uart_read_line(char* buffer, uint16_t max_len);
uint8_t uart_data_available(void);
char uart_read_char(void);
void process_uart_commands(void);

// =============================
// Variables globales partagees
// =============================
extern volatile char rxQueue[UART_BUFFER_SIZE];
extern volatile uint16_t rxHead;
extern volatile uint16_t rxTail;
extern volatile uint8_t rxOverflowed;
extern volatile char debug_buf[DEBUG_BUF_SIZE];
extern volatile uint16_t debug_head;
extern volatile uint16_t debug_tail;
extern volatile debug_flags_t debug_flags;
extern volatile char isr_log_buf[ISR_LOG_BUF_SIZE];
extern volatile uint16_t isr_log_head;
extern volatile uint16_t isr_log_tail;


#endif	/* SYSTEM_DEBUG_H */

