#include "includes.h"
#include "system_definitions.h"
#include "system_debug.h"
#include "system_comms.h"
#include "protocol_data.h"  // Ajout pour les fonctions de trame
#include "rf_interface.h"   // Pour les fonctions RF
#include "spi2_test.h"      // Test de compatibilité SPI2
#include "drivers/mcp4922_driver.h"  // Driver MCP4922
#include "gps_nmea.h"       // GPS NMEA support

// Declarations externes
extern volatile uint32_t millis_counter;
extern volatile tx_phase_t tx_phase;
extern volatile uint8_t beacon_frame[];

// Declaration de la fonction start_beacon_frame
void start_beacon_frame(beacon_frame_type_t frame_type);

// Declarations for new RF control functions
extern void rf_start_transmission(void);
extern void rf_stop_transmission(void);
extern void rf_adf4351_enable_chip(uint8_t state);
extern const uint32_t adf4351_regs_403mhz[];

// Lecture du switch de sélection mode
beacon_frame_type_t get_frame_type_from_switch(void) {
    // RB2 = 0 (pull-down) → TEST mode
    // RB2 = 1 (switch pressed) → EXERCISE mode
    return PORTBbits.RB2 ? BEACON_EXERCISE_FRAME : BEACON_TEST_FRAME;

    // TEMPORARY: Force EXERCISE mode for GPS testing (uncomment to override switch)
    //return BEACON_EXERCISE_FRAME;
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
    //set_tx_interval(5000);  // Initial interval (will be adjusted by frame type)
    __builtin_enable_interrupts();

    __delay_ms(4000);
    
    // Generate HHMMSS timestamp from rf_interface.c compilation time
    extern const char rf_build_time[];  // Defined in rf_interface.c
    DEBUG_LOG_FLUSH("=== SESSION ");
    // Extract HHMMSS: positions 0,1,3,4,6,7 from "HH:MM:SS"
    debug_print_char(rf_build_time[0]);  // H
    debug_print_char(rf_build_time[1]);  // H  
    debug_print_char(rf_build_time[3]);  // M
    debug_print_char(rf_build_time[4]);  // M
    debug_print_char(rf_build_time[6]);  // S
    debug_print_char(rf_build_time[7]);  // S
    DEBUG_LOG_FLUSH(" ===\r\n");
    DEBUG_LOG_FLUSH("System initialized\r\n");
    DEBUG_LOG_FLUSH("About to initialize RF modules\r\n");

    // Initialize RF modules
    rf_initialize_all_modules();
    DEBUG_LOG_FLUSH("RF modules init completed\r\n");

    // Test de compatibilité SPI2 (logiciel uniquement)
    //DEBUG_LOG_FLUSH("Starting SPI2 compatibility test...\r\n");
    //run_spi2_compatibility_test();

    DEBUG_LOG_FLUSH("=== SESSION ");
    debug_print_char(rf_build_time[0]);
    debug_print_char(rf_build_time[1]);
    debug_print_char(rf_build_time[3]);
    debug_print_char(rf_build_time[4]);
    debug_print_char(rf_build_time[6]);
    debug_print_char(rf_build_time[7]);
    DEBUG_LOG_FLUSH(" INIT COMPLETE ===\r\n");

    // Test MCP4922 pattern pour vérifier SPI2
    DEBUG_LOG_FLUSH("Testing MCP4922 pattern...\r\n");
    mcp4922_test_pattern();
    DEBUG_LOG_FLUSH("MCP4922 pattern test completed\r\n");

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

        // Process GPS data
        if (gps_update()) {
            // New GPS data received - frame will be rebuilt at next transmission
        }

        // Transfert des logs UART
        while (isr_log_tail != isr_log_head) {
            while (U2STAHbits.UTXBF);
            U2TXREG = isr_log_buf[isr_log_tail];
            isr_log_tail = (isr_log_tail + 1) % ISR_LOG_BUF_SIZE;
        }

        // Periodic transmission trigger (read switch each time)
        if (should_transmit_beacon()) {
            beacon_frame_type_t current_frame_type = get_frame_type_from_switch();

            // Print GPS status if available
            if (gps_has_fix()) {
                DEBUG_LOG_FLUSH("GPS Fix: ");
                const gps_data_t *gps = gps_get_data();
                debug_print_uint16(gps->satellites);
                DEBUG_LOG_FLUSH(" sats, Pos: ");
                debug_print_float(gps->latitude, 6);
                DEBUG_LOG_FLUSH(", ");
                debug_print_float(gps->longitude, 6);
                DEBUG_LOG_FLUSH("\r\n");
            }

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
            DEBUG_LOG_FLUSH(" gps_rx=");
            extern volatile uint16_t gps_rx_count;
            debug_print_uint16(gps_rx_count);
            DEBUG_LOG_FLUSH(" gps_irq=");
            extern volatile uint16_t gps_irq_count;
            debug_print_uint16(gps_irq_count);
            DEBUG_LOG_FLUSH(" gps_oerr=");
            extern volatile uint16_t gps_oerr_count;
            debug_print_uint16(gps_oerr_count);
            DEBUG_LOG_FLUSH("\r\n");
        }
        
        // Periodic PLL lock verification with retry (every 5 seconds)
        static uint32_t last_pll_check = 0;
        static uint8_t pll_was_locked = 1;  // Assume locked after init
        
        if((current_time - last_pll_check) >= 5000) {
            last_pll_check = current_time;
            
            if (ADF4351_LD_PIN) {
                pll_was_locked = 1;
            } else {
                
                // PLL unlock detected during normal operation
                if (pll_was_locked) {
                    DEBUG_LOG_FLUSH("WARNING: PLL unlock detected during operation\r\n");
                    DEBUG_LOG_FLUSH("Attempting automatic PLL recovery...\r\n");
                    
                    // Attempt recovery - full ADF4351 reset sequence
                    rf_adf4351_enable_chip(0);  // Disable chip first
                    __delay_ms(10);              // Reset delay
                    rf_adf4351_enable_chip(1);  // Re-enable chip
                    __delay_ms(10);              // Power-up delay

                    // Reprogram all registers
                    for(int i = 0; i < 6; i++) {
                        adf4351_write_register(adf4351_regs_403mhz[i]);
                        __delay_ms(2);
                    }
                    __delay_ms(50);  // Extra settling time
                    
                    // Check if recovery successful
                    if (ADF4351_LD_PIN) {
                        DEBUG_LOG_FLUSH("PLL recovery successful\r\n");
                        pll_was_locked = 1;
                    } else {
                        DEBUG_LOG_FLUSH("PLL recovery failed - entering critical mode\r\n");
                        rf_system_halt("PLL UNLOCK DURING OPERATION - RECOVERY FAILED");
                    }
                }
                pll_was_locked = 0;
            }
        }
        
        __delay_ms(10);
    }
    
    return 0;
}
