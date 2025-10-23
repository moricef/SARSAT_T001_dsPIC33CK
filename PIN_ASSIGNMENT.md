# dsPIC33CK Pin Assignment - COSPAS-SARSAT T.001 Beacon

## 📌 Pin Mapping Summary

### Port A
| Pin | RP# | Function | Direction | Description |
|-----|-----|----------|-----------|-------------|
| RA3 | - | DACOUT | Output | Internal DAC output (analog) |

### Port B
| Pin | RP# | Function | Direction | Description |
|-----|-----|----------|-----------|-------------|
| RB0 | RP32 | Debug Pin | Output | General debug GPIO |
| RB1 | RP33 | Frequency Select | Input | Configuration switch (pull-down) |
| RB2 | RP34 | Frame Type Select | Input | TEST/EXERCISE mode switch (pull-down) |
| RB3 | RP36 | U1RX | Input | UART1 RX (currently unused) |
| RB4 | RP35 | U1TX | Output | UART1 TX (currently unused) |
| RB5 | RP37 | - | **Available** | **FREE** |
| RB6 | RP38 | - | **Available** | **FREE** |
| RB7 | RP39 | SCK2 | Output | SPI2 Clock (MCP4922 DAC) |
| RB8 | RP40 | SDO2 | Output | SPI2 Data Out (MCP4922 DAC) |
| RB9 | RP41 | CS2_DAC | Output | SPI2 Chip Select (MCP4922) |
| RB10 | RP42 | RF_AMP_EN | Output | RF Amplifier Enable |
| RB11 | RP43 | RF_PWR_SEL | Output | RF Power Level Select |

### Port C
| Pin | RP# | Function | Direction | Description |
|-----|-----|----------|-----------|-------------|
| RC0 | RP48 | SDO1 | Output | SPI1 Data Out (ADF4351) |
| RC1 | RP49 | CS_ADF4351 | Output | ADF4351 Chip Select |
| RC2 | RP50 | SCK1 | Output | SPI1 Clock |
| RC3 | RP51 | CS_ADL5375 | Output | ADL5375 Chip Select |
| RC4 | RP52 | **U3RX** | **Input** | **UART3 RX (GPS)** ✅ |
| RC5 | RP53 | **U3TX** | **Output** | **UART3 TX (GPS)** ✅ |
| RC6 | RP54 | - | **Available** | **FREE** |
| RC7 | RP55 | - | **Available** | **FREE** |
| RC8 | RP56 | LE_ADF4351 | Output | ADF4351 Latch Enable |
| RC9 | RP57 | ADF4351_LD | Input | ADF4351 Lock Detect |
| RC10 | RP58 | U2TX | Output | UART2 TX (Debug) |
| RC11 | RP59 | U2RX | Input | UART2 RX (Debug) |

### Port D
| Pin | RP# | Function | Direction | Description |
|-----|-----|----------|-----------|-------------|
| RD10 | - | LED_TX | Output | Transmission LED indicator |
| RD13 | - | BUTTON | Input | Reset/Mode button (pull-up) |

## 🔌 Available Pins for Future Expansion

### Fully Available:
- **RB5** (RP37) - Digital I/O
- **RB6** (RP38) - Digital I/O
- **RC6** (RP54) - Digital I/O
- **RC7** (RP55) - Digital I/O

## 📡 UART Configuration

### UART1 (RB3/RB4) - Communication
- **Status**: Configured but **UNUSED**
- **Baud**: Configurable
- **Pins**: RB3 (RX/RP36), RB4 (TX/RP35)
- **Notes**: Buffer circulaire `rxQueue[]` configuré mais jamais lu

### UART2 (RC10/RC11) - Debug Output
- **Status**: **ACTIVE**
- **Baud**: Configurable (typically 115200)
- **Pins**: RC10 (TX/RP58), RC11 (RX/RP59)
- **Function**: Debug messages, command input via `process_uart_commands()`

### UART3 (RC4/RC5) - GPS NMEA
- **Status**: **ACTIVE** (GPS Module)
- **Baud**: 9600 (standard NMEA)
- **Pins**: RC4 (RX/RP52), RC5 (TX/RP53)
- **Function**: GPS NMEA sentence reception
- **PPS Config**: Automatic in `gps_init()`

## 🎛️ SPI Configuration

### SPI1 (ADF4351 + ADL5375)
- **SCK**: RC2 (RP50)
- **SDO**: RC0 (RP48)
- **CS_ADF4351**: RC1 (RP49)
- **CS_ADL5375**: RC3 (RP51)
- **LE_ADF4351**: RC8 (RP56)
- **LD_ADF4351**: RC9 (RP57) - Input

### SPI2 (MCP4922 DAC)
- **SCK**: RB7 (RP39)
- **SDO**: RB8 (RP40)
- **CS**: RB9 (RP41)

## ⚡ Power and Control

### RF Control
- **Amplifier Enable**: RB10 (RP42)
- **Power Level Select**: RB11 (RP43)

### User Interface
- **TX LED**: RD10
- **Mode Button**: RD13 (with pull-up)
- **Frequency Switch**: RB1 (with pull-down)
- **Frame Type Switch**: RB2 (with pull-down)

## 📝 Notes

1. **RP Numbers**: Remappable pins follow the pattern RPx where x is the RP number
2. **PPS Configuration**: Use `__builtin_write_OSCCONL()` to unlock/lock PPS
3. **Analog Pins**: Set `ANSELx = 0` for digital mode
4. **UART1**: Currently unused - available for future repurposing
5. **GPS Module**: Connect to RC4 (GPS TX → U3RX) and RC5 (GPS RX → U3TX, optional)

## 🔧 GPIO Initialization Order

1. Disable all analog functions (`ANSELA/B/C/D = 0`)
2. Configure TRIS registers (1=input, 0=output)
3. Configure ANSEL registers for specific analog needs (e.g., RA3 for DAC)
4. Configure PPS for UARTs and SPI
5. Initialize pull-ups/pull-downs (CNPU/CNPD)
6. Initialize peripheral modules (SPI, UART, DAC, etc.)
