# Définition du Jitter

## 1. Définition Générale

Le **jitter** (ou gigue temporelle) est la **variation non désirée dans le temps** d'un signal périodique par rapport à sa position temporelle idéale. C'est un écart aléatoire ou pseudo-aléatoire de la temporisation d'un événement par rapport à sa position théorique.

```
Signal idéal:     |---|---|---|---|---|
Signal avec jitter: |-|--|----|--|-----|
                    ↑   ↑    ↑   ↑
                  Variations temporelles aléatoires
```

## 2. Types de Jitter

### 2.1 Jitter Absolu
Variation temporelle mesurée par rapport à une référence de temps absolue (horloge parfaite).

### 2.2 Jitter Relatif (Period Jitter)
Variation de la période d'un signal par rapport à sa période nominale.

### 2.3 Jitter Cycle-à-Cycle
Différence de période entre deux cycles consécutifs d'un signal.

### 2.4 Jitter d'Accumulation (Phase Jitter)
Accumulation des erreurs de phase sur plusieurs cycles.

## 3. Jitter dans le Contexte des DACs

### 3.1 DAC Interne (dsPIC33CK)
```c
// Écriture déterministe
DAC1DATH = value;  // Temps d'exécution: 1 cycle CPU (10 ns @ 100MHz)
```

**Caractéristiques jitter :**
- **Jitter nominal** : < 1 ns
- **Type** : Pratiquement nul (accès mémoire direct)
- **Cause** : Uniquement variations horloge système
- **Prévisibilité** : Parfaite (déterministe)

### 3.2 MCP4922 (DAC Externe SPI)
```c
void mcp4922_write_dac_a(uint16_t value) {
    MCP4922_CS_LAT = 0;                    // ~10 ns
    while(SPI2STATLbits.SPITBF);           // ⚠️ Attente variable
    SPI2BUFL = command;                    // 16 bits @ 4MHz = 4000 ns
    while(!SPI2STATLbits.SPIRBF);          // ⚠️ Attente variable
    (void)SPI2BUFL;                        // ~10 ns
    MCP4922_CS_LAT = 1;                    // ~10 ns
}
```

**Caractéristiques jitter :**
- **Jitter typique** : 0.5 - 1 µs
- **Type** : Variable selon état SPI
- **Causes** : Attentes buffer, interruptions, charge CPU
- **Prévisibilité** : Faible (non-déterministe)

## 4. Sources de Jitter dans le MCP4922

### 4.1 Jitter d'Attente Buffer SPI
```c
while(SPI2STATLbits.SPITBF);  // Temps variable selon état buffer
```
- **Cause** : Buffer SPI peut être occupé par opération précédente
- **Variation** : 0 à plusieurs cycles d'horloge
- **Impact** : Retard de démarrage transmission

### 4.2 Jitter de Transmission SPI
```c
SPI2BUFL = command;  // Transmission 16 bits
```
- **Cause** : Interruptions pendant transmission
- **Variation** : Fonction des interruptions système
- **Impact** : Allongement durée transmission

### 4.3 Jitter d'Interruption
```c
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    // Si SPI en cours, l'ISR peut être retardée
    mcp4922_write_dac_a(value);  // Timing non-déterministe
}
```

## 5. Impact du Jitter sur le Système SARSAT

### 5.1 Contraintes Temporelles SARSAT
```c
#define SAMPLE_RATE_HZ          6400    // Échantillonnage 6.4 kHz
#define SYMBOL_RATE_HZ          400     // 400 bauds
#define SAMPLES_PER_SYMBOL      16      // Suréchantillonnage
```

**Périodes critiques :**
- **Période échantillon** : 156 µs (1/6400 Hz)
- **Période symbole** : 2.5 ms (1/400 Hz)
- **Tolérance jitter** : Typiquement < 1% période symbole = 25 µs

### 5.2 Analyse d'Impact Jitter MCP4922

#### Scenario pessimiste :
```
Latence nominale MCP4922:     4000 ns
Jitter maximum estimé:        ±1000 ns
Plage totale:                 3000-5000 ns (variation 67%)
```

#### Impact sur modulation BPSK :
```c
// Dans l'ISR à 6400 Hz
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    // Timing critique pour modulation cohérente
    uint8_t bit = beacon_frame[bit_index];
    uint16_t dac_value = calculate_bpsk_value(bit, sample_count);

    // ⚠️ Jitter ici affecte directement la qualité RF
    mcp4922_write_dac_a(dac_value);  // 4±1 µs avec jitter
}
```

**Conséquences :**
- **Distorsion spectrale** : Élargissement bande passante
- **Dégradation SNR** : Réduction rapport signal/bruit
- **Instabilité PLL** : Difficultés verrouillage récepteur

## 6. Mesure du Jitter

### 6.1 Méthode Oscilloscope
```c
// Code de test pour mesurer jitter
void measure_dac_jitter(void) {
    // Pin de synchronisation pour oscilloscope
    #define SYNC_PIN LATBbits.LATB0

    for (int i = 0; i < 1000; i++) {
        SYNC_PIN = 1;  // Déclenchement oscilloscope

        // Mesurer temps d'écriture DAC
        uint32_t start = _CP0_GET_COUNT();
        mcp4922_write_dac_a(2048);
        uint32_t end = _CP0_GET_COUNT();

        SYNC_PIN = 0;

        // Statistiques jitter
        uint32_t duration = end - start;
        // Analyser distribution des durées

        __delay_us(100);  // Période test
    }
}
```

### 6.2 Méthode Statistique Logicielle
```c
// Structure statistiques jitter
typedef struct {
    uint32_t min_latency_ns;
    uint32_t max_latency_ns;
    uint32_t avg_latency_ns;
    uint32_t jitter_rms_ns;
    uint32_t sample_count;
} jitter_stats_t;

volatile jitter_stats_t dac_jitter_stats = {0};

void update_jitter_stats(uint32_t latency_ns) {
    if (dac_jitter_stats.sample_count == 0) {
        // Premier échantillon
        dac_jitter_stats.min_latency_ns = latency_ns;
        dac_jitter_stats.max_latency_ns = latency_ns;
        dac_jitter_stats.avg_latency_ns = latency_ns;
    } else {
        // Mise à jour statistiques
        if (latency_ns < dac_jitter_stats.min_latency_ns) {
            dac_jitter_stats.min_latency_ns = latency_ns;
        }
        if (latency_ns > dac_jitter_stats.max_latency_ns) {
            dac_jitter_stats.max_latency_ns = latency_ns;
        }

        // Moyenne mobile
        dac_jitter_stats.avg_latency_ns =
            (dac_jitter_stats.avg_latency_ns * 7 + latency_ns) / 8;
    }

    dac_jitter_stats.sample_count++;

    // Calcul jitter (écart pic-pic)
    uint32_t jitter_pp = dac_jitter_stats.max_latency_ns -
                         dac_jitter_stats.min_latency_ns;
}
```

## 7. Réduction du Jitter MCP4922

### 7.1 Optimisation SPI Haute Vitesse
```c
// SPI à 20 MHz pour réduire temps transmission
SPI2BRGL = 1;  // 20 MHz au lieu de 4 MHz

// Réduction jitter transmission : 4000 ns → 800 ns
```

### 7.2 Driver Non-Bloquant
```c
// Version non-bloquante (risquée mais moins de jitter)
uint8_t mcp4922_write_nonblocking(uint16_t value) {
    if (SPI2STATLbits.SPITBF) {
        return 0;  // Échec - SPI occupé
    }

    MCP4922_CS_LAT = 0;
    SPI2BUFL = MCP4922_DAC_A_CMD | (value & 0x0FFF);
    // ⚠️ Ne pas attendre - retour immédiat
    MCP4922_CS_LAT = 1;

    return 1;  // Succès (apparent)
}
```

### 7.3 Buffer Double avec DMA
```c
// Réduction jitter par transfert DMA asynchrone
volatile uint16_t dac_buffer[2];
volatile uint8_t write_index = 0;

// ISR écrit dans buffer (jitter minimal)
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    dac_buffer[write_index] = dac_value;  // ⚡ < 100 ns
    write_index = !write_index;
    // DMA transfert buffer vers SPI en arrière-plan
}
```

## 8. Spécifications Jitter Typiques

### 8.1 Systèmes de Communication RF

| Application | Jitter Toléré | Commentaire |
|-------------|---------------|-------------|
| **GSM** | < 0.1 ppm | Communications mobiles |
| **GPS** | < 1 ns RMS | Navigation satellitaire |
| **WiFi 802.11** | < 25 ps RMS | Communications haut débit |
| **SARSAT** | < 1% période symbole | Balises de détresse |

### 8.2 Calcul Tolérance SARSAT
```c
// SARSAT T.001 - Calcul tolérance jitter
#define SYMBOL_RATE_HZ          400     // 400 bauds
#define JITTER_TOLERANCE_PCT    1.0     // 1% tolérance

uint32_t symbol_period_us = 1000000 / SYMBOL_RATE_HZ;  // 2500 µs
uint32_t max_jitter_us = symbol_period_us * JITTER_TOLERANCE_PCT / 100;  // 25 µs

// Comparaison avec MCP4922
uint32_t mcp4922_jitter_us = 1;  // 1 µs estimé
float jitter_ratio = (float)mcp4922_jitter_us / max_jitter_us * 100;  // 4%

// Conclusion: 4% de la tolérance → Acceptable mais à surveiller
```

## 9. Exemples Pratiques de Mesure

### 9.1 Mesure Temps Réel en ISR
```c
// Mesure jitter intégrée dans ISR (débogage uniquement)
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    static uint32_t last_isr_time = 0;
    static uint32_t jitter_max = 0;

    uint32_t current_time = _CP0_GET_COUNT();

    if (last_isr_time != 0) {
        uint32_t period = current_time - last_isr_time;
        uint32_t expected_period = FCY / SAMPLE_RATE_HZ;  // Période théorique

        // Calcul jitter ISR
        uint32_t jitter = (period > expected_period) ?
                         (period - expected_period) :
                         (expected_period - period);

        if (jitter > jitter_max) {
            jitter_max = jitter;
        }
    }

    last_isr_time = current_time;

    // ... logique ISR normale ...

    // Mesure jitter écriture DAC
    uint32_t dac_start = _CP0_GET_COUNT();
    mcp4922_write_dac_a(dac_value);
    uint32_t dac_end = _CP0_GET_COUNT();

    update_jitter_stats((dac_end - dac_start) * 10);  // Conversion en ns

    IFS0bits.T1IF = 0;
}
```

### 9.2 Test Complet de Caractérisation
```c
// Test complet pour caractériser jitter DAC
void characterize_dac_jitter(void) {
    const uint32_t NUM_SAMPLES = 10000;
    uint32_t samples[NUM_SAMPLES];

    DEBUG_LOG_FLUSH("Starting DAC jitter characterization...\r\n");

    // Désactiver interruptions pour mesure pure
    __builtin_disable_interrupts();

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        uint16_t test_value = (i * 37) & 0x0FFF;  // Pattern pseudo-aléatoire

        uint32_t start = _CP0_GET_COUNT();
        mcp4922_write_dac_a(test_value);
        uint32_t end = _CP0_GET_COUNT();

        samples[i] = end - start;
    }

    __builtin_enable_interrupts();

    // Analyse statistique complète
    uint32_t min_cycles = samples[0];
    uint32_t max_cycles = samples[0];
    uint64_t sum_cycles = 0;
    uint64_t sum_squares = 0;

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        uint32_t sample = samples[i];

        if (sample < min_cycles) min_cycles = sample;
        if (sample > max_cycles) max_cycles = sample;

        sum_cycles += sample;
        sum_squares += ((uint64_t)sample * sample);
    }

    // Calculs statistiques
    uint32_t avg_cycles = sum_cycles / NUM_SAMPLES;
    uint32_t jitter_pp_cycles = max_cycles - min_cycles;

    // Écart-type (approximation RMS jitter)
    uint64_t variance = (sum_squares / NUM_SAMPLES) - ((uint64_t)avg_cycles * avg_cycles);
    uint32_t std_dev_cycles = sqrt_approx(variance);

    // Conversion en nanosecondes (FCY = 100 MHz)
    uint32_t avg_ns = (avg_cycles * 1000) / (FCY / 1000000);
    uint32_t jitter_pp_ns = (jitter_pp_cycles * 1000) / (FCY / 1000000);
    uint32_t jitter_rms_ns = (std_dev_cycles * 1000) / (FCY / 1000000);

    // Rapport des résultats
    DEBUG_LOG_FLUSH("\r\n=== Jitter Characterization Results ===\r\n");
    DEBUG_LOG_FLUSH("Samples analyzed: "); debug_print_uint32(NUM_SAMPLES); DEBUG_LOG_FLUSH("\r\n");
    DEBUG_LOG_FLUSH("Min latency: "); debug_print_uint32(min_cycles); DEBUG_LOG_FLUSH(" cycles\r\n");
    DEBUG_LOG_FLUSH("Max latency: "); debug_print_uint32(max_cycles); DEBUG_LOG_FLUSH(" cycles\r\n");
    DEBUG_LOG_FLUSH("Avg latency: "); debug_print_uint32(avg_cycles); DEBUG_LOG_FLUSH(" cycles (");
    debug_print_uint32(avg_ns); DEBUG_LOG_FLUSH(" ns)\r\n");
    DEBUG_LOG_FLUSH("Peak-to-peak jitter: "); debug_print_uint32(jitter_pp_cycles); DEBUG_LOG_FLUSH(" cycles (");
    debug_print_uint32(jitter_pp_ns); DEBUG_LOG_FLUSH(" ns)\r\n");
    DEBUG_LOG_FLUSH("RMS jitter: "); debug_print_uint32(std_dev_cycles); DEBUG_LOG_FLUSH(" cycles (");
    debug_print_uint32(jitter_rms_ns); DEBUG_LOG_FLUSH(" ns)\r\n");

    // Analyse distribution
    uint32_t histogram[10] = {0};  // 10 bins
    uint32_t bin_size = (max_cycles - min_cycles) / 10;

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        uint32_t bin = (samples[i] - min_cycles) / (bin_size + 1);
        if (bin < 10) histogram[bin]++;
    }

    DEBUG_LOG_FLUSH("Distribution histogram:\r\n");
    for (int i = 0; i < 10; i++) {
        DEBUG_LOG_FLUSH("Bin "); debug_print_uint16(i); DEBUG_LOG_FLUSH(": ");
        debug_print_uint32(histogram[i]); DEBUG_LOG_FLUSH(" samples\r\n");
    }

    // Évaluation conformité SARSAT
    uint32_t sarsat_tolerance_ns = 25000;  // 25 µs (1% de 2.5ms)
    float jitter_percent = (float)jitter_pp_ns / sarsat_tolerance_ns * 100;

    DEBUG_LOG_FLUSH("\r\n=== SARSAT Compliance Analysis ===\r\n");
    DEBUG_LOG_FLUSH("SARSAT tolerance: "); debug_print_uint32(sarsat_tolerance_ns); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Measured jitter: "); debug_print_uint32(jitter_pp_ns); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Jitter ratio: "); debug_print_float(jitter_percent); DEBUG_LOG_FLUSH("% of tolerance\r\n");

    if (jitter_percent < 10.0) {
        DEBUG_LOG_FLUSH("Status: EXCELLENT (< 10% tolerance)\r\n");
    } else if (jitter_percent < 50.0) {
        DEBUG_LOG_FLUSH("Status: GOOD (< 50% tolerance)\r\n");
    } else if (jitter_percent < 100.0) {
        DEBUG_LOG_FLUSH("Status: ACCEPTABLE (< 100% tolerance)\r\n");
    } else {
        DEBUG_LOG_FLUSH("Status: WARNING (> 100% tolerance)\r\n");
    }

    DEBUG_LOG_FLUSH("=== Characterization Complete ===\r\n\r\n");
}

// Fonction auxiliaire pour calcul approximatif racine carrée
uint32_t sqrt_approx(uint64_t value) {
    if (value == 0) return 0;

    uint32_t x = value;
    uint32_t y = (x + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + value / x) / 2;
    }

    return x;
}
```

## 10. Outils de Diagnostic Temps Réel

### 10.1 Monitor de Performance Intégré
```c
// Système de monitoring jitter en temps réel
typedef struct {
    // Statistiques courantes
    uint32_t current_latency_ns;
    uint32_t min_latency_ns;
    uint32_t max_latency_ns;
    uint32_t avg_latency_ns;

    // Compteurs d'événements
    uint32_t total_samples;
    uint32_t timeout_errors;
    uint32_t excessive_jitter_count;

    // Seuils d'alerte
    uint32_t warning_threshold_ns;
    uint32_t critical_threshold_ns;

    // Flags d'état
    uint8_t monitoring_active;
    uint8_t alert_triggered;
} jitter_monitor_t;

volatile jitter_monitor_t jitter_monitor = {
    .warning_threshold_ns = 2000,   // 2 µs warning
    .critical_threshold_ns = 5000,  // 5 µs critical
    .monitoring_active = 1
};

// Fonction de monitoring appelée après chaque écriture DAC
void monitor_dac_performance(uint32_t latency_ns) {
    if (!jitter_monitor.monitoring_active) return;

    jitter_monitor.current_latency_ns = latency_ns;
    jitter_monitor.total_samples++;

    // Mise à jour statistiques
    if (jitter_monitor.total_samples == 1) {
        jitter_monitor.min_latency_ns = latency_ns;
        jitter_monitor.max_latency_ns = latency_ns;
        jitter_monitor.avg_latency_ns = latency_ns;
    } else {
        if (latency_ns < jitter_monitor.min_latency_ns) {
            jitter_monitor.min_latency_ns = latency_ns;
        }
        if (latency_ns > jitter_monitor.max_latency_ns) {
            jitter_monitor.max_latency_ns = latency_ns;
        }

        // Moyenne mobile sur 256 échantillons
        jitter_monitor.avg_latency_ns =
            (jitter_monitor.avg_latency_ns * 255 + latency_ns) / 256;
    }

    // Détection d'anomalies
    if (latency_ns > jitter_monitor.critical_threshold_ns) {
        jitter_monitor.excessive_jitter_count++;
        jitter_monitor.alert_triggered = 1;

        // Log critique (seulement une fois toutes les 1000 occurrences)
        if (jitter_monitor.excessive_jitter_count % 1000 == 1) {
            DEBUG_LOG_FLUSH("CRITICAL: DAC jitter exceeded ");
            debug_print_uint32(jitter_monitor.critical_threshold_ns);
            DEBUG_LOG_FLUSH(" ns (measured: ");
            debug_print_uint32(latency_ns);
            DEBUG_LOG_FLUSH(" ns)\r\n");
        }
    } else if (latency_ns > jitter_monitor.warning_threshold_ns) {
        // Comptage warnings silencieux
        static uint32_t warning_count = 0;
        if (++warning_count % 10000 == 1) {
            DEBUG_LOG_FLUSH("WARNING: DAC jitter high (");
            debug_print_uint32(latency_ns);
            DEBUG_LOG_FLUSH(" ns)\r\n");
        }
    }
}

// Rapport périodique de performance
void report_jitter_statistics(void) {
    if (jitter_monitor.total_samples == 0) {
        DEBUG_LOG_FLUSH("No DAC performance samples collected\r\n");
        return;
    }

    uint32_t jitter_pp = jitter_monitor.max_latency_ns - jitter_monitor.min_latency_ns;

    DEBUG_LOG_FLUSH("\r\n=== DAC Performance Report ===\r\n");
    DEBUG_LOG_FLUSH("Total samples: "); debug_print_uint32(jitter_monitor.total_samples); DEBUG_LOG_FLUSH("\r\n");
    DEBUG_LOG_FLUSH("Current latency: "); debug_print_uint32(jitter_monitor.current_latency_ns); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Min latency: "); debug_print_uint32(jitter_monitor.min_latency_ns); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Max latency: "); debug_print_uint32(jitter_monitor.max_latency_ns); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Avg latency: "); debug_print_uint32(jitter_monitor.avg_latency_ns); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Peak-to-peak jitter: "); debug_print_uint32(jitter_pp); DEBUG_LOG_FLUSH(" ns\r\n");
    DEBUG_LOG_FLUSH("Excessive jitter events: "); debug_print_uint32(jitter_monitor.excessive_jitter_count); DEBUG_LOG_FLUSH("\r\n");
    DEBUG_LOG_FLUSH("Timeout errors: "); debug_print_uint32(jitter_monitor.timeout_errors); DEBUG_LOG_FLUSH("\r\n");

    // Évaluation qualité
    if (jitter_pp < 500) {
        DEBUG_LOG_FLUSH("Performance: EXCELLENT\r\n");
    } else if (jitter_pp < 2000) {
        DEBUG_LOG_FLUSH("Performance: GOOD\r\n");
    } else if (jitter_pp < 10000) {
        DEBUG_LOG_FLUSH("Performance: ACCEPTABLE\r\n");
    } else {
        DEBUG_LOG_FLUSH("Performance: POOR - INVESTIGATION REQUIRED\r\n");
    }

    DEBUG_LOG_FLUSH("==============================\r\n\r\n");
}
```

## 11. Résumé Comparatif

| Aspect                 | DAC Interne     | MCP4922 @ 4MHz | MCP4922 @ 20MHz |
|------------------------|-----------------|----------------|-----------------|
| **Latence nominale**   | 10 ns           | 4000 ns        | 800 ns          |
| **Jitter typique**     | < 1 ns          | ±1000 ns       | ±200 ns         |
| **Jitter/Latence**     | < 10%           | ±25%           | ±25%            |
| **Prévisibilité**      | Parfaite        | Faible         | Moyenne         | 
| **Impact SARSAT**      | Négligeable     | Significatif   | Acceptable      |
| **Monitoring requis**  | Non             | Oui            | Recommandé      |
| **Optimisation**       | Non nécessaire  | Critique       | Importante      |

## 12. Recommandations Finales

### 12.1 Pour Usage SARSAT Critique
- **Privilégier DAC interne** pour applications temps réel strict
- **Jitter < 1% période symbole** (25 µs) obligatoire
- **Monitoring continu** si utilisation MCP4922

### 12.2 Pour Migration vers MCP4922
1. **SPI obligatoirement à 20 MHz** (réduction jitter facteur 5)
2. **Driver hautement optimisé** avec cache et timeout
3. **Monitoring temps réel** des performances
4. **Tests conformité** SARSAT approfondis
5. **Plan de fallback** vers DAC interne si nécessaire

### 12.3 Critères de Décision
```c
// Critères de choix DAC
if (application_requires_deterministic_timing() &&
    jitter_tolerance_ns < 100) {
    use_internal_dac();
} else if (dual_channel_required() ||
           external_vref_needed()) {
    use_mcp4922_optimized();
} else {
    use_internal_dac();  // Choix par défaut
}
```

## Conclusion

Le **jitter** représente l'**incertitude temporelle** dans l'exécution des opérations DAC. Pour le système SARSAT :

- **DAC interne** : Jitter négligeable (< 1 ns) → Performance optimale
- **MCP4922** : Jitter significatif (±1 µs) → Nécessite optimisation
- **Migration** : Possible avec SPI 20 MHz + driver optimisé pour maintenir jitter < 1% tolérance système

Le contrôle du jitter est **critique** pour maintenir la qualité et la conformité du signal SARSAT. La mesure et le monitoring continus sont essentiels lors de l'utilisation du MCP4922 en remplacement du DAC interne.
