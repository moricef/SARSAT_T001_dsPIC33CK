# 🛰️ COSPAS-SARSAT Beacon Generator

[![Build Status](https://img.shields.io/badge/MPLAB%20X-Compile%20Success-brightgreen)](https://github.com)
[![COSPAS-SARSAT](https://img.shields.io/badge/Standard-T.001%20Compliant-blue)](https://cospas-sarsat.int)
[![License](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-orange)](LICENSE)

> **Générateur de balises 406 MHz conforme COSPAS-SARSAT pour formation ADRASEC et exercices SATER**

## 📡 Architecture RF

```
dsPIC33CK64MC105 → ADF4351 (403MHz PLL) → ADL5375-05 (I/Q Mod) → RA07M4047M (PA) → 403MHz Output
        ↓              ↓                        ↓                      ↓
    DAC + SPI     25MHz → 403MHz         I=500mV, Q=DAC+bias      100mW / 5W
                                        (Interface adaptée)
```

### Interface dsPIC33CK ↔ ADL5375-05
```
RA3 DACOUT → Circuit adaptation → QBBP (canal Q modulé)
(0-1V)       R1(50Ω) + C1(100nF)   QBBN (500mV bias)
                                   IBBP (500mV constant)  
                                   IBBN (500mV constant)
```

## 🎯 Fonctionnalités

### ✅ Générateur de trames 1ère Génération (T.001)
- **Modulation** : Biphase-L BPSK (±1.1 rad)
- **Trames** : 112/144 bits avec BCH error correction
- **Fréquences** : 403 MHz (évite fausses alertes SARSAT)
- **Modes** : TEST (5s) et EXERCICE (50s)

### 🔧 Hardware Supporté
- **Microcontrôleur** : dsPIC33CK64MC105 Curiosity Nano
- **PLL Synthesizer** : ADF4351 (35 MHz - 4.4 GHz)
- **I/Q Modulator** : ADL5375-05 (400 MHz - 6 GHz, bias 500mV)  
- **Power Amplifier** : RA07M4047M (400-520 MHz)
- **Interface Circuit** : Adaptation niveaux DAC → I/Q inputs

### ⚡ Puissances Configurables
- **100mW** : Tests locaux ADRASEC
- **5W** : Exercices SATER longue portée

## 🚀 Quick Start

### Prérequis
- MPLAB X IDE v6.25+
- XC-DSC Compiler v3.21+
- dsPIC33CK64MC105 Curiosity Nano
- Modules RF : ADF4351 + ADL5375-05 + RA07M4047M
- Circuit d'interface : Résistances 50Ω + condensateurs + référence 500mV

### Compilation
```bash
git clone [votre-repo]
cd SARSAT_IQ_BPSK_dsPIC33CK_RF.X
# Ouvrir avec MPLAB X IDE
# Build → Clean and Build
```

### Configuration
```c
// Mode TEST (balises fréquentes)
start_beacon_frame(BEACON_TEST_FRAME);

// Mode EXERCICE (balises 50s)  
start_beacon_frame(BEACON_EXERCISE_FRAME);

// Puissance
rf_set_power_level(RF_POWER_LOW);   // 100mW
rf_set_power_level(RF_POWER_HIGH);  // 5W
```

## 📋 Structure du Projet

```
SARSAT_IQ_BPSK_dsPIC33CK_RF.X/
├── main.c                  # Point d'entrée principal
├── system_comms.c/h        # Modulation I/Q et timing
├── rf_interface.c/h        # Drivers RF (ADF4351, ADL5375, PA)
├── protocol_data.c/h       # Génération trames T.001
├── system_debug.c/h        # Debugging et logs UART
├── ADL5375_INTERFACE_CIRCUIT.md # Circuit d'adaptation DAC → I/Q
├── Docs/                   # Documentation technique
│   ├── Microchip_PIC/      # Datasheets dsPIC33CK
│   ├── adf4351.pdf         # Datasheet PLL
│   ├── adl5375.pdf         # Datasheet I/Q modulator
│   └── T001-OCT-24-2024.pdf # Standard COSPAS-SARSAT
└── second_generation/      # Développement 2G (en cours)
```

## 🛠️ Validation Technique

### Conformité Standards
- ✅ **COSPAS-SARSAT T.001** : Trames 144 bits, BCH(21,15)
- ✅ **Modulation** : Biphase-L ±1.1 rad @ 400 bauds
- ✅ **Timing** : Preamble 160ms, Data 360ms, Postamble 320ms
- ✅ **GPS Encoding** : PDF-1 + PDF-2 avec offset 4 secondes

### Hardware Validation
- ✅ **dsPIC33CK64MC105** : Validé contre DS70005399D
- ✅ **I/Q Modulation** : DAC 12-bit → ADL5375-05 Q channel (500mV bias)
- ✅ **Interface Circuit** : Adaptation 0-3.3V → 0-1V + bias 500mV
- ✅ **RF Chain** : Séquences power-up/down optimisées

## 🎓 Usage ADRASEC/SATER

### Formation Locale (100mW)
```c
rf_set_power_level(RF_POWER_LOW);
start_beacon_frame(BEACON_TEST_FRAME);  // Trames toutes les 5s
```

### Exercices Régionaux (5W)
```c
rf_set_power_level(RF_POWER_HIGH);
start_beacon_frame(BEACON_EXERCISE_FRAME);  // Trames toutes les 50s
```

### Intégration Décodeur
Compatible avec le décodeur 406 MHz disponible dans `../dec406_v10.2/`

## 📊 Spécifications Techniques

| Paramètre | Valeur | Conformité |
|-----------|--------|------------|
| **Fréquence** | 403 MHz | Radiosondes (évite 406 MHz) |
| **Modulation** | Biphase-L BPSK | T.001 §A3.2.3 |
| **Débit** | 400 bauds | T.001 §A3.2.2 |
| **Phase shift** | ±1.1 radians | T.001 §A3.2.3.1 |
| **Puissance** | 100mW / 5W | Configurable |
| **Trame** | 144 bits | 15+9+120 bits |

## 🗺️ Roadmap

- [x] **Génération 1G** : Terminée et validée
- [x] **Modulation I/Q** : Optimisée pour ADL5375-05 (bias 500mV)
- [x] **Interface DAC→I/Q** : Circuit d'adaptation implémenté
- [x] **Architecture modulaire** : RF drivers séparés
- [ ] **Génération 2G** : BCH(250,202) en cours
- [ ] **Interface Web** : Configuration via navigateur
- [ ] **Tests Hardware** : Validation RF complète avec oscilloscope

## 🏆 Credits

**Based on original work by [loorisr/sarsat](https://github.com/loorisr/sarsat)**

Ce projet s'appuie sur les concepts et algorithmes du projet SARSAT original de loorisr, adapté pour une implémentation hardware embarquée dsPIC33CK avec modules RF dédiés. Voir [CREDITS.md](CREDITS.md) pour les remerciements complets.

## 📚 Documentation

### Datasheets Intégrées
- **dsPIC33CK64MC105** : [DS70005399D](Docs/Microchip_PIC/dsPIC33CK64MC105-Family-Data-Sheet-DS70005399D.pdf)
- **ADF4351** : [Datasheet](Docs/adf4351.pdf)
- **ADL5375** : [Datasheet](Docs/adl5375.pdf)
- **COSPAS-SARSAT T.001** : [Standard](Docs/T001-OCT-24-2024.pdf)

### Guides Techniques
- **Architecture RF** : [Schémas](Docs/Chaine\ RF\ complete.png)
- **Interface ADL5375-05** : [Circuit d'adaptation](ADL5375_INTERFACE_CIRCUIT.md)
- **Guide d'intégration** : [Documentation](Docs/Guide\ d'intégration\ dsPIC33CK\ +\ CS-T001.txt)

## 🤝 Contribution

Ce projet est destiné à la **formation ADRASEC** et aux **exercices SATER**. 

### Applications
- ✅ **Formation** : Tests décodeurs 406 MHz
- ✅ **Exercices** : Simulation balises détresse
- ✅ **Validation** : Tests conformité équipements
- ❌ **Usage réel** : ⚠️ Fréquence 403 MHz non autorisée pour vraies alertes

## 📧 Contact

**Développé pour les ADRASEC de France**
- **Usage** : Formation et exercices uniquement
- **Support** : Documentation technique incluse
- **Compatibilité** : Décodeur 406 MHz intégré

## ⚖️ Licence

**Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International**

Ce projet est sous licence [CC BY-NC-SA 4.0](LICENSE) - Usage éducatif et formation ADRASEC/SATER uniquement.

### Permissions
- ✅ **Partage** : Copier et redistribuer
- ✅ **Adaptation** : Modifier et développer  
- ✅ **Formation** : Usage ADRASEC/SATER

### Restrictions  
- ❌ **Commercial** : Pas d'usage commercial
- ⚠️ **Attribution** : Créditer l'auteur original
- 🔄 **ShareAlike** : Même licence pour modifications

---

> 🚨 **IMPORTANT** : Ce générateur utilise 403 MHz pour éviter les fausses alertes COSPAS-SARSAT. Usage formation uniquement.

> 📡 **ADRASEC** : Association Départementale des RadioAmateurs au service de la Sécurité Civile
