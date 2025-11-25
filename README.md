# SARSAT T.001 Beacon - dsPIC33CK Implementation

Firmware for COSPAS-SARSAT T.001 emergency beacon using dsPIC33CK64MC105 microcontroller.

**Status: Proof of Concept (breadboard prototype validated)**

## Hardware

- **MCU**: dsPIC33CK64MC105 @ 100 MHz
- **PLL**: ADF4351 (403 MHz)
- **Modulator**: ADL5375 I/Q modulator
- **DAC**: Internal 12-bit DAC (RA3 output)
- **Filter**: 4th order Bessel lowpass 800Hz (2x LM358P Sallen-Key stages)
- **GPS**: Ublox NEO-6M @ 9600 baud (UART3, RC5/RC4)

## Signal Chain

```
DAC (RA3) → Bessel Filter → C 47µF → R 15Ω → QBBP (ADL5375)
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
- **Cutoff**: 800 Hz
- **Stages**: 2x 2nd order Sallen-Key
- **Gain**: Unity (1.0)
- **Values**: R1a=R2a=R1b=R2b=15kΩ, C1a=9.679nF, C2a=8.885nF, C1b=13.324nF, C2b=5.135nF

## Signal Levels

- **DAC output (RA3)**: 896mV p-p (1.20V - 2.10V)
- **Filter output**: 896mV p-p
- **QBBP input**: 672mV p-p @ 15Ω, DC bias 480mV
- **Attenuation**: 75% (R 15Ω vs 45Ω load impedance)

## Demodulation Performance

**ADF4351 configured frequency**: 403.040 MHz (development/calibration)

**Optimized configuration (Bessel 800Hz, R 15Ω, C 47µF):**
| Mode     | Frequency (MHz)       | Decoding Performance |
|----------|----------------------|----------------------|
| NFM      | 403.034-403.040      | Excellent            |
| RAW IQ   | 403.040              | ~100% (on-frequency) |
| scan406  | 403.037 (centered)   | Compatible           |

**Signal chain:**
- Coupling cap: 47µF (improved low-freq response)
- Series R: 15Ω (optimized attenuation)
- QBBP amplitude: 672mV p-p
- Compatible with scan406.pl optimized parameters

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

MPLAB X IDE with XC-DSC compiler (v3.21 or later).

## Project Status

**Current stage: Proof of Concept - GPS Integration Validated**
- ✅ Breadboard prototype validated
- ✅ Signal chain optimized (DAC → Bessel filter → ADL5375)
- ✅ Excellent decoding performance (NFM 403.035-403.040 MHz)
- ✅ GPS receiver operational (Ublox NEO-6M, NMEA parsing, position encoding)
- ✅ GPS UART3 interrupt-driven reception (URXISEL=0b011, FIFO read optimization)
- ✅ Real GPS coordinates transmission validated (race condition and BCH2 bugs fixed)
- ⏭️ Next step: PCB design with proper RF layout and ground plane

## Critical Bugs Fixed

### BCH2_POLY_MASK Validation Failure (2025-11-25)
**Symptom**: System entered infinite loop when GPS obtained fix. Frame validation consistently failed with real GPS coordinates but succeeded in TEST mode with fixed coordinates.

**Root Cause**: `BCH2_POLY_MASK` was incorrectly set to `0x0FFF` (12-bit mask) instead of `0x1FFF` (13-bit mask). For a BCH polynomial of degree 12, the mask must accommodate 13 bits to handle all possible register values (0 to 2^13-1).

**Why It Was Hidden**: The bug was latent and only manifested when the calculated BCH2 value had bit 12 set to 1. With TEST coordinates (lat=42.95463, lon=1.364479), BCH2 never activated bit 12, so validation passed. With real GPS coordinates (lat=42.960594, lon=1.371029), BCH2=0x1E77, and the incorrect 0x0FFF mask truncated it to 0x0E77, causing validation mismatch.

**Fix**: Changed `BCH2_POLY_MASK` from `0x0FFF` to `0x1FFF` in `protocol_data.h`. System now successfully validates and transmits beacon frames with real GPS position data.

**Impact**: Geographic-dependent bug that affected beacons only when operating at specific locations where BCH2 calculation produces values with bit 12=1.

### GPS Race Condition (2025-11-23)
**Symptom**: GPS data corruption during frame construction, causing non-deterministic BCH validation failures.

**Root Cause**: GPS ISR updated `current_latitude/longitude/altitude` while `build_compliant_frame()` was reading them. On dsPIC33CK, `double` requires 4 memory accesses (64 bits), creating TOCTOU vulnerability.

**Fix**: Multi-layer protection implemented:
1. GPS variables declared `volatile`
2. Atomic snapshot with interrupts disabled in `build_compliant_frame()`
3. GPS ISR (UART3) disabled during entire EXERCISE frame TX sequence (construction → validation → transmission)
4. Atomic `memcpy()` to protect `beacon_frame[]` from Timer1 ISR reads

See `ISR_CONFLICTS.md` for detailed analysis.

## Notes

- R 15Ω optimized for maximum amplitude while maintaining signal integrity
- C 47µF provides better low-frequency coupling for Biphase-L signal
- Bessel filter provides linear phase response critical for BPSK integrity
- ADF4351 configured at 403.040 MHz - production beacons use 406.xxx MHz channels per T.012
- NFM decoding range: 403.034-403.040 MHz (6 kHz bandwidth)
- RAW IQ decodes on exact frequency (403.040 MHz)
- **PCB required for production** - breadboard introduces noise and impedance issues
