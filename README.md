# SARSAT T.001 Beacon - dsPIC33CK Implementation

Firmware for COSPAS-SARSAT T.001 beacon generator using dsPIC33CK64MC105.

**Status: OTA validated — 5W PA, SDR decode 6/6 CRC OK**

## Hardware

- **MCU**: dsPIC33CK64MC105 @ 50 MHz
- **PLL**: ADF4351 (431.975 MHz, ref OCXO 10 MHz, R_counter=1)
- **DAC**: MCP4922 dual 12-bit SPI2 (I/Q channels)
- **Modulator**: ADL5375 I/Q (bias 1.65V, swing 750 mV p-p)
- **Drivers**: PGA-103+ (11 dB) → PHA-13LN+ (18 dB)
- **PA**: RA07M4047M via H46S board, 5W permanent (Vdd 7.2V)
- **Filter**: 4th order Bessel lowpass 800 Hz (2x Sallen-Key)
- **GPS**: Ublox NEO-6M @ 9600 baud (UART3, RC5/RC4)
- **LCD**: 20x4 HD44780 via PCF8574 I2C (RD1/RD8, addr 0x27)
- **Keypad**: 4x4 matrix (RA0-RA4 rows, RC6/RC7/RC12/RC13 cols)

## RF Chain

```
ADF4351 (431.975 MHz) → ADL5375 → PGA-103+ → PHA-13LN+ → RA07M4047M → Antenne
         ↑                    ↑                                   ↑
     MCP4922 DAC           Bessel 800Hz                      H46S + PTT
     I/Q (SPI2)           LM358 buffer                     (RB10/MOSFET)
```

## Protocol

- **Standard**: SARSAT T.001 BPSK, User Test Protocol (0xC)
- **Sync**: 0x0D0 (Self-Test)
- **Country**: 227 (France)
- **Beacon ID**: placeholder AD0911 (awaiting SPOC/MCC attribution)
- **BCH**: BCH1 22-bit (0x26D9E3), BCH2 12-bit (0x1539)
- **Position fallback**: CNES Toulouse 43.5647°N, 1.4823°E

## Modulation

- **Symbol rate**: 400 baud
- **Encoding**: Biphase-L
- **Sample rate**: 6400 Hz (16 samples/symbol)
- **Phase shift**: ±1.1 rad
- **Message**: 144 bits (long frame)
- **Carrier**: 160 ms, **Data**: 360 ms

## Transmission Modes

| Mode | Interval | LCD | Power |
|------|----------|-----|-------|
| TEST | 5s | MODE TEST 5s | 5W |
| EXERCISE | 50s | MODE EXERC 50s | 5W |

## LCD Display (20x4, rotation 3 écrans / 5s)

| Écran | L0 | L1 | L2 | L3 |
|-------|----|----|----|-----|
| 1 | MODE | Freq:431.975 MHz | GNSS:fix / no fix | ID:AD0911 |
| 2 | MODE | Freq:431.975 MHz | Position GPS/Tlse | ID:AD0911 |
| 3 | A=Frequence | B=. | C=Mode TEST/EXERCICE | #=Valider |

Keypad: A=Hz entry, B=dot, C=Mode toggle, *=back, #=confirm, D=cancel

## Pin Assignment

See [PIN_ASSIGNMENT.md](PIN_ASSIGNMENT.md)

## OTA Validation (2026-05-09)

- **Decoder**: dec406_v10.2 via scan406.pl + RTL-SDR
- **Result**: 6/6 frames CRC OK
- **Position**: 43.56444 N, 1.48222 E (Toulouse fallback)
- **Frequency**: 431.975 MHz (OCXO 10 MHz, PLL locked)
- **Auto-gain RTL-SDR**: prevents ADC saturation on strong signals

## ADF4351 Calibration

- **REF_HZ**: 10000000 (OCXO 10 MHz, R_counter=1)
- **Measured**: 431.975.130 MHz → error 0.3 ppm (within PLL resolution)
- **MOD**: 4095 → RF step ~305 Hz

## Build

MPLAB X IDE with XC-DSC compiler v3.21+.

## Related Repos

- [dec406_V10.2](/home/fab2/Developpement/COSPAS-SARSAT/balise_406MHz/dec406_V10.2) — SDR decoder (scan406.pl, MIC_406.pl)
- [Mod_QPSK_DSPpic](/home/fab2/Developpement/COSPAS-SARSAT/Mod_QPSK_DSPpic) — RF PCB (MMIC drivers + filter)

## License

This project is licensed under the MIT License. See `LICENSE`.
