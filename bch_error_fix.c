// =============================
// CORRECTION CRITIQUE: Erreurs BCH detectees
// Frame re�ue: FFFFA08E39048D1582C01A8A81C2BE8827ED
// =============================

#include "protocol_data.h"

// =============================
// DIAGNOSTIC: Analyse de la frame défaillante
// =============================
void analyze_corrupted_frame(void) {
    // Frame hex re�ue avec erreurs
    uint8_t corrupted_hex[] = {
        0xFF, 0xFF, 0xA0, 0x8E, 0x39, 0x04, 0x8D, 0x15,
        0x82, 0xC0, 0x1A, 0x8A, 0x81, 0xC2, 0xBE, 0x88, 0x27, 0xED
    };
    
    // Conversion vers bits
    uint8_t corrupted_frame[MESSAGE_BITS];
    for(int byte = 0; byte < 18; byte++) {
        for(int bit = 0; bit < 8; bit++) {
            corrupted_frame[byte * 8 + bit] = (corrupted_hex[byte] >> (7 - bit)) & 1;
        }
    }
    
    debug_print_str("=== CORRUPTED FRAME ANALYSIS ===\r\n");
    
    // Analyse PDF1 (bits 25-85)
    uint64_t pdf1_corrupted = get_bit_field(corrupted_frame, 25, 61);
    uint32_t bch1_received = (uint32_t)get_bit_field(corrupted_frame, 86, 21);
    uint32_t bch1_correct = compute_bch1(pdf1_corrupted);
    
    debug_print_str("PDF1 Data: 0x");
    debug_print_hex64(pdf1_corrupted);
    debug_print_str("\r\nBCH1 Received: 0x");
    debug_print_hex24(bch1_received);
    debug_print_str("\r\nBCH1 Correct:  0x");
    debug_print_hex24(bch1_correct);
    debug_print_str("\r\nBCH1 Status: ");
    debug_print_str((bch1_received == bch1_correct) ? "VALID" : "CORRUPTED");
    debug_print_str("\r\n");
    
    // Analyse PDF2 (bits 107-132)
    uint32_t pdf2_corrupted = (uint32_t)get_bit_field(corrupted_frame, 107, 26);
    uint16_t bch2_received = (uint16_t)get_bit_field(corrupted_frame, 133, 12);
    uint16_t bch2_correct = compute_bch2(pdf2_corrupted);
    
    debug_print_str("PDF2 Data: 0x");
    debug_print_hex32(pdf2_corrupted);
    debug_print_str("\r\nBCH2 Received: 0x");
    debug_print_hex16(bch2_received);
    debug_print_str("\r\nBCH2 Correct:  0x");
    debug_print_hex16(bch2_correct);
    debug_print_str("\r\nBCH2 Status: ");
    debug_print_str((bch2_received == bch2_correct) ? "VALID" : "CORRUPTED");
    debug_print_str("\r\n");
    
    // Analyse Frame Sync
    uint16_t sync_received = (uint16_t)get_bit_field(corrupted_frame, 16, 9);
    debug_print_str("Frame Sync Received: 0x");
    debug_print_hex16(sync_received);
    debug_print_str(" (Expected Test: 0x");
    debug_print_hex16(SYNC_SELF_TEST);
    debug_print_str(", Normal: 0x");
    debug_print_hex16(SYNC_NORMAL_LONG);
    debug_print_str(")\r\n");
}

// =============================
// CORRECTION 1: BCH polynomial validation
// =============================
void validate_bch_polynomials(void) {
    debug_print_str("=== BCH POLYNOMIAL VALIDATION ===\r\n");
    
    // Test avec vecteurs CS-T001 Annexe C
    uint64_t test_pdf1 = 0x11C662468AC5600ULL;  // Annexe C.3.1
    uint32_t expected_bch1 = 0x53E3E;
    uint32_t calculated_bch1 = compute_bch1(test_pdf1);
    
    debug_print_str("BCH1 Test Vector:\r\n");
    debug_print_str("  Input: 0x");
    debug_print_hex64(test_pdf1);
    debug_print_str("\r\n  Expected: 0x");
    debug_print_hex24(expected_bch1);
    debug_print_str("\r\n  Calculated: 0x");
    debug_print_hex24(calculated_bch1);
    debug_print_str("\r\n  Status: ");
    debug_print_str((calculated_bch1 == expected_bch1) ? "CORRECT" : "POLYNOMIAL ERROR");
    debug_print_str("\r\n");
    
    // Test BCH2
    uint32_t test_pdf2 = 0x0DB0040;
    uint16_t expected_bch2 = 0x679;
    uint16_t calculated_bch2 = compute_bch2(test_pdf2);
    
    debug_print_str("BCH2 Test Vector:\r\n");
    debug_print_str("  Input: 0x");
    debug_print_hex32(test_pdf2);
    debug_print_str("\r\n  Expected: 0x");
    debug_print_hex16(expected_bch2);
    debug_print_str("\r\n  Calculated: 0x");
    debug_print_hex16(calculated_bch2);
    debug_print_str("\r\n  Status: ");
    debug_print_str((calculated_bch2 == expected_bch2) ? "CORRECT" : "POLYNOMIAL ERROR");
    debug_print_str("\r\n");
}

// =============================
// CORRECTION 2: Frame sync pattern fix
// =============================
void fix_frame_sync_pattern(void) {
    debug_print_str("=== FRAME SYNC PATTERN CORRECTION ===\r\n");
    
    // Patterns corrects selon CS-T001
    debug_print_str("Correct Sync Patterns:\r\n");
    debug_print_str("  Normal Mode: 0x");
    debug_print_hex16(SYNC_NORMAL_LONG);
    debug_print_str(" (binary: 000101111)\r\n");
    debug_print_str("  Self-Test:   0x");
    debug_print_hex16(SYNC_SELF_TEST);
    debug_print_str(" (binary: 011010000)\r\n");
    
    // Vérification implémentation
    if(SYNC_NORMAL_LONG != 0x2F) {
        debug_print_str("ERROR: SYNC_NORMAL_LONG incorrect!\r\n");
    }
    if(SYNC_SELF_TEST != 0x1A0) {
        debug_print_str("ERROR: SYNC_SELF_TEST incorrect!\r\n");
    }
}

// =============================
// CORRECTION 3: Construction frame corrigée
// =============================
void build_corrected_test_frame(void) {
    debug_print_str("=== BUILDING CORRECTED FRAME ===\r\n");
    
    // Set test position (Andorra comme dans l'exemple)
    set_gps_position(42.954632, 1.364479, 1080.0);
    beacon_mode = BEACON_MODE_TEST;
    
    // Construction frame
    uint8_t frame[MESSAGE_BITS] = {0};
    
    // Preamble (bits 1-15)
    set_bit_field(frame, 1, 15, 0x7FFF);
    
    // Frame sync CORRECT pour mode test (bits 16-24)
    set_bit_field(frame, 16, 9, SYNC_SELF_TEST);  // 0x1A0 = 011010000
    
    // Format flag (bit 25)
    set_bit_field(frame, 25, 1, 1);
    
    // Protocol flag (bit 26)
    set_bit_field(frame, 26, 1, 0);
    
    // Country code France (bits 27-36)
    set_bit_field(frame, 27, 10, 227);  // France
    
    // Protocol ELT-DT (bits 37-40)
    set_bit_field(frame, 37, 4, 0x9);
    
    // Aircraft address (bits 41-66)
    set_bit_field(frame, 41, 26, 0x123456);
    
    // Position GPS (bits 67-85)
    cs_gps_position_t pos = encode_gps_position_complete(42.954632, 1.364479);
    set_bit_field(frame, 67, 19, pos.fine_position_19bit);
    
    // BCH1 CORRECT (bits 86-106)
    uint64_t pdf1_data = get_bit_field(frame, 25, 61);
    uint32_t bch1_correct = compute_bch1(pdf1_data);
    set_bit_field(frame, 86, 21, bch1_correct);
    
    // PDF2 activation (bits 107-108)
    set_bit_field(frame, 107, 2, 0);  // Manual activation
    
    // Altitude (bits 109-112)
    uint8_t alt_code = altitude_to_code(1080.0);
    set_bit_field(frame, 109, 4, alt_code);
    
    // Freshness (bits 113-114)
    set_bit_field(frame, 113, 2, 2);  // >2s, <=60s
    
    // Position offset (bits 115-132)
    set_bit_field(frame, 115, 18, pos.offset_position_18bit);
    
    // BCH2 CORRECT (bits 133-144)
    uint32_t pdf2_data = get_bit_field(frame, 107, 26);
    uint16_t bch2_correct = compute_bch2(pdf2_data);
    set_bit_field(frame, 133, 12, bch2_correct);
    
    // Copie vers buffer global
    memcpy((void*)beacon_frame, frame, MESSAGE_BITS);
    
    // Validation finale
    uint64_t pdf1_check = get_bit_field(frame, 25, 61);
    uint32_t bch1_check = (uint32_t)get_bit_field(frame, 86, 21);
    uint32_t bch1_calc = compute_bch1(pdf1_check);
    
    uint32_t pdf2_check = (uint32_t)get_bit_field(frame, 107, 26);
    uint16_t bch2_check = (uint16_t)get_bit_field(frame, 133, 12);
    uint16_t bch2_calc = compute_bch2(pdf2_check);
    
    debug_print_str("=== CORRECTED FRAME VALIDATION ===\r\n");
    debug_print_str("BCH1: 0x");
    debug_print_hex24(bch1_calc);
    debug_print_str(" vs 0x");
    debug_print_hex24(bch1_check);
    debug_print_str((bch1_calc == bch1_check) ? " ✓" : " ✗");
    debug_print_str("\r\nBCH2: 0x");
    debug_print_hex16(bch2_calc);
    debug_print_str(" vs 0x");
    debug_print_hex16(bch2_check);
    debug_print_str((bch2_calc == bch2_check) ? " ✓" : " ✗");
    debug_print_str("\r\n");
    
    // Hex dump frame corrigée
    debug_print_str("Corrected Frame HEX: ");
    for(int byte = 0; byte < 18; byte++) {
        uint8_t byte_val = 0;
        for(int bit = 0; bit < 8; bit++) {
            byte_val = (byte_val << 1) | (frame[byte * 8 + bit] & 1);
        }
        debug_print_hex(byte_val);
    }
    debug_print_str("\r\n");
}

// =============================
// CORRECTION 4: Diagnostic complet
// =============================
/*
void full_error_diagnostic(void) {
    debug_print_str("\r\n");
    debug_print_str("==============================================\r\n");
    debug_print_str("CS-T001 ERROR DIAGNOSTIC & CORRECTION\r\n");
    debug_print_str("==============================================\r\n");
    
    // 1. Analyse frame corrompue
    analyze_corrupted_frame();
    debug_print_str("\r\n");
    
    // 2. Validation polynômes
    validate_bch_polynomials();
    debug_print_str("\r\n");
    
    // 3. Correction sync pattern
    fix_frame_sync_pattern();
    debug_print_str("\r\n");
    
    // 4. Construction frame corrigée
    build_corrected_test_frame();
    debug_print_str("\r\n");
    
    debug_print_str("==============================================\r\n");
    debug_print_str("DIAGNOSTIC COMPLETED\r\n");
    debug_print_str("==============================================\r\n");
}*/
void full_error_diagnostic(void) {
    // V�rification initiale du mat�riel
    debug_print_str("=== DIAGNOSTIC SYSTEME ===\r\n");
    
    // 1. Validation polyn�mes BCH
    if(BCH1_POLY != 0x26D9E3 || BCH2_POLY != 0x1539) {
        debug_print_str("ERREUR: Polynomes BCH non conformes!\r\n");
        while(1); // Blocage syst�me
    }
    
    // 2. Test GPS - SUPPRIMER la variable 'pos' inutilis�e
    set_gps_position(TEST_LATITUDE, TEST_LONGITUDE, TEST_ALTITUDE);
    encode_gps_position_complete(current_latitude, current_longitude);
    
    // 3. V�rification m�moire
    if(sizeof(beacon_frame) != MESSAGE_BITS) {
        debug_print_str("ERREUR: Taille frame incorrecte!\r\n");
    }
    
    debug_print_str("Diagnostic termine\r\n");
}

// =============================
// CORRECTION 5: Vérification constantes
// =============================
void verify_cs_t001_constants(void) {
    debug_print_str("=== CS-T001 CONSTANTS VERIFICATION ===\r\n");
    
    // Vérification polynômes BCH
    debug_print_str("BCH1_POLY: 0x");
    debug_print_hex24(BCH1_POLY);
    debug_print_str((BCH1_POLY == 0x26D9E3) ? " ✓" : " ✗ SHOULD BE 0x26D9E3");
    debug_print_str("\r\n");
    
    debug_print_str("BCH2_POLY: 0x");
    debug_print_hex16(BCH2_POLY);
    debug_print_str((BCH2_POLY == 0x1539) ? " ✓" : " ✗ SHOULD BE 0x1539");
    debug_print_str("\r\n");
    
    // Vérification sync patterns
    debug_print_str("SYNC_NORMAL_LONG: 0x");
    debug_print_hex16(SYNC_NORMAL_LONG);
    debug_print_str((SYNC_NORMAL_LONG == 0x2F) ? " ✓" : " ✗ SHOULD BE 0x2F");
    debug_print_str("\r\n");
    
    debug_print_str("SYNC_SELF_TEST: 0x");
    debug_print_hex16(SYNC_SELF_TEST);
    debug_print_str((SYNC_SELF_TEST == 0x1A0) ? " ✓" : " ✗ SHOULD BE 0x1A0");
    debug_print_str("\r\n");
    
    // Vérification country code
    debug_print_str("COUNTRY_CODE_FRANCE: ");
    debug_print_int32(COUNTRY_CODE_FRANCE);
    debug_print_str((COUNTRY_CODE_FRANCE == 227) ? " ✓" : " ✗ SHOULD BE 227");
    debug_print_str("\r\n");
}
