/* main.c - Version corrig�e */
#include "includes.h"
#include "system_definitions.h"
#include "system_debug.h"
#include "system_comms.h"
#include "protocol_data.h"  // Ajout pour les fonctions de trame
#include "rf_interface.h"   // Pour les fonctions RF

// D�clarations externes
extern volatile uint32_t millis_counter;
extern volatile tx_phase_t tx_phase;
extern volatile uint8_t beacon_frame[];

// D�claration de la fonction start_beacon_frame
void start_beacon_frame(beacon_frame_type_t frame_type);

// Lecture du switch de sélection mode
beacon_frame_type_t get_frame_type_from_switch(void) {
    // RB12 = 0 (pull-down) → TEST mode
    // RB12 = 1 (switch pressed) → EXERCISE mode
    return PORTBbits.RB12 ? BEACON_EXERCISE_FRAME : BEACON_TEST_FRAME;
}

uint8_t should_transmit_beacon(void) {
    uint8_t phase;
    uint32_t current_millis, last_tx;
    
    // Lecture atomique des variables partag�es
    __builtin_disable_interrupts();
    phase = tx_phase;
    current_millis = millis_counter;
    last_tx = last_tx_time;
    __builtin_enable_interrupts();

    return (phase == IDLE_STATE) && 
           ((current_millis - last_tx) >= tx_interval_ms);
}

int main(void) {
	__builtin_disable_interrupts();
    system_init();
    set_tx_interval(5000);  // Initial interval (will be adjusted by frame type)
    __builtin_enable_interrupts();
    
    DEBUG_LOG_FLUSH("System initialized\r\n");

    rf_set_power_level(RF_POWER_LOW);
    
    beacon_frame_type_t frame_type = get_frame_type_from_switch();
    DEBUG_LOG_FLUSH("Starting transmission - Mode: ");
    DEBUG_LOG_FLUSH(frame_type == BEACON_TEST_FRAME ? "TEST\r\n" : "EXERCISE\r\n");
    start_beacon_frame(frame_type);  // Generate according to switch

    while(1) {
		process_uart_commands();  // Handle commands
		
        uint32_t current_time;
        __builtin_disable_interrupts();
        current_time = millis_counter;
        __builtin_enable_interrupts();
        
        // Transfert des logs UART
        while (isr_log_tail != isr_log_head) {
            while (U2STAHbits.UTXBF); 
            U2TXREG = isr_log_buf[isr_log_tail];
            isr_log_tail = (isr_log_tail + 1) % ISR_LOG_BUF_SIZE;
        }
        
        // Periodic transmission trigger (read switch each time)
        if (should_transmit_beacon()) {
            beacon_frame_type_t current_frame_type = get_frame_type_from_switch();
            DEBUG_LOG_FLUSH("Starting periodic transmission - Mode: ");
            DEBUG_LOG_FLUSH(current_frame_type == BEACON_TEST_FRAME ? "TEST\r\n" : "EXERCISE\r\n");
            start_beacon_frame(current_frame_type);
        }
        
        // Periodic status report
        static uint32_t last_status = 0;
        if((current_time - last_status) >= 1000) {
            last_status = current_time;
            DEBUG_LOG_FLUSH("Status: phase=");
            debug_print_uint16(tx_phase);
            DEBUG_LOG_FLUSH("\r\n");
        }
        
        __delay_ms(10);
    }
    
    return 0;
}