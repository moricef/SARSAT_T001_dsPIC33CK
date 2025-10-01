# Analyse Comparative : DAC Interne vs MCP4922

## Vue d'Ensemble

Cette analyse compare les performances entre le DAC interne du dsPIC33CK et le DAC externe MCP4922 dans le contexte du système SARSAT, avec un focus particulier sur le remplacement du DAC interne par le MCP4922.

## 1. Spécifications Techniques Comparatives

| Aspect | DAC Interne (dsPIC33CK) | MCP4922 Externe |
|--------|--------------------------|-----------------|
| **Résolution** | 12 bits (4096 niveaux) | 12 bits (4096 niveaux) |
| **Temps de conversion** | Accès direct registre (~1 cycle) | Communication SPI (16 bits) |
| **Fréquence d'horloge** | FCY = 100 MHz | SPI @ 4 MHz (configurable jusqu'à 20 MHz) |
| **Interface** | Accès mémoire direct | SPI2 (BRGL = 6) |
| **Canaux** | 1 canal | 2 canaux (DAC A & B) |
| **Tension de référence** | VDD (3.3V fixe) | Externe (programmable) |
| **Gain** | Fixe | Programmable (1x/2x) |
| **Consommation** | Intégrée au MCU | ~350 µA @ 5V |

## 2. Performances Temporelles Détaillées

### 2.1 DAC Interne

#### Code d'écriture :
```c
// Accès direct - très rapide
DAC1DATH = dac_value & 0x0FFF;  // ~1 cycle CPU
```

#### Performances :
- **Latence** : ~10 ns (1 cycle @ 100 MHz)
- **Débit théorique** : 100 MSamples/s
- **Jitter** : Négligeable (<1 ns)
- **Déterminisme** : Parfait (accès mémoire direct)

### 2.2 MCP4922 Externe

#### Code d'écriture :
```c
void mcp4922_write_dac_a(uint16_t value) {
    value &= 0x0FFF;
    uint16_t command = MCP4922_DAC_A_CMD | value;
    MCP4922_CS_LAT = 0;                    // ~1 cycle
    while(SPI2STATLbits.SPITBF);           // Attente buffer
    SPI2BUFL = command;                    // 16 bits @ 4 MHz
    while(!SPI2STATLbits.SPIRBF);          // Attente transmission
    (void)SPI2BUFL;                        // Lecture dummy
    MCP4922_CS_LAT = 1;                    // ~1 cycle
}
```

#### Performances :
- **Latence** : ~4 µs (16 bits @ 4 MHz + overhead)
- **Débit théorique** : ~250 kSamples/s @ 4 MHz
- **Débit optimisé** : ~1.25 MSamples/s @ 20 MHz
- **Jitter** : Variable selon état SPI (~0.5-1 µs)

## 3. Configuration Système SARSAT

### 3.1 Contraintes Temporelles Actuelles

```c
#define SAMPLE_RATE_HZ          6400    // Taux d'échantillonnage
#define SAMPLES_PER_SYMBOL      16      // Suréchantillonnage
#define SYMBOL_RATE_HZ          400     // 400 bauds (standard SARSAT)
```

#### Budget temporel :
- **Période ISR** : 156 µs (6400 Hz)
- **Temps ISR actuel** : ~5-10 µs estimé
- **Marge disponible** : ~90%

### 3.2 Utilisation Actuelle dans le Code

#### DAC Interne - Modulation temps réel :
```c
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    // ... logique de modulation ...

    switch(tx_phase) {
        case IDLE_STATE:
            dac_value = calculate_idle_dac_value();      // 0V
            break;
        case CARRIER_TX:
            dac_value = calculate_carrier_dac_value();   // 0.5V bias
            break;
        case DATA_TX:
            dac_value = calculate_bpsk_dac_value(current_bit, sample_count);
            break;
    }

    DAC1DATH = dac_value & 0x0FFF;  // Écriture ultra-rapide
    IFS0bits.T1IF = 0;
}
```

#### MCP4922 - Applications de test :
```c
// Fonctions disponibles mais utilisées hors ISR critique
mcp4922_set_iq_outputs(float i_amplitude, float q_amplitude);
mcp4922_output_oqpsk_symbol(uint8_t symbol_data);
mcp4922_test_pattern(void);
```

## 4. Analyse d'Impact pour le Remplacement

### 4.1 Défis Critiques Identifiés

#### A. Contrainte Temporelle ISR
**Problème majeur :** Le MCP4922 nécessite **4 µs** par écriture, soit **2.5%** du budget temporel ISR.

```c
// Impact temporel comparatif
DAC1DATH = value;           // ~10 ns  (0.006% budget ISR)
mcp4922_write_dac_a(value); // ~4000 ns (2.5% budget ISR)
```

#### B. Impact sur le Jitter
- **DAC interne** : Latence déterministe (~1 cycle)
- **MCP4922** : Latence variable due au SPI (attentes de buffer)
- **Impact SARSAT** : 4 µs représentent **16%** de la période symbole (25 µs) ⚠️

### 4.2 Facteurs de Jitter MCP4922

1. **Attente buffer SPI** : `while(SPI2STATLbits.SPITBF);`
2. **Temps transmission** : 16 bits @ 4 MHz = 4 µs
3. **Attente réception** : `while(!SPI2STATLbits.SPIRBF);`
4. **Overhead CS** : Activation/désactivation chip select

## 5. Stratégies de Migration

### 5.1 Stratégie A : Remplacement Direct (❌ Non Recommandée)

```c
// Remplacement simple mais risqué
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    // ... même logique état ...

    // DAC1DATH = dac_value & 0x0FFF;  // Ancien
    mcp4922_write_dac_a(dac_value);    // Nouveau - TRÈS RISQUÉ

    IFS0bits.T1IF = 0;
}
```

**⚠️ Problèmes :**
- Latence 400x plus élevée
- Jitter important (16% période symbole)
- Risque de dépassement budget ISR
- Dégradation qualité signal RF

### 5.2 Stratégie B : Optimisation SPI Haute Vitesse (✅ Recommandée)

#### Configuration SPI optimisée :
```c
void mcp4922_init_high_speed(void) {
    // ... configuration pins ...

    // SPI à 20 MHz (maximum MCP4922)
    SPI2BRGL = 1;               // 20 MHz au lieu de 4 MHz
    SPI2CON2Lbits.WLENGTH = 15; // 16-bit word length
    SPI2CON1Lbits.SPIEN = 1;    // Enable SPI2
}
```

#### Driver optimisé pour ISR :
```c
void mcp4922_write_dac_a_fast(uint16_t value) {
    static uint16_t cached_value = 0xFFFF;

    // Optimisation : éviter écritures redondantes
    if (value == cached_value) return;
    cached_value = value;

    value &= 0x0FFF;
    uint16_t command = MCP4922_DAC_A_CMD | value;

    // Écriture optimisée pour ISR
    MCP4922_CS_LAT = 0;
    SPI2BUFL = command;

    // Attente optimisée avec timeout
    uint16_t timeout = 1000;
    while(!SPI2STATLbits.SPIRBF && --timeout);
    if (timeout) (void)SPI2BUFL;

    MCP4922_CS_LAT = 1;
}
```

**Gains :** Réduction de 4 µs à ~0.8 µs (80% amélioration)

### 5.3 Stratégie C : Buffer Double + DMA (🔄 Alternative Avancée)

```c
// ISR : Écriture dans buffer rapide
volatile uint16_t dac_buffer[2];
volatile uint8_t buffer_index = 0;

void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    // ... logique état ...
    dac_buffer[buffer_index] = dac_value;  // ⚡ Écriture rapide
    buffer_index = !buffer_index;

    // Déclencher transfert DMA/SPI asynchrone
    trigger_spi_dma_transfer();

    IFS0bits.T1IF = 0;
}
```

### 5.4 Stratégie D : Fréquence ISR Adaptée

```c
// Option : Réduire la fréquence d'échantillonnage
#define SAMPLE_RATE_HZ          3200    // Réduit de moitié
#define SAMPLES_PER_SYMBOL      8       // Réduit de moitié

// Calculs de performance
// Nouvelle période ISR : 312 µs
// Budget MCP4922 : 0.8 µs = 0.25% (acceptable)
```

**Impact :** Qualité de modulation réduite mais timing plus relaxé

## 6. Plan de Migration Recommandé

### Phase 1 : Optimisation SPI (Priorité Critique)
1. **Configurer SPI à 20 MHz** (maximum MCP4922)
2. **Implémenter driver optimisé** avec cache et timeout
3. **Tests de stabilité SPI** haute vitesse
4. **Validation timing** sur oscilloscope

### Phase 2 : Adaptation des Fonctions

#### Nouvelles fonctions de conversion :
```c
uint16_t convert_dac_internal_to_mcp4922(uint16_t internal_value) {
    // Conversion de la plage DAC interne vers MCP4922
    // DAC interne : 0-4095 (0-3.3V)
    // MCP4922 : 0-4095 avec VREF externe

    if (VREF_VOLTAGE == 3.3f) {
        return internal_value;  // Mapping direct
    } else {
        // Scaling selon VREF
        return (uint16_t)((internal_value * 3.3f) / VREF_VOLTAGE);
    }
}

uint16_t calculate_mcp4922_idle_value(void) {
    return 0;  // Ou utiliser mode shutdown
}

uint16_t calculate_mcp4922_carrier_value(void) {
    // 500mV bias pour ADL5375 → 2048 avec VREF=3.3V
    return MCP4922_OFFSET;  // 2048 pour 1.65V
}

uint16_t calculate_mcp4922_bpsk_value(uint8_t bit_value, uint16_t sample_index) {
    // Adapter la fonction existante pour MCP4922
    uint16_t base_value = signal_processor_get_biphase_l_value(
        bit_value, sample_index, SAMPLES_PER_SYMBOL);

    return convert_dac_internal_to_mcp4922(base_value);
}
```

### Phase 3 : Modifications Code Principal

#### A. system_comms.c - Modifications ISR :
```c
// Fonction d'initialisation modifiée
void system_init(void) {
    init_clock();
    init_gpio();
    // init_dac();                    // ❌ Supprimer DAC interne
    mcp4922_init_high_speed();        // ✅ Initialiser MCP4922
    init_comm_uart();
    system_debug_init();
    init_timer1();
    signal_processor_init();
    rf_initialize_all_modules();

    DEBUG_LOG_FLUSH("System migrated to MCP4922 DAC\r\n");
}

// ISR modifiée avec gestion d'erreur
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    static uint8_t debug_pin_state = 0;
    static uint32_t spi_error_count = 0;

    // ... même logique de timing et d'état ...

    // Calcul valeur DAC selon état
    uint16_t dac_value = 0;
    switch(tx_phase) {
        case IDLE_STATE:
            dac_value = calculate_mcp4922_idle_value();
            break;
        case CARRIER_TX:
            dac_value = calculate_mcp4922_carrier_value();
            if (++sample_count >= CARRIER_SAMPLES) {
                tx_phase = DATA_TX;
                sample_count = 0;
                bit_index = 0;
            }
            break;
        case DATA_TX:
            if (bit_index < MESSAGE_BITS) {
                uint8_t current_bit = beacon_frame[bit_index];
                dac_value = calculate_mcp4922_bpsk_value(current_bit, sample_count);

                if (++sample_count >= SAMPLES_PER_SYMBOL) {
                    sample_count = 0;
                    bit_index++;
                }
            } else {
                tx_phase = RF_SHUTDOWN;
                sample_count = 0;
            }
            break;
        case RF_SHUTDOWN:
            // ... logique shutdown existante ...
            break;
    }

    // Écriture MCP4922 optimisée
    if (!mcp4922_write_dac_a_fast(dac_value)) {
        spi_error_count++;
        // Gestion d'erreur : log ou action corrective
    }

    IFS0bits.T1IF = 0;
}
```

#### B. mcp4922_driver.c - Driver Production :
```c
// Configuration haute performance
void mcp4922_init_high_speed(void) {
    // Configuration pins SPI2 optimisée
    ANSELBbits.ANSELB7 = 0;  // RB7 digital (SCK2)
    ANSELBbits.ANSELB8 = 0;  // RB8 digital (SDO2)
    ANSELBbits.ANSELB9 = 0;  // RB9 digital (CS)

    TRISBbits.TRISB7 = 0;    // SCK2 output
    TRISBbits.TRISB8 = 0;    // SDO2 output
    MCP4922_CS_TRIS = 0;     // CS output
    MCP4922_CS_LAT = 1;      // CS idle high

    // PPS configuration
    __builtin_write_RPCON(0x0000);
    RPOR3bits.RP39R = 8;     // SCK2 on RB7
    RPOR4bits.RP40R = 7;     // SDO2 on RB8
    __builtin_write_RPCON(0x0800);

    // SPI2 configuration haute vitesse
    SPI2CON1L = 0;
    SPI2CON1Lbits.MSTEN = 1;     // Master mode
    SPI2CON1Lbits.CKP = 0;       // Clock polarity: idle low
    SPI2CON1Lbits.CKE = 1;       // Clock edge: transmit on active to idle
    SPI2CON2Lbits.WLENGTH = 15;  // 16-bit word length

    // SPI à 20 MHz (BRGL = (FCY/2)/FSPI - 1)
    // FSPI = 20MHz, FCY = 100MHz → BRGL = (50MHz/20MHz) - 1 = 1.5 → 1
    SPI2BRGL = 1;                // 20 MHz clock (maximum MCP4922)

    SPI2CON1Lbits.SPIEN = 1;     // Enable SPI2

    DEBUG_LOG_FLUSH("MCP4922: High-speed SPI2 initialized @ 20MHz\r\n");

    // Test de configuration
    mcp4922_write_dac_a(MCP4922_OFFSET);
    mcp4922_write_dac_b(MCP4922_OFFSET);

    DEBUG_LOG_FLUSH("MCP4922: DACs initialized to mid-scale\r\n");
}

// Driver ISR optimisé avec gestion d'erreur
uint8_t mcp4922_write_dac_a_fast(uint16_t value) {
    static uint16_t cached_value = 0xFFFF;

    // Optimisation : éviter écritures redondantes
    if (value == cached_value) return 1;

    value &= 0x0FFF;
    cached_value = value;

    uint16_t command = MCP4922_DAC_A_CMD | value;

    // Vérification disponibilité SPI
    if (SPI2STATLbits.SPITBF) {
        return 0;  // SPI busy - échec
    }

    // Transaction SPI optimisée
    MCP4922_CS_LAT = 0;
    SPI2BUFL = command;

    // Attente avec timeout (sécurité)
    uint16_t timeout = 2500;  // ~0.8µs @ 20MHz
    while (!SPI2STATLbits.SPIRBF && --timeout) {
        // Attente active optimisée
    }

    if (timeout) {
        (void)SPI2BUFL;  // Clear buffer
        MCP4922_CS_LAT = 1;
        return 1;  // Succès
    } else {
        MCP4922_CS_LAT = 1;
        return 0;  // Timeout - échec
    }
}

// Version debug pour mesures de performance
void mcp4922_performance_test(void) {
    uint32_t start_time, end_time;

    // Test latence écriture
    start_time = _CP0_GET_COUNT();
    mcp4922_write_dac_a_fast(2048);
    end_time = _CP0_GET_COUNT();

    uint32_t cycles = end_time - start_time;
    uint32_t nanoseconds = (cycles * 1000) / (FCY / 1000000);

    DEBUG_LOG_FLUSH("MCP4922 write latency: ");
    debug_print_uint32(nanoseconds);
    DEBUG_LOG_FLUSH(" ns (");
    debug_print_uint32(cycles);
    DEBUG_LOG_FLUSH(" cycles)\r\n");
}
```

### Phase 4 : Tests et Validation

#### A. Tests de Performance
1. **Mesure latence oscilloscope** : Vérifier <1 µs @ 20 MHz
2. **Test jitter** : Analyse statistique sur 1000 échantillons
3. **Test charge CPU** : Vérification budget ISR <5%
4. **Test stabilité** : Fonctionnement continu 24h

#### B. Tests Fonctionnels SARSAT
1. **Analyse spectrale** : FFT du signal modulé
2. **Test conformité** : Validation protocole SARSAT T.001
3. **Test PLL stabilité** : Vérification verrouillage
4. **Test longue durée** : Transmission 1000 trames

#### C. Code de validation :
```c
// Test de performance intégré
void validate_mcp4922_migration(void) {
    DEBUG_LOG_FLUSH("=== MCP4922 Migration Validation ===\r\n");

    // Test 1: Performance SPI
    mcp4922_performance_test();

    // Test 2: Pattern test
    DEBUG_LOG_FLUSH("Running test pattern...\r\n");
    mcp4922_test_pattern();

    // Test 3: ISR load test
    uint32_t isr_start = millis_counter;
    for (int i = 0; i < 1000; i++) {
        mcp4922_write_dac_a_fast(i);
        __delay_us(156);  // Simulate ISR period
    }
    uint32_t isr_duration = millis_counter - isr_start;

    DEBUG_LOG_FLUSH("ISR load test: ");
    debug_print_uint32(isr_duration);
    DEBUG_LOG_FLUSH(" ms for 1000 samples\r\n");

    // Test 4: Signal quality
    DEBUG_LOG_FLUSH("Testing signal levels...\r\n");
    mcp4922_write_dac_a(0);        // Min
    __delay_ms(10);
    mcp4922_write_dac_a(4095);     // Max
    __delay_ms(10);
    mcp4922_write_dac_a(2048);     // Mid

    DEBUG_LOG_FLUSH("=== Validation Complete ===\r\n");
}
```

## 7. Analyse des Risques et Mitigation

### 7.1 Risques Majeurs Identifiés

| Risque | Impact | Probabilité | Mitigation |
|--------|--------|-------------|------------|
| **Jitter temporel** | Dégradation qualité RF | Élevée | SPI 20 MHz + driver optimisé |
| **Dépassement budget ISR** | Corruption données | Moyenne | Monitoring temps + timeout |
| **Non-conformité SARSAT** | Échec certification | Faible | Tests complets + validation |
| **Instabilité PLL** | Perte verrouillage | Moyenne | Calibration timing RF |
| **Défaillance SPI** | Perte transmission | Faible | Gestion erreur + fallback |

### 7.2 Plan de Mitigation

#### A. Monitoring Performance
```c
// Système de monitoring intégré
typedef struct {
    uint32_t spi_errors;
    uint32_t timeout_errors;
    uint32_t max_latency_ns;
    uint32_t avg_latency_ns;
} mcp4922_stats_t;

volatile mcp4922_stats_t mcp4922_stats = {0};

void mcp4922_update_stats(uint32_t latency_ns, uint8_t success) {
    if (!success) {
        mcp4922_stats.spi_errors++;
    } else {
        if (latency_ns > mcp4922_stats.max_latency_ns) {
            mcp4922_stats.max_latency_ns = latency_ns;
        }
        // Moving average
        mcp4922_stats.avg_latency_ns =
            (mcp4922_stats.avg_latency_ns * 7 + latency_ns) / 8;
    }
}
```

#### B. Fallback Strategy
```c
// Option de fallback vers DAC interne
#define ENABLE_DAC_FALLBACK 1

#if ENABLE_DAC_FALLBACK
uint8_t use_internal_dac = 0;  // Flag de fallback

void dac_write_universal(uint16_t value) {
    if (use_internal_dac) {
        DAC1DATH = value & 0x0FFF;
    } else {
        if (!mcp4922_write_dac_a_fast(value)) {
            // Échec MCP4922 → basculer vers DAC interne
            use_internal_dac = 1;
            DAC1DATH = value & 0x0FFF;
            DEBUG_LOG_FLUSH("FALLBACK: Switched to internal DAC\r\n");
        }
    }
}
#endif
```

## 8. Avantages et Inconvénients du Remplacement

### 8.1 Avantages du MCP4922

✅ **Avantages Techniques :**
- **Deux canaux indépendants** : Possibilité I/Q simultané
- **Référence externe** : Précision améliorée avec référence stable
- **Gain programmable** : Adaptation dynamique (1x/2x)
- **Shutdown sélectif** : Économie d'énergie par canal
- **Isolation** : Séparation galvanique du MCU
- **Flexibilité** : Configuration runtime avancée

✅ **Avantages Système :**
- Libération ressources MCU (DAC interne disponible pour autre usage)
- Possibilité modulation I/Q complexe (QPSK, etc.)
- Meilleur contrôle amplitude et offset
- Compatibilité avec références haute précision

### 8.2 Inconvénients du MCP4922

❌ **Inconvénients Performance :**
- **Latence élevée** : 400x supérieure (10 ns → 4000 ns)
- **Jitter variable** : Dépendant de l'état SPI
- **Complexité temporelle** : Gestion attentes et timeouts
- **Budget ISR** : Consommation 2.5% temps processeur

❌ **Inconvénients Système :**
- Consommation supplémentaire (~350 µA)
- Pins E/S additionnelles (3 pins SPI)
- Coût composant externe
- Complexité débogage (SPI vs registre direct)
- Risque défaillance matérielle supplémentaire

## 9. Recommandations Finales

### 9.1 Recommandation Principale

**Le remplacement est techniquement faisable** avec les conditions suivantes :

1. **SPI obligatoirement à 20 MHz** (réduction latence à ~0.8 µs)
2. **Driver hautement optimisé** avec cache et gestion d'erreur
3. **Tests approfondis** de performance et conformité SARSAT
4. **Plan de fallback** vers DAC interne en cas de problème

### 9.2 Critères de Décision

#### Remplacer si :
- **Application I/Q requise** (modulation complexe future)
- **Référence externe nécessaire** (précision critique)
- **Gain programmable requis** (adaptation dynamique)
- **Ressources MCU limitées** (DAC interne nécessaire ailleurs)

#### Conserver DAC interne si :
- **Performance temps réel critique** (latence <100 ns requise)
- **Simplicité prioritaire** (maintenance, débogage)
- **Budget énergétique serré** (applications batterie)
- **Fiabilité maximale** (systèmes critiques)

### 9.3 Plan d'Implémentation Recommandé

```
Phase 1: Préparation (1-2 semaines)
├── Optimisation driver MCP4922 @ 20 MHz
├── Tests performance isolés
└── Validation timing sur oscilloscope

Phase 2: Intégration (2-3 semaines)
├── Modification ISR system_comms.c
├── Adaptation fonctions de conversion
├── Tests fonctionnels système
└── Validation protocole SARSAT

Phase 3: Validation (2-4 semaines)
├── Tests longue durée (stabilité)
├── Analyse spectrale RF
├── Certification conformité SARSAT
└── Documentation finale

Phase 4: Déploiement (1 semaine)
├── Tests sur matériel final
├── Formation équipe maintenance
└── Procédures de fallback
```

### 9.4 Métriques de Succès

- **Latence MCP4922** < 1 µs (objectif 0.8 µs @ 20 MHz)
- **Taux erreur SPI** < 0.01% (1 erreur/10000 transactions)
- **Budget ISR** < 5% (préservation marge sécurité)
- **Conformité SARSAT** 100% (aucune déviation protocole)
- **Stabilité 24h** sans défaillance

## Conclusion

Le remplacement du DAC interne par le MCP4922 représente un **défi technique significatif** principalement dû à la différence de latence (facteur 400x). Cependant, avec une **optimisation agressive du SPI à 20 MHz** et un **driver hautement optimisé**, cette migration devient **techniquement réalisable**.

La **décision finale** doit être basée sur les **besoins fonctionnels spécifiques** :
- Si les capacités étendues du MCP4922 (I/Q, référence externe, gain programmable) sont requises, la migration est justifiée
- Si seule la modulation BPSK simple est nécessaire, le DAC interne reste la solution optimale

**Recommandation système :** Implémenter une **architecture hybride** permettant de basculer entre les deux solutions selon le mode de fonctionnement requis.