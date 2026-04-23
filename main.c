#include "includes.h"
#include "system_definitions.h"
#include "system_debug.h"
#include "system_comms.h"
#include "protocol_data.h"  // Ajout pour les fonctions de trame
#include "rf_interface.h"   // Pour les fonctions RF
// spi2_test.h excluded - test code removed from build
#include "drivers/mcp4922_driver.h"  // Driver MCP4922
#include "drivers/i2c_sw.h"          // Software I2C for LCD
#include "drivers/lcd1604_i2c.h"     // LCD 16x4 via PCF8574
#include "drivers/keypad_matrix.h"   // 4x4 matrix keypad
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

// Parse "403.040" or "403040" → kHz (dot key = 'B')
static uint32_t freq_parse_khz(const char *s) {
    uint32_t int_part = 0, frac_part = 0;
    uint8_t has_dot = 0, frac_digits = 0;
    for (uint8_t i = 0; s[i]; i++) {
        if (s[i] == '.') { has_dot = 1; continue; }
        if (has_dot) {
            if (frac_digits < 3) { frac_part = frac_part * 10 + (uint8_t)(s[i] - '0'); frac_digits++; }
        } else {
            int_part = int_part * 10 + (uint8_t)(s[i] - '0');
        }
    }
    while (frac_digits < 3) { frac_part *= 10; frac_digits++; }
    return int_part * 1000UL + frac_part;
}

static void lcd_freq_display(uint32_t freq_khz) {
    uint16_t mhz = (uint16_t)(freq_khz / 1000UL);
    uint16_t khz = (uint16_t)(freq_khz % 1000UL);
    lcd_set_cursor(1, 0);
    lcd_print_str("Freq:");
    lcd_print_int((int16_t)mhz);
    lcd_print_char('.');
    if (khz < 100) lcd_print_char('0');
    if (khz < 10)  lcd_print_char('0');
    lcd_print_int((int16_t)khz);
    lcd_print_str(" MHz   ");
}

// Mode sélectionnable au clavier (C = toggle TEST/EXERCISE)
static beacon_frame_type_t current_frame_type = BEACON_TEST_FRAME;

beacon_frame_type_t get_frame_type_from_switch(void) {
    return current_frame_type;
}

static void lcd_mode_display(void) {
    lcd_set_cursor(0, 0);
    lcd_print_str(current_frame_type == BEACON_TEST_FRAME
        ? "TEST 5s 100mW       "
        : "EXERC 30s 5W        ");
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

    // Initialize LCD 1604 + keypad
    i2c_sw_init();
    i2c_sw_scan();  // Log I2C devices found (debug only)
    lcd_init();
    keypad_init();
    lcd_mode_display();
    lcd_set_cursor(1, 0);
    lcd_print_str("Freq:403.040 MHz");
    lcd_set_cursor(2, 0);
    lcd_print_str("GNSS:no fix         ");
    DEBUG_LOG_FLUSH("LCD+Keypad init completed\r\n");

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

    // Test MCP4922 pattern pour vérifier SPI2 (désactivé: 36s de délai au boot)
    //DEBUG_LOG_FLUSH("Testing MCP4922 pattern...\r\n");
    //mcp4922_test_pattern();
    //DEBUG_LOG_FLUSH("MCP4922 pattern test completed\r\n");

    rf_set_power_level(RF_POWER_LOW);
    
    DEBUG_LOG_FLUSH("Starting transmission - Mode: ");
    DEBUG_LOG_FLUSH(current_frame_type == BEACON_TEST_FRAME ? "TEST\r\n" : "EXERCISE\r\n");
    start_beacon_frame(current_frame_type);

    while(1) {
		process_uart_commands();  // Handle commands

        uint32_t current_time;
        __builtin_disable_interrupts();
        current_time = millis_counter;
        __builtin_enable_interrupts();

        // Keypad: frequency entry state machine
        // A=enter, 0-9=digit, B=decimal point, *=backspace, #=confirm, D=cancel
        static uint8_t freq_entry_mode = 0;
        static char freq_buf[8] = {0};
        static uint8_t freq_buf_len = 0;

        char key = keypad_get_key();
        if (key != KEYPAD_NO_KEY && tx_phase == IDLE_STATE) {
            if (!freq_entry_mode) {
                if (key == 'C' && tx_phase == IDLE_STATE) {
                    current_frame_type = (current_frame_type == BEACON_TEST_FRAME)
                        ? BEACON_EXERCISE_FRAME : BEACON_TEST_FRAME;
                    lcd_mode_display();
                } else if (key == 'A') {
                    freq_entry_mode = 1;
                    freq_buf_len = 0;
                    freq_buf[0] = '\0';
                    lcd_set_cursor(1, 0);
                    lcd_print_str("Freq:           ");
                    lcd_set_cursor(3, 0);
                    lcd_print_str("B=. *=eff #=OK D=ann");
                }
            } else {
                if ((key >= '0' && key <= '9') && freq_buf_len < 7) {
                    freq_buf[freq_buf_len++] = key;
                    freq_buf[freq_buf_len] = '\0';
                    lcd_set_cursor(1, 5);
                    lcd_print_str(freq_buf);
                } else if (key == 'B' && freq_buf_len < 7) {
                    freq_buf[freq_buf_len++] = '.';
                    freq_buf[freq_buf_len] = '\0';
                    lcd_set_cursor(1, 5);
                    lcd_print_str(freq_buf);
                } else if (key == '*' && freq_buf_len > 0) {
                    freq_buf[--freq_buf_len] = '\0';
                    lcd_set_cursor(1, 5 + freq_buf_len);
                    lcd_print_char(' ');
                } else if (key == '#' && freq_buf_len > 0) {
                    uint32_t freq_khz = freq_parse_khz(freq_buf);
                    if (freq_khz >= 400000UL && freq_khz <= 450000UL) {
                        adf4351_set_frequency(freq_khz);
                        lcd_freq_display(freq_khz);
                        lcd_set_cursor(3, 0);
                        lcd_print_str(ADF4351_LD_PIN ? "PLL:locked          " : "PLL:UNLOCK!         ");
                    } else {
                        lcd_set_cursor(1, 0);
                        lcd_print_str("Freq:hors plage     ");
                    }
                    freq_entry_mode = 0;
                } else if (key == 'D') {
                    freq_entry_mode = 0;
                    lcd_set_cursor(3, 0);
                    lcd_print_str("Annule              ");
                }
            }
        }

        // Periodic LCD refresh (every 500 ms, only when idle)
        static uint32_t last_lcd_update = 0;
        if ((current_time - last_lcd_update) >= 500 && tx_phase == IDLE_STATE) {
            last_lcd_update = current_time;

            // Line 2: GPS lat/lon
            lcd_set_cursor(2, 0);
            if (gps_has_fix()) {
                const gps_data_t *gps = gps_get_data();
                double lat = gps->latitude;
                double lon = gps->longitude;
                char ns = (lat >= 0) ? 'N' : 'S';
                char ew = (lon >= 0) ? 'E' : 'W';
                if (lat < 0) lat = -lat;
                if (lon < 0) lon = -lon;
                int16_t lat_d = (int16_t)lat;
                int16_t lat_f = (int16_t)((lat - lat_d) * 1000);
                int16_t lon_d = (int16_t)lon;
                int16_t lon_f = (int16_t)((lon - lon_d) * 1000);
                // Pos:N43.565 E001.482  (20 chars)
                lcd_print_str("Pos:");
                lcd_print_char(ns);
                lcd_print_int(lat_d);
                lcd_print_char('.');
                if (lat_f < 100) lcd_print_char('0');
                if (lat_f < 10)  lcd_print_char('0');
                lcd_print_int(lat_f);
                lcd_print_char(' ');
                lcd_print_char(ew);
                if (lon_d < 100) lcd_print_char('0');
                if (lon_d < 10)  lcd_print_char('0');
                lcd_print_int(lon_d);
                lcd_print_char('.');
                if (lon_f < 100) lcd_print_char('0');
                if (lon_f < 10)  lcd_print_char('0');
                lcd_print_int(lon_f);
                lcd_set_cursor(3, 0);
                lcd_print_str("                    ");
            } else {
                lcd_print_str("GNSS:no fix         ");
                // Line 3: show fallback position being used in frame
                double lat = TEST_LATITUDE;
                double lon = TEST_LONGITUDE;
                char ns = (lat >= 0) ? 'N' : 'S';
                char ew = (lon >= 0) ? 'E' : 'W';
                if (lat < 0) lat = -lat;
                if (lon < 0) lon = -lon;
                int16_t lat_d = (int16_t)lat;
                int16_t lat_f = (int16_t)((lat - lat_d) * 100);
                int16_t lon_d = (int16_t)lon;
                int16_t lon_f = (int16_t)((lon - lon_d) * 100);
                lcd_set_cursor(3, 0);
                lcd_print_str("Tlse:");
                lcd_print_char(ns);
                lcd_print_int(lat_d);
                lcd_print_char('.');
                if (lat_f < 10) lcd_print_char('0');
                lcd_print_int(lat_f);
                lcd_print_char(' ');
                lcd_print_char(ew);
                if (lon_d < 100) lcd_print_char('0');
                if (lon_d < 10)  lcd_print_char('0');
                lcd_print_int(lon_d);
                lcd_print_char('.');
                if (lon_f < 10) lcd_print_char('0');
                lcd_print_int(lon_f);
                lcd_print_str(" ");
            }
        }

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

            // Print GPS status if available (simplified to avoid timing issues)
            if (gps_has_fix()) {
                const gps_data_t *gps = gps_get_data();
                DEBUG_LOG_FLUSH("GPS Fix: ");
                debug_print_uint16(gps->satellites);
                DEBUG_LOG_FLUSH(" sats\r\n");
            }

            DEBUG_LOG_FLUSH("Starting periodic transmission - Mode: ");
            DEBUG_LOG_FLUSH(current_frame_type == BEACON_TEST_FRAME ? "TEST\r\n" : "EXERCISE\r\n");
            start_beacon_frame(current_frame_type);
        }
        
        // Periodic status report
        static uint32_t last_status = 0;
        if((current_time - last_status) >= 10000) {
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
        
        if((current_time - last_pll_check) >= 5000 && tx_phase != IDLE_STATE) {
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
