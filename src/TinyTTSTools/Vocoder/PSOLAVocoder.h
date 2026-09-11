/**
 * @file PSOLAVocoder.h
 * @brief TD-PSOLA-based speech synthesizer for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "VocoderBase.h"
#include "../Basic/PhonemeModifiers.h"
#include "../SoundDictionary/AudioDictionary.h"

#ifndef M_PI
#define M_PI 3.14159265359f
#endif

/**
 * @brief Speech synthesizer using TD-PSOLA (Time-Domain Pitch-Synchronous
 * Overlap-Add) on pre-recorded phoneme samples.
 * @details TD-PSOLA is the technique Praat (the phonetics software) is
 * best known for: it re-synthesizes a recording at a different pitch
 * and/or duration by re-placing short, windowed excerpts of the original
 * signal ("pitch-synchronous" because each excerpt is centered on one
 * estimated glottal pulse / pitch period) at new positions, rather than
 * simply resampling (which would change pitch and duration together and
 * inevitably introduces some artifacts) or truncating (which is what
 * PhonemeVocoder/ConcatenatedAudioVocoder do -- a real recording can only
 * ever be cut short to fit a target duration, never genuinely stretched
 * or re-pitched).
 *
 * Unlike PhonemeVocoder, this class does its own single-phoneme
 * synthesis from scratch rather than going through
 * ConcatenatedAudioVocoder's truncate-and-crossfade pipeline -- PSOLA
 * needs the *whole* source recording as raw samples to re-synthesize
 * from, not a pre-truncated/crossfaded excerpt.
 *
 * ## Algorithm (per phoneme)
 *
 * 1. Decode the phoneme's stored recording to `float` samples.
 * 2. Estimate the recording's own pitch period once, via autocorrelation
 *    over a plausible human-voice range (60-300Hz) -- a single estimate
 *    for the whole unit, not frame-by-frame re-estimation; adequate for
 *    the short (tens-of-ms), largely steady-state units this library's
 *    phoneme recordings are.
 * 3. Compute the target pitch period from `params.pitchHz` (or a default
 *    voice pitch if unset) and the target duration in samples from
 *    `params.durationMs` (or the phoneme's own table duration).
 * 4. Place synthesis marks evenly spaced at the target period across the
 *    target duration. For each one, map its position back to the source
 *    recording (linear time-scaling from target duration to source
 *    duration), extract a Hann-windowed excerpt spanning one source pitch
 *    period on each side of that position, and overlap-add it into the
 *    output at the synthesis mark.
 * 5. Normalize by the accumulated window weight at each output sample
 *    (standard overlap-add reconstruction) so unevenly-overlapping
 *    regions -- inevitable whenever the synthesis period differs from
 *    the source period -- don't come out too loud or too quiet.
 *
 * @note Unvoiced phonemes (most fricatives/stops) don't really have a
 * "pitch period" in the glottal sense -- the autocorrelation estimate for
 * these just locks onto whatever periodicity-like structure the noise
 * has, which still gives PSOLA a reasonable, consistent window size to
 * work with for duration changes, even though pitch-shifting a genuinely
 * unvoiced sound has no real perceptual meaning.
 *
 * ## PhonemeModifier support (see PhonemeModifiers.h)
 * A token's modifier tag (e.g. "T_j", stripped via parsePhonemeModifiers())
 * is combined with any caller-set PhonemeSynthesisParams via
 * deriveModifierEffect(), the same shared mapping FormantVocoder uses --
 * so a given modifier means the same thing regardless of which vocoder
 * renders it. What PSOLA can and can't honor differs fundamentally from a
 * procedural vocoder, though, since it resynthesizes a single fixed
 * recording rather than generating a source from scratch:
 *  - Duration (MOD_LONG/HALF_LONG/STRESS_PRIMARY/SECONDARY, TONE_CHECKED,
 *    and MOD_ASPIRATED/MOD_SYLLABIC's fixed +45ms/+40ms extension): fully
 *    supported -- this is exactly what PSOLA's own time-stretch mechanism
 *    already does.
 *  - Pitch contour (TONE_* values' f0StartRatio/MidRatio/EndRatio, or a
 *    caller's own): fully supported, and arguably a more natural fit here
 *    than in a procedural vocoder -- PSOLA already places one synthesis
 *    mark per pitch period, so each mark's target period is simply
 *    computed from the contour ratio at its own position instead of one
 *    fixed target pitch for the whole phoneme.
 *  - Voicing (MOD_DEVOICED/MOD_BREATHY, and the pre-existing but
 *    previously-unused PhonemeSynthesisParams::voicing): the ONE
 *    direction this can meaningfully realize -- blending the
 *    PSOLA-reconstructed signal with generated broadband noise scaled to
 *    the same RMS, post-reconstruction. Not a true voiced/unvoiced source
 *    switch (there's no separate unvoiced recording to blend from), but
 *    directionally correct and audible. MOD_VOICED is honestly a no-op
 *    here beyond restoring full voicing if something else had lowered
 *    it: there's no way to inject periodicity INTO an inherently noisy
 *    recording the way FormantVocoder can switch its procedural source
 *    from noise to a glottal pulse -- the recording's own natural
 *    voicing quality plays back unchanged either way.
 *  - Secondary articulation/place-shifting modifiers (MOD_NASALIZED,
 *    MOD_PALATALIZED, MOD_LABIALIZED, MOD_VELARIZED, MOD_PHARYNGEALIZED,
 *    MOD_RHOTACIZED, MOD_UNRELEASED): NOT implemented here. These need a
 *    formant/spectral-domain shift (an EQ or filter stage) that's
 *    orthogonal to PSOLA's own purely time-domain pitch/duration
 *    mechanism -- adding one is a separate, real DSP task, not a natural
 *    extension of this class's existing algorithm. A token carrying one
 *    of these still resynthesizes correctly (via
 *    AudioDictionary::getSoundEntry()'s modifier-stripping fallback), it
 *    just sounds identical to the unmodified base phoneme.
 */
class PSOLAVocoder : public VocoderBase {
 public:
  /// @param dictionary Audio dictionary of individual phoneme recordings
  /// (e.g. ArpabetWAVDictionary) -- same convention as PhonemeVocoder.
  /// @param defaultPitchHz Fallback pitch when params.pitchHz isn't set.
  explicit PSOLAVocoder(AudioDictionary& dictionary, float defaultPitchHz = 120.0f)
      : dictionary_(dictionary),
        defaultPitchHz_(defaultPitchHz),
        noiseSeed_(nextInstanceSeed()) {}

  std::string getType() const override { return "PSOLAVocoder"; }
  bool isReady() const override { return true; }

 protected:
  int sampleRate() const override { return dictionary_.sampleRate(); }

  /// Splits on spaces itself (matches FormantVocoder's own convention --
  /// see its doc): sayPhoneme() calls synthesizePhoneme() directly with
  /// whatever it was given, which for a whole-utterance TinyTTSTools::say()
  /// call is the full space-separated phoneme sequence, not one token.
  bool synthesizePhoneme(const std::string& phoneme, ::Print& out,
                         const PhonemeSynthesisParams& params) override {
    std::vector<std::string> tokens = StringUtils::split(phoneme, ' ');
    if (tokens.empty()) return false;

    bool success = true;
    for (const std::string& token : tokens) {
      // Check silence against the bare (stress- and modifier-stripped)
      // name: audio dictionaries and Phonemes::isSilence() both key on
      // bare phoneme names, and isSilence() treats any unrecognized
      // symbol as silence -- without stripping first, every stressed
      // vowel (e.g. "IH1") or modifier-tagged token (e.g. "AA:", "P_h")
      // would silently turn to silence (see
      // ConcatenatedAudioVocoder::processSequenceWithLookahead's own copy
      // of this fix for the same reason). synthesizeOnePhoneme() does its
      // own stripping internally too -- it still needs the original,
      // stress/modifier-marked token to apply the stress-duration boost
      // and modifier effects.
      int stress;
      std::string bare = stripStressMarker(token, stress);
      PhonemeModifier unusedMod = PhonemeModifier::MOD_NONE;
      bare = parsePhonemeModifiers(bare, unusedMod);
      if (translator_.isSilence(defaultPhonemeType, bare.c_str())) {
        uint16_t durationMs = resolveDuration(
            translator_.getPhonemeDuration(defaultPhonemeType, bare), params);
        outputSilence(sampleRate(), durationMs, out);
      } else if (!synthesizeOnePhoneme(token, out, params)) {
        success = false;
      }
    }
    return success;
  }

 private:
  AudioDictionary& dictionary_;
  float defaultPitchHz_;
  uint32_t noiseSeed_;  // per-instance PRNG state, for the voicing blend

  /// Deterministic, still-distinct-per-instance seed -- see
  /// FormantVocoder.h's own copy of this helper for why it's a counter
  /// (reproducible run-to-run), not derived from `this` (ASLR-dependent).
  static uint32_t nextInstanceSeed() {
    static uint32_t counter = 0;
    ++counter;
    return 1u + (counter * 2654435761u);
  }

  float generateNoiseSource() {
    noiseSeed_ = noiseSeed_ * 1103515245u + 12345u;
    return ((float)(noiseSeed_ >> 16) / 32768.0f) - 1.0f;
  }

  bool synthesizeOnePhoneme(const std::string& phoneme, ::Print& out,
                            const PhonemeSynthesisParams& params) {
    // Dictionaries are keyed by bare phoneme names (e.g. "AA", not "AA1") --
    // strip the stress marker before lookup, same as the isSilence() check
    // in synthesizePhoneme() above.
    int stress;
    std::string bare = stripStressMarker(phoneme, stress);

    // Modifier tag (e.g. "T_j"), if the token carries one, on top of the
    // digit-stress convention above -- a PREFIX stress tag folds into the
    // same `stress` int (so it drives getPhonemeWithStressDuration() the
    // same way a digit-suffixed "IH1" would), everything else becomes the
    // modifier this phoneme applies. Token-level wins over a caller-set
    // params.modifier, matching FormantVocoder's own precedence.
    PhonemeModifier tagMod = PhonemeModifier::MOD_NONE;
    bare = parsePhonemeModifiers(bare, tagMod);
    if (tagMod == PhonemeModifier::MOD_STRESS_PRIMARY) {
      stress = 1;
      tagMod = PhonemeModifier::MOD_NONE;
    } else if (tagMod == PhonemeModifier::MOD_STRESS_SECONDARY) {
      stress = 2;
      tagMod = PhonemeModifier::MOD_NONE;
    }
    PhonemeModifier modifier =
        tagMod != PhonemeModifier::MOD_NONE ? tagMod : params.modifier;
    ModifierEffect modEffect = deriveModifierEffect(modifier);

    const SoundEntry* entry = dictionary_.getSoundEntry(bare.c_str());
    if (!entry) return false;

    size_t n = entry->samples();
    if (n == 0) return false;

    std::vector<float> src(n);
    for (size_t i = 0; i < n; i++) src[i] = static_cast<float>((*entry)[i]);

    // MOD_ASPIRATED/MOD_SYLLABIC add a fixed extra chunk of time to the
    // phoneme's own natural duration -- same amounts and rationale as
    // FormantVocoder::preparePhonemeSynthesis() (a VOT gap / absorbed
    // syllable-nucleus time), additive rather than a speed multiplier.
    uint16_t defaultDurationMs = getPhonemeWithStressDuration(bare, stress);
    if (modifier == PhonemeModifier::MOD_ASPIRATED) {
      defaultDurationMs += 45;
    } else if (modifier == PhonemeModifier::MOD_SYLLABIC) {
      defaultDurationMs += 40;
    }
    // MOD_LONG/MOD_HALF_LONG/TONE_CHECKED (via modEffect.speedMul) stretch
    // or shorten the syllable the same way a caller's own params.speed
    // would -- composed multiplicatively with it, not overriding it.
    PhonemeSynthesisParams effectiveParams = params;
    effectiveParams.speed *= modEffect.speedMul;
    uint16_t targetDurationMs = resolveDuration(defaultDurationMs, effectiveParams);
    size_t targetSamples = (static_cast<size_t>(targetDurationMs) * sampleRate()) / 1000;
    if (targetSamples == 0) targetSamples = n;

    float targetHz = params.pitchHz > 0.0f ? params.pitchHz : defaultPitchHz_;
    int sourcePeriod = estimatePitchPeriod(src, sampleRate());

    std::vector<float> outBuf(targetSamples, 0.0f);
    std::vector<float> weight(targetSamples, 0.0f);

    double srcScale = static_cast<double>(n) / static_cast<double>(targetSamples);
    int64_t windowLen = 2 * static_cast<int64_t>(sourcePeriod);

    // Pitch contour (see PhonemeSynthesisParams::f0StartRatio/MidRatio/
    // EndRatio, and PhonemeModifier's TONE_* values via modEffect):
    // composed multiplicatively the same way FormantVocoder does. Unlike
    // a procedural vocoder, PSOLA already places one synthesis mark per
    // pitch period, so the contour is realized by computing each mark's
    // OWN target period from its position-derived ratio, rather than one
    // fixed synthPeriod for the whole phoneme.
    float f0StartRatio = params.f0StartRatio * modEffect.f0StartRatio;
    float f0MidRatio = params.f0MidRatio * modEffect.f0MidRatio;
    float f0EndRatio = params.f0EndRatio * modEffect.f0EndRatio;

    for (int64_t synthMark = 0; synthMark < static_cast<int64_t>(targetSamples);) {
      float progress = targetSamples > 1
                            ? static_cast<float>(synthMark) /
                                  static_cast<float>(targetSamples - 1)
                            : 0.0f;
      float contourRatio =
          progress < 0.5f
              ? f0StartRatio + (f0MidRatio - f0StartRatio) * (progress * 2.0f)
              : f0MidRatio + (f0EndRatio - f0MidRatio) * ((progress - 0.5f) * 2.0f);
      int synthPeriod = static_cast<int>(sampleRate() / (targetHz * contourRatio));
      if (synthPeriod < kMinPeriodSamples) synthPeriod = kMinPeriodSamples;

      int64_t rawCenter = static_cast<int64_t>(synthMark * srcScale + 0.5);
      // Snap the extracted content to the nearest real source pitch-period
      // boundary, reusing or skipping source periods as needed, instead of
      // a raw proportional position: when target duration ~= source
      // duration, srcScale ~= 1, so an unsnapped center tracks synthMark
      // almost 1:1 regardless of the requested pitch, reconstructing the
      // source's own original waveform near-verbatim and making pitchHz a
      // no-op. Snapping decouples *what content* gets played (tied to
      // real source periods) from *how densely* it's placed (tied to the
      // target pitch), which is what actually lets overlap-add raise or
      // lower the perceived rate.
      int64_t analysisIdx = (rawCenter + sourcePeriod / 2) / sourcePeriod;
      int64_t center = analysisIdx * sourcePeriod;
      int64_t srcStart = center - sourcePeriod;
      int64_t outStart = synthMark - sourcePeriod;

      for (int64_t k = 0; k < windowLen; k++) {
        int64_t srcIdx = srcStart + k;
        int64_t outIdx = outStart + k;
        if (srcIdx < 0 || srcIdx >= static_cast<int64_t>(n)) continue;
        if (outIdx < 0 || outIdx >= static_cast<int64_t>(targetSamples)) continue;
        // Hann window over the excerpt -- tapers each overlapping copy to
        // zero at its edges so consecutive excerpts blend rather than
        // click at their boundaries.
        float hann = 0.5f - 0.5f * cosf(2.0f * M_PI * k / static_cast<float>(windowLen - 1));
        outBuf[outIdx] += src[srcIdx] * hann;
        weight[outIdx] += hann;
      }
      synthMark += synthPeriod;
    }

    // Voicing (MOD_DEVOICED/MOD_BREATHY, and the pre-existing
    // params.voicing this class didn't honor before): blend the
    // PSOLA-reconstructed signal with generated broadband noise scaled to
    // the same RMS, post-reconstruction -- see this class's own doc for
    // why this is an approximation (and why MOD_VOICED is a no-op here).
    float composedVoicing = params.voicing * modEffect.voicingMul;
    if (modEffect.voicingIsAbsolute) composedVoicing = modEffect.voicingAbsolute;
    if (composedVoicing < 0.0f) composedVoicing = 0.0f;
    if (composedVoicing > 1.0f) composedVoicing = 1.0f;
    if (composedVoicing < 0.999f) {
      double sumSq = 0.0;
      for (size_t i = 0; i < targetSamples; i++) {
        float v = weight[i] > 1e-6f ? outBuf[i] / weight[i] : 0.0f;
        sumSq += static_cast<double>(v) * v;
      }
      float rms = targetSamples > 0
                      ? sqrtf(static_cast<float>(sumSq / targetSamples))
                      : 0.0f;
      for (size_t i = 0; i < targetSamples; i++) {
        float v = weight[i] > 1e-6f ? outBuf[i] / weight[i] : 0.0f;
        float noise = generateNoiseSource() * rms;
        v = v * composedVoicing + noise * (1.0f - composedVoicing);
        outBuf[i] = v;
        weight[i] = 1.0f;  // already blended -- normalize step below is a no-op
      }
    }

    float volume = params.volume;
    std::vector<int16_t> pcm(targetSamples);
    for (size_t i = 0; i < targetSamples; i++) {
      float v = weight[i] > 1e-6f ? outBuf[i] / weight[i] : 0.0f;
      v *= volume;
      if (v > 32767.0f) v = 32767.0f;
      if (v < -32768.0f) v = -32768.0f;
      pcm[i] = static_cast<int16_t>(v);
    }

    out.write(reinterpret_cast<const uint8_t*>(pcm.data()), pcm.size() * sizeof(int16_t));
    return true;
  }

  static constexpr int kMinPeriodSamples = 8;  // ~1000Hz ceiling at 8kHz, a sanity floor

  /// Single autocorrelation-based pitch period estimate for the whole
  /// unit (see class doc for why one estimate, not frame-by-frame, is
  /// enough here). Searches lags corresponding to 60-300Hz.
  int estimatePitchPeriod(const std::vector<float>& x, int sr) const {
    int minLag = sr / 300;
    int maxLag = sr / 60;
    if (minLag < kMinPeriodSamples) minLag = kMinPeriodSamples;
    if (maxLag >= static_cast<int>(x.size())) maxLag = static_cast<int>(x.size()) - 1;
    if (minLag >= maxLag) return sr / 120;  // unit too short to estimate -- fall back to ~120Hz

    double bestScore = -1.0;
    int bestLag = sr / 120;
    for (int lag = minLag; lag <= maxLag; lag++) {
      double sum = 0.0;
      for (size_t i = 0; i + static_cast<size_t>(lag) < x.size(); i++) {
        sum += static_cast<double>(x[i]) * x[i + lag];
      }
      if (sum > bestScore) {
        bestScore = sum;
        bestLag = lag;
      }
    }
    return bestLag;
  }
};
