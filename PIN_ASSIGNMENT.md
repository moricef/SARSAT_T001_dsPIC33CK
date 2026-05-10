# dsPIC33CK64MC105 — Pinout SARSAT T.001

## Port A — Clavier 4x4 (lignes sorties)

| Pin  | Fonction   | Dir    | Description          |
|------|------------|--------|----------------------|
| RA0  | KP_ROW0    | Output | Ligne 0 clavier      |
| RA1  | KP_ROW1    | Output | Ligne 1 clavier      |
| RA2  | KP_ROW2    | Output | Ligne 2 clavier      |
| RA3  | DACOUT     | Output | DAC interne (analog) |
| RA4  | KP_ROW3    | Output | Ligne 3 clavier      |

## Port B — RF chain

| Pin  | RP# | Fonction     | Dir    | Description                   |
|------|-----|--------------|--------|-------------------------------|
| RB7  | 39  | SCK2         | Output | SPI2 Horloge (MCP4922)        |
| RB8  | 40  | SDO2         | Output | SPI2 Data (MCP4922)           |
| RB9  | 41  | CS_DAC       | Output | SPI2 Chip Select MCP4922      |
| RB10 | 42  | AMP_ENABLE   | Output | PA ON/OFF via PTT MOSFET 2N7000 |
| RB11 | 43  | RF_PWR_SEL   | Output | Inutilisé (H46S, 5W permanent)|

## Port C — SPI1 / UART / Clavier (colonnes)

| Pin  | RP# | Fonction     | Dir    | Description                        |
|------|-----|--------------|--------|------------------------------------|
| RC0  | 48  | SDO1         | Output | SPI1 Data (ADF4351 / ADL5375)      |
| RC1  | 49  | ADF4351_LD   | Input  | ADF4351 Lock Detect (pull-down)    |
| RC2  | 50  | SCK1         | Output | SPI1 Horloge (ADF4351)             |
| RC3  | 51  | ADF4351_LE   | Output | ADF4351 Latch Enable               |
| RC4  | 52  | U3TX         | Output | UART3 TX → GNSS RX (optionnel)     |
| RC5  | 53  | U3RX         | Input  | UART3 RX ← GNSS TX (NMEA 9600)    |
| RC6  | 54  | KP_COL0      | Input  | Colonne 0 clavier (pull-up interne)|
| RC7  | 55  | KP_COL1      | Input  | Colonne 1 clavier (pull-up interne)|
| RC8  | 56  | ADF4351_RF_EN| Output | ADF4351 RF Output Enable           |
| RC9  | 57  | ADF4351_CE   | Output | ADF4351 Chip Enable                |
| RC10 | 58  | U2TX         | Output | UART2 TX → Debug console (115200)  |
| RC11 | 59  | U2RX         | Input  | UART2 RX ← Debug console          |
| RC12 | 60  | KP_COL2      | Input  | Colonne 2 clavier (pull-up interne)|
| RC13 | 61  | KP_COL3      | Input  | Colonne 3 clavier (pull-up interne)|

## Port D — LCD I2C / LED

| Pin  | Fonction   | Dir    | Description                          |
|------|------------|--------|--------------------------------------|
| RD1  | I2C_SDA    | I/O    | SDA I2C logiciel → LCD PCF8574 0x27  |
| RD8  | I2C_SCL    | I/O    | SCL I2C logiciel → LCD PCF8574 0x27  |
| RD10 | LED_TX     | Output | LED témoin émission                  |

---

## Périphériques connectés

### Écran LCD 20x4 — HD44780 via PCF8574
| Signal | MCU  | Description                    |
|--------|------|--------------------------------|
| SCL    | RD8  | I2C logiciel, 5 µs/bit         |
| SDA    | RD1  | I2C logiciel, open-drain       |
| Addr   | 0x27 | PCF8574 avec A0=A1=A2=1        |

Disposition affichage (rotation 5s, 3 écrans) :
- Ligne 0 : MODE TEST 5s / MODE EXERC 50s
- Ligne 1 : Freq:431.975 MHz (modifiable A=Hz B=. #=OK)
- Ligne 2 : GNSS:fix ↔ Position GPS/Tlse ↔ Aide (rotation)
- Ligne 3 : ID:AD0911 (permanent, sauf saisie fréquence)

### Clavier matriciel 4x4
| Signal | MCU  | Clé associée (col→)    |
|--------|------|------------------------|
| ROW0   | RA0  | 1  4  7  *             |
| ROW1   | RA1  | 2  5  8  0             |
| ROW2   | RA2  | 3  6  9  #             |
| ROW3   | RA4  | A  B  C  D             |
| COL0   | RC6  | ↑ pull-up interne      |
| COL1   | RC7  | ↑ pull-up interne      |
| COL2   | RC12 | ↑ pull-up interne      |
| COL3   | RC13 | ↑ pull-up interne      |

Fonctions spéciales :
- `A` : entrer fréquence
- `B` : point décimal (pendant saisie fréq)
- `*` : effacement (pendant saisie fréq)
- `#` : confirmer fréquence
- `D` : annuler saisie
- `C` : basculer mode TEST / EXERCISE (hors TX)

### Module GNSS — UART3
| Signal | MCU  | Description                  |
|--------|------|------------------------------|
| TX     | RC5  | RP53 — U3RX (réception NMEA) |
| RX     | RC4  | RP52 — U3TX (commandes opt.) |
| Baud   | 9600 | NMEA standard                |

Position fallback (sans fix) : CNES Toulouse — 43.5647°N, 1.4823°E

### ADF4351 — SPI1 (PLL 431.975 MHz, ref OCXO 10 MHz)
| Signal | MCU  | Description              |
|--------|------|--------------------------|
| CLK    | RC2  | SPI1 SCK                 |
| DATA   | RC0  | SPI1 SDO                 |
| LE     | RC3  | Latch Enable             |
| CE     | RC9  | Chip Enable              |
| RF_EN  | RC8  | RF Output Enable         |
| LD     | RC1  | Lock Detect (input)      |

### MCP4922 DAC — SPI2 (I/Q 12-bit)
| Signal | MCU  |
|--------|------|
| CLK    | RB7  |
| SDI    | RB8  |
| CS     | RB9  |
| LDAC   | RB15 |

### Chaîne RF externe (PCB Mod_QPSK_DSPpic)
| Étage       | Composant    | Gain    | Alim   |
|-------------|--------------|---------|--------|
| Modulateur  | ADL5375      | ~0 dBm  | 5V     |
| Driver 1    | PGA-103+     | ~11 dB  | 5V     |
| Driver 2    | PHA-13LN+    | ~18 dB  | 5V     |
| PA          | RA07M4047M   | 5W      | 7.2V   |
| PA board    | H46S (Enigma)| trimmer | Vgg fixe|

Bias ADL5375 I/Q : R15-R18 = 2.2kΩ, R21 strap → 500 mV.
