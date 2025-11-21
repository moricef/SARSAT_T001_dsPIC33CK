/* system_debug.c */
#include "includes.h"
#include "system_definitions.h"
#include "system_comms.h"
#include "system_debug.h"
#include "protocol_data.h"
#include "gps_nmea.h"

// =============================
// Variables globales
// =============================
// Buffer debug
volatile char debug_buf[DEBUG_BUF_SIZE];
volatile uint16_t debug_head = 0;
volatile uint16_t debug_tail = 0;
volatile char rxQueue[UART_BUFFER_SIZE];
volatile uint16_t rxHead = 0;
volatile uint16_t rxTail = 0;
volatile uint8_t rxOverflowed = 0;
volatile debug_flags_t debug_flags = {0};
volatile char isr_log_buf[ISR_LOG_BUF_SIZE];
volatile uint16_t isr_log_head = 0;
volatile uint16_t isr_log_tail = 0;
// =============================


// Verifie si des donnees sont disponibles
uint8_t uart_data_available(void) {
    return !U2STAHbits.URXBE;  // Bit 1 de U2STAH (1 = buffer vide)
}

// Lit une ligne depuis l'UART
void uart_read_line(char* buffer, uint16_t max_len) {
    uint16_t index = 0;
    while (index < max_len - 1) {
        while (U2STAHbits.URXBE);  // Attendre tant que buffer RX vide (URXBE=1)
        
        buffer[index] = U2RXREG;  // Lire registre reception
        
        if (buffer[index] == '\r' || buffer[index] == '\n') {
            buffer[index] = '\0';
            return;
        }
        index++;
    }
    buffer[max_len - 1] = '\0';
}

// Version optimisee pour transfert logs ISR
void isr_log_transfer_direct(void) {
    while (isr_log_tail != isr_log_head) {
        if (U2STAHbits.UTXBF) return;  // Bit 4 de U2STAH (1 = buffer TX plein)
        
        U2TXREG = isr_log_buf[isr_log_tail];
        isr_log_tail = (isr_log_tail + 1) % ISR_LOG_BUF_SIZE;
    }
}


// Fonctions de gestion du buffer debug
// =============================
void debug_push_char(char c) {
    while(U2STAHbits.UTXBF);  // Attendre buffer libre
    U2TXREG = c;
}

void debug_push_str(const char *str) {
    while (*str) {
        debug_push_char(*str++);
    }
}


void debug_flush(void) {
    while (debug_tail != debug_head) {
        if (!U2STAHbits.UTXBF) {
            U2TXREG = debug_buf[debug_tail];
            debug_tail = (debug_tail + 1) % DEBUG_BUF_SIZE;
        } else {
            break;
        }
    }
}

void debug_full_flush(void) {
    uint32_t timeout = millis_counter + 500;  // Timeout apres 500ms
    //uint32_t timeout = millis_counter + 100;  // Timeout apres 100ms
    
    while (debug_tail != debug_head) {
        uint32_t start_wait = millis_counter;
        while (U2STAHbits.UTXBF && (millis_counter - start_wait < 10)); 
        
        if (U2STAHbits.UTXBF) break;  // Timeout
        
        U2TXREG = debug_buf[debug_tail];
        debug_tail = (debug_tail + 1) % DEBUG_BUF_SIZE;
        
        if (millis_counter > timeout) break;  // Eviter les blocages infinis
    }
    
    uint32_t start_wait = millis_counter;
    while (!U2STAbits.TRMT && (millis_counter - start_wait < 10));
}

void debug_print_uint16(uint16_t value) {
    char buffer[6];
    uint8_t i = 5;
    buffer[5] = '\0';
    do {
        buffer[--i] = '0' + (value % 10);
        value /= 10;
    } while (value && i > 0);
    debug_push_str(&buffer[i]);
}

// =============================
// Interruption UART1
// =============================
void __attribute__((__interrupt__, __auto_psv__)) _U1RXInterrupt(void) {
    volatile uint16_t nextTail = (rxTail + 1) % UART_BUFFER_SIZE;
    
    if (nextTail == rxHead) {
        rxOverflowed = 1;
    } else {
        rxQueue[rxTail] = U1RXREG;
        rxTail = nextTail;
    }
    IFS0bits.U1RXIF = 0;
}

// =============================
// Initialisation UART Debug (UART2)
// =============================
void init_debug_uart(void) {
    // 1. Desactiver UART avant configuration
    U2MODEbits.UARTEN = 0;
    U2MODEbits.UTXEN = 0;
    
    // 2. Reinitialisation complete des registres
    U2MODE = 0;
    U2STAH = 0; // CRITIQUE - Reinitialiser le registre etendu
    
    // 3. Configuration BRG identique au test
    U2MODEbits.BRGH = 1;
    U2BRG = (FCY / (4 * DEBUG_BAUD_RATE)) - 1;
    
    // 4. Configuration PPS EXACTEMENT comme le test
    __builtin_write_OSCCONL(OSCCONL | 0x40);  // Deverrouiller PPS
    _RP58R = 0x0003;   // U2TX sur RP58 (RC10) - valeur 3
    _U2RXR = 59;       // U2RX sur RP59 (RC11) - valeur 59
    __builtin_write_OSCCONL(OSCCONL & ~0x40); // Verrouiller PPS
    
    // 5. Configuration broches physique
    TRISCbits.TRISC10 = 0;  // TX (RC10) en sortie
    TRISCbits.TRISC11 = 1;  // RX (RC11) en entree
    LATCbits.LATC10 = 1;    // etat inactif HIGH
    
    // 6. Activation avec sequence EXACTE du test
    U2MODEbits.UARTEN = 1;
    __builtin_nop();  // Delai critique
    __builtin_nop();
    U2MODEbits.UTXEN = 1;
}

// =============================
// Initialisation UART Communication (UART1)
// =============================
void init_comm_uart(void) {
    static uint8_t initialized = 0;
    if (initialized) return;
    initialized = 1;
	    // 1. Desactiver temporairement
    U1MODEbits.UARTEN = 0;
    U1MODEbits.UTXEN = 0;
    
    // 2. Reinitialisation complete (identique au test)
    U1MODE = 0;
    U1STAH = 0; //
   
    // Deverrouillage PPS
    __builtin_write_OSCCONL(OSCCONL | 0x40); // Deverouille PPS
    _U1RXR = 36;              // RB3 (RP36)
    _RP35R = 0x0003;        // RB4 (RP35)
    __builtin_write_OSCCONL(OSCCONL & ~0x40); // Verouille PPS
    
    // Configuration registres
    U1MODE = 0x0000;
    U1MODEH = 0x0800;
    U1STA = 0x0080;
    U1STAH = 0x002E;
    
	// Calcul BRG
    uint32_t brg = (uint32_t)(FCY / (16UL * UART1_BAUD_RATE)) - 1;
    if (brg > 65535) brg = 65535;
    U1BRG = (uint16_t)brg;
	
	// Configuration broches
    TRISBbits.TRISB4 = 0;    // TX sortie
    TRISBbits.TRISB3 = 1;     // RX Entree
	LATBbits.LATB4 = 1;    // etat inactif HIGH
    
    // Activation
    U1MODEbits.UARTEN = 1;
    U1MODEbits.UTXEN = 1;
    U1MODEbits.URXEN = 1;
    
    // Configuration interruptions
    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;
    IPC2bits.U1RXIP = 4;
	
	// 6. Activation avec sequence TEST
    U1MODEbits.UARTEN = 1;
    __builtin_nop();
    __builtin_nop();
    U1MODEbits.UTXEN = 1;
    
    // 7. Test immediat (identique au test)
    while(U1STAHbits.UTXBF);  // Attendre buffer libre
    U1TXREG = 'S';  // Envoyer caractere de test
    
    DEBUG_LOG_FLUSH("UART communication pret\r\n");
}


// =============================
// Fonctions debug
// =============================
char uart_read_char(void) {
    while (!uart_data_available());  // Attendre un caractère
    return U2RXREG;
}

uint8_t uart_get_line(char *buffer, uint16_t max_len) {
    uint16_t idx = 0;
    
    if (rxOverflowed) {
        buffer[0] = '\0';
        rxOverflowed = 0;
        return 0;
    }
    
    while (idx < max_len - 1) {
        if (rxHead == rxTail) {
            return 0;
        }
        
        char c = rxQueue[rxHead];
        rxHead = (rxHead + 1) % UART_BUFFER_SIZE;
        
        buffer[idx++] = c;
        if (c == '\n' || c == '\r') break;
    }
    buffer[idx] = '\0';
    return (idx > 0);
}

void debug_print_char(char c) {
    debug_push_char(c);
}

void debug_print_str(const char *str) {
    debug_push_str(str);
}

void debug_print_hex(uint8_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    debug_print_char(hex_chars[(value >> 4) & 0x0F]);
    debug_print_char(hex_chars[value & 0x0F]);
}

void debug_print_hex16(uint16_t value) {
    debug_print_hex((value >> 8) & 0xFF);
    debug_print_hex(value & 0xFF);
}

void debug_print_hex24(uint32_t value) {
    debug_print_hex((value >> 16) & 0xFF);
    debug_print_hex((value >> 8) & 0xFF);
    debug_print_hex(value & 0xFF);
}

void debug_print_hex32(uint32_t value) {
    debug_print_hex((value >> 24) & 0xFF);
    debug_print_hex((value >> 16) & 0xFF);
    debug_print_hex((value >> 8) & 0xFF);
    debug_print_hex(value & 0xFF);
}

void debug_print_hex64(uint64_t value) {
    debug_print_hex32(value >> 32);
    debug_print_hex32(value & 0xFFFFFFFF);
}

void debug_print_int(int value) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d", value);
    debug_print_str(buffer);
}

void debug_print_uint32(uint32_t value) {
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    debug_print_str(buffer);
}

void debug_print_float(double value, int precision) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    debug_print_str(buffer);
}

void debug_print_int32(int32_t value) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%ld", (long)value);
    debug_print_str(buffer);
}

void isr_log_push_char(char c) {
    __builtin_disable_interrupts();
    uint16_t next_head = (isr_log_head + 1) % ISR_LOG_BUF_SIZE;
    if (next_head != isr_log_tail) {
        isr_log_buf[isr_log_head] = c;
        isr_log_head = next_head;
    }
    __builtin_enable_interrupts();
}

void isr_log_push_str(const char *str) {
    while (*str) {
        isr_log_push_char(*str++);
    }
}
void isr_log_push_hex_nibble(uint8_t value) {
    value &= 0x0F;
    isr_log_push_char(value < 10 ? '0' + value : 'A' + value - 10);
}

void isr_log_push_uint16(uint16_t value) {
    char buffer[6];
    char *ptr = buffer + 5;
    *ptr = '\0';
    
    do {
        *--ptr = '0' + (value % 10);
        value /= 10;
    } while (value && ptr > buffer);
    
    while (*ptr) {
        isr_log_push_char(*ptr++);
    }
}

// =============================
// Choisir le mode de logs
// =============================
void process_uart_commands(void) {
    static char cmd_buffer[32];
    static uint8_t cmd_index = 0;
    
    while (uart_data_available()) {
        char c = uart_read_char();
        
        if (c == '\r' || c == '\n') {
            cmd_buffer[cmd_index] = '\0';
            cmd_index = 0;
            
            // Traitement des commandes
            if (strcmp(cmd_buffer, "LOG ALL") == 0) {
                debug_flags.log_mode = LOG_MODE_ALL;
                DEBUG_LOG_FLUSH("Debug mode: ALL\r\n");
            }
            else if (strcmp(cmd_buffer, "LOG SYSTEM") == 0) {
                debug_flags.log_mode = LOG_MODE_SYSTEM;
                DEBUG_LOG_FLUSH("Debug mode: SYSTEM\r\n");
            }
            else if (strcmp(cmd_buffer, "LOG ISR") == 0) {
                debug_flags.log_mode = LOG_MODE_ISR;
                DEBUG_LOG_FLUSH("Debug mode: ISR\r\n");
            }
            else if (strcmp(cmd_buffer, "LOG NONE") == 0) {
                debug_flags.log_mode = LOG_MODE_NONE;
                DEBUG_LOG_FLUSH("Debug mode: NONE\r\n");
            }
            else if (strcmp(cmd_buffer, "GPS") == 0) {
                gps_print_status();
            }
            else if (strcmp(cmd_buffer, "GPS RAW ON") == 0) {
                gps_debug_raw = 1;
                DEBUG_LOG_FLUSH("GPS RAW mode: ON\r\n");
            }
            else if (strcmp(cmd_buffer, "GPS RAW OFF") == 0) {
                gps_debug_raw = 0;
                DEBUG_LOG_FLUSH("GPS RAW mode: OFF\r\n");
            }
            else {
                DEBUG_LOG_FLUSH("Unknown command: ");
                DEBUG_LOG_FLUSH(cmd_buffer);
                DEBUG_LOG_FLUSH("\r\nCommands: LOG ALL, LOG SYSTEM, LOG ISR, LOG NONE, GPS, GPS RAW ON, GPS RAW OFF\r\n");
            }
        }
        else if (cmd_index < sizeof(cmd_buffer)-1) {
            cmd_buffer[cmd_index++] = c;
        }
    }
}

// =============================
// Surveillance systeme
// =============================
void debug_system_status(void) {
    static uint32_t last_debug_time = 0;
    
    if (millis_counter - last_debug_time >= 100) {
        last_debug_time = millis_counter;
        
        char buf[64];
        snprintf(buf, sizeof(buf), 
                "Mod:%u Phase:%X State:%u Gain:%.2f\r\n",
                modulation_counter,
                carrier_phase & 0x0F,
                tx_phase,
                (double)envelope_gain);
        
        debug_push_str(buf);
    }
}

// =============================
// Initialisation debug systeme
// =============================

void system_debug_init(void){
 init_debug_uart();
 
 DEBUG_LOG_FLUSH("Initialisation systeme demarree\r\n");
    
    
     DEBUG_LOG_FLUSH("Test phase porteuse: ");
    for(uint8_t i = 0; i < 20; i++) {
        debug_print_hex(i % 16);
        DEBUG_LOG_FLUSH(" ");
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    
    DEBUG_LOG_FLUSH("Initialisation systeme complete @50 MHz\r\n");
    DEBUG_LOG_FLUSH("Tables DAC: ");
    DEBUG_LOG_FLUSH("16");  // Valeur fixe
    DEBUG_LOG_FLUSH(" points\r\n");
    
};
