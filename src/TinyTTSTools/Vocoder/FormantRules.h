#pragma once
#include <cstddef>
#include "stdint.h"


/// Data-driven phoneme rule table
enum PhonemeFlagBits : uint16_t {
  PF_UNVOICED = 0,          // explicit alias for unvoiced (no flag bits set)
  PF_VOICED = 1 << 0,
  PF_STRONG_SIBILANT = 1 << 1,
  PF_POSTALVEOLAR = 1 << 2,
  PF_NASAL = 1 << 3,
  PF_SILENCE = 1 << 4
};

/// Static formant frequency, amplitude and bandwidth specification
struct FormantParams {
  float f1, f2, f3;     ///< Formant frequencies (Hz)
  float a1, a2, a3;     ///< Formant amplitudes (0-1)
  float bw1, bw2, bw3;  ///< Formant bandwidths (Hz)
};

/// Table entry describing one phoneme's acoustic parameters and behavior
struct PhonemeRule {
  const char* symbol;      // phoneme string (ARPAbet)
  uint16_t flags;          // bitmask of PhonemeFlagBits
  int16_t duration_ms;     // default duration in milliseconds
  bool hasDiphthong;       // true if diphthong with target2
  float dF1, dF2, dF3;     // diphthong 2nd target F1-F3 (amplitudes/bw same)
  FormantParams params;    // base formant parameters
};

/// ARPAbet phoneme vocoder rules
static const PhonemeRule kRules[] = {
  {"SIL", PF_SILENCE, 400,  false, 0,0,0, {0,0,0,0,0,0,0,0,0}}, // full silence
  {"SP",  PF_SILENCE, 30,   false, 0,0,0, {0,0,0,0,0,0,0,0,0}}, // short pause
  {"AA", PF_VOICED, 150, false, 0,0,0, {730,1090,2440,1.0f,0.8f,0.6f,80,90,120}},
  {"AE", PF_VOICED, 150, false, 0,0,0, {660,1720,2410,1.0f,0.9f,0.6f,80,90,120}},
  {"AH", PF_VOICED, 150, false, 0,0,0, {520,1190,2390,1.0f,0.8f,0.6f,70,80,100}},
  {"AH0", PF_VOICED, 80,  false, 0,0,0, {450,1400,2500,0.9f,0.6f,0.5f,70,90,120}},
  {"AO", PF_VOICED, 150, false, 0,0,0, {570,840,2410,1.0f,0.8f,0.6f,80,90,120}},
  {"AW", PF_VOICED, 150, true,  440,1020,2240, {570,1020,2410,1.0f,0.8f,0.6f,80,90,120}},
  {"AY", PF_VOICED, 150, true,  300,2200,2600, {660,1200,2550,1.0f,0.9f,0.6f,80,90,120}},
  {"EH", PF_VOICED, 150, false, 0,0,0, {530,1840,2480,1.0f,0.9f,0.6f,70,90,120}},
  {"ER", PF_VOICED, 150, false, 0,0,0, {490,1350,1690,1.0f,0.8f,0.7f,70,80,100}},
  {"ER0", PF_VOICED, 90,  false, 0,0,0, {470,1350,1600,0.9f,0.7f,0.6f,70,80,100}},
  {"EY", PF_VOICED, 150, true,  270,2290,3010, {530,1840,2480,1.0f,0.9f,0.6f,70,90,120}},
  {"IH", PF_VOICED, 150, false, 0,0,0, {390,1990,2550,1.0f,0.9f,0.6f,60,90,120}},
  {"IY", PF_VOICED, 150, false, 0,0,0, {270,2290,3010,1.0f,1.0f,0.7f,50,100,140}},
  {"OW", PF_VOICED, 150, true,  300,870,2240, {570,840,2410,1.0f,0.8f,0.6f,80,90,120}},
  {"OY", PF_VOICED, 150, true,  390,1990,2550, {570,840,2410,1.0f,0.8f,0.6f,80,90,120}},
  {"UH", PF_VOICED, 150, false, 0,0,0, {440,1020,2240,1.0f,0.7f,0.5f,70,80,110}},
  {"UW", PF_VOICED, 150, false, 0,0,0, {300,870,2240,1.0f,0.7f,0.5f,50,80,110}},
  {"B", PF_VOICED, 70,  false, 0,0,0, {200,1000,2500,0.3f,0.2f,0.1f,100,150,200}},
  {"D", PF_VOICED, 70,  false, 0,0,0, {250,1500,2800,0.3f,0.2f,0.1f,100,150,200}},
  {"G", PF_VOICED, 70,  false, 0,0,0, {250,2000,2500,0.3f,0.2f,0.1f,100,150,200}},
  {"K", PF_UNVOICED, 70,  false, 0,0,0, {250,2000,2500,0.3f,0.2f,0.1f,100,150,200}},
  {"P", PF_UNVOICED, 70,  false, 0,0,0, {200,1000,2500,0.3f,0.2f,0.1f,100,150,200}},
  {"T", PF_UNVOICED, 70,  false, 0,0,0, {250,1500,2800,0.3f,0.2f,0.1f,100,150,200}},
  {"DH", PF_VOICED,100, false, 0,0,0, {300,1500,2500,0.1f,0.2f,0.1f,150,200,250}},
  {"F",  PF_UNVOICED, 100, false, 0,0,0, {200,1000,3000,0.1f,0.1f,0.3f,200,250,300}},
  {"HH", PF_UNVOICED, 100, false, 0,0,0, {500,1500,2500,0.1f,0.1f,0.1f,200,250,300}},
  {"S",  (uint16_t)(PF_UNVOICED|PF_STRONG_SIBILANT), 100, false, 0,0,0, {200,3000,7000,0.1f,0.3f,0.5f,200,300,500}},
  {"SH", (uint16_t)(PF_UNVOICED|PF_POSTALVEOLAR), 100, false, 0,0,0, {200,2000,4000,0.1f,0.3f,0.4f,200,250,400}},
  {"TH", PF_UNVOICED, 100, false, 0,0,0, {300,1500,2500,0.1f,0.2f,0.1f,150,200,250}},
  {"V", PF_VOICED, 100, false, 0,0,0, {200,1000,3000,0.2f,0.2f,0.3f,150,200,250}},
  {"Z", PF_VOICED, 100, false, 0,0,0, {200,3000,7000,0.2f,0.3f,0.5f,150,250,400}},
  {"ZH", PF_VOICED,100, false, 0,0,0, {200,2000,4000,0.2f,0.3f,0.4f,150,200,300}},
  {"CH", PF_UNVOICED, 70, false, 0,0,0, {200,2000,3500,0.2f,0.3f,0.4f,120,180,250}},
  {"JH", PF_VOICED, 70, false, 0,0,0, {200,2000,3500,0.2f,0.3f,0.4f,120,180,250}},
  {"L", PF_VOICED, 120, false, 0,0,0, {350,1200,2700,0.7f,0.5f,0.3f,80,120,150}},
  {"R", PF_VOICED, 120, false, 0,0,0, {350,1200,1600,0.7f,0.6f,0.5f,80,120,150}},
  {"W", PF_VOICED,  80, false, 0,0,0, {300,870,2240,0.8f,0.7f,0.5f,70,80,110}},
  {"Y", PF_VOICED,  80, false, 0,0,0, {270,2290,3010,0.8f,0.9f,0.6f,60,90,120}},
  {"M", (uint16_t)(PF_VOICED|PF_NASAL), 120, false, 0,0,0, {250,1000,2200,0.8f,0.3f,0.2f,80,120,150}},
  {"N", (uint16_t)(PF_VOICED|PF_NASAL), 120, false, 0,0,0, {250,1500,2500,0.8f,0.3f,0.2f,80,120,150}},
  {"NG", (uint16_t)(PF_VOICED|PF_NASAL), 120, false, 0,0,0, {250,2000,2500,0.8f,0.3f,0.2f,80,120,150}},

  // ==================== International / IPA extension (Phone ids 43-120) ====================
  //
  // Formant frequencies (F1-F3, male voice) are literature-informed
  // starting points, not measured/calibrated data:
  //  - German vowels: Sendlmeier & Seebold-style average male formant charts.
  //  - French vowels/nasals: Delattre/Fant-style average male formant charts.
  //  - Everything else: approximated from the closest ARPAbet
  //    consonant/vowel by place of articulation (each entry says which,
  //    "cf. X") and adjusted per known acoustic tendencies (retroflexion
  //    lowers F3, backing lowers F2, rounding lowers F2, pharyngealization
  //    raises F1, etc.) -- dedicated formant tables for most of these
  //    segments are sparse or don't exist at all in the literature.
  // Per the project's own listening-test standard, treat every value here
  // as a first draft to be verified against real rendered audio and
  // retuned by ear -- do not assume correctness from the numbers alone.
  //
  // Clicks, implosives and ejectives (the CL-, IM- and EJ-prefixed entries
  // below) are a different case from everything else here: they change
  // the *source* mechanism (a separate velaric airstream for clicks, a
  // glottalic one for ejectives/implosives), not just the filter, so no
  // amount of 3-formant tuning makes them accurate the way it does for an
  // unusual place-of-articulation consonant or vowel. Those entries are
  // best-effort approximations (short broadband bursts for clicks, stops
  // with adjusted burst amplitude/voicing for ejectives/implosives) good
  // enough to be distinguishable from silence and from each other, not
  // intended as faithful synthesis -- a real source model (or a
  // recording, per PhonemeModifiers.h's @note on when recordings are
  // actually needed) would be required for that.

  // -- German/French/Spanish (43-58) --------------------------------------

  // Front rounded vowels: same shape as their unrounded ARPAbet cousins
  // (IY/EY/EH) but with F2 pulled down toward the rounded (back-vowel-like)
  // range -- rounding lowers F2 without moving F1 much.
  {"UF", PF_VOICED, 150, false, 0, 0, 0,
   {270, 1850, 2200, 1.0f, 0.9f, 0.6f, 50, 100, 140}},  // /y/, cf. IY
  {"UF0", PF_VOICED, 110, false, 0, 0, 0,
   {390, 1550, 2100, 1.0f, 0.8f, 0.6f, 60, 90, 120}},  // /ʏ/, cf. IH
  {"OF", PF_VOICED, 150, false, 0, 0, 0,
   {390, 1600, 2100, 1.0f, 0.8f, 0.6f, 70, 90, 120}},  // /ø/, cf. EY
  {"OE", PF_VOICED, 140, false, 0, 0, 0,
   {550, 1650, 2100, 1.0f, 0.8f, 0.6f, 80, 90, 120}},  // /œ/, cf. EH

  // French nasal vowels: oral targets nasalized -- lower/damped F2 relative
  // to the corresponding oral vowel (AA/EH/AO/ER-ish), plus the NASAL flag
  // so the vocoder adds nasal-murmur coupling.
  {"AN", (uint16_t)(PF_VOICED | PF_NASAL), 170, false, 0, 0, 0,
   {800, 1000, 2500, 1.0f, 0.6f, 0.4f, 90, 110, 150}},  // /ɑ̃/, cf. AA
  {"EN", (uint16_t)(PF_VOICED | PF_NASAL), 170, false, 0, 0, 0,
   {500, 1650, 2400, 1.0f, 0.7f, 0.4f, 90, 110, 150}},  // /ɛ̃/, cf. EH
  {"ON", (uint16_t)(PF_VOICED | PF_NASAL), 170, false, 0, 0, 0,
   {450, 800, 2400, 1.0f, 0.6f, 0.4f, 90, 110, 150}},  // /ɔ̃/, cf. AO
  {"UN", (uint16_t)(PF_VOICED | PF_NASAL), 170, false, 0, 0, 0,
   {500, 1350, 2300, 1.0f, 0.6f, 0.4f, 90, 110, 150}},  // /œ̃/, cf. ER

  // German dorsal fricatives: unvoiced, no strong sibilant energy (unlike
  // S/SH) -- broader, weaker turbulence noise. C (ich-Laut) is
  // higher/palatal, X (ach-Laut/jota) is lower/velar-uvular.
  {"C", PF_UNVOICED, 100, false, 0, 0, 0,
   {250, 2700, 3700, 0.1f, 0.3f, 0.3f, 200, 300, 400}},  // /ç/
  {"X", PF_UNVOICED, 110, false, 0, 0, 0,
   {400, 1400, 2500, 0.1f, 0.3f, 0.2f, 250, 300, 350}},  // /x/

  // Affricates: brief stop-like onset then frication, spectrally close to
  // the fricative half (S / F) but shorter and unvoiced throughout.
  {"TS", (uint16_t)(PF_UNVOICED | PF_STRONG_SIBILANT), 90, false, 0, 0, 0,
   {200, 3000, 7000, 0.1f, 0.3f, 0.5f, 150, 250, 400}},  // /ts/, cf. S
  {"PF", PF_UNVOICED, 90, false, 0, 0, 0,
   {200, 1000, 3000, 0.1f, 0.1f, 0.3f, 150, 200, 250}},  // /pf/, cf. F

  // Spanish rhotics: a tap is essentially a very short voiced stop
  // (closest to D); the trill reuses R's approximant formants but held
  // longer to stand in for repeated contacts (no per-contact modeling).
  {"RT", PF_VOICED, 40, false, 0, 0, 0,
   {300, 1300, 1700, 0.3f, 0.2f, 0.1f, 90, 130, 160}},  // /ɾ/, cf. D
  {"RR", PF_VOICED, 150, false, 0, 0, 0,
   {350, 1200, 1600, 0.7f, 0.6f, 0.5f, 80, 120, 150}},  // /r/, cf. R

  // Spanish palatals: N/L with F2 raised toward the palatal region.
  {"NY", (uint16_t)(PF_VOICED | PF_NASAL), 120, false, 0, 0, 0,
   {280, 2300, 2700, 0.8f, 0.3f, 0.2f, 80, 120, 150}},  // /ɲ/, cf. N
  {"LY", PF_VOICED, 110, false, 0, 0, 0,
   {300, 2200, 2700, 0.7f, 0.5f, 0.3f, 80, 120, 150}},  // /ʎ/, cf. L

  // -- Rest of the IPA charts (59-120) -------------------------------------

  // Bilabial fricatives/trill: like F/V but lower place resonance (no
  // labiodental cavity), and a trill held long enough to suggest repeated
  // lip contact.
  {"PHI", PF_UNVOICED, 100, false, 0, 0, 0,
   {200, 800, 2500, 0.1f, 0.1f, 0.2f, 200, 250, 300}},  // /ɸ/, cf. F
  {"BETA", PF_VOICED, 100, false, 0, 0, 0,
   {200, 800, 2500, 0.15f, 0.15f, 0.25f, 150, 200, 250}},  // /β/, cf. V
  {"BR", PF_VOICED, 140, false, 0, 0, 0,
   {200, 900, 2400, 0.3f, 0.2f, 0.1f, 90, 140, 180}},  // /ʙ/, cf. B

  // Labiodental nasal/approximant: cf. M/V.
  {"MV", (uint16_t)(PF_VOICED | PF_NASAL), 100, false, 0, 0, 0,
   {220, 1100, 2300, 0.8f, 0.3f, 0.2f, 80, 120, 150}},  // /ɱ/, cf. M
  {"VV", PF_VOICED, 90, false, 0, 0, 0,
   {250, 1000, 2300, 0.6f, 0.4f, 0.3f, 100, 130, 160}},  // /ʋ/, cf. V/W

  // Alveolar laterals: L's formant shape carrying fricative/flap energy
  // instead of a clean approximant.
  {"LH", PF_UNVOICED, 110, false, 0, 0, 0,
   {350, 1700, 2600, 0.1f, 0.3f, 0.3f, 150, 200, 250}},  // /ɬ/, cf. L
  {"LZ", PF_VOICED, 110, false, 0, 0, 0,
   {350, 1700, 2600, 0.5f, 0.4f, 0.3f, 120, 160, 200}},  // /ɮ/, cf. L
  {"LF", PF_VOICED, 40, false, 0, 0, 0,
   {350, 1300, 2600, 0.5f, 0.4f, 0.2f, 80, 120, 150}},  // /ɺ/, cf. RT

  // Retroflex series: cf. the matching ARPAbet consonant with F3 pulled
  // down (retroflexion's signature acoustic effect).
  {"TR", PF_UNVOICED, 80, false, 0, 0, 0,
   {250, 1300, 2200, 0.3f, 0.2f, 0.1f, 100, 150, 200}},  // /ʈ/, cf. T
  {"DR", PF_VOICED, 70, false, 0, 0, 0,
   {250, 1300, 2200, 0.3f, 0.2f, 0.1f, 100, 150, 200}},  // /ɖ/, cf. D
  {"NR", (uint16_t)(PF_VOICED | PF_NASAL), 120, false, 0, 0, 0,
   {250, 1300, 2000, 0.8f, 0.3f, 0.2f, 80, 120, 150}},  // /ɳ/, cf. N
  {"SR", (uint16_t)(PF_UNVOICED | PF_POSTALVEOLAR), 120, false, 0, 0, 0,
   {250, 1800, 3200, 0.1f, 0.3f, 0.4f, 200, 250, 350}},  // /ʂ/, cf. SH
  {"ZR", (uint16_t)(PF_VOICED | PF_POSTALVEOLAR), 110, false, 0, 0, 0,
   {250, 1800, 3200, 0.2f, 0.3f, 0.4f, 150, 200, 300}},  // /ʐ/, cf. ZH
  {"RA", PF_VOICED, 90, false, 0, 0, 0,
   {350, 1100, 1500, 0.7f, 0.6f, 0.5f, 80, 120, 150}},  // /ɻ/, cf. R
  {"LR", PF_VOICED, 100, false, 0, 0, 0,
   {350, 1200, 2100, 0.7f, 0.5f, 0.3f, 80, 120, 150}},  // /ɭ/, cf. L
  {"RF", PF_VOICED, 40, false, 0, 0, 0,
   {300, 1300, 2000, 0.3f, 0.2f, 0.1f, 90, 130, 160}},  // /ɽ/, cf. D/RT

  // Palatal stops/fricative: cf. K/G/C(ich-Laut) with F2 raised toward the
  // palatal region.
  {"CJ", PF_UNVOICED, 70, false, 0, 0, 0,
   {250, 2400, 3000, 0.3f, 0.2f, 0.1f, 100, 150, 200}},  // /c/, cf. K
  {"JJ", PF_VOICED, 60, false, 0, 0, 0,
   {250, 2400, 3000, 0.3f, 0.2f, 0.1f, 100, 150, 200}},  // /ɟ/, cf. G
  {"JZ", PF_VOICED, 100, false, 0, 0, 0,
   {250, 2700, 3700, 0.2f, 0.35f, 0.35f, 180, 250, 350}},  // /ʝ/, cf. C

  // Velar fricative/approximants: cf. X(ach-Laut)/W/L with the velar
  // place's characteristic F2.
  {"GH", PF_VOICED, 100, false, 0, 0, 0,
   {350, 1600, 2600, 0.15f, 0.3f, 0.2f, 200, 250, 300}},  // /ɣ/, cf. X
  {"WV", PF_VOICED, 90, false, 0, 0, 0,
   {300, 1300, 2400, 0.7f, 0.6f, 0.4f, 70, 90, 120}},  // /ɰ/, cf. W
  {"LL", PF_VOICED, 100, false, 0, 0, 0,
   {350, 900, 2200, 0.7f, 0.4f, 0.2f, 80, 120, 150}},  // /ʟ/, cf. L

  // Uvular series: cf. the velar/nasal/fricative analogue with F2/F3
  // pulled further back than velar.
  {"QQ", PF_UNVOICED, 80, false, 0, 0, 0,
   {250, 1200, 2000, 0.3f, 0.2f, 0.1f, 100, 150, 200}},  // /q/, cf. K
  {"GU", PF_VOICED, 70, false, 0, 0, 0,
   {250, 1200, 2000, 0.3f, 0.2f, 0.1f, 100, 150, 200}},  // /ɢ/, cf. G
  {"NU", (uint16_t)(PF_VOICED | PF_NASAL), 120, false, 0, 0, 0,
   {250, 1200, 1900, 0.8f, 0.3f, 0.2f, 80, 120, 150}},  // /ɴ/, cf. N
  {"CU", PF_UNVOICED, 110, false, 0, 0, 0,
   {380, 1200, 2300, 0.1f, 0.3f, 0.2f, 250, 300, 350}},  // /χ/, cf. X
  {"RU", PF_VOICED, 100, false, 0, 0, 0,
   {380, 1200, 2300, 0.2f, 0.3f, 0.2f, 180, 220, 280}},  // /ʁ/, cf. CU
  {"RT2", PF_VOICED, 150, false, 0, 0, 0,
   {380, 1100, 2200, 0.6f, 0.5f, 0.4f, 120, 160, 200}},  // /ʀ/, cf. RU/RR

  // Pharyngeal/glottal: pharyngeals raise F1 and lower F2 (the
  // "darkening" effect); GS (glottal stop) is a brief near-total energy
  // drop, distinct from SIL/SP (which mark word/phrase pauses, not an
  // in-word segment) so it gets its own low-amplitude entry rather than
  // reusing PF_SILENCE.
  {"HP", PF_UNVOICED, 110, false, 0, 0, 0,
   {500, 1000, 2400, 0.15f, 0.2f, 0.15f, 250, 300, 350}},  // /ħ/, cf. HH
  {"AP", PF_VOICED, 100, false, 0, 0, 0,
   {600, 1100, 2400, 0.4f, 0.3f, 0.2f, 150, 200, 250}},  // /ʕ/, cf. AP~AA
  {"GS", PF_UNVOICED, 50, false, 0, 0, 0,
   {0, 0, 0, 0.05f, 0.05f, 0.05f, 50, 50, 50}},  // /ʔ/ -- brief closure
  {"HV", PF_VOICED, 90, false, 0, 0, 0,
   {500, 1500, 2500, 0.15f, 0.15f, 0.15f, 200, 250, 300}},  // /ɦ/, cf. HH

  // Co-articulated approximants: cf. W/Y-UF blend.
  {"WH", PF_UNVOICED, 80, false, 0, 0, 0,
   {300, 870, 2240, 0.15f, 0.15f, 0.1f, 150, 180, 220}},  // /ʍ/, cf. W
  {"HU", PF_VOICED, 80, false, 0, 0, 0,
   {270, 1850, 2200, 0.8f, 0.8f, 0.5f, 60, 90, 120}},  // /ɥ/, cf. UF/Y

  // Clicks: best-effort broadband bursts only -- see the @note above. Not
  // a real velaric-airstream model.
  {"CLB", PF_UNVOICED, 50, false, 0, 0, 0,
   {400, 1500, 3000, 0.3f, 0.3f, 0.3f, 400, 400, 400}},  // /ʘ/
  {"CLD", PF_UNVOICED, 40, false, 0, 0, 0,
   {500, 2500, 4000, 0.2f, 0.3f, 0.3f, 400, 400, 400}},  // /ǀ/
  {"CLA", PF_UNVOICED, 45, false, 0, 0, 0,
   {450, 2000, 3500, 0.3f, 0.35f, 0.3f, 400, 400, 400}},  // /ǃ/
  {"CLP", PF_UNVOICED, 45, false, 0, 0, 0,
   {450, 2200, 3800, 0.25f, 0.3f, 0.3f, 400, 400, 400}},  // /ǂ/
  {"CLL", PF_UNVOICED, 50, false, 0, 0, 0,
   {400, 1800, 3200, 0.25f, 0.3f, 0.3f, 400, 400, 400}},  // /ǁ/

  // Implosives: voiced stops with a weaker, lower-amplitude burst (real
  // implosives have a distinctive lowered-F0/breathy release this doesn't
  // capture) -- see the @note above.
  {"IMB", PF_VOICED, 80, false, 0, 0, 0,
   {180, 900, 2400, 0.25f, 0.15f, 0.08f, 90, 140, 180}},  // /ɓ/, cf. B
  {"IMD", PF_VOICED, 80, false, 0, 0, 0,
   {230, 1400, 2700, 0.25f, 0.15f, 0.08f, 90, 140, 180}},  // /ɗ/, cf. D
  {"IMJ", PF_VOICED, 80, false, 0, 0, 0,
   {230, 2300, 2900, 0.25f, 0.15f, 0.08f, 90, 140, 180}},  // /ʄ/, cf. JJ
  {"IMG", PF_VOICED, 80, false, 0, 0, 0,
   {230, 1900, 2400, 0.25f, 0.15f, 0.08f, 90, 140, 180}},  // /ɠ/, cf. G
  {"IMQ", PF_VOICED, 80, false, 0, 0, 0,
   {230, 1200, 1900, 0.25f, 0.15f, 0.08f, 90, 140, 180}},  // /ʛ/, cf. GU

  // Ejectives: unvoiced stops/fricatives with a sharper, louder burst
  // (real ejectives have a distinctive glottalic release this doesn't
  // capture) -- see the @note above.
  {"EJP", PF_UNVOICED, 80, false, 0, 0, 0,
   {200, 1000, 2500, 0.4f, 0.3f, 0.15f, 90, 140, 180}},  // /pʼ/, cf. P
  {"EJT", PF_UNVOICED, 80, false, 0, 0, 0,
   {250, 1500, 2800, 0.4f, 0.3f, 0.15f, 90, 140, 180}},  // /tʼ/, cf. T
  {"EJK", PF_UNVOICED, 80, false, 0, 0, 0,
   {250, 2000, 2500, 0.4f, 0.3f, 0.15f, 90, 140, 180}},  // /kʼ/, cf. K
  {"EJS", (uint16_t)(PF_UNVOICED | PF_STRONG_SIBILANT), 100, false, 0, 0, 0,
   {200, 3000, 7000, 0.15f, 0.35f, 0.55f, 180, 280, 450}},  // /sʼ/, cf. S
  {"EJC", PF_UNVOICED, 90, false, 0, 0, 0,
   {200, 2000, 3500, 0.25f, 0.35f, 0.45f, 110, 170, 240}},  // /tʃʼ/, cf. CH

  // Vowels: central vowels sit between the front/back ARPAbet targets
  // they're named relative to; EP/OP are the pure monophthong
  // counterparts of English's diphthongal EY/OW; AF is front /a/ vs.
  // ARPAbet AA's back /ɑ/.
  {"IB", PF_VOICED, 130, false, 0, 0, 0,
   {300, 1500, 2400, 1.0f, 0.85f, 0.6f, 55, 95, 130}},  // /ɨ/, cf. IY/UW
  {"UB", PF_VOICED, 130, false, 0, 0, 0,
   {300, 1300, 2000, 1.0f, 0.75f, 0.55f, 55, 90, 120}},  // /ʉ/, cf. UW
  {"UM", PF_VOICED, 130, false, 0, 0, 0,
   {300, 1200, 2300, 1.0f, 0.75f, 0.55f, 55, 85, 115}},  // /ɯ/, cf. UW
  {"EP", PF_VOICED, 130, false, 0, 0, 0,
   {390, 2000, 2550, 1.0f, 0.9f, 0.6f, 65, 90, 120}},  // /e/, cf. EY (no glide)
  {"OP", PF_VOICED, 130, false, 0, 0, 0,
   {400, 900, 2300, 1.0f, 0.8f, 0.55f, 75, 90, 120}},  // /o/, cf. OW (no glide)
  {"EB", PF_VOICED, 120, false, 0, 0, 0,
   {420, 1400, 2400, 0.95f, 0.65f, 0.5f, 65, 85, 115}},  // /ɘ/, cf. AH0
  {"OB", PF_VOICED, 120, false, 0, 0, 0,
   {420, 1200, 2100, 0.95f, 0.6f, 0.5f, 65, 85, 115}},  // /ɵ/, cf. EB
  {"OM", PF_VOICED, 130, false, 0, 0, 0,
   {400, 1300, 2400, 1.0f, 0.75f, 0.55f, 70, 90, 120}},  // /ɤ/, cf. OP
  {"EC", PF_VOICED, 130, false, 0, 0, 0,
   {500, 1400, 2200, 1.0f, 0.75f, 0.6f, 75, 90, 110}},  // /ɜ/, cf. ER (no rhotic F3 dip)
  {"AC", PF_VOICED, 110, false, 0, 0, 0,
   {620, 1400, 2450, 1.0f, 0.85f, 0.6f, 75, 90, 115}},  // /ɐ/, cf. AH/AE
  {"AF", PF_VOICED, 140, false, 0, 0, 0,
   {750, 1650, 2500, 1.0f, 0.9f, 0.6f, 85, 90, 120}},  // /a/, cf. AA (front, not back)
  {"OER", PF_VOICED, 140, false, 0, 0, 0,
   {720, 1400, 2350, 1.0f, 0.85f, 0.6f, 85, 90, 120}},  // /ɶ/, cf. AF (rounded)
  {"OB2", PF_VOICED, 140, false, 0, 0, 0,
   {700, 950, 2400, 1.0f, 0.8f, 0.6f, 85, 90, 120}},  // /ɒ/, cf. AA
};

/// Rule count -- 43 ARPAbet phoneme rules (matching Phonemes::phoneme_map
/// ids 0-42) plus 78 international/IPA extension rules (ids 43-120).
static constexpr size_t kRuleCount = sizeof(kRules)/sizeof(kRules[0]);
