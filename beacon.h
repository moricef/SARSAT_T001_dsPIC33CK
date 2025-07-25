/* 
 * File:   beacon.h
 * Author: Fab2
 *
 * Created on 25 juillet 2025, 05:13
 */

#ifndef BEACON_H
#define	BEACON_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define MESSAGE_BITS 121

// Déclarations externes utilisées dans tout le projet
extern uint8_t beacon_frame[MESSAGE_BITS];
extern volatile size_t symbol_index;
extern volatile uint32_t sample_count;

extern volatile uint16_t debug_dac_value;
extern volatile uint8_t tx_phase;
extern volatile uint8_t carrier_phase;
extern volatile uint32_t preamble_count;
extern volatile uint16_t idle_count;

#ifdef	__cplusplus
}
#endif

#endif	/* BEACON_H */
