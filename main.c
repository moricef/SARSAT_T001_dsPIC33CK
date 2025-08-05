#include "includes.h"
#include "system_comms.h"
#include "system_definitions.h"
#include "protocol_data.h"

int main(void) {
    initialize_debug_system();
     
    // Initialisations
    WDTCONLbits.ON = 0;
    init_clock();
    init_gpio();
    init_rf_amplifier();
    UART1_Initialize();
    init_debug_uart();
    init_dac();
    init_timer1();
    
    if (tx_interval_ms < MIN_TX_INTERVAL_MS) {
        tx_interval_ms = MIN_TX_INTERVAL_MS;
        debug_print_str("DUTY CYCLE ENFORCED\r\n");
    }
    
    // DIAGNOSTIC COMPLET (exécution unique)
    full_error_diagnostic();
    
    // Création de la trame TEST initiale
    build_test_frame();
    
    if(validate_frame_hardware()) {
        transmit_beacon_frame();
    } else {
        debug_print_str("ERREUR: Trame invalide!\r\n");
    }

    __builtin_enable_interrupts();
    debug_print_str("System started\r\n");
    
    // State tracking
    static uint8_t last_frame_type = 0;
    uint8_t current_frame_type = 0;
    uint32_t min_interval = (uint32_t)(TOTAL_TX_DURATION_MS / MAX_DUTY_CYCLE);
    
    // Gestion unique de la puissance initiale
    uint8_t current_power = POWER_LOW;
    set_rf_power_level(current_power);
    debug_flags.power_printed = 0;
    
    while(1) {
              
        // Process GPS data
        char nmea_line[128];
        if (uart_get_line(nmea_line, sizeof(nmea_line))) {
            if (strncmp(nmea_line, "$GPGGA", 6) == 0) {
                parse_nmea_gga(nmea_line);
            }
        }
        
        // Update transmission interval based on switch
        if(PORTBbits.RB13 == 0) { // Mode TEST
            tx_interval_ms = 5000;
        } else { // Mode EXERCICE
            tx_interval_ms = 50000;
        }
        
        // Gestion unifiée des changements de mode
        if(current_frame_type != last_frame_type || gps_updated) {
            last_frame_type = current_frame_type;
            
            __builtin_disable_interrupts();
            if(current_frame_type) {
                build_exercice_frame();
                current_power = POWER_HIGH;
            } else {
                build_test_frame();
                current_power = POWER_LOW;
            }
            set_rf_power_level(current_power);
            __builtin_enable_interrupts();
            
            // Messages uniques
            debug_print_str(current_frame_type ? "Mode: Exercice\r\n" : "Mode: Test\r\n");
            
            // Réinitialisation flags pour nouvelle impression
            debug_flags.power_printed = 0;
            debug_flags.interval_adjusted_printed = 0;
            
            gps_updated = 0;
        }
        
           // Impression unique puissance RF
        if (!debug_flags.power_printed) {
            debug_print_str(current_power == POWER_HIGH ? "RF Power: 5W\r\n" : "RF Power: <1W\r\n");
            debug_flags.power_printed = 1;
        }
        
        // Vérification du duty cycle
        if (tx_interval_ms < min_interval) {
            tx_interval_ms = min_interval;
            
            // Impression unique ajustement intervalle
            if (!debug_flags.interval_adjusted_printed) {
                debug_print_str("Interval adjusted for RF compliance\r\n");
                debug_flags.interval_adjusted_printed = 1;
            }
        }
        
        // Transmit periodically
        if ((tx_phase == IDLE_STATE) && 
            ((millis_counter - last_tx_time) >= tx_interval_ms)) {
            last_tx_time = millis_counter;
            tx_phase = PRE_AMPLI;
            phase_sample_count = 0;
            
            // VALIDATION ET TRANSMISSION UNIQUE
            if(validate_frame_hardware()) {
                transmit_beacon_frame();
            }
        }
        
        __builtin_pwrsav(1);
    }
    return 0;
}