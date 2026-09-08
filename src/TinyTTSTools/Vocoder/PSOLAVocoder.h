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
 */
class PSOLAVocoder : public VocoderBase {
 public:
  /// @param dictionary Audio dictionary of individual phoneme recordings
  /// (e.g. ArpabetWAVDictionary) -- same convention as PhonemeVocoder.
  /// @param defaultPitchHz Fallback pitch when params.pitchHz isn't set.
  explicit PSOLAVocoder(AudioDictionary& dictionary, float defaultPitchHz = 120.0f)
      : dictionary_(dictionary), defaultPitchHz_(defaultPitchHz) {}

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
      // Check silence against the bare (stress-stripped) name: audio
      // dictionaries and Phonemes::isSilence() both key on bare phoneme
      // names, and isSilence() treats any unrecognized symbol as silence
      // -- without stripping first, every stressed vowel (e.g. "IH1")
      // would silently turn to silence (see
      // ConcatenatedAudioVocoder::processSequenceWithLookahead's own copy
      // of this fix for the same reason). synthesizeOnePhoneme() does its
      // own stripping internally too -- it still needs the original,
      // stress-marked token to apply the stress-duration boost.
      int stress;
      std::string bare = stripStressMarker(token, stress);
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

  bool synthesizeOnePhoneme(const std::string& phoneme, ::Print& out,
                            const PhonemeSynthesisParams& params) {
    // Dictionaries are keyed by bare phoneme names (e.g. "AA", not "AA1") --
    // strip the stress marker before lookup, same as the isSilence() check
    // in synthesizePhoneme() above.
    int stress;
    std::string bare = stripStressMarker(phoneme, stress);

    const SoundEntry* entry = dictionary_.getSoundEntry(bare.c_str());
    if (!entry) return false;

    size_t n = entry->samples();
    if (n == 0) return false;

    std::vector<float> src(n);
    for (size_t i = 0; i < n; i++) src[i] = static_cast<float>((*entry)[i]);

    uint16_t defaultDurationMs = getPhonemeWithStressDuration(bare, stress);
    uint16_t targetDurationMs = resolveDuration(defaultDurationMs, params);
    size_t targetSamples = (static_cast<size_t>(targetDurationMs) * sampleRate()) / 1000;
    if (targetSamples == 0) targetSamples = n;

    float targetHz = params.pitchHz > 0.0f ? params.pitchHz : defaultPitchHz_;
    int synthPeriod = static_cast<int>(sampleRate() / targetHz);
    if (synthPeriod < kMinPeriodSamples) synthPeriod = kMinPeriodSamples;

    int sourcePeriod = estimatePitchPeriod(src, sampleRate());

    std::vector<float> outBuf(targetSamples, 0.0f);
    std::vector<float> weight(targetSamples, 0.0f);

    double srcScale = static_cast<double>(n) / static_cast<double>(targetSamples);
    int64_t windowLen = 2 * static_cast<int64_t>(sourcePeriod);

    for (int64_t synthMark = 0; synthMark < static_cast<int64_t>(targetSamples);
         synthMark += synthPeriod) {
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
