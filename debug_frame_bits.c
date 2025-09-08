// Debug pour calculer et afficher les 144 bits de la trame TEST
#include "protocol_data.h"
#include "system_debug.h"
#include "system_comms.h"
#include <string.h>

void debug_print_complete_144_bits(void) {
    DEBUG_LOG_FLUSH("=== CALCUL TRAME TEST 144 BITS ===\r\n");
    
    // Variables locales pour le calcul
    uint8_t frame[144];
    memset(frame, 0, 144);
    
    // === BITS 1-15: PREAMBLE (tous à 1) ===
    for (int i = 0; i < 15; i++) {
        frame[i] = 1;
    }
    DEBUG_LOG_FLUSH("Bits 1-15 (SYNC):     111111111111111\r\n");
    
    // === BITS 16-24: FRAME SYNC TEST (SYNC_SELF_TEST = 0x0D0 = 011010000) ===
    uint16_t sync_test = 0x0D0; // 011010000
    for (int i = 0; i < 9; i++) {
        frame[15 + i] = (sync_test >> (8 - i)) & 1;
    }
    DEBUG_LOG_FLUSH("Bits 16-24 (FRAME):   011010000\r\n");
    
    // === BIT 25: FORMAT FLAG (1 = Long message) ===
    frame[24] = 1;
    DEBUG_LOG_FLUSH("Bit 25 (FORMAT):      1\r\n");
    
    // === BIT 26: PROTOCOL FLAG (0 = Location) ===
    frame[25] = 0;
    DEBUG_LOG_FLUSH("Bit 26 (PROTOCOL):    0\r\n");
    
    // === BITS 27-36: COUNTRY CODE (France = 227 = 0011100011) ===
    uint16_t country = 227; // 0011100011
    for (int i = 0; i < 10; i++) {
        frame[26 + i] = (country >> (9 - i)) & 1;
    }
    DEBUG_LOG_FLUSH("Bits 27-36 (COUNTRY): 0011100011\r\n");
    
    // === BITS 37-40: PROTOCOL CODE (ELT-DT = 9 = 1001) ===
    uint8_t protocol = 9; // 1001
    for (int i = 0; i < 4; i++) {
        frame[36 + i] = (protocol >> (3 - i)) & 1;
    }
    DEBUG_LOG_FLUSH("Bits 37-40 (PROTO):   1001\r\n");
    
    // === BITS 41-66: BEACON ID (0x123456 sur 26 bits) ===
    uint32_t beacon_id = 0x123456; // Example ID
    DEBUG_LOG_FLUSH("Bits 41-66 (ID):      ");
    for (int i = 0; i < 26; i++) {
        frame[40 + i] = (beacon_id >> (25 - i)) & 1;
        DEBUG_LOG_FLUSH(frame[40 + i] ? "1" : "0");
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    // === BITS 67-85: GPS POSITION (calculée pour TEST coords) ===
    // Coordonnées TEST: 42.95463°N, 1.364479°E
    // Encodage simplifié pour demo - utiliserait encode_gps_position_complete()
    uint32_t gps_pos = 0x4A5B6; // Exemple encodage GPS 19 bits
    DEBUG_LOG_FLUSH("Bits 67-85 (GPS):     ");
    for (int i = 0; i < 19; i++) {
        frame[66 + i] = (gps_pos >> (18 - i)) & 1;
        DEBUG_LOG_FLUSH(frame[66 + i] ? "1" : "0");
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    // === BITS 86-106: BCH1 (21 bits) ===
    uint32_t bch1 = 0x1A2B3C; // Exemple BCH1
    DEBUG_LOG_FLUSH("Bits 86-106 (BCH1):   ");
    for (int i = 0; i < 21; i++) {
        frame[85 + i] = (bch1 >> (20 - i)) & 1;
        DEBUG_LOG_FLUSH(frame[85 + i] ? "1" : "0");
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    // === BITS 107-132: PDF2 (26 bits) ===
    uint32_t pdf2 = 0x2345678; // Exemple PDF2
    DEBUG_LOG_FLUSH("Bits 107-132 (PDF2):  ");
    for (int i = 0; i < 26; i++) {
        frame[106 + i] = (pdf2 >> (25 - i)) & 1;
        DEBUG_LOG_FLUSH(frame[106 + i] ? "1" : "0");
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    // === BITS 133-144: BCH2 (12 bits) ===
    uint16_t bch2 = 0xABC; // Exemple BCH2
    DEBUG_LOG_FLUSH("Bits 133-144 (BCH2):  ");
    for (int i = 0; i < 12; i++) {
        frame[132 + i] = (bch2 >> (11 - i)) & 1;
        DEBUG_LOG_FLUSH(frame[132 + i] ? "1" : "0");
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    // === SEQUENCE COMPLETE ===
    DEBUG_LOG_FLUSH("\r\nSEQUENCE COMPLETE 144 BITS:\r\n");
    for (int i = 0; i < 144; i++) {
        DEBUG_LOG_FLUSH(frame[i] ? "1" : "0");
        if ((i + 1) % 24 == 0) DEBUG_LOG_FLUSH("\r\n");
    }
    DEBUG_LOG_FLUSH("\r\n=== FIN CALCUL ===\r\n");
}