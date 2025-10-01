#include "includes.h"
#include "system_definitions.h"
#include "system_comms.h"
#include "system_debug.h"
#include "protocol_data.h"

void full_error_diagnostic(void) {
    if (debug_flags.diagnostic_printed) return;
    debug_flags.diagnostic_printed = 1;
    // Vérification initiale du matériel
    debug_print_str("=== DIAGNOSTIC SYSTEME ===\r\n");
    debug_full_flush();
    
    // 1. Validation polynômes BCH
    if(BCH1_POLY != 0x26D9E3 || BCH2_POLY != 0x1539) {
        debug_print_str("ERREUR: Polynomes BCH non conformes!\r\n");
        while(1); // Blocage système
    }
    
    // 2. Test GPS - SUPPRIMER la variable 'pos' inutilisée
    set_gps_position(TEST_LATITUDE, TEST_LONGITUDE, TEST_ALTITUDE);
    encode_gps_position_complete(current_latitude, current_longitude);
    
    // 3. Vérification mémoire
    if(sizeof(beacon_frame) != MESSAGE_BITS) {
        debug_print_str("ERREUR: Taille frame incorrecte!\r\n");
    }
    
    debug_print_str("Diagnostic termine\r\n");
}

void verify_cs_t001_constants(void) {
    debug_print_str("=== CS-T001 CONSTANTS VERIFICATION ===\r\n");
    
    // Verification polynomes BCH
    debug_print_str("BCH1_POLY: 0x");
    debug_print_hex24(BCH1_POLY);
    debug_print_str((BCH1_POLY == 0x26D9E3) ? " âœ“" : " âœ— SHOULD BE 0x26D9E3");
    debug_print_str("\r\n");
    
    debug_print_str("BCH2_POLY: 0x");
    debug_print_hex16(BCH2_POLY);
    debug_print_str((BCH2_POLY == 0x1539) ? " âœ“" : " âœ— SHOULD BE 0x1539");
    debug_print_str("\r\n");
    
    // Verification sync patterns
    debug_print_str("SYNC_NORMAL_LONG: 0x");
    debug_print_hex16(SYNC_NORMAL_LONG);
    debug_print_str((SYNC_NORMAL_LONG == 0x2F) ? " âœ“" : " âœ— SHOULD BE 0x2F");
    debug_print_str("\r\n");
    
    debug_print_str("SYNC_SELF_TEST: 0x");
    debug_print_hex16(SYNC_SELF_TEST);
    debug_print_str((SYNC_SELF_TEST == 0x1A0) ? " âœ“" : " âœ— SHOULD BE 0x1A0");
    debug_print_str("\r\n");
    
    // Verification country code
    debug_print_str("COUNTRY_CODE_FRANCE: ");
    debug_print_int32(COUNTRY_CODE_FRANCE);
    debug_print_str((COUNTRY_CODE_FRANCE == 227) ? " âœ“" : " âœ— SHOULD BE 227");
    debug_print_str("\r\n");
}
