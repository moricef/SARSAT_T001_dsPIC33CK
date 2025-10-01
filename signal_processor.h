#ifndef SIGNAL_PROCESSOR_H
#define SIGNAL_PROCESSOR_H

#include <stdint.h>

void signal_processor_init(void);
uint16_t signal_processor_get_biphase_l_value(uint8_t bit_value, uint16_t sample_index, uint16_t samples_per_bit);

#endif
