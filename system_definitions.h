#ifndef SYSTEM_DEFINITIONS_H
#define SYSTEM_DEFINITIONS_H

#include <stdint.h>

// États de la machine à états uniquement
#define PRE_AMPLI       0
#define PREAMBLE_PHASE  1
#define DATA_PHASE      2
#define POSTAMBLE_PHASE 3
#define POST_AMPLI      4
#define IDLE_STATE      6

// Déclarer les variables globales
extern volatile uint8_t tx_phase;
extern volatile uint8_t beacon_frame[];  // Taille définie dans protocol_data.h

// Debug macro
#define DEBUG_BREAKPOINT() do { asm("nop"); } while(0)


#endif