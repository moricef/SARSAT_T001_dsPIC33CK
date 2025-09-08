#include "signal_processor.h"
#include "system_comms.h"
#include <math.h>
#include <stdint.h>

static uint16_t phase_plus_value = 0;
static uint16_t phase_minus_value = 0;

void signal_processor_init(void) {
    // Calcul des valeurs DAC pour ±1.1 rad
    float voltage_plus = (ADL5375_BIAS_MV / 1000.0f) + 
                        (sinf(PHASE_SHIFT_RADIANS) * (ADL5375_SWING_MV / 2000.0f));
    float voltage_minus = (ADL5375_BIAS_MV / 1000.0f) + 
                         (sinf(-PHASE_SHIFT_RADIANS) * (ADL5375_SWING_MV / 2000.0f));
    
    phase_plus_value = (uint16_t)((voltage_plus * DAC_RESOLUTION) / VOLTAGE_REF_3V3);
    phase_minus_value = (uint16_t)((voltage_minus * DAC_RESOLUTION) / VOLTAGE_REF_3V3);
}

uint16_t signal_processor_get_biphase_l_value(uint8_t bit_value, uint16_t sample_index, uint16_t samples_per_bit) {
    uint16_t half_bit = samples_per_bit / 2;
    
    // Biphase-L encoding
    if (sample_index < half_bit) {
        // Première moitié du bit
        return (bit_value == 1) ? phase_minus_value : phase_plus_value;
    } else {
        // Seconde moitié - transition inverse
        return (bit_value == 1) ? phase_plus_value : phase_minus_value;
    }
}
