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
};

/// Rule count (currently 43 ARPAbet phoneme rules matching Phonemes::phoneme_map)
static constexpr size_t kRuleCount = sizeof(kRules)/sizeof(kRules[0]);
