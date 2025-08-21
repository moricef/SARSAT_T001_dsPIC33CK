# Interface Circuit dsPIC33CK ↔ ADL5375-05

## Vue d'ensemble

Ce document décrit le circuit d'adaptation nécessaire pour interfacer la sortie DAC du dsPIC33CK (RA3 DACOUT) avec l'ADL5375-05 (modulateur I/Q).

## Spécifications

### dsPIC33CK DAC (RA3)
- **Sortie** : 0 - 3.3V (12-bit, 0-4095)
- **Impédance de sortie** : ~1kΩ
- **Point milieu (DAC_OFFSET)** : 1.65V (2048)
- **Fréquence d'échantillonnage** : 6400 Hz

### ADL5375-05 Baseband Inputs
- **Bias DC requis** : 500mV sur chaque entrée différentielle
- **Signal AC** : 500mV p-p par pin (1V p-p différentiel)
- **Entrées** : IBBP, IBBN, QBBP, QBBN (haute impédance ~100kΩ)
- **Plage de tension** : 0-1V + 500mV bias

## Circuit d'interface recommandé

### Configuration matérielle

```
dsPIC33CK                Circuit d'adaptation                ADL5375-05
    |                           |                              |
RA3 DACOUT ──────┬──── R1 (50Ω) ──── C1 (100nF) ──── QBBP
(0-1V)           │                                    
                 │                                    
                 └──── R2 (50Ω) ──────────────────── QBBN
                                    │
                            +500mV_REF

Canal I (constant) :
+500mV_REF ──── R3 (50Ω) ──── IBBP
+500mV_REF ──── R4 (50Ω) ──── IBBN
```

### Composants requis

| Composant | Valeur | Description |
|-----------|--------|-------------|
| R1 | 50Ω | Résistance d'adaptation QBBP |
| R2 | 50Ω | Résistance de bias QBBN |
| R3, R4 | 50Ω | Résistances de bias canal I |
| C1 | 100nF | Condensateur de couplage AC |
| 500mV_REF | Référence 500mV | Diviseur résistif ou régulateur |

### Génération de la référence 500mV

#### Option 1: Diviseur résistif
```
VDD (+3.3V) ──── R5 (2.8kΩ) ────┬──── +500mV_REF
                                 │
                                 R6 (1.7kΩ)
                                 │
                               GND
```

#### Option 2: Régulateur de précision
- Utiliser un régulateur LDO 500mV (ex: TPS7A02)
- Meilleure stabilité et précision

## Modifications logicielles implémentées

### Nouvelles constantes (system_comms.h)

```c
// ADL5375-05 Interface Configuration
#define ADL5375_BIAS_MV         500              // 500mV bias level
#define ADL5375_SWING_MV        500              // 500mV p-p per pin
#define ADL5375_MIN_VOLTAGE     0.0f             // Minimum voltage
#define ADL5375_MAX_VOLTAGE     1.0f             // Maximum voltage
#define VOLTAGE_REF_3V3         3.3f             // dsPIC33CK supply
```

### Nouvelles fonctions (system_comms.c)

#### `calculate_adl5375_q_channel()`
```c
uint16_t calculate_adl5375_q_channel(float phase_shift, uint8_t apply_envelope) {
    // ADL5375-05: 500mV bias ± 500mV swing = 0-1V range
    float q_voltage = 0.5f;  // 500mV bias
    
    // Add BPSK modulation: ±sin(1.1 rad)
    if (phase_shift >= 0) {
        q_voltage += (sinf(1.1f) * 0.25f);  // +modulation
    } else {
        q_voltage += (sinf(-1.1f) * 0.25f); // -modulation  
    }
    
    // Apply envelope for RF ramp
    if (apply_envelope) {
        q_voltage = 0.5f + (q_voltage - 0.5f) * envelope_gain;
    }
    
    // Convert to DAC units (scaled to 0-1V range)
    return (uint16_t)((q_voltage * 4096.0f) / 3.3f);
}
```

#### `adapt_dac_for_adl5375()`
```c
uint16_t adapt_dac_for_adl5375(uint16_t dac_value) {
    // Convert 0-3.3V DAC range to 0-1V for ADL5375-05
    float voltage = ((float)dac_value * 3.3f) / 4096.0f;
    voltage = voltage * (1.0f / 3.3f);  // Scale to 0-1V
    return (uint16_t)((voltage * 4096.0f) / 3.3f);
}
```

## Signaux générés

### Canal Q (QBBP/QBBN)
- **Porteuse non modulée** : 500mV (bias level)
- **Bit '1'** : 500mV + sin(+1.1 rad) × 250mV
- **Bit '0'** : 500mV + sin(-1.1 rad) × 250mV
- **Rampes RF** : Enveloppe appliquée selon `envelope_gain`

### Canal I (IBBP/IBBN) 
- **Constant** : 500mV (bias level)
- Généré par circuit externe (référence 500mV)

## Validation et test

### Points de test recommandés
1. **RA3 DACOUT** : Vérifier la plage 0-1V après adaptation
2. **QBBP** : Signal modulé 500mV ± modulation
3. **QBBN** : Référence stable 500mV
4. **IBBP/IBBN** : Référence stable 500mV

### Mesures oscilloscope
- **Fréquence porteuse** : 403MHz (sortie ADF4351)
- **Débit symbole** : 400 baud
- **Modulation** : BPSK ±1.1 rad
- **Spectro** : Vérifier la qualité du signal modulé

## Notes d'implémentation

1. **Compatibilité** : L'ancienne fonction `calculate_modulated_value()` appelle automatiquement la nouvelle fonction ADL5375-05
2. **Performance** : Les calculs en virgule flottante sont optimisés pour le dsPIC33CK
3. **Précision** : La référence 500mV doit être stable (±1%)
4. **Filtre** : Ajouter un filtre anti-repliement si nécessaire

## Chaîne RF complète

```
dsPIC33CK → DAC → Circuit adaptation → ADL5375-05 → RA07M4047M → 403MHz
     ↑                                      ↑
     |                                      |
 Modulation BPSK                      LO 403MHz
                                  (depuis ADF4351)
```

L'interface est maintenant optimisée pour l'ADL5375-05 avec les niveaux de bias corrects (500mV) et la modulation BPSK conforme au standard SARSAT.