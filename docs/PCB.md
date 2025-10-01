# Guide PCB RF pour SARSAT 403 MHz
*Documentation pour architecture PCB empilée avec cartes d'évaluation*

## 🎯 Architecture Finale
Système empilé 4 étages avec cartes d'évaluation interconnectées par câbles coax RG316 et alimentation distribuée.

## 🏗️ Configuration Étagement Vertical

### Structure "Millefeuille"
```
┌─────────────────────────────────┐ ← 4ème étage
│  RA07M4047M + RADIATEUR ALU     │   (Chaleur évacuée vers le haut)
│  Régulateur 7.5V                │   (PA 403 MHz)
├─────────────────────────────────┤ ← 3ème étage
│  Carte Eval ADL5375-05          │   (Modulateur I/Q)
│  Connecteurs SMA + 5V intégrée  │   (Loin de la chaleur PA)
├─────────────────────────────────┤ ← 2ème étage
│  Carte Eval ADF4351             │   (Synthétiseur LO)
│  Connecteurs SMA + 5V intégrée  │   (Isolation RF)
├─────────────────────────────────┤ ← 1er étage (base)
│  dsPIC33CK64MC105 Curiosity Nano│   (USB accessible)
│  MCP4922 + LMV358 + Régulateurs │   (Analogique sensible)
│  Alimentation 3.3V + 5V + 7.5V  │
└─────────────────────────────────┘
```

### Chaîne RF Complète
```
[dsPIC33CK] → [MCP4922] → [LMV358] → [ADL5375] → [PA] → [Antenne]
     ↓            ↓         ↓           ↑        ↑
   SPI Ctrl    I/Q DAC   Op-Amp     RG316     PCB
     ↓
[ADF4351] ──────────RG316──────────┘ (LO)
```

## ⚡ Distribution Alimentation

### Sources d'Alimentation
- **USB-C** : Curiosity Nano uniquement (5V intégré)
- **Batterie 12V LiFePO4** : Alimentation principale propre (sans bruit RF)

### Régulateurs Linéaires (Anti-Bruit RF)
```
12V LiFePO4 ──┬── 7805 ──► 5V/1A (Cartes eval ADF4351 + ADL5375)
              ├── LM1117-3.3 ──► 3.3V/800mA (MCP4922 + LMV358)
              └── LM317 ──► 7.5V/1.5A (PA RA07M4047M)

USB-C ──► 5V (Curiosity Nano uniquement)
```

### Pourquoi Régulateurs Linéaires ?
- **Pas de bruit RF** (vs alimentations à découpage)
- **Stabilité** pour circuits analogiques sensibles
- **Simplicité** et robustesse
- **Isolation** entre étages

## Calculs Impédance 50Ω

### Stackup Double Face Standard
```
TOP    : Composants + Routage RF + Numérique
BOTTOM : Plan de masse quasi-continu + quelques pistes

FR4 1.6mm double face :
- Largeur piste ≈ 2.8-3.2mm pour ~50Ω
- Vérification nécessaire par coupon de test
```

### Formule Microstrip (approximative)
```
Largeur piste 50Ω = W = 2 × h × (Z₀/377) × √εᵣ

Exemple FR4 standard :
- h = 1.6mm (épaisseur PCB double face)
- εᵣ = 4.3 (FR4)
- Z₀ = 50Ω
→ W ≈ 3mm (largeur piste)
```

## Techniques Double Face pour RF

### 1. Plan de Masse "Hacké"
```
- Bottom = 90% cuivre (plan de masse)
- Petites coupures pour pistes critiques (alim, signaux)
- Reconnexions par vias nombreux
```

### 2. Fabrication Vias
**Équipement nécessaire :**
- Perceuse + mèche 0.8mm
- Fil de cuivre 0.6mm
- Soudure

**Technique :**
1. Perçage perpendiculaire
2. Insertion fil cuivre 0.6mm
3. Soudure des 2 côtés
4. Découpe ras

**Espacement :** Via de masse tous les 5mm le long des pistes RF

### 3. Layout Physique Optimal
```
TOP:    [ADF4351] ═══► [ADL5375] ═══► [PA] ═══► [Sortie]
         ↓ vias     ↓ vias      ↓ vias    ↓ vias
BOTTOM:  ████████████████████████████████████ (masse)
```

## 🛡️ Règles de Routage RF

### Pistes RF
- **Largeur** : 3mm (à valider par coupon)
- **Coudes** : Jamais 90° → utiliser 45° ou courbes
- **Garde** : 2× largeur piste autour du RF (6mm minimum)
- **Pas de via** sur pistes RF (sauf indispensable)

### Plan de Masse
- **Continu** sous toutes les pistes RF
- Vias de masse tous les 5mm le long des pistes RF
- **Isolation** entre sections : gardes de 2mm minimum

### Découplage Alimentation
- **100nF très proche** de chaque IC RF (<2mm)
- **10µF** pour découplage basse fréquence
- **Via direct** vers plan de masse

## 🔬 Coupons de Test

### Principe
Tester différentes largeurs AVANT le PCB final pour optimiser l'impédance.

### Conception Coupon
```
PCB test 50mm × 30mm :

Piste 1 : ═══ 2.0mm ═══  (50mm long) + SMA
Piste 2 : ═══ 2.5mm ═══  (50mm long) + SMA
Piste 3 : ═══ 3.0mm ═══  (50mm long) + SMA
Piste 4 : ═══ 3.5mm ═══  (50mm long) + SMA

Toutes avec plan de masse BOTTOM + vias tous les 5mm
```

### Procédure Test avec NanoVNA

1. **Calibrer** NanoVNA (Open/Short/Load)

2. **Test chaque piste** :
   - Connecteur SMA sur début piste
   - Charge 50Ω sur fin piste
   - Mesure S11 → impédance

3. **Interprétation** :
   ```
   S11 ≈ -20dB = impédance proche 50Ω ✅
   S11 ≈ -10dB = impédance éloignée 50Ω ❌
   S11 ≈ -5dB  = très mauvais ❌
   ```

### Résultats Attendus
| Largeur | Impédance | S11 | Verdict |
|---------|-----------|-----|---------|
| 2.0mm   | ~65Ω      | -12dB | Trop fin |
| 2.5mm   | ~55Ω      | -18dB | Presque bon |
| **3.0mm** | **~50Ω** | **-25dB** | **PARFAIT** ✅ |
| 3.5mm   | ~45Ω      | -15dB | Trop large |

## 🔌 Connecteurs SMA

### SMA Femelle Bord de PCB (Recommandé)
- **Avantage** : Facile à souder, précis
- **Usage** : Parfait pour coupons de test
- **Connexion** : Piste RF directe, masse sur plan

### Footprint SMA Edge
```
Pad central : connexion piste RF
Pads masse : connexion plan de masse
Position : au bord du PCB
```

## 📐 Layout Composants RF

### Chaîne RF Linéaire
```
[uC/DAC] → [ADF4351] ──50Ω──► [ADL5375] ──50Ω──► [PA] ──50Ω──► [ANT]
              ↑                    ↑                 ↑
         Découplage          Découplage        Découplage
         100nF+10µF         100nF+10µF       100nF+10µF
```

### Isolation Entre Sections
- **Garde physique** : 2mm minimum entre ADF4351/ADL5375/PA
- **Plan de masse** : Vias de garde pour isolation
- **Keepout zones** : Pas de routage numérique sous RF

## 🔧 Outils de Mesure

### NanoVNA (Analyseur Réseau)
- **S11** : Vérification impédance pistes
- **S21** : Pertes insertion entre modules
- **TDR** : Localisation défauts sur pistes
- **Calibration** : Obligatoire avant mesures

### Oscilloscope 150MHz
- **Signaux digitaux** : OK pour logique
- **RF 403MHz** : Limité mais utile pour enveloppes
- **Transients** : Détection problèmes temporels

## ✅ Points principaux

1. **Coupons de test** avec mesures NanoVNA documentées
2. **Via stitching** régulier (tous les 5mm)
3. **Impédance contrôlée** mesurée et validée
4. **Isolation RF** entre sections
5. **Découplage** approprié et positionné
6. **Plan de masse** continu et bien connecté

## 🎯 Méthodologie de Développement

1. **Coupon de test** → Validation largeurs pistes
2. **Mesures NanoVNA** → Optimisation impédance
3. **PCB final** → Application valeurs testées
4. **Tests fonctionnels** → Validation performance RF
5. **Comparaison** avec version breadboard

## 📋 Checklist Finale PCB RF

- [ ] Largeur pistes RF validée par coupon (≈3mm)
- [ ] Plan de masse continu sur bottom
- [ ] Vias de masse tous les 5mm
- [ ] Découplage 100nF + 10µF sur chaque IC RF
- [ ] Garde 6mm autour des pistes RF
- [ ] Pas de coudes 90° sur RF
- [ ] Connecteurs SMA correctement positionnés
- [ ] Isolation entre sections RF
- [ ] Test points pour mesures futures

## 🔌 Interconnexions Entre Étages

### Signaux RF (Câbles Coax RG316)
```
ADF4351 (LO) ──RG316 SMA M/M──► ADL5375 (LO Input)
ADL5375 (RF Out) ──RG316 SMA M/M──► PA (RF Input)
PA (RF Out) ──RG316 SMA M/M──► Antenne
```

### Signaux Analogiques I/Q
```
MCP4922 ──► LMV358 ──► Connecteur vers ADL5375
(DAC)      (Op-Amp)   (I+ I- Q+ Q- + bias 500mV)
```

### Signaux Numériques
```
dsPIC33CK ──SPI──► MCP4922 (DAC I/Q)
dsPIC33CK ──SPI──► ADF4351 (Fréquence LO)
dsPIC33CK ──GPIO──► ADL5375 (Enable)
dsPIC33CK ──GPIO──► PA (Enable)
```

### Distribution Alimentation
```
1er étage ──5V──► 2ème étage (ADF4351)
1er étage ──5V──► 3ème étage (ADL5375)
1er étage ──7.5V──► 4ème étage (PA)
```

## 📐 PCB Principal (1er Étage)

### Composants sur PCB
- **Support Curiosity Nano** (headers femelles)
- **MCP4922** (DAC SPI dual 12-bit)
- **LMV358** (Op-amp dual pour I/Q)
- **Régulateurs** : 7805, LM1117-3.3, LM317
- **Connecteurs** : SMA, headers, alimentation
- **Découplage** : 100nF + 10µF par section

### Sections PCB Critiques

#### 1. Section Alimentation
- **Régulateurs linéaires** avec dissipateurs
- **Découplage massif** : 100µF + 10µF + 100nF
- **Filtrage RF** : ferrites sur lignes d'alimentation
- **Protection** : fusibles, diodes

#### 2. Section Analogique I/Q
- **Plan de masse** continu sous MCP4922/LMV358
- **Pistes courtes** DAC → Op-amp → connecteur
- **Découplage** très proche des ICs
- **Blindage** section analogique vs numérique

#### 3. Section Connexions
- **Connecteurs SMA** pour sorties I/Q vers ADL5375
- **Headers** alimentation vers étages supérieurs
- **Headers** signaux SPI/GPIO vers cartes eval

## 🔥 PCB PA (4ème Étage)

### Spécifications RF Critiques
- **Impédance 50Ω** : Pistes 3mm sur FR4 1.6mm
- **Plan de masse** massif pour dissipation thermique
- **Via thermiques** : connexion PCB → dissipateur alu
- **Pistes courtes** : entrée SMA → PA → sortie SMA

### Gestion Thermique
```
RA07M4047M ──Pad thermique──► PCB (via thermiques) ──► Dissipateur Alu
```

### Layout PA
```
[SMA In] ──3mm/50Ω──► [RA07M4047M] ──3mm/50Ω──► [SMA Out]
                           ↓
                    [Via thermiques]
                           ↓
                    [Plan masse PCB]
                           ↓
                    [Dissipateur Alu]
```

## 🎯 Avantages Architecture Empilée

### Thermique
- **PA isolé en haut** : chaleur évacuée naturellement
- **Composants sensibles protégés** en bas
- **Dissipation optimale** par convection

### RF
- **Liaisons courtes** : RG316 de qualité
- **Isolation** : cartes eval sur étages séparés
- **Pas de couplage** entre sections RF

### Mécanique
- **Structure stable** : étages auto-supportés
- **Accessibilité** : debug Nano en bas
- **Flexibilité** : modification d'un étage sans impact

### Électrique
- **Alimentation propre** : régulateurs linéaires
- **Distribution étage par étage** : découplage optimal
- **Signaux séparés** : RF / analogique / numérique

## 💡 Points principaux

1. **Architecture thermique** réfléchie (PA en haut)
2. **Alimentation propre** (linéaires vs découpage)
3. **Interconnexions maîtrisées** (RG316 + SMA)
4. **Sections séparées** (RF/analogique/numérique)
5. **Validation par étapes** (approche méthodique)
6. **Gestion thermique PA** (via + dissipateur)

---
*Architecture finale suite aux corrections timing RF et choix cartes d'évaluation*
