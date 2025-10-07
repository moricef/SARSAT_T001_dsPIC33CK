# SARSAT T.001 Beacon - dsPIC33CK Implementation

Firmware for COSPAS-SARSAT T.001 emergency beacon using dsPIC33CK64MC105 microcontroller.

## Hardware

- **MCU**: dsPIC33CK64MC105 @ 100 MHz
- **PLL**: ADF4351 (403 MHz)
- **Modulator**: ADL5375 I/Q modulator
- **DAC**: Internal 12-bit DAC (RA3 output)
- **Filter**: 4th order Bessel lowpass 600Hz (2x LM358P Sallen-Key stages)

## Signal Chain

```
DAC (RA3) → Bessel Filter → C 47µF → R 12Ω → QBBP (ADL5375)
                                                ↓
                                     5V → R 470Ω → QBBP
                                                ↓
                                             R 50Ω → GND
```

**Other ADL5375 inputs (IBBP, QBBN, IBBN):**
```
5V → R 470Ω → Input → R 50Ω → GND
                 ↓
           C 4.7µF // C 100nF → GND
```

## DAC Voltage Levels

- **Bias**: 1.65V (3.3V/2)
- **Swing**: ±500mV (1000mV p-p)
- **Range**: 1.15V - 2.15V
- **Idle**: 0V
- **Carrier**: 1.65V
- **BPSK modulation**: ±1.1 rad phase shift

## Modulation

- **Standard**: SARSAT T.001 BPSK
- **Symbol rate**: 400 baud
- **Encoding**: Biphase-L (Manchester)
- **Sample rate**: 6400 Hz (16 samples/symbol)
- **Message length**: 144 bits
- **Carrier duration**: 160 ms
- **Data duration**: 360 ms

## Filter Characteristics

- **Type**: Active Bessel 4th order
- **Cutoff**: 600 Hz (R scaled 2×)
- **Stages**: 2x 2nd order Sallen-Key
- **Gain**: Unity (1.0)
- **Values**: R1a=R2a=R1b=R2b=20kΩ, C1a=9.679nF, C2a=8.885nF, C1b=13.324nF, C2b=5.135nF

## Signal Levels

- **DAC output (RA3)**: 896mV p-p (1.20V - 2.10V)
- **Filter output**: 896mV p-p
- **QBBP input**: 708mV p-p @ 12Ω, DC bias 480mV
- **Attenuation**: 79% (R 12Ω vs 45Ω load impedance)

## Demodulation Performance

**Test frequency**: 403.037 MHz (development/calibration)

### Initial tests (R 70Ω):
| Mode     | Frequency (MHz)       | R range (Ω) | Decoding |
|----------|----------------------|-------------|----------|
| NFM      | 403.037              | >65-70      | Good     |
| USB      | 403.036500           | 0-500       | 100%     |
| RAW IQ   | 403.038200-403.038500| 0-300       | ~100%    |

### Optimized configuration (R 12Ω):
| Parameter        | Value          | Notes                    |
|------------------|----------------|--------------------------|
| Coupling cap     | 47µF           | Improved low-freq response|
| Series R         | 12Ω            | Reduced attenuation       |
| QBBP amplitude   | 708mV p-p      | 5× improvement vs 70Ω    |
| Decoding         | Excellent      | Compatible scan406.pl     |

## Git Branches

- **master**: Original code (500mV bias, 500mV swing)
- **dac-internal-voltage-adjust**: Current code (1650mV bias, 1000mV swing)

## Reception & Decoding

**scan406.pl optimized parameters:**
- **SNR threshold**: 5 (instead of 6)
- **Gain**: +20 minimum
- **Scan step**: 100 Hz (instead of 400 Hz)
- **Center frequency**: 406.037 MHz (rtl_power)

## Build

MPLAB X IDE with XC16 compiler.

## Notes

- R 12Ω optimized for maximum amplitude while maintaining signal integrity
- C 47µF provides better low-frequency coupling for Biphase-L signal
- Bessel filter provides linear phase response critical for BPSK integrity
- Breadboard prototype - PCB required for production
- Test frequency 403.037 MHz - production beacons use 406.xxx MHz channels per T.012
