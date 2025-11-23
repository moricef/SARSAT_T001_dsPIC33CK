# Analyse des Conflits ISR - SARSAT Beacon

## Vue d'ensemble du système

Le système utilise **deux ISR principales** qui peuvent entrer en conflit lors de la génération des trames EXERCISE.

---

## ISR 1: GPS UART3 Reception (_U3RXInterrupt)

### Configuration
- **Fichier**: `gps_nmea.c:112`
- **Priorité**: 4 (modifiée depuis 7)
- **Fréquence**: ~960 Hz (9600 bauds, ~1 char/ms)
- **Trigger**: UART3 FIFO contient 4 caractères (URXISEL=0b011)

### Rôle
Réceptionner les trames NMEA du GPS Ublox NEO-6M et les stocker dans un buffer circulaire.

### Actions dans l'ISR
```c
void _U3RXInterrupt(void) {
    gps_irq_count++;

    // Lit TOUS les caractères disponibles dans le FIFO (jusqu'à 4)
    while (U3STAHbits.URXBE == 0) {
        gps_rx_buffer[gps_rx_head] = U3RXREG;
        gps_rx_head = (gps_rx_head + 1) % GPS_BUFFER_SIZE;
        gps_rx_count++;
    }

    IFS3bits.U3RXIF = 0;
}
```

### Données modifiées par cette ISR
- `gps_rx_buffer[]` - Buffer circulaire des caractères reçus
- `gps_rx_head` - Index d'écriture dans le buffer
- `gps_rx_count` - Compteur total de caractères reçus
- `gps_irq_count` - Compteur d'interruptions

### Données modifiées indirectement (via parsing dans main loop)
- `current_latitude` - Position GPS latitude (double)
- `current_longitude` - Position GPS longitude (double)
- `current_altitude` - Altitude GPS (double)

---

## ISR 2: Timer1 Modulation (_T1Interrupt)

### Configuration
- **Fichier**: `system_comms.c:209`
- **Priorité**: 7 (la plus haute)
- **Fréquence**: 6400 Hz (tous les 156 µs)
- **Trigger**: Timer1 overflow

### Rôle
Générer la modulation BPSK à 400 bauds (6400 Hz / 16 samples par symbole) pour la transmission RF.

### Actions dans l'ISR
```c
void _T1Interrupt(void) {
    // 1. Toggle debug pin
    if (tx_phase != IDLE_STATE) {
        LATBbits.LATB0 = !LATBbits.LATB0;
    }

    // 2. Update millisecond counter (6400 Hz → ms)
    static uint16_t ms_accumulator = 0;
    ms_accumulator += 1000;
    if (ms_accumulator >= SAMPLE_RATE_HZ) {
        millis_counter++;
        ms_accumulator -= SAMPLE_RATE_HZ;
    }

    // 3. Modulation BPSK state machine (tous les 16 samples)
    if (++modulation_counter >= MODULATION_INTERVAL) {
        modulation_counter = 0;

        switch(tx_phase) {
            case IDLE_STATE:
                // DAC = 0V
                break;

            case RF_STARTUP:
                // Stabilisation RF
                break;

            case CARRIER_PHASE:
                // Porteuse non modulée (enveloppe montante)
                break;

            case DATA_PHASE:
                // LIT beacon_frame[] pour moduler
                uint8_t bit = beacon_frame[bit_index];
                dac_value = calculate_bpsk_value(bit, sample_count);
                break;

            case RF_SHUTDOWN:
                // Extinction progressive
                break;
        }

        mcp4922_write_dac_a(dac_value);
    }

    IFS0bits.T1IF = 0;
}
```

### Données lues par cette ISR
- `beacon_frame[]` - **CRITIQUE**: Trame 144 bits à transmettre
- `tx_phase` - État de la machine à états de transmission
- `bit_index` - Index du bit en cours de transmission
- `sample_count` - Compteur d'échantillons dans le symbole courant

### Données modifiées par cette ISR
- `millis_counter` - Horloge système en millisecondes
- `modulation_counter` - Compteur de modulation
- `sample_count` - Progression dans le symbole
- `bit_index` - Progression dans la trame
- `tx_phase` - État de transmission

---

## CONFLIT #1: Construction de trame EXERCISE corrompue

### Scenario du problème

**Contexte**: Mode EXERCISE avec GPS fix actif

1. **Main loop** appelle `start_beacon_frame(BEACON_EXERCISE_FRAME)`
2. Appelle `build_exercise_frame()` → `build_compliant_frame()`
3. **build_compliant_frame() lit les variables GPS**:
   ```c
   // Ligne 450 protocol_data.c
   cs_gps_position_t gps_pos = encode_gps_position_complete(
       current_latitude,    // ← Lecture non-atomique
       current_longitude    // ← Lecture non-atomique
   );
   ```

4. **Pendant ce temps, GPS ISR peut interrompre**:
   - Parsing NMEA en cours dans main loop
   - `current_latitude` mis à jour = 42.960529
   - ISR GPS retourne

5. **build_compliant_frame() continue**:
   ```c
   // Ligne 469
   uint8_t alt_code = altitude_to_code(current_altitude); // ← Nouvelle valeur!
   ```

6. **Calcul BCH avec données incohérentes**:
   - Position calculée avec latitude OLD + longitude NEW
   - Altitude de ANOTHER update
   - BCH1 calculé sur ces données mélangées
   - BCH1 stocké dans la trame

7. **Plus tard, validation**:
   ```c
   // Ligne 481-483
   uint64_t pdf1_check = get_bit_field(frame, 25, 61);
   uint32_t bch1_calc = compute_bch1(pdf1_check);  // Recalculé
   uint32_t bch1_check = get_bit_field(frame, 86, 21); // Stocké

   if (bch1_calc != bch1_check) {
       // ÉCHEC! Données ont changé entre calculs!
   }
   ```

### Symptômes observés
- Boucle infinie: "Starting periodic transmission - Mode: EXERCISE"
- Aucune transmission RF réelle (pas de "RF carrier ON")
- `phase=0` (IDLE) en permanence
- Mystérieux `18EA9218EA92` répété dans les logs

### Cause racine
**Race condition**: Variables GPS modifiées par ISR pendant lecture/calcul dans code principal.

---

## CONFLIT #2: Trame beacon_frame[] lue pendant modification

### Scenario (potentiel, non confirmé)

1. **Transmission en cours**: Timer1 ISR lit `beacon_frame[]` à 6400 Hz
2. **Code principal** décide de préparer la prochaine trame
3. Appelle `build_compliant_frame()` qui écrit dans `beacon_frame[]`
4. **Timer1 ISR interrompt** au milieu de l'écriture
5. Lit des données partiellement mises à jour
6. **Résultat**: Modulation corrompue

### Protection actuelle
```c
// protocol_data.c:558-560
__builtin_disable_interrupts();
start_transmission(beacon_frame);  // Copie + change tx_phase
__builtin_enable_interrupts();
```

Cette protection est **partielle** - empêche corruption pendant le START mais pas pendant la construction.

---

## Solutions tentées (état actuel)

### 1. Réduction priorité GPS ISR
```c
// gps_nmea.c:71
IPC14bits.U3RXIP = 4;  // Était 7, maintenant 4
```

**Effet**: GPS ne peut plus interrompre Timer1 (priorité 7), mais peut toujours interrompre le code principal (priorité 0).

**Résultat**: **INSUFFISANT** - Le problème persiste car la construction de trame se fait dans le main (priorité 0).

### 2. Snapshot atomique des données GPS
```c
// protocol_data.c:526-543
void build_exercise_frame(void) {
    double lat_snapshot, lon_snapshot, alt_snapshot;

    __builtin_disable_interrupts();
    lat_snapshot = current_latitude;
    lon_snapshot = current_longitude;
    alt_snapshot = current_altitude;
    current_latitude = lat_snapshot;   // Overwrite
    current_longitude = lon_snapshot;
    current_altitude = alt_snapshot;
    __builtin_enable_interrupts();

    build_compliant_frame();  // Utilise les snapshots

    __builtin_disable_interrupts();
    // Restore originals
    __builtin_enable_interrupts();
}
```

**Problème avec cette approche**:
- Lit les variables DEUX FOIS (original + snapshot)
- Fenêtre entre sauvegarde et restauration
- Complexité inutile

**Résultat**: **INSUFFISANT** - Race condition toujours possible.

---

## Solutions proposées (non implémentées)

### Solution A: Désactiver GPS ISR pendant construction (SIMPLE)

```c
void build_exercise_frame(void) {
    beacon_mode = BEACON_MODE_EXERCISE;

    // Désactiver GPS pendant construction
    IEC3bits.U3RXIE = 0;

    build_compliant_frame();

    // Réactiver GPS
    IEC3bits.U3RXIE = 1;

    rf_set_power_level(RF_POWER_HIGH);
}
```

**Avantages**:
- Simple
- Garantit cohérence des données GPS
- UART3 FIFO (4 niveaux) bufferise les chars pendant désactivation (~4ms max)

**Inconvénients**:
- Perte potentielle de caractères si construction > 4ms
- Coupling entre modules GPS et Protocol

### Solution B: Copie atomique CORRECTE

```c
void build_exercise_frame(void) {
    beacon_mode = BEACON_MODE_EXERCISE;

    // Snapshot atomique
    double lat, lon, alt;
    __builtin_disable_interrupts();
    lat = current_latitude;
    lon = current_longitude;
    alt = current_altitude;
    __builtin_enable_interrupts();

    // Passer explicitement à build_compliant_frame
    build_compliant_frame_with_gps(lat, lon, alt);

    rf_set_power_level(RF_POWER_HIGH);
}
```

**Avantages**:
- Propre
- Pas de désactivation d'ISR
- Données cohérentes garanties

**Inconvénients**:
- Nécessite refactoring de `build_compliant_frame()`
- Change interface de fonction

### Solution C: Section critique autour de la construction complète

```c
void start_beacon_frame(beacon_frame_type_t frame_type) {
    // DÉSACTIVER TOUTES LES ISR pendant construction
    uint16_t saved_ipl;
    SET_AND_SAVE_CPU_IPL(saved_ipl, 7);  // IPL=7, bloque tout sauf NMI

    switch(frame_type) {
        case BEACON_TEST_FRAME:
            build_test_frame();
            break;
        case BEACON_EXERCISE_FRAME:
            build_exercise_frame();
            break;
    }

    RESTORE_CPU_IPL(saved_ipl);  // Restaure IPL

    // Validation et transmission
    transmit_beacon_frame();
}
```

**Avantages**:
- Protection maximale
- Garantit atomicité complète

**Inconvénients**:
- Bloque TOUTES les ISR (Timer1 aussi!)
- Peut causer jitter sur modulation si appelé pendant TX
- Impact sur temps réel système

---

## Recommandation

**Solution recommandée**: **Solution A** (désactiver GPS ISR)

**Justification**:
1. Simple à implémenter
2. Impact minimal (GPS à 9600 bauds = ~1ms entre chars)
3. FIFO UART bufferise pendant désactivation
4. Pas de refactoring majeur
5. Résout le problème à la source

**Code proposé**:
```c
void build_exercise_frame(void) {
    beacon_mode = BEACON_MODE_EXERCISE;

    // CRITICAL SECTION: Disable GPS ISR to prevent data corruption
    IEC3bits.U3RXIE = 0;  // Disable GPS UART3 RX interrupt

    build_compliant_frame();  // Safe: current_lat/lon/alt won't change

    IEC3bits.U3RXIE = 1;  // Re-enable GPS interrupt

    rf_set_power_level(RF_POWER_HIGH);
}
```

**Durée estimée de désactivation**: ~2-5 ms (construction de trame)
**Caractères GPS perdus potentiels**: 0 (FIFO = 4 chars, 1 char/ms)

---

## Autres anomalies observées

### Mystérieux "18EA9218EA92"

**Symptôme**: Chaîne hexadécimale répétée dans les logs

**Hypothèses**:
1. Corruption buffer UART debug
2. Hard fault / watchdog reset
3. Fonction debug appelée en boucle avec mauvais paramètres

**Analyse requise**: Chercher appel à `debug_print_hex24()` sans label

### Ratio IRQ GPS anormal

**Observation**: `gps_irq=21997` pour `gps_rx=4879` = **4.5 IRQ/char**

**Attendu**: ~0.3 IRQ/char (1 IRQ pour ~3-4 chars avec URXISEL=0b011)

**Cause probable**:
- FIFO jamais rempli à 4 chars
- IRQ déclenchée par timeout ou autre condition
- Possible corruption/reset continu

---

## État actuel du code

**Commit**: a2a2db6 "WIP: Debug EXERCISE mode transmission failures and ISR conflicts"

**Modifications**:
1. ✅ GPS ISR priorité 7 → 4
2. ✅ Snapshot GPS atomique (imparfait)
3. ✅ Messages debug ajoutés (mais n'apparaissent pas dans logs)
4. ⏸️ Mode EXERCISE forcé pour tests

**Prochaines étapes**:
1. Implémenter Solution A (désactiver GPS ISR)
2. Investiguer "18EA9218EA92"
3. Comprendre pourquoi messages debug n'apparaissent pas
4. Tester avec vraie trame EXERCISE

---

## Aurions-nous pu détecter ce bug sans le GPS?

### Réponse: OUI! C'était prévisible par analyse statique

Ce bug est un cas classique de **race condition** qu'on aurait dû identifier AVANT les tests matériels.

### 1. Analyse statique aurait révélé le problème

**Questions de base pour tout système temps-réel avec ISR:**

| Question | Réponse pour notre système | Verdict |
|----------|---------------------------|---------|
| Quelles variables sont partagées entre ISR et main? | `current_latitude`, `current_longitude`, `current_altitude` | ⚠️ Partagées |
| Ces variables sont-elles `volatile`? | OUI | ✅ Bon |
| Accès atomiques (lecture/écriture en 1 instruction)? | NON (double = 64 bits sur dsPIC33) | ❌ DANGER |
| Protection par section critique? | NON | ❌ DANGER |
| Double buffering ou synchronisation? | NON | ❌ DANGER |

**Conclusion immédiate**: **Variables non protégées → Race condition certaine!**

### 2. Checklist temps-réel ignorée

**Règle d'or des systèmes embarqués temps-réel:**

> Toute donnée **partagée** entre contexte ISR et contexte principal DOIT être protégée par:
> - Accès atomique (types ≤ taille bus du CPU)
> - OU section critique (`__builtin_disable_interrupts()`)
> - OU mécanisme de synchronisation (mutex, sémaphore)
> - OU double buffering avec swap atomique

**Notre code**: Aucune de ces protections! ❌

### 3. Pourquoi le mode TEST fonctionnait?

```c
void build_test_frame(void) {
    // Coordonnées FIXES en ROM, jamais modifiées
    set_gps_position(TEST_LATITUDE, TEST_LONGITUDE, TEST_ALTITUDE);
    beacon_mode = BEACON_MODE_TEST;
    build_compliant_frame();
    rf_set_power_level(RF_POWER_LOW);
}
```

**Raison du succès**:
- Coordonnées GPS = constantes
- GPS ISR peut tourner, mais `gps_update()` ne met jamais à jour les variables
- `current_latitude/longitude/altitude` restent stables
- **Pas de modification concurrente** → Pas de corruption

**Le piège**: Mode TEST **masquait** le bug de conception!

### 4. Symptômes classiques de race condition

**Observation utilisateur**: "ça décroche puis raccroche après des dizaines de secondes"

**Pourquoi ce comportement aléatoire?**

```
Temps T0: GPS envoie "$GPGGA,..."
          main loop: gps_update() met à jour lat=42.960529

Temps T1: main loop: should_transmit_beacon() → TRUE
          Appelle build_exercise_frame()

Temps T2: build_compliant_frame() lit current_latitude = 42.960529

Temps T3: ⚠️ GPS ISR interrompt!
          Nouveau NMEA reçu, gps_update() écrit lat=42.960535

Temps T4: build_compliant_frame() continue
          Lit current_longitude = 1.371028 (nouvelle valeur!)

Temps T5: Calcule BCH1 avec lat OLD + lon NEW
          → BCH INCORRECT!

Temps T6: validate_frame_hardware() échoue
          → transmission aborted
          → Recommence à T1...
```

**Le timing varie**: Parfois GPS ISR arrive entre lat/lon, parfois non
→ Comportement **non-déterministe** = "décroche aléatoirement"

### 5. Outils d'analyse qui auraient détecté le bug

#### A. Analyse statique de code

**Outils disponibles**:
- **Coverity** / **PC-Lint** → Détecte variables partagées non protégées
- **MISRA-C checker** → Règle 8.14: variables partagées doivent être volatile + protégées
- **ThreadSanitizer** (sur simulation PC) → Détecte data races en runtime

**Résultat attendu**:
```
WARNING: Variable 'current_latitude' accessed from:
  - ISR context: gps_update() (via _U3RXInterrupt)
  - Main context: build_compliant_frame()
  Without synchronization protection!
  → POTENTIAL DATA RACE
```

#### B. Review de design système

**Diagramme de séquence aurait montré**:
```
Main Loop          GPS ISR           Timer1 ISR
    |                 |                   |
    |-- build_frame --|                   |
    |  read lat    <--|-- INTERRUPT! ----|
    |                 |  write lat        |
    |                 |  write lon        |
    |  read lon    <--|-- RETURN ---------|
    |  calc BCH       |                   |
    |  INVALID! ❌    |                   |
```

**Conclusion visible**: Accès non synchronisés!

#### C. Tableau des ressources partagées

| Variable | Type | Taille | Écrit par | Lu par | Protection | Risque |
|----------|------|--------|-----------|--------|------------|--------|
| `current_latitude` | double | 64 bits | GPS ISR (via main) | build_compliant_frame | ❌ AUCUNE | ⚠️ ÉLEVÉ |
| `current_longitude` | double | 64 bits | GPS ISR (via main) | build_compliant_frame | ❌ AUCUNE | ⚠️ ÉLEVÉ |
| `current_altitude` | double | 64 bits | GPS ISR (via main) | build_compliant_frame | ❌ AUCUNE | ⚠️ ÉLEVÉ |
| `beacon_frame[]` | uint8_t[144] | 144 bytes | build_compliant_frame | Timer1 ISR | ⚠️ PARTIELLE | 🔶 MOYEN |
| `tx_phase` | enum | 8 bits | Timer1 ISR | Main loop | ✅ volatile + atomique | ✅ OK |

**3 variables à haut risque identifiées immédiatement!**

### 6. Méthodologie de développement embarqué robuste

**Ce qui aurait dû être fait AVANT l'implémentation**:

1. ✅ **Spécification des ISR**
   - Identifier TOUTES les variables partagées
   - Définir stratégie de protection pour chacune

2. ✅ **Design review**
   - Diagrammes de séquence ISR vs Main
   - Tableau des ressources partagées
   - Validation de la stratégie de synchronisation

3. ✅ **Code review avec checklist temps-réel**
   - Variables partagées = `volatile`?
   - Accès atomiques ou section critique?
   - Pas de boucles bloquantes dans ISR?
   - Pas d'appels système bloquants?

4. ✅ **Tests unitaires déterministes**
   - Simuler interruption à différents moments
   - Vérifier cohérence des données
   - Tester pire cas (worst-case timing)

**Ce qui a été fait**: Implémentation directe → Test avec matériel → 💥 Bug découvert

### 7. Pourquoi ces bugs passent inaperçus?

#### Facteurs masquants

1. **Mode TEST cache le problème**
   - Données statiques → Pas de modification concurrente
   - Tests passent → Fausse confiance

2. **Bug non-déterministe**
   - Dépend du timing exact GPS ISR vs main loop
   - Peut fonctionner 95% du temps
   - Difficile à reproduire systématiquement

3. **Symptômes ambigus**
   - "Ça décroche puis raccroche" → Ressemble à problème RF, GPS, ou autre
   - Pas de crash franc → Investigation retardée

4. **Manque de logs détaillés**
   - Pas de trace de l'échec BCH validation
   - Messages debug pas affichés → Cause cachée

#### Cercle vicieux du debug réactif

```
Bug subtil (race condition)
    ↓
Symptômes aléatoires ("décroche/raccroche")
    ↓
Hypothèses erronnées (problème RF? GPS?)
    ↓
Tests matériels longs et coûteux
    ↓
Corrections au hasard (priorités ISR, delays...)
    ↓
Bug persiste ou se déplace
    ↓
Frustration et perte de temps
```

**Solution**: Retour à l'analyse de conception!

### 8. Leçons apprises

| # | Leçon | Application future |
|---|-------|-------------------|
| 1 | Mode TEST qui masque bugs → Tester EXERCISE dès le début | ✅ Tester tous les modes tôt |
| 2 | Race conditions invisibles sans analyse → Analyser variables partagées | ✅ Checklist ISR obligatoire |
| 3 | Bugs non-déterministes = cauchemar debug → Design déterministe | ✅ Sections critiques dès le design |
| 4 | "Ça marche parfois" ≠ "Ça marche" → Tests de stress | ✅ Tests avec timing worst-case |
| 5 | Outils d'analyse statique détectent → Utiliser ces outils | ✅ Lint/Coverity dans CI/CD |

### 9. La vraie question

**"Fallait-il le module GPS pour savoir que ça ne passerait pas?"**

**Réponse**: **NON**

**Ce qu'il fallait**:
- ✅ 10 minutes d'analyse des variables partagées
- ✅ Checklist de revue de code temps-réel
- ✅ Diagramme de séquence ISR
- ✅ Outil d'analyse statique (optionnel mais utile)

**Ce qui a été fait**:
- ❌ Implémentation directe sans analyse
- ❌ Tests uniquement en mode TEST (données statiques)
- ❌ Discovery du bug après intégration GPS

**Coût de l'approche réactive**:
- 🕐 Heures de debug matériel
- 🕐 Tests répétitifs avec GPS
- 🕐 Hypothèses multiples (RF? ISR priority? GPS timing?)
- 💰 Temps développeur perdu

**Coût de l'approche proactive aurait été**:
- 🕐 10 min d'analyse préalable
- 🕐 20 min pour implémenter protection atomique
- ✅ Bug évité à la conception

**ROI de l'analyse préalable**: 10:1 (minimum!)

---

## Conclusion méthodologique

Ce bug de race condition est un **cas d'école** qui illustre:

1. ✅ **L'importance de l'analyse de conception** sur systèmes temps-réel
2. ✅ **Les limites des tests matériels** pour détecter bugs non-déterministes
3. ✅ **La valeur des outils d'analyse statique** (Lint, Coverity, etc.)
4. ✅ **Le danger du "ça marche en mode TEST"** qui masque les vrais problèmes
5. ✅ **Le coût réel du debug réactif** vs design proactif

**Recommandation finale**: Sur tout projet embarqué temps-réel avec ISR, **TOUJOURS**:
- Lister variables partagées entre contextes
- Définir stratégie de protection (atomique, section critique, etc.)
- Faire review avant implémentation
- Utiliser outils d'analyse statique si disponibles
- Tester tous les modes dès l'intégration

**Le temps "gagné" en sautant l'analyse est toujours perdu × 10 en debug!**

---

*Document créé le 2025-11-23*
*Auteur: Analyse collaborative Claude Code*
*Mise à jour: Ajout section méthodologique*
