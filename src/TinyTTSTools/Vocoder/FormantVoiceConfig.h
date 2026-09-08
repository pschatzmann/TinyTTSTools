/**
 * @file FormantVoiceConfig.h
 * @brief Voice configuration structures and predefined voice types for
 * FormantVocoder
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-08-20
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstddef>
#include "stdint.h"

/**
 * @brief Voice configuration parameters for tuning timbre & prosody
 * @details This structure contains all parameters needed to configure the
 * FormantVocoder for different voice characteristics. Parameters are grouped by
 * functionality:
 * - Basic voice characteristics (pitch, naturalness)
 * - Processing settings (overlap, gains)
 * - Dynamic range control (RMS, peak limiting)
 * - Naturalness features (randomization, stress)
 * - Spectral shaping (nasal, sibilant processing)
 */
struct FormantVoiceConfig {
  // === BASIC VOICE CHARACTERISTICS ===

  /// @brief Base fundamental frequency in Hz (determines perceived pitch)
  /// @details Typical ranges: Male 85-180Hz, Female 165-265Hz, Child 250-400Hz
  float baseF0 = 120.0f;

  /// @brief Pitch jitter amplitude as fraction of baseF0 (0.0-0.05)
  /// @details Adds natural pitch variation. Higher values = more roughness
  float jitterPct = 0.015f;

  /// @brief Amplitude shimmer as fraction (0.0-0.05)
  /// @details Adds natural amplitude variation. Higher values = more
  /// breathiness
  float shimmerPct = 0.02f;

  /// @brief Aspiration noise level (0.0-0.2)
  /// @details Amount of breath noise added to voiced sounds. Higher = more
  /// breathy
  float breathiness = 0.06f;

  /// @brief Spectral tilt filter coefficient (0.0-1.0)
  /// @details Controls brightness/darkness. Lower = brighter, Higher = darker
  float spectralTilt = 0.85f;

  /// @brief Pitch declination rate in Hz per second
  /// @details Natural downward pitch drift during phonemes. 0 = no declination
  float pitchFallPerSec = 3.0f;

  /// @brief Voice's own default speaking-rate multiplier (1.0 = normal)
  /// @details Composes with (multiplies) PhonemeSynthesisParams::speed at
  /// call time rather than replacing it -- a caller's own speed request
  /// still applies on top of this voice's natural rate. Same idea as the
  /// "speed" parameter in SoftVoice's SAM engine (github.com/pschatzmann/
  /// arduino-SAM), which bakes a default rate into each named voice.
  float speedScale = 1.0f;

  // === VOICE TIMBRE (SAM-style mouth/throat scaling) ===
  // Same idea as SAM's SetMouthThroat(): a single scale factor applied
  // uniformly across every phoneme's F1 ("mouth") or F2 ("throat"), on
  // top of FormantRules.h's own per-phoneme absolute values -- not a
  // replacement for that table, just a cheap, uniform "vocal tract size"
  // knob layered on top of it. 1.0 = neutral (FormantRules.h's own values,
  // unscaled).

  /// @brief Uniform F1 (mouth cavity) scale factor across every phoneme
  /// @details >1.0 = larger/deeper mouth resonance (F1 raised), <1.0 = smaller
  float mouthScale = 1.0f;

  /// @brief Uniform F2 (throat cavity) scale factor across every phoneme
  /// @details >1.0 = larger/deeper throat resonance (F2 raised), <1.0 = smaller
  float throatScale = 1.0f;

  // === FORMANT PROCESSING ===

  /// @brief Enable optional 4th formant for enhanced clarity
  /// @details Useful for female/child voices. Adds computational cost
  bool enableF4 = false;

  /// @brief Crossfade overlap duration in milliseconds
  /// @details Smooth transitions between phonemes. Range: 2-10ms
  float overlapMs = 5.0f;

  // === FRICATIVE AND SIBILANT CONTROL ===

  /// @brief Gain multiplier for strong sibilants (S, Z sounds)
  /// @details Lower values reduce harshness. Range: 0.3-0.8
  float sibilantGain = 0.55f;

  /// @brief Gain multiplier for postalveolar fricatives (SH, ZH sounds)
  /// @details Controls strength of "sh" type sounds. Range: 0.5-1.0
  float postalveolarGain = 0.70f;

  /// @brief Peak amplitude limit for fricatives (PCM units)
  /// @details Prevents clipping of harsh consonants. Range: 8000-15000
  float fricativeMaxPeak = 10000.0f;

  // === DYNAMIC RANGE CONTROL ===

  /// @brief Target RMS level for voiced sounds (PCM units)
  /// @details Controls overall loudness. Range: 3000-8000
  float targetRMS = 5000.0f;

  /// @brief Target RMS level for unvoiced fricatives (PCM units)
  /// @details Usually lower than voiced sounds. Range: 2500-6000
  float fricativeTargetRMS = 4000.0f;

  /// @brief Adaptive gain adjustment rate (0.0-1.0)
  /// @details Speed of automatic level adjustment. Higher = faster response
  float rmsAdaptRate = 0.25f;

  // === DE-ESSING (HARSH SIBILANT REDUCTION) ===

  /// @brief High-frequency energy threshold for de-essing (0.0-1.0)
  /// @details Lower values = more aggressive de-essing
  float deEsserThreshold = 0.18f;

  /// @brief Maximum attenuation factor for de-essing (0.0-1.0)
  /// @details Amount of harsh frequency reduction. Higher = more reduction
  float deEsserAtten = 0.35f;

  // === NASAL SOUND PROCESSING ===

  /// @brief Low-pass filter blend for nasal sounds (0.0-1.0)
  /// @details Higher values = more muffled nasal quality
  float nasalLowpass = 0.25f;

  // === NATURALNESS AND VARIATION ===

  /// @brief Enable random formant variations for naturalness
  /// @details Adds slight per-phoneme variations to prevent robotic sound
  bool enableFormantRandom = true;

  /// @brief Formant frequency variation range in cents (musical pitch units)
  /// @details ±variation applied to F1-F3. Range: 5-25 cents
  float formantRandomCents = 15.0f;

  /// @brief Formant bandwidth variation as percentage (0.0-0.2)
  /// @details Adds variation to formant sharpness/width
  float bandwidthRandomPct = 0.08f;

  // === STRESS AND PROSODY (ARPAbet style digits 1,2) ===

  /// @brief Enable pitch rises for stressed syllables
  /// @details Implements natural stress patterns in speech
  bool enableStressPitch = true;

  /// @brief Pitch rise for primary stress (1) in Hz
  /// @details How much pitch increases for stressed syllables
  float stressPitchRisePrimary = 18.0f;

  /// @brief Pitch rise for secondary stress (2) in Hz
  /// @details Smaller pitch increase for secondary stress
  float stressPitchRiseSecondary = 10.0f;

  /// @brief Amplitude boost for primary stress (multiplier)
  /// @details Energy increase for stressed syllables. Range: 1.0-1.5
  float stressEnergyBoostPrimary = 1.18f;

  /// @brief Amplitude boost for secondary stress (multiplier)
  /// @details Smaller energy increase for secondary stress
  float stressEnergyBoostSecondary = 1.10f;

  /// @brief Fraction of phoneme over which stress effects decay (0.0-1.0)
  /// @details Controls how quickly stress effects fade during the sound
  float stressDecayPortion = 0.55f;

  // === NASAL ANTI-FORMANTS (SPECTRAL NOTCHES) ===

  /// @brief Enable nasal anti-formant filtering
  /// @details Creates characteristic nasal sound by adding spectral notches
  bool enableNasalNotch = true;

  /// @brief First nasal notch frequency in Hz
  /// @details Primary anti-formant frequency. Typical: 800-1200Hz
  float nasalNotchFreq1 = 1000.0f;

  /// @brief First nasal notch bandwidth in Hz
  /// @details Width of the spectral notch. Range: 80-200Hz
  float nasalNotchBw1 = 120.0f;

  /// @brief Second nasal notch frequency in Hz
  /// @details Secondary anti-formant. Typical: 2000-3000Hz
  float nasalNotchFreq2 = 2500.0f;

  /// @brief Second nasal notch bandwidth in Hz
  /// @details Width of second notch. Range: 200-400Hz
  float nasalNotchBw2 = 300.0f;

  /// @brief Notch filter depth (0.0-1.0)
  /// @details Strength of anti-formant effect. 1.0 = maximum nasal quality
  float nasalNotchDepth = 0.65f;

  // === ADVANCED SIBILANT SHAPING ===

  /// @brief Scale factor for high formant (F3) in strong sibilants
  /// @details Reduces harsh high frequencies. Range: 0.3-0.8
  float sibilantHFFormantScale = 0.55f;

  /// @brief Target RMS for sibilant dynamic compression (PCM units)
  /// @details Level above which compression is applied to sibilants
  float sibilantDynamicTargetRMS = 3200.0f;

  /// @brief Compression ratio for sibilant dynamics (1.0-5.0)
  /// @details Higher values = more compression. 1.0 = no compression
  float sibilantCompressionRatio = 3.0f;

  /// @brief Attack time constant for sibilant compression (0.0-1.0)
  /// @details Speed of compression onset. Lower = faster attack
  float sibilantAttack = 0.4f;

  /// @brief Release time constant for sibilant compression (0.0-1.0)
  /// @details Speed of compression release. Higher = slower release
  float sibilantRelease = 0.9f;

  /// @brief Final gain applied to processed sibilants
  /// @details Overall level adjustment after processing. Range: 0.3-1.0
  float sibilantFinalGain = 0.50f;

  /// @brief Low-pass filter blend for sibilant softening (0.0-1.0)
  /// @details Mixes in softened version to reduce harshness
  float sibilantLowpassMix = 0.35f;

  // === CONSTRUCTORS ===

  /// @brief Default constructor using all default values
  FormantVoiceConfig() = default;

  /// @brief Constructor with all parameters for creating custom voice
  /// configurations
  FormantVoiceConfig(
      float baseF0, float jitterPct, float shimmerPct, float breathiness,
      float spectralTilt, float pitchFallPerSec, bool enableF4, float overlapMs,
      float sibilantGain, float postalveolarGain, float fricativeMaxPeak,
      float targetRMS, float fricativeTargetRMS, float rmsAdaptRate,
      float deEsserThreshold, float deEsserAtten, float nasalLowpass,
      bool enableFormantRandom, float formantRandomCents,
      float bandwidthRandomPct, bool enableStressPitch,
      float stressPitchRisePrimary, float stressPitchRiseSecondary,
      float stressEnergyBoostPrimary, float stressEnergyBoostSecondary,
      float stressDecayPortion, bool enableNasalNotch, float nasalNotchFreq1,
      float nasalNotchBw1, float nasalNotchFreq2, float nasalNotchBw2,
      float nasalNotchDepth, float sibilantHFFormantScale,
      float sibilantDynamicTargetRMS, float sibilantCompressionRatio,
      float sibilantAttack, float sibilantRelease, float sibilantFinalGain,
      float sibilantLowpassMix)
      : baseF0(baseF0),
        jitterPct(jitterPct),
        shimmerPct(shimmerPct),
        breathiness(breathiness),
        spectralTilt(spectralTilt),
        pitchFallPerSec(pitchFallPerSec),
        enableF4(enableF4),
        overlapMs(overlapMs),
        sibilantGain(sibilantGain),
        postalveolarGain(postalveolarGain),
        fricativeMaxPeak(fricativeMaxPeak),
        targetRMS(targetRMS),
        fricativeTargetRMS(fricativeTargetRMS),
        rmsAdaptRate(rmsAdaptRate),
        deEsserThreshold(deEsserThreshold),
        deEsserAtten(deEsserAtten),
        nasalLowpass(nasalLowpass),
        enableFormantRandom(enableFormantRandom),
        formantRandomCents(formantRandomCents),
        bandwidthRandomPct(bandwidthRandomPct),
        enableStressPitch(enableStressPitch),
        stressPitchRisePrimary(stressPitchRisePrimary),
        stressPitchRiseSecondary(stressPitchRiseSecondary),
        stressEnergyBoostPrimary(stressEnergyBoostPrimary),
        stressEnergyBoostSecondary(stressEnergyBoostSecondary),
        stressDecayPortion(stressDecayPortion),
        enableNasalNotch(enableNasalNotch),
        nasalNotchFreq1(nasalNotchFreq1),
        nasalNotchBw1(nasalNotchBw1),
        nasalNotchFreq2(nasalNotchFreq2),
        nasalNotchBw2(nasalNotchBw2),
        nasalNotchDepth(nasalNotchDepth),
        sibilantHFFormantScale(sibilantHFFormantScale),
        sibilantDynamicTargetRMS(sibilantDynamicTargetRMS),
        sibilantCompressionRatio(sibilantCompressionRatio),
        sibilantAttack(sibilantAttack),
        sibilantRelease(sibilantRelease),
        sibilantFinalGain(sibilantFinalGain),
        sibilantLowpassMix(sibilantLowpassMix) {}
};

// ============================================================================
// PREDEFINED VOICE CONFIGURATIONS
// ============================================================================

/**
 * @brief Predefined voice configurations for common voice types
 * @details This namespace contains carefully tuned voice configurations
 * that can be used directly or as starting points for custom voices.
 * Each configuration represents a different vocal characteristic optimized
 * for natural-sounding speech synthesis.
 */
namespace FormantVoice {

/**
 * @brief Default voice configuration using structure defaults
 * @details Reference configuration using all default values from
 * FormantVoiceConfig. This serves as:
 * - A baseline reference for all voice parameters
 * - Starting point for creating custom voices
 * - Fallback configuration when no specific voice is selected
 *
 * Characteristics match AdultMale but explicitly uses struct defaults.
 */
static const FormantVoiceConfig DefaultVoice{};

/**
 * @brief Standard adult male voice configuration
 * @details Well-balanced male voice suitable for general purpose TTS.
 * Characteristics:
 * - Fundamental frequency: 120 Hz (typical male range)
 * - Moderate naturalness features (jitter, shimmer)
 * - Balanced spectral characteristics
 * - Good intelligibility with natural prosody
 */
static const FormantVoiceConfig AdultMale{
    120.0f,    // baseF0 - Typical male fundamental frequency
    0.015f,    // jitterPct - Natural pitch variation
    0.02f,     // shimmerPct - Natural amplitude variation
    0.06f,     // breathiness - Subtle breath noise
    0.85f,     // spectralTilt - Slightly warm tone
    3.0f,      // pitchFallPerSec - Natural declination
    false,     // enableF4 - Not needed for male voice
    5.0f,      // overlapMs - Smooth transitions
    0.55f,     // sibilantGain - Controlled sibilant strength
    0.70f,     // postalveolarGain
    10000.0f,  // fricativeMaxPeak
    5000.0f,   // targetRMS - Good audibility
    4000.0f,   // fricativeTargetRMS
    0.25f,     // rmsAdaptRate
    0.18f,     // deEsserThreshold
    0.35f,     // deEsserAtten
    0.25f,     // nasalLowpass - Natural nasal quality
    true,      // enableFormantRandom
    15.0f,     // formantRandomCents
    0.08f,     // bandwidthRandomPct
    true,      // enableStressPitch
    18.0f,     // stressPitchRisePrimary
    10.0f,     // stressPitchRiseSecondary
    1.18f,     // stressEnergyBoostPrimary
    1.10f,     // stressEnergyBoostSecondary
    0.55f,     // stressDecayPortion
    true,      // enableNasalNotch
    1000.0f,   // nasalNotchFreq1
    120.0f,    // nasalNotchBw1
    2500.0f,   // nasalNotchFreq2
    300.0f,    // nasalNotchBw2
    0.65f,     // nasalNotchDepth
    0.55f,     // sibilantHFFormantScale
    3200.0f,   // sibilantDynamicTargetRMS
    3.0f,      // sibilantCompressionRatio
    0.4f,      // sibilantAttack
    0.9f,      // sibilantRelease
    0.50f,     // sibilantFinalGain
    0.35f      // sibilantLowpassMix
}; /**
    * @brief Standard adult female voice configuration
    * @details Natural-sounding female voice with enhanced clarity.
    * Characteristics:
    * - Fundamental frequency: 200 Hz (typical female range)
    * - Enhanced breathiness and brightness
    * - 4th formant enabled for clarity
    * - Optimized sibilant processing for female acoustics
    * - Higher stress pitch variations
    */
static const FormantVoiceConfig AdultFemale{
    200.0f,    // baseF0 - Female fundamental frequency
    0.012f,    // jitterPct - Slightly less variation
    0.015f,    // shimmerPct - Controlled amplitude variation
    0.03f,     // breathiness - Less breath noise
    0.75f,     // spectralTilt - Brighter tone
    2.5f,      // pitchFallPerSec - Gentle declination
    true,      // enableF4 - Important for female voice
    4.0f,      // overlapMs - Smooth female transitions
    0.65f,     // sibilantGain - Enhanced clarity
    0.75f,     // postalveolarGain
    11000.0f,  // fricativeMaxPeak - Higher for female
    4500.0f,   // targetRMS
    3800.0f,   // fricativeTargetRMS
    0.3f,      // rmsAdaptRate - Faster adaptation
    0.15f,     // deEsserThreshold - More sensitive
    0.4f,      // deEsserAtten
    0.3f,      // nasalLowpass
    true,      // enableFormantRandom
    12.0f,     // formantRandomCents - Less variation
    0.06f,     // bandwidthRandomPct
    true,      // enableStressPitch
    22.0f,     // stressPitchRisePrimary - More expressive
    12.0f,     // stressPitchRiseSecondary
    1.2f,      // stressEnergyBoostPrimary
    1.12f,     // stressEnergyBoostSecondary
    0.6f,      // stressDecayPortion
    true,      // enableNasalNotch
    1100.0f,   // nasalNotchFreq1 - Higher for female
    110.0f,    // nasalNotchBw1
    2700.0f,   // nasalNotchFreq2
    280.0f,    // nasalNotchBw2
    0.6f,      // nasalNotchDepth
    0.65f,     // sibilantHFFormantScale
    3500.0f,   // sibilantDynamicTargetRMS
    2.8f,      // sibilantCompressionRatio
    0.35f,     // sibilantAttack
    0.85f,     // sibilantRelease
    0.55f,     // sibilantFinalGain
    0.3f       // sibilantLowpassMix
};

/**
 * @brief Deep male voice configuration (bass/baritone)
 * @details Authoritative deep voice with rich low-frequency content.
 * Characteristics:
 * - Very low fundamental frequency: 85 Hz
 * - Enhanced naturalness features for character
 * - Darker spectral balance
 * - Powerful low-frequency emphasis
 * - Reduced high-frequency content
 */
static const FormantVoiceConfig DeepMale{
    85.0f,    // baseF0 - Very low for deep voice
    0.02f,    // jitterPct - More variation for character
    0.025f,   // shimmerPct - Enhanced roughness
    0.1f,     // breathiness - More breath for depth
    0.95f,    // spectralTilt - Very warm/dark
    4.0f,     // pitchFallPerSec - Strong declination
    false,    // enableF4 - Not needed for deep male
    6.0f,     // overlapMs - Slower transitions
    0.45f,    // sibilantGain - Reduced for warmth
    0.6f,     // postalveolarGain
    9000.0f,  // fricativeMaxPeak - Lower for warmth
    5500.0f,  // targetRMS - Higher for presence
    4500.0f,  // fricativeTargetRMS
    0.2f,     // rmsAdaptRate - Slower adaptation
    0.3f,     // deEsserThreshold - Less sensitive
    0.25f,    // deEsserAtten
    0.35f,    // nasalLowpass - More nasal warmth
    true,     // enableFormantRandom
    20.0f,    // formantRandomCents - More variation
    0.1f,     // bandwidthRandomPct
    true,     // enableStressPitch
    15.0f,    // stressPitchRisePrimary - Less rise for deep voice
    8.0f,     // stressPitchRiseSecondary
    1.25f,    // stressEnergyBoostPrimary - More energy
    1.15f,    // stressEnergyBoostSecondary
    0.5f,     // stressDecayPortion
    true,     // enableNasalNotch
    900.0f,   // nasalNotchFreq1 - Lower for deep voice
    140.0f,   // nasalNotchBw1
    2200.0f,  // nasalNotchFreq2
    350.0f,   // nasalNotchBw2
    0.7f,     // nasalNotchDepth
    0.45f,    // sibilantHFFormantScale - Softer highs
    2800.0f,  // sibilantDynamicTargetRMS
    3.5f,     // sibilantCompressionRatio
    0.5f,     // sibilantAttack
    1.0f,     // sibilantRelease
    0.45f,    // sibilantFinalGain
    0.45f     // sibilantLowpassMix - More softening
};

/**
 * @brief Child/young voice configuration
 * @details High-pitched voice simulating child or young speaker.
 * Characteristics:
 * - Very high fundamental frequency: 280 Hz
 * - Clean, bright sound with minimal roughness
 * - Enhanced breathiness for youthful quality
 * - Fast pitch changes and high stress variations
 * - Minimal nasal processing for clarity
 */
static const FormantVoiceConfig Child{
    280.0f,    // baseF0 - Very high for child voice
    0.008f,    // jitterPct - Minimal variation for clarity
    0.01f,     // shimmerPct - Clean amplitude
    0.05f,     // breathiness - Some breath for youth
    0.65f,     // spectralTilt - Bright, clear tone
    1.5f,      // pitchFallPerSec - Less declination
    true,      // enableF4 - Important for high voice
    3.0f,      // overlapMs - Fast transitions
    0.7f,      // sibilantGain - Clear sibilants
    0.8f,      // postalveolarGain
    12000.0f,  // fricativeMaxPeak - Very high for clarity
    4000.0f,   // targetRMS
    3500.0f,   // fricativeTargetRMS
    0.4f,      // rmsAdaptRate - Fast adaptation
    0.12f,     // deEsserThreshold - Very sensitive
    0.45f,     // deEsserAtten
    0.2f,      // nasalLowpass - Minimal nasal processing
    true,      // enableFormantRandom
    8.0f,      // formantRandomCents - Minimal variation
    0.04f,     // bandwidthRandomPct
    true,      // enableStressPitch
    28.0f,     // stressPitchRisePrimary - Very expressive
    15.0f,     // stressPitchRiseSecondary
    1.25f,     // stressEnergyBoostPrimary
    1.18f,     // stressEnergyBoostSecondary
    0.7f,      // stressDecayPortion
    false,     // enableNasalNotch - Disabled for clarity
    1200.0f,   // nasalNotchFreq1
    100.0f,    // nasalNotchBw1
    2800.0f,   // nasalNotchFreq2
    250.0f,    // nasalNotchBw2
    0.4f,      // nasalNotchDepth
    0.75f,     // sibilantHFFormantScale - Preserve highs
    3800.0f,   // sibilantDynamicTargetRMS
    2.2f,      // sibilantCompressionRatio
    0.25f,     // sibilantAttack
    0.7f,      // sibilantRelease
    0.65f,     // sibilantFinalGain
    0.2f       // sibilantLowpassMix - Minimal softening
};

/**
 * @brief Robotic/synthetic voice configuration
 * @details Mechanical, artificial voice with no naturalness features.
 * Characteristics:
 * - No jitter, shimmer, or breathiness (perfectly mechanical)
 * - No pitch contours or stress patterns
 * - Higher overall levels for clarity
 * - Minimal processing for clean, digital sound
 * - Suitable for robot characters or synthetic announcements
 */
static const FormantVoiceConfig Robotic{
    150.0f,    // baseF0 - Fixed monotone pitch
    0.0f,      // jitterPct - No variation (mechanical)
    0.0f,      // shimmerPct - No variation (mechanical)
    0.0f,      // breathiness - No breath (digital)
    0.5f,      // spectralTilt - Neutral spectral balance
    0.0f,      // pitchFallPerSec - No declination
    false,     // enableF4 - Not needed for robotic
    2.0f,      // overlapMs - Minimal overlap for clarity
    0.8f,      // sibilantGain - Strong sibilants for clarity
    0.85f,     // postalveolarGain
    15000.0f,  // fricativeMaxPeak - Very high for digital clarity
    6000.0f,   // targetRMS - High level for presence
    5000.0f,   // fricativeTargetRMS
    0.5f,      // rmsAdaptRate - Fast adaptation
    0.2f,      // deEsserThreshold - Less sensitive
    0.3f,      // deEsserAtten
    0.1f,      // nasalLowpass - Minimal nasal processing
    false,     // enableFormantRandom - No variation
    0.0f,      // formantRandomCents - No variation
    0.0f,      // bandwidthRandomPct - No variation
    false,     // enableStressPitch - No stress patterns
    0.0f,      // stressPitchRisePrimary - No stress
    0.0f,      // stressPitchRiseSecondary - No stress
    1.0f,      // stressEnergyBoostPrimary - No boost
    1.0f,      // stressEnergyBoostSecondary - No boost
    0.0f,      // stressDecayPortion - No decay
    false,     // enableNasalNotch - No nasal processing
    1000.0f,   // nasalNotchFreq1
    100.0f,    // nasalNotchBw1
    2500.0f,   // nasalNotchFreq2
    300.0f,    // nasalNotchBw2
    0.0f,      // nasalNotchDepth - No notch
    1.0f,      // sibilantHFFormantScale - Full highs
    4000.0f,   // sibilantDynamicTargetRMS
    1.0f,      // sibilantCompressionRatio - No compression
    0.1f,      // sibilantAttack
    0.1f,      // sibilantRelease
    0.8f,      // sibilantFinalGain
    0.0f       // sibilantLowpassMix - No softening
};

/**
 * @brief Layer SAM-style mouth/throat/speed/pitch onto an existing base voice
 * @details Starts from `base` (inheriting all its already-tuned naturalness/
 * dynamics dials) and overrides just the four character-defining knobs --
 * mirrors how github.com/pschatzmann/arduino-SAM defines its named voices
 * as (speed, pitch, throat, mouth) tuples layered on its own default voice.
 */
static inline FormantVoiceConfig withVoiceCharacter(FormantVoiceConfig base,
                                                    float baseF0,
                                                    float mouthScale,
                                                    float throatScale,
                                                    float speedScale) {
  base.baseF0 = baseF0;
  base.mouthScale = mouthScale;
  base.throatScale = throatScale;
  base.speedScale = speedScale;
  return base;
}

// ============================================================================
// SAM-INSPIRED CHARACTER VOICES
// ============================================================================
// mouthScale/throatScale/speedScale below are exact ratios of SAM's own
// (mouth, throat, speed) values for each named voice against its own
// neutral "Sam" voice (mouth=128, throat=128, speed=72) -- see
// arduino-SAM's SetMouthThroat()/trans() (mouth scales F1, throat scales
// F2, both normalized around 128) and its README's voice table. baseF0
// (Hz) and any extra per-character dial tweaks below are this library's
// own choice to match each name's character, NOT a translation of SAM's
// own "pitch" parameter -- that parameter is an inverse, non-linear unit
// internal to SAM's own C64-derived engine, not a Hz value, and reverse-
// engineering its exact mapping wasn't attempted.

/// @brief Bright, small-sounding voice (SAM: mouth=160, throat=110, speed=72)
static const FormantVoiceConfig Elf =
    withVoiceCharacter(AdultFemale, 220.0f, 160.0f / 128.0f, 110.0f / 128.0f, 72.0f / 72.0f);

/// @brief Fast, mechanical voice (SAM: mouth=190, throat=190, speed=92)
/// @details Built on `Robotic` (already monotone/mechanical) rather than
/// `AdultMale` -- SAM's own "Little Robot" name implies the same
/// no-naturalness character this library's `Robotic` preset already has.
static const FormantVoiceConfig LittleRobot =
    withVoiceCharacter(Robotic, 140.0f, 190.0f / 128.0f, 190.0f / 128.0f, 92.0f / 72.0f);

/// @brief Deep, congested-sounding voice (SAM: mouth=105, throat=110, speed=82)
static const FormantVoiceConfig StuffyGuy = [] {
  FormantVoiceConfig c = withVoiceCharacter(AdultMale, 110.0f, 105.0f / 128.0f,
                                            110.0f / 128.0f, 82.0f / 72.0f);
  // "Stuffy" (congested/nasal) character: exaggerate the existing nasal
  // processing rather than add a new mechanism.
  c.nasalLowpass = 0.5f;
  c.nasalNotchDepth = 0.8f;
  return c;
}();

/// @brief Frail, higher-pitched voice (SAM: mouth=145, throat=145, speed=82)
static const FormantVoiceConfig LittleOldLady = [] {
  FormantVoiceConfig c = withVoiceCharacter(AdultFemale, 190.0f, 145.0f / 128.0f,
                                            145.0f / 128.0f, 82.0f / 72.0f);
  // Frail/aged character: more pitch/amplitude irregularity and breath.
  c.jitterPct = 0.03f;
  c.shimmerPct = 0.04f;
  c.breathiness = 0.12f;
  return c;
}();

/// @brief Otherworldly voice (SAM: mouth=200, throat=150, speed=100)
static const FormantVoiceConfig ExtraTerrestrial = [] {
  FormantVoiceConfig c = withVoiceCharacter(AdultMale, 170.0f, 200.0f / 128.0f,
                                            150.0f / 128.0f, 100.0f / 72.0f);
  // Otherworldly character: wider, more irregular formant wobble.
  c.formantRandomCents = 35.0f;
  c.enableF4 = true;
  return c;
}();

}  // namespace FormantVoice
