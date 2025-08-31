# COSPAS-SARSAT T001 - 1st Generation Beacon (dsPIC33CK64MC105)

[![Build Status](https://img.shields.io/badge/MPLAB%20X-Compile%20Success-brightgreen)](https://github.com)
[![COSPAS-SARSAT](https://img.shields.io/badge/Standard-T.001%20Compliant-blue)](https://cospas-sarsat.int)
[![License](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-orange)](LICENSE)

> **Générateur de trames conforme COSPAS-SARSAT sur fréquence 403 MHz pour formation ADRASEC et exercices SATER**

## Objectif

Générateur de trames 1ère génération conforme COSPAS-SARSAT pour :
- **Formation** des opérateurs ADRASEC  
- **Validation** de décodeurs SARSAT  
- **Exercices** de recherche et sauvetage  
- **Tests** de conformité T.001 (hors fréquence d'urgence 406MHz)

## Architecture Hardware

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



## Fonctionnalités

## Modes de Fonctionnement

### Mode TEST (balises fréquentes)
- **Objectif** : Validation décodeur et développement
- **Timing** : Trames toutes les 5 secondes
- **Données** : Trame prédictible/connue

### Mode EXERCICE (balises 50s)
- **Objectif** : Simulation exercices SATER
- **Timing** : Trames toutes les 50 secondes
- **Données** : Configuration réaliste

## Spécifications T.001

### Trame 144 bits
- **Modulation** : Biphase-L BPSK (±1.1 rad)
- **Trames** : 112/144 bits avec BCH error correction
- **Fréquences** : 403 MHz (évite fausses alertes SARSAT)
- **Débit** : 400 bauds
- **Phase shift** : ±1.1 radians

### Hardware Supporté

- **Microcontrôleur** : dsPIC33CK64MC105 Curiosity Nano
- **PLL Synthesizer** : ADF4351 (35 MHz - 4.4 GHz)
- **I/Q Modulator** : ADL5375-05 (400 MHz - 6 GHz, bias 500mV)  
- **Power Amplifier** : RA07M4047M (400-520 MHz)
- **Interface Circuit** : Adaptation niveaux DAC → I/Q inputs

### Configuration ADF4351 (403MHz)

**Registres de configuration (programmés via SPI):**
```c
const uint32_t adf4351_regs_403mhz[6] = {
    0x00580005,  // R5: Reserved settings + LD pin mode
    0x00BC802C,  // R4: Feedback select + RF divider
    0x000004B3,  // R3: 12-bit clock divider value
    0x18004E42,  // R2: Low noise and spur modes
    0x080080C9,  // R1: 8-bit prescaler value
    0x004000C0   // R0: 16-bit Integer-N value (403MHz)
};
```

**Paramètres de synthèse:**
- **Fréquence de référence** : 25MHz (oscillateur externe)
- **Fréquence de sortie** : 403MHz ±0.01% (formation/test)
- **Mode** : Integer-N PLL (optimisé pour bruit de phase)
- **Puissance de sortie** : +5dBm (3.2mW)
- **Temps de verrouillage** : <1ms typique, timeout 1s

**Outil de calcul:**
- **Logiciel** : Analog Devices ADF435x Evaluation Software
- **Configuration** : 25MHz REF_IN → 403MHz RF_OUT
- **Export** : Génère automatiquement les 6 valeurs de registres
- **Validation** : Simulation loop filter et bruit de phase
- **Download** : [ADI Evaluation Software](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/eval-adf4351.html)

**Modification des fréquences:**
1. Lancer l'ADF435x Evaluation Software
2. Configurer REF_IN = 25MHz, RF_OUT = fréquence désirée
3. Optimiser les paramètres PLL (Integer-N recommandé)
4. Copier les 6 valeurs générées dans `adf4351_regs_403mhz[]`
5. Recompiler et flasher le firmware

### Puissances Configurables
- **100mW** : Tests locaux ADRASEC
- **5W** : Exercices SATER longue portée

## Configuration Pins dsPIC33CK64MC105

```
Pin Assignment:
├── RA3   : DACOUT (sortie analogique)
├── RC0   : ADF4351 SPI Data (SDO1)
├── RC1   : ADF4351 Lock Detect (LD input + pull-down)
├── RC2   : ADF4351 SPI Clock (SCK1)
├── RC3   : ADF4351 Latch Enable (LE)
├── RC8   : ADF4351 RF Enable (RF_EN)
├── RC9   : ADF4351 Chip Enable (CE)
├── RB9   : ADL5375 Enable
├── RB10  : PA Control (100mW/5W)
├── RB13  : PA Status LED
├── RB15  : ADF4351 Lock LED
├── RD10  : System LED (transmission indicator)
├── RD13  : Emergency Reset Button (pull-up)
└── UART1 : Debug 115200 bps
```

## Build et Compilation

### Prérequis
- **MPLAB X IDE** v6.25 ou supérieur
- **XC-DSC Compiler** v3.21 (optimisé dsPIC33CK)
- dsPIC33CK64MC105 Curiosity Nano
- Modules RF : ADF4351 + ADL5375-05 + RA07M4047M
- Circuit d'interface : Résistances 50Ω + condensateurs + référence 500mV

### Commandes Build
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

## Sécurité et Gestion d'Erreurs

### Surveillance PLL ADF4351

**Mécanisme de retry à l'initialisation:**
- 3 tentatives maximum avec reprogrammation complète des registres
- En cas d'échec total → arrêt système avec signal SOS

**Surveillance continue en fonctionnement:**
- Vérification du verrouillage PLL toutes les 5 secondes
- Détection automatique des décrochages (RC1 = Lock Detect)
- Recovery automatique en cas de décrochage temporaire
- Arrêt sécurisé si recovery impossible

### Signal d'Erreur Critique (SOS)

**Pattern Morse sur RD10:** `...---...` (SOS international)
- **DIT**: 200ms ON, 200ms OFF
- **DAH**: 600ms ON, 200ms OFF  
- **Espacement lettres**: 600ms
- **Espacement mots**: 1400ms
- **Cycle complet**: ~6 secondes, répété en continu

**Conditions déclenchant le SOS:**
- Échec PLL à l'initialisation (3 tentatives)
- Décrochage PLL irréparable pendant le fonctionnement

### Bouton de Reset d'Urgence (RD13)

**Fonctionnement:**
- Actif UNIQUEMENT en mode erreur critique (SOS)
- Debouncing matériel: 50ms
- Reset software complet (`asm("reset")`)
- Redémarrage total du système

**Utilisation:**
1. Système en erreur → SOS visible sur RD10
2. Appui sur RD13 → "RESET BUTTON PRESSED - RESTARTING"  
3. Redémarrage complet avec nouvelle tentative d'initialisation

### LEDs de Statut

```
├── RD10  : LED Système
│   ├── Fonctionnement normal : Clignote pendant transmissions
│   └── Mode critique : Signal SOS morse continu
├── RB15  : LED Verrouillage PLL ADF4351
│   ├── Allumée fixe : PLL verrouillé (RC1=HIGH)
│   └── Éteinte : PLL déverrouillé (RC1=LOW)
└── RB13  : LED Status PA RA07M4047M
    ├── Allumée : Mode 5W activé
    └── Éteinte : Mode 100mW (défaut)
```

### Messages Debug UART

**Séquence normale d'initialisation:**
```
=== SESSION HHMMSS ===
*** RF INIT START ***
Programming ADF4351 registers...
PLL lock attempt 1/3
Waiting for PLL lock - LOCKED
ADF4351 initialized successfully at 403 MHz
```

**Séquence d'erreur avec retry:**
```
PLL lock attempt 1/3 - TIMEOUT
PLL lock attempt 2/3 - TIMEOUT  
PLL lock attempt 3/3 - TIMEOUT
CRITICAL ERROR: ADF4351 PLL LOCK FAILED AFTER 3 ATTEMPTS
[Signal SOS sur RD10]
```

**Recovery automatique en fonctionnement:**
```
WARNING: PLL unlock detected during operation
Attempting automatic PLL recovery...
PLL recovery successful
```

## Architecture Logicielle

### Structure du Projet
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
│   └── T001-OCT-24-2024.pdf # Standard COSPAS-SARSAT T.001
```

## Statut Implémentation

### Implémenté et Testé
- **Modulation BPSK** : Biphase-L ±1.1 rad @ 400 bauds
- **Trames T.001** : 144 bits, BCH(21,15)
- **Timing précis** : Preamble 160ms, Data 360ms, Postamble 320ms
- **GPS Encoding** : PDF-1 + PDF-2 avec offset 4 secondes
- **Hardware validation** : dsPIC33CK64MC105 validé contre DS70005399D
- **I/Q Modulation** : DAC 12-bit → ADL5375-05 Q channel (500mV bias)
- **Interface Circuit** : Adaptation 0-3.3V → 0-1V + bias 500mV
- **RF Chain** : Séquences power-up/down optimisées
- **SPI Hardware** : ADF4351 control à 1MHz (SPI1, PPS mapping)
- **Configuration SPI** : CPOL=0, CPHA=0, 32-bit register programming
- **Registres ADF4351** : 6 registres 32-bit (R5→R0) pour synthèse 403MHz
- **PLL Surveillance** : Lock detect + retry mechanism + recovery
- **Système de sécurité** : SOS morse + reset button d'urgence
- **Pin mapping complet** : PORTC (SPI + control), PORTB (LEDs), PORTD (system)
- **Pull-up/Pull-down** : RC1 (pull-down), RD13 (pull-up)

## Conformité et Légalité

### Fréquences d'opération
- **403 MHz** : Fréquence de formation et test (ce projet)
- **406 MHz** : Fréquence d'urgence réservée (interdite pour formation)
- **Objectif** : Éviter interférences avec balises d'urgence réelles

### Usage ADRASEC/SATER

#### Formation Locale (100mW)
```c
rf_set_power_level(RF_POWER_LOW);
start_beacon_frame(BEACON_TEST_FRAME);  // Trames toutes les 5s
```

#### Exercices Régionaux (5W)
```c
rf_set_power_level(RF_POWER_HIGH);
start_beacon_frame(BEACON_EXERCISE_FRAME);  // Trames toutes les 50s
```

#### Intégration Décodeur
Compatible avec le décodeur 406 MHz disponible dans `../dec406_v10.2/`

### Applications autorisées
- **Formation** : Tests décodeurs 403 MHz
- **Exercices** : Simulation balises détresse
- **Validation** : Tests conformité équipements
- **Usage réel** : Fréquence 403 MHz non autorisée pour vraies alertes

## Support

**Développé par ADRASEC09**
- **Auteur** : F4MLV (f4mlv09@gmail.com)
- **Usage** : Formation et exercices uniquement
- **Support** : Documentation technique incluse
- **Compatibilité** : Décodeur 406 MHz intégré

### Spécifications Techniques

| Paramètre | Valeur | Conformité |
|-----------|--------|------------|
| **Fréquence** | 403 MHz | Radiosondes (évite 406 MHz) |
| **Modulation** | Biphase-L BPSK | T.001 §A3.2.3 |
| **Débit** | 400 bauds | T.001 §A3.2.2 |
| **Phase shift** | ±1.1 radians | T.001 §A3.2.3.1 |
| **Puissance** | 100mW / 5W | Configurable |
| **Trame** | 144 bits | 15+9+120 bits |

### Roadmap

- [x] **Génération 1G** : Terminée et validée
- [x] **Modulation I/Q** : Optimisée pour ADL5375-05 (bias 500mV)
- [x] **Interface DAC→I/Q** : Circuit d'adaptation implémenté
- [x] **Architecture modulaire** : RF drivers séparés
- [ ] **Tests Hardware** : Validation RF complète avec oscilloscope
- [ ] **Optimisations futures** : Amélioration performances T.001

### Credits

**Based on original work by [loorisr/sarsat](https://github.com/loorisr/sarsat)**

Ce projet s'appuie sur les concepts et algorithmes du projet SARSAT original de loorisr, adapté pour une implémentation hardware embarquée dsPIC33CK avec modules RF dédiés. Voir [CREDITS.md](CREDITS.md) pour les remerciements complets.

### Documentation

#### Datasheets Intégrées
- **dsPIC33CK64MC105** : [DS70005399D](Docs/Microchip_PIC/dsPIC33CK64MC105-Family-Data-Sheet-DS70005399D.pdf)
- **ADF4351** : [Datasheet](Docs/adf4351.pdf)
- **ADL5375** : [Datasheet](Docs/adl5375.pdf)
- **COSPAS-SARSAT T.001** : [Standard](Docs/T001-OCT-24-2024.pdf)

#### Guides Techniques
- **Architecture RF** : [Schémas](Docs/Chaine\ RF\ complete.png)
- **Interface ADL5375-05** : [Circuit d'adaptation](ADL5375_INTERFACE_CIRCUIT.md)
- **Guide d'intégration** : [Documentation](Docs/Guide\ d'intégration\ dsPIC33CK\ +\ CS-T001.txt)



## Licence

Ce projet est sous licence Creative Commons Attribution - Pas d'Utilisation Commerciale - Partage dans les Mêmes Conditions 4.0 International (CC BY-NC-SA 4.0).

[![Licence Creative Commons](https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png)](http://creativecommons.org/licenses/by-nc-sa/4.0/)

### Vous êtes autorisé à :

- **Partager** : copier, distribuer et communiquer le matériel par tous moyens et sous tous formats
- **Adapter** : remixer, transformer et créer à partir du matériel

### Selon les conditions suivantes :

- **Attribution** : Vous devez créditer l'œuvre, intégrer un lien vers la licence et indiquer si des modifications ont été effectuées
- **Pas d'Utilisation Commerciale** : Vous n'êtes pas autorisé à faire un usage commercial de cette œuvre
- **Partage dans les Mêmes Conditions** : Dans le cas où vous remixez, transformez ou créez à partir du matériel composant l'œuvre originale, vous devez diffuser l'œuvre modifiée dans les mêmes conditions

**Avertissement** : Cette licence ne s'applique pas aux données de décodage des balises de détresse réelles, qui restent soumises aux réglementations nationales et internationales sur les communications d'urgence.

Pour plus de détails, consultez le texte complet de la licence : https://creativecommons.org/licenses/by-nc-sa/4.0/deed.fr## Licence

---

> **IMPORTANT** : Ce générateur utilise 403 MHz pour éviter les fausses alertes COSPAS-SARSAT. Usage formation uniquement.

> **ADRASEC** : Association Départementale des RadioAmateurs au service de la Sécurité Civile
