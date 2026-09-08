/**
 * @file FormantVocoder.h
 * @brief Formant-based speech synthesizer for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "VocoderBase.h"
#include "FormantVoiceConfig.h"
#include "FormantRules.h"

#ifndef M_PI
#  define M_PI 3.14159265359f
#endif

/**
 * @class FormantVocoder
 * @brief Compact real-time formant (parallel resonator) speech synthesizer.
 * @details Uses a table of phoneme rules (see FormantRules.h) defining
 * formant frequencies, bandwidths, amplitudes, flags, diphthong targets and
 * default durations. A custom rule table can be injected via the constructor
 * for alternate phoneme sets. The engine adds basic naturalness features
 * (jitter, shimmer, spectral tilt, diphthong interpolation, stress shaping,
 * nasal notches, sibilant control and adaptive RMS leveling).
 * @note Memory footprint: no audio data required (procedural synthesis) --
 * the smallest flash footprint of the available vocoders, at the cost of a
 * more robotic sound. See
 * https://github.com/pschatzmann/TinyTTSTools/blob/main/docs/MEMORY.md for a
 * comparison table across all vocoders and G2P models.
 */
class FormantVocoder : public VocoderBase {
 public:
  /// Construct vocoder with sampleRate (Hz), optional volumeFactor and optional injected rule table
  /// @details Default volumeFactor is 1.7, not 1.0: measured output after
  /// RMS normalization alone peaks at only ~44% of full scale (~14500 of
  /// 32767) for typical speech, so there's ample headroom. 1.7 was chosen
  /// empirically as the highest factor that produced zero clipped samples
  /// across a long, varied test phrase (stressed vowels, sibilants) --
  /// 2.0 measured occasional clipping on peaks.
  FormantVocoder(uint32_t sampleRate, float volumeFactor = 1.7f,
                 const PhonemeRule* rules = kRules,
                 size_t ruleCount = kRuleCount)
      : sampleRate_(sampleRate),
        volumeFactor_(volumeFactor),
        rules_(rules),
        ruleCount_(ruleCount),
        voiceF0_(120.0f),  // Base fundamental frequency (will be overridden by
                           // config)
        voicePhase_(0.0f),
        noisePhase_(0.0f),
        lastF1_(500.0f),
        lastF2_(1500.0f),
        lastF3_(2500.0f),
        formantSmoothingFactor_(
            0.94f),  // stronger smoothing (closer to natural coarticulation)
        spectralTiltState_(0.0f),
        f0Current_(voiceF0_),
        jitterCounter_(0),
        noiseSeed_(1u + static_cast<uint32_t>(
                            reinterpret_cast<uintptr_t>(this) & 0xffffu)) {
    cfg_.baseF0 = voiceF0_;
    // Pre-allocate buffer for max expected phoneme duration (0.3s)
    maxSamples_ = static_cast<size_t>(0.3f * sampleRate_);
    audioBuffer_.reserve(maxSamples_);

    // Initialize formant filters
    initializeFilters();
  }

  /// Return vocoder type string ("FormantVocoder")
  std::string getType() const override { return "FormantVocoder"; }

  /// Set voice configuration (updates base fundamental frequency)
  void setVoiceConfig(const FormantVoiceConfig c) {
    cfg_ = c;
    voiceF0_ = c.baseF0;
  }
  /// Get current voice configuration copy
  FormantVoiceConfig getVoiceConfig() const { return cfg_; }

  /**
   * @brief Get sample rate for silence generation
   * @return Sample rate in Hz
   */
  int sampleRate() const override {
    return sampleRate_;
  }

 protected:
  uint32_t sampleRate_;
  float volumeFactor_;
  const PhonemeRule* rules_ = kRules;
  size_t ruleCount_ = kRuleCount;
  float voiceF0_;     // Fundamental frequency for voiced sounds
  float voicePhase_;  // Phase for voice oscillator
  float noisePhase_;  // Phase for noise generation
  size_t maxSamples_;
  std::vector<int16_t> audioBuffer_;  // Reusable buffer

  // Formant smoothing for natural transitions
  float lastF1_, lastF2_, lastF3_;
  float formantSmoothingFactor_;

  // Source processing states
  float spectralTiltState_;       // one-pole LP for glottal spectral tilt
  float f0Current_;               // current (jittered) fundamental
  uint16_t jitterCounter_;        // counter for periodic jitter updates
  float deEsserLPState_ = 0.0f;   // low-pass state for de-esser
  float adaptiveGain_ = 1.0f;     // smoothed adaptive gain
  float sibilantDynGain_ = 1.0f;  // dynamic gain applied to sibilants

  // Per-instance filter/noise state (must NOT be function-local statics --
  // those would be shared across every FormantVocoder instance)
  float lp_hp_ = 0.0f;         // low-pass state feeding the noise high-pass
  float prevNoise1_ = 0.0f;    // previous high-passed noise sample (strong sibilants)
  float prevNoise2_ = 0.0f;    // previous high-passed noise sample (postalveolar)
  float prevSample_ = 0.0f;    // previous output sample (light smoothing)
  float nasalLP_ = 0.0f;       // low-pass state for nasal coloration
  uint32_t noiseSeed_;         // per-instance PRNG state for generateNoiseSource()

  FormantVoiceConfig cfg_;

  // 4th optional formant
  /// Filter coefficients and state for a single formant resonator
  struct FormantFilter {
    float x1 = 0, x2 = 0;          // Input history (unused with direct form)
    float y1 = 0, y2 = 0;          // Output history
    float a1 = 0, a2 = 0, b0 = 0;  // Filter coefficients
  };

  /// Simple IIR notch filter used for nasal anti-formants
  struct NotchFilter {
    float x1 = 0, x2 = 0;
    float y1 = 0, y2 = 0;
    float b0 = 1.0f, b1 = 0.0f, b2 = 1.0f, a1 = 0.0f, a2 = 0.0f;
    bool enabled = false;
  };

  /// Classification flags derived from a phoneme rule
  struct PhonemeFlags {
    bool voiced = false;
    bool strongSibilant = false;
    bool postalveolar = false;
    bool nasal = false;
    bool silence = false;
  };

  // Context for per-phoneme synthesis (moved earlier to allow passing into
  // synthesis)
  /// Aggregate per-phoneme synthesis parameters and evolving state
  struct PhonemeSynthesisContext {  // moved from later position
    std::string phonStr;
    int stress = 0;
    float duration = 0.0f;
    size_t numSamples = 0;
    FormantParams params{};
    FormantParams target2{};
    bool dynamic = false;
    PhonemeFlags flags{};
    float f0 = 0.0f;
    float stressPitchRise = 0.0f;
    float voicing = 1.0f;
    // Snapshot of lastF1_/2_/3_ taken BEFORE this phoneme's own formants
    // overwrite them -- i.e. where the PREVIOUS phoneme's formants ended
    // up (or a neutral resting position for the very first phoneme of an
    // utterance). synthesizeSamples() glides from this entry point toward
    // this phoneme's own target over attackSamples, instead of snapping
    // straight to the target -- real coarticulatory formant transitions
    // carry a lot of the perceptual cues that make consonants and vowels
    // distinguishable, so starting every phoneme already "at rest" made
    // speech sound like separately-glued blobs.
    float entryF1 = 0.0f, entryF2 = 0.0f, entryF3 = 0.0f;
    size_t attackSamples = 0;
  };

  FormantFilter f1Filter_, f2Filter_, f3Filter_;
  FormantFilter f4Filter_;   // optional
  NotchFilter nasalNotch1_;  // nasal anti-formant 1
  NotchFilter nasalNotch2_;  // nasal anti-formant 2

  // Tail buffer for crossfade overlap
  std::vector<int16_t> previousTail_;
  size_t previousOverlap_ = 0;

  /// Synthesize a phoneme sequence and write samples to the output stream
  /// @details `phoneme` may be a whole multi-phoneme (even whole-utterance)
  /// sequence -- TinyTTSTools joins an entire utterance into one call, so
  /// synthesizePhoneme() must split on spaces itself rather than assume a
  /// single token. Previously it didn't: the whole joined string (e.g.
  /// "DH AH0 SP K W IH K SP ...") was treated as ONE unrecognized phoneme,
  /// so findPhonemeRule() never matched, and only a single ~100ms burst of
  /// default-fallback noise was produced for the entire utterance --
  /// audible as "no real audio". Splits here and delegates each token to
  /// synthesizeSingleToken(), which still does this class's own
  /// crossfade-aware silence handling (handleSilenceSetup) per token
  /// rather than the generic VocoderBase silence path.
  bool synthesizePhoneme(const std::string& phoneme, ::Print& out,
                         const PhonemeSynthesisParams& params) override {
    std::vector<std::string> tokens = StringUtils::split(phoneme, ' ');
    bool success = true;
    for (const std::string& token : tokens) {
      if (token.empty()) continue;
      if (!synthesizeSingleToken(token, out, params)) success = false;
    }
    return success;
  }

  /// Synthesize a single phoneme token and write samples to the output stream
  bool synthesizeSingleToken(const std::string& phoneme, ::Print& out,
                             const PhonemeSynthesisParams& params) {
    PhonemeSynthesisContext ctx;
    preparePhonemeSynthesis(phoneme, ctx, params);
    if (handleSilenceSetup(ctx, out)) return true;
    synthesizeSamples(ctx);
    applyStressEnergyBoost(ctx.numSamples, ctx.stress);
    applyRMSNormalization(ctx.numSamples, ctx.flags);
    applySibilantPostProcessing(ctx.numSamples, ctx.flags);
    applyCrossfade(ctx.numSamples);
    applyVolumeFactor(ctx.numSamples, params.volume);
    out.write(reinterpret_cast<const uint8_t*>(audioBuffer_.data()),
              ctx.numSamples * sizeof(int16_t));
    return true;
  }

  // Simple helper to get small random float in range [-1,1]
  inline float frand() { return generateNoiseSource(); }

  /// Initialize formant filters
  void initializeFilters() {
    // Reset filter states
    f1Filter_ = FormantFilter();
    f2Filter_ = FormantFilter();
    f3Filter_ = FormantFilter();
    f4Filter_ = FormantFilter();
  }

  /// Configure a formant filter resonator (frequency & bandwidth)
  void configureFormantFilter(FormantFilter& filter, float frequency,
                              float bandwidth) {
    if (frequency <= 0.0f) {  // handle silence
      filter.a1 = 0.0f;
      filter.a2 = 0.0f;
      filter.b0 = 0.0f;
      return;
    }
    float r = expf(-M_PI * bandwidth / sampleRate_);  // pole radius
    float theta = 2.0f * M_PI * frequency / sampleRate_;
    // Canonical resonator: y[n] = b0*x[n] + a1*y[n-1] + a2*y[n-2]; poles at
    // r*e^{±j theta}
    filter.a1 = 2.0f * r * cosf(theta);
    filter.a2 = -r * r;
    filter.b0 =
        1.0f -
        r;  // energy normalization keeping level stable across bandwidths
  }

  /// Apply formant filter and return processed sample
  float applyFormantFilter(FormantFilter& filter, float input) {
    float y = filter.b0 * input + filter.a1 * filter.y1 + filter.a2 * filter.y2;
    filter.y2 = filter.y1;
    filter.y1 = y;
    return y;
  }

  /// Generate voiced glottal source sample (with jitter/shimmer)
  float generateVoiceSource(float t, float f0Base) {
    // Pitch with jitter updated every few milliseconds
    const uint16_t jitterIntervalSamples = sampleRate_ / 200;  // ~5ms
    if (++jitterCounter_ >= jitterIntervalSamples) {
      jitterCounter_ = 0;
      float jitter = cfg_.jitterPct * frand();
      // Base the jittered pitch on f0Base (the per-phoneme pitch passed in,
      // which may carry a stress rise or an explicit pitchHz override from
      // PhonemeSynthesisParams) rather than the instance's static voiceF0_ --
      // using voiceF0_ here made f0Base a dead parameter and silently
      // discarded any per-call pitch adjustment.
      f0Current_ = f0Base * (1.0f + jitter);
    }
    // Add small downward pitch contour over phoneme duration (approx t
    // fraction)
    float dynamicF0 = f0Current_ - cfg_.pitchFallPerSec * t;
    if (dynamicF0 < 50.0f) dynamicF0 = 50.0f;
    voicePhase_ += 2.0f * M_PI * dynamicF0 / sampleRate_;
    if (voicePhase_ >= 2.0f * M_PI) voicePhase_ -= 2.0f * M_PI;
    float phase_norm = voicePhase_ / (2.0f * M_PI);
    // Simplified LF derivative (two-piece polynomial) + closing spike
    float glottal = 0.0f;
    const float openFrac = 0.45f;
    const float closeFrac = 0.8f;
    if (phase_norm < openFrac) {
      float x = phase_norm / openFrac;
      glottal = x - x * x * x * 0.3333f;  // gentle rise
    } else if (phase_norm < closeFrac) {
      float x = (phase_norm - openFrac) / (closeFrac - openFrac);
      glottal = 1.0f - 0.5f * x * x;  // quasi-plateau with slight fall
    } else {
      float x = (phase_norm - closeFrac) / (1.0f - closeFrac);
      glottal = 0.2f * (1.0f - x);  // abrupt closure & return to zero
    }
    // Shimmer (amplitude variation)
    glottal *= (1.0f + cfg_.shimmerPct * frand());
    // Spectral tilt
    spectralTiltState_ = (1.0f - cfg_.spectralTilt) * glottal +
                         cfg_.spectralTilt * spectralTiltState_;
    // Add controlled breathiness
    float aspiration = cfg_.breathiness * generateNoiseSource();
    return 0.55f * spectralTiltState_ + 0.35f * glottal + aspiration;
  }

  /// Generate broadband noise source sample
  float generateNoiseSource() {
    // Simple linear congruential generator for noise (per-instance state,
    // so multiple simultaneous FormantVocoder instances don't share/corrupt
    // each other's noise stream)
    noiseSeed_ = noiseSeed_ * 1103515245 + 12345;
    return ((float)(noiseSeed_ >> 16) / 32768.0f) - 1.0f;
  }

  /// Remove trailing stress digit (1/2) from phoneme and return stress value
  std::string stripStress(const std::string& phoneme, int& stress) {
    return stripStressMarker(phoneme, stress);  // Use common implementation from parent
  }

  /// Linear search for phoneme rule by symbol
  const PhonemeRule* findPhonemeRule(const std::string& p) const {
    for (size_t i = 0; i < ruleCount_; ++i) {
      if (p == rules_[i].symbol) return &rules_[i];
    }
    return nullptr;
  }

  /// Apply small random perturbations to formants/bandwidths if enabled
  void randomizeFormantsIfNeeded(FormantParams& params,
                                 const std::string& phonStr) {
    if (cfg_.enableFormantRandom &&
        (isVoicedPhoneme(phonStr) || phonStr.size() == 2)) {
      auto randCents = [&](float cents) {
        return powf(2.0f, (cents * frand()) / 1200.0f);
      };
      float cspan = cfg_.formantRandomCents;
      float r1 = randCents(cspan * 0.5f);
      float r2 = randCents(cspan);
      float r3 = randCents(cspan);
      params.f1 *= r1;
      params.f2 *= r2;
      params.f3 *= r3;
      params.bw1 *= (1.0f + cfg_.bandwidthRandomPct * frand());
      params.bw2 *= (1.0f + cfg_.bandwidthRandomPct * frand());
      params.bw3 *= (1.0f + cfg_.bandwidthRandomPct * frand());
    }
  }

  /// Derive flag classification (voiced, sibilant, nasal, etc.) for phoneme
  PhonemeFlags classifyPhoneme(const std::string& p) {
    PhonemeFlags fl;
    const PhonemeRule* r = findPhonemeRule(p);
    if (r) {
      fl.voiced = (r->flags & PF_VOICED) != 0;
      fl.strongSibilant =
          (p == "S" || p == "Z" || p == "s" ||
           p == "z");  // keep explicit to match prior logic for scaling
      fl.postalveolar = (r->params.f2 == 2000 &&
                         (p == "SH" || p == "ʃ" || p == "ZH" || p == "ʒ"));
      fl.nasal = (r->flags & PF_NASAL) != 0;
      fl.silence = (r->flags & PF_SILENCE) != 0;
    }
    return fl;
  }

  /// Return pitch rise in Hz based on stress level
  float computeStressPitchRise(int stress) {
    if (!cfg_.enableStressPitch) return 0.0f;
    if (stress == 1) return cfg_.stressPitchRisePrimary;
    if (stress == 2) return cfg_.stressPitchRiseSecondary;
    return 0.0f;
  }

  /// Initialize formant filters from starting parameter set
  void configureInitialFilters(const FormantParams& params) {
    // Deliberately do NOT reset lastF1_/2_/3_ to this phoneme's own
    // targets here -- they persist across calls (see the coarticulation
    // comment on PhonemeSynthesisContext) so filters start from wherever
    // the previous phoneme left off, and synthesizeSamples() glides them
    // toward `params` over the attack window instead of snapping.
    configureFormantFilter(f1Filter_, lastF1_, params.bw1);
    configureFormantFilter(f2Filter_, lastF2_, params.bw2);
    configureFormantFilter(f3Filter_, lastF3_, params.bw3);
    if (cfg_.enableF4) {
      float f4 = 3400.0f;
      configureFormantFilter(f4Filter_, f4, 250.0f);
    }
  }

  /// Configure/apply nasal notch filters if phoneme is nasal
  void applyNasalNotchesIfNeeded(float& sample, bool isNasal, size_t i) {
    if (!(isNasal && cfg_.enableNasalNotch)) return;
    if (i == 0) {
      auto configNotch = [&](NotchFilter& nf, float f, float bw) {
        float r = expf(-M_PI * bw / sampleRate_);
        float theta = 2.0f * M_PI * f / sampleRate_;
        nf.b0 = 1.0f;
        nf.b1 = -2.0f * cosf(theta);
        nf.b2 = 1.0f;
        nf.a1 = 2.0f * r * cosf(theta);
        nf.a2 = -r * r;
        nf.x1 = nf.x2 = nf.y1 = nf.y2 = 0.0f;
        nf.enabled = true;
      };
      configNotch(nasalNotch1_, cfg_.nasalNotchFreq1, cfg_.nasalNotchBw1);
      configNotch(nasalNotch2_, cfg_.nasalNotchFreq2, cfg_.nasalNotchBw2);
    }
    auto applyNotch = [&](NotchFilter& nf, float in) {
      if (!nf.enabled) return in;
      float y = nf.b0 * in + nf.b1 * nf.x1 + nf.b2 * nf.x2 + nf.a1 * nf.y1 +
                nf.a2 * nf.y2;
      nf.x2 = nf.x1;
      nf.x1 = in;
      nf.y2 = nf.y1;
      nf.y1 = y;
      return y;
    };
    float before = sample;
    sample = applyNotch(nasalNotch1_, sample);
    sample = applyNotch(nasalNotch2_, sample);
    sample =
        (1.0f - cfg_.nasalNotchDepth) * before + cfg_.nasalNotchDepth * sample;
  }

  /// Simple high-band de-esser for strong sibilants
  void applyDeEsserIfNeeded(float& sample, const PhonemeFlags& flags) {
    if (flags.voiced) return;
    if (!(flags.strongSibilant || flags.postalveolar)) return;
    deEsserLPState_ = deEsserLPState_ + 0.1f * (sample - deEsserLPState_);
    float high = sample - deEsserLPState_;
    float ahigh = fabsf(high);
    if (ahigh > cfg_.deEsserThreshold) {
      float over =
          (ahigh - cfg_.deEsserThreshold) / (0.6f - cfg_.deEsserThreshold);
      if (over > 1.0f) over = 1.0f;
      float atten = 1.0f - cfg_.deEsserAtten * over;
      sample *= atten;
    }
  }

  /// Apply nasal low-pass coloration if nasal phoneme
  void finalizeSampleNasalLP(float& sample, bool isNasal) {
    if (!isNasal) return;
    nasalLP_ = nasalLP_ + 0.15f * (sample - nasalLP_);
    sample = (1.0f - cfg_.nasalLowpass) * sample + cfg_.nasalLowpass * nasalLP_;
  }

  /// Generate all samples for current phoneme into audio buffer
  void synthesizeSamples(PhonemeSynthesisContext& ctx) {
    size_t overlapSamples =
        static_cast<size_t>((cfg_.overlapMs / 1000.0f) * sampleRate_);
    if (overlapSamples > ctx.numSamples / 2)
      overlapSamples = ctx.numSamples / 2;
    for (size_t i = 0; i < ctx.numSamples; ++i) {
      float t = (float)i / (float)sampleRate_;
      float progress =
          (ctx.numSamples > 1) ? (float)i / (float)(ctx.numSamples - 1) : 0.0f;
      // Steady-state (or, for diphthongs, mid-phoneme-eased) formant
      // target for this sample.
      float tf1, tf2, tf3;
      if (ctx.dynamic) {
        float ease = progress < 0.5f
                         ? 2.0f * progress * progress
                         : -1.0f + (4.0f - 2.0f * progress) * progress;
        tf1 = ctx.params.f1 + (ctx.target2.f1 - ctx.params.f1) * ease;
        tf2 = ctx.params.f2 + (ctx.target2.f2 - ctx.params.f2) * ease;
        tf3 = ctx.params.f3 + (ctx.target2.f3 - ctx.params.f3) * ease;
      } else {
        tf1 = ctx.params.f1;
        tf2 = ctx.params.f2;
        tf3 = ctx.params.f3;
      }
      // Coarticulation: for the first attackSamples, blend from where the
      // PREVIOUS phoneme's formants ended (ctx.entryF1/2/3) toward this
      // sample's own target, instead of starting already at rest -- real
      // formant transitions between adjacent phonemes carry much of
      // speech's intelligibility (especially consonant place-of-
      // articulation cues), so snapping every phoneme instantly to its own
      // target made speech sound like separately-glued blobs.
      if (i < ctx.attackSamples) {
        float attackProgress = (float)i / (float)ctx.attackSamples;
        // Raised-cosine ease: smooth start and end of the glide.
        float ease = 0.5f - 0.5f * cosf((float)M_PI * attackProgress);
        tf1 = ctx.entryF1 + (tf1 - ctx.entryF1) * ease;
        tf2 = ctx.entryF2 + (tf2 - ctx.entryF2) * ease;
        tf3 = ctx.entryF3 + (tf3 - ctx.entryF3) * ease;
      }
      lastF1_ = formantSmoothingFactor_ * lastF1_ +
                (1.0f - formantSmoothingFactor_) * tf1;
      lastF2_ = formantSmoothingFactor_ * lastF2_ +
                (1.0f - formantSmoothingFactor_) * tf2;
      lastF3_ = formantSmoothingFactor_ * lastF3_ +
                (1.0f - formantSmoothingFactor_) * tf3;
      if ((i & 31) == 0) {
        configureFormantFilter(f1Filter_, lastF1_, ctx.params.bw1);
        configureFormantFilter(f2Filter_, lastF2_, ctx.params.bw2);
        configureFormantFilter(f3Filter_, lastF3_, ctx.params.bw3);
      }
      float source = 0.0f;
      if (ctx.flags.voiced) {
        float f0Adj = ctx.f0;
        if (ctx.stressPitchRise > 0.0f) {
          float sd = progress / (cfg_.stressDecayPortion <= 0.01f
                                     ? 0.01f
                                     : cfg_.stressDecayPortion);
          if (sd > 1.0f) sd = 1.0f;
          float decay = expf(-2.2f * sd);
          f0Adj += ctx.stressPitchRise * decay;
        }
        float g = generateVoiceSource(progress * ctx.duration, f0Adj);
        float harmonics = 0.0f;
        const int NH = 5;
        for (int h = 2; h <= NH; ++h)
          harmonics += (1.0f / (float)h) * sinf(voicePhase_ * h);
        float voicedSource = 0.65f * g + 0.30f * harmonics;
        if (ctx.voicing >= 1.0f) {
          source = voicedSource;
        } else {
          // Blend toward an unvoiced/whispered version of this phoneme:
          // replace the harmonic glottal source with broadband noise as
          // voicing decreases from 1.0 (fully voiced) to 0.0 (whisper).
          // Naturally unvoiced phonemes never reach this branch, so this
          // only affects vowels/voiced consonants.
          float n = generateNoiseSource();
          lp_hp_ = 0.85f * lp_hp_ + 0.15f * n;
          float hp = n - lp_hp_;
          float clampedVoicing = ctx.voicing < 0.0f ? 0.0f : ctx.voicing;
          source = voicedSource * clampedVoicing + hp * (1.0f - clampedVoicing);
        }
      } else {
        float n = generateNoiseSource();
        lp_hp_ = 0.85f * lp_hp_ + 0.15f * n;
        float hp = n - lp_hp_;
        if (ctx.flags.strongSibilant) {
          float bp = hp - 0.6f * prevNoise1_;
          prevNoise1_ = hp;
          source = cfg_.sibilantGain * bp;
        } else if (ctx.flags.postalveolar) {
          float bp = (hp - prevNoise2_);
          prevNoise2_ = hp;
          source = cfg_.postalveolarGain * bp;
        } else
          source = hp;
      }
      float out1 = applyFormantFilter(f1Filter_, source) * ctx.params.a1;
      float out2 = applyFormantFilter(f2Filter_, source) * ctx.params.a2;
      float out3 = applyFormantFilter(f3Filter_, source) * ctx.params.a3;
      float out4 = 0.0f;
      if (cfg_.enableF4) out4 = applyFormantFilter(f4Filter_, source) * 0.25f;
      if (!ctx.flags.voiced && ctx.flags.strongSibilant)
        out3 *= cfg_.sibilantHFFormantScale;
      float sample = out1 + out2 + out3 + out4;
      applyNasalNotchesIfNeeded(sample, ctx.flags.nasal, i);
      sample = 0.9f * sample + 0.1f * prevSample_;
      prevSample_ = sample;
      float envelope = generateEnvelope(t, ctx.duration);
      sample *= envelope;
      finalizeSampleNasalLP(sample, ctx.flags.nasal);
      applyDeEsserIfNeeded(sample, ctx.flags);
      if (sample > 1.0f) sample = 1.0f - 0.5f * (sample - 1.0f);
      if (sample < -1.0f) sample = -1.0f - 0.5f * (sample + 1.0f);
      audioBuffer_[i] = (int16_t)(sample * 15000.0f);
    }
  }

  /// Adaptive RMS normalization (and mild limiting) per phoneme
  void applyRMSNormalization(size_t numSamples, const PhonemeFlags& flags) {
    int64_t sumSq = 0;
    for (size_t i = 0; i < numSamples; ++i) {
      int32_t v = audioBuffer_[i];
      sumSq += (int64_t)v * v;
    }
    float rms =
        (numSamples > 0) ? sqrtf((float)sumSq / (float)numSamples) : 0.0f;
    float target = flags.voiced ? cfg_.targetRMS
                                : ((flags.strongSibilant || flags.postalveolar)
                                       ? cfg_.fricativeTargetRMS
                                       : cfg_.targetRMS * 0.9f);
    if (rms > 1.0f) {
      float desiredGain = target / rms;
      if (desiredGain > 1.5f) desiredGain = 1.5f;
      adaptiveGain_ = (1.0f - cfg_.rmsAdaptRate) * adaptiveGain_ +
                      cfg_.rmsAdaptRate * desiredGain;
      for (size_t i = 0; i < numSamples; ++i) {
        int32_t v = (int32_t)(audioBuffer_[i] * adaptiveGain_);
        if (v > 32767)
          v = 32767;
        else if (v < -32768)
          v = -32768;
        audioBuffer_[i] = (int16_t)v;
      }
    }
  }

  /// Peak limiting, compression and softening for sibilant/fricatives
  void applySibilantPostProcessing(size_t numSamples,
                                   const PhonemeFlags& flags) {
    if (flags.voiced || !(flags.strongSibilant || flags.postalveolar)) return;
    // Peak limiting
    int16_t peak = 0;
    for (size_t i = 0; i < numSamples; ++i) {
      int16_t v = audioBuffer_[i];
      int16_t a = v < 0 ? -v : v;
      if (a > peak) peak = a;
    }
    float targetP = cfg_.fricativeMaxPeak;
    if (peak > targetP && peak > 0) {
      float scale = targetP / (float)peak;
      for (size_t i = 0; i < numSamples; ++i) {
        audioBuffer_[i] = (int16_t)(audioBuffer_[i] * scale);
      }
    }
    // Dynamic high-band compression
    float lp = 0.0f;
    float sumSqHB = 0.0f;
    for (size_t i = 0; i < numSamples; ++i) {
      float x = (float)audioBuffer_[i];
      lp = lp + 0.08f * (x - lp);
      float hb = x - lp;
      sumSqHB += hb * hb;
    }
    float rmsHB =
        (numSamples > 0) ? sqrtf(sumSqHB / numSamples) : 0.0f;
    float targetHB = cfg_.sibilantDynamicTargetRMS;
    if (rmsHB > targetHB && rmsHB > 1.0f) {
      float over = rmsHB / targetHB;
      float expn = (1.0f / cfg_.sibilantCompressionRatio) - 1.0f;
      float desiredGain = powf(over, expn);
      if (desiredGain < 0.25f) desiredGain = 0.25f;
      float smooth = (desiredGain < sibilantDynGain_) ? cfg_.sibilantAttack
                                                      : cfg_.sibilantRelease;
      sibilantDynGain_ =
          smooth * sibilantDynGain_ + (1.0f - smooth) * desiredGain;
      for (size_t i = 0; i < numSamples; ++i) {
        int32_t v = (int32_t)(audioBuffer_[i] * sibilantDynGain_);
        if (v > 32767)
          v = 32767;
        else if (v < -32768)
          v = -32768;
        audioBuffer_[i] = (int16_t)v;
      }
    } else {
      sibilantDynGain_ = cfg_.sibilantRelease * sibilantDynGain_ +
                         (1.0f - cfg_.sibilantRelease);
    }
    if (cfg_.sibilantLowpassMix > 0.0f) {
      float lp2 = 0.0f;
      float mix = cfg_.sibilantLowpassMix;
      for (size_t i = 0; i < numSamples; ++i) {
        float x = (float)audioBuffer_[i];
        lp2 += 0.12f * (x - lp2);
        float softened = lp2;
        float y = x * (1.0f - mix) + softened * mix;
        int32_t v = (int32_t)(y);
        if (v > 32767)
          v = 32767;
        else if (v < -32768)
          v = -32768;
        audioBuffer_[i] = (int16_t)v;
      }
    }
    if (cfg_.sibilantFinalGain < 0.999f) {
      float g = cfg_.sibilantFinalGain;
      for (size_t i = 0; i < numSamples; ++i) {
        int32_t v = (int32_t)(audioBuffer_[i] * g);
        if (v > 32767)
          v = 32767;
        else if (v < -32768)
          v = -32768;
        audioBuffer_[i] = (int16_t)v;
      }
    }
  }

  /// Apply final volume scaling (instance-wide volumeFactor_ combined with
  /// the optional per-call PhonemeSynthesisParams::volume override)
  void applyVolumeFactor(size_t numSamples, float callVolume = 1.0f) {
    float gain = volumeFactor_ * callVolume;
    for (size_t i = 0; i < numSamples; ++i) {
      int32_t v = (int32_t)(gain * audioBuffer_[i]);
      if (v > 32767)
        v = 32767;
      else if (v < -32768)
        v = -32768;
      audioBuffer_[i] = (int16_t)v;
    }
  }

  /// Crossfade overlap with tail of previous phoneme (if any)
  void applyCrossfade(size_t numSamples) {
    if (!previousTail_.empty()) {
      size_t overlap = std::min(previousTail_.size(), numSamples);
      for (size_t i = 0; i < overlap; ++i) {
        float w = (float)i / (float)(overlap - 1);
        float a = previousTail_[i] * (1.0f - w);
        float b = audioBuffer_[i] * w;
        int32_t mix = (int32_t)a + (int32_t)b;
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        audioBuffer_[i] = (int16_t)mix;
      }
      previousTail_.clear();
    }
    size_t overlapSamples =
        static_cast<size_t>((cfg_.overlapMs / 1000.0f) * sampleRate_);
    if (overlapSamples > 8 && overlapSamples < numSamples) {
      // Bound the capture to the samples actually written for THIS phoneme
      // (numSamples), not audioBuffer_.end() -- audioBuffer_ never shrinks,
      // so its size() can exceed numSamples whenever an earlier phoneme was
      // longer, and audioBuffer_.end() would then point past what was just
      // written, capturing stale leftover data as the crossfade tail.
      previousTail_.assign(audioBuffer_.begin() + (numSamples - overlapSamples),
                           audioBuffer_.begin() + numSamples);
    }
  }

  /// If diphthong, set second target formants and mark dynamic flag
  void setupDiphthongTargets(const std::string& phonStr, FormantParams& target2,
                             bool& dynamic) {
    const PhonemeRule* r = findPhonemeRule(phonStr);
    if (r && r->hasDiphthong) {  
      target2.f1 = r->dF1;
      target2.f2 = r->dF2;
      target2.f3 = r->dF3;
      dynamic = true;
    }
  }

  /// Apply energy gain for stressed vowels and decay over time
  void applyStressEnergyBoost(size_t numSamples, int stress) {
    if (stress == 1 ||
        stress == 2) {  // formerly multiline if inside synthesizePhoneme
      float g = (stress == 1) ? cfg_.stressEnergyBoostPrimary
                              : cfg_.stressEnergyBoostSecondary;
      for (size_t i = 0; i < numSamples; ++i) {
        int32_t v = (int32_t)(audioBuffer_[i] * g);
        if (v > 32767)
          v = 32767;
        else if (v < -32768)
          v = -32768;
        audioBuffer_[i] = (int16_t)v;
      }
    }
  }

  /// Populate synthesis context (rules lookup, envelopes, flags, targets)
  void preparePhonemeSynthesis(const std::string& phoneme,
                               PhonemeSynthesisContext& ctx,
                               const PhonemeSynthesisParams& params) {
    ctx.phonStr = stripStress(phoneme, ctx.stress);
    uint16_t defaultDurationMs =
        static_cast<uint16_t>(getPhoneDuration(ctx.phonStr) * 1000.0f + 0.5f);
    // cfg_.speedScale is this voice's own default rate; a caller's own
    // params.speed still composes with (multiplies) it rather than being
    // overridden by it.
    PhonemeSynthesisParams effectiveParams = params;
    effectiveParams.speed *= cfg_.speedScale;
    ctx.duration = resolveDuration(defaultDurationMs, effectiveParams) / 1000.0f;
    ctx.numSamples = (size_t)(ctx.duration * sampleRate_);
    if (ctx.numSamples > audioBuffer_.size())
      audioBuffer_.resize(ctx.numSamples);
    ctx.params = getFormantParams(ctx.phonStr);
    // SAM-style "mouth"/"throat" scaling: a uniform F1/F2 multiplier across
    // every phoneme, layered on top of FormantRules.h's own per-phoneme
    // values (see FormantVoiceConfig::mouthScale/throatScale doc).
    ctx.params.f1 *= cfg_.mouthScale;
    ctx.params.f2 *= cfg_.throatScale;
    ctx.target2 = ctx.params;
    ctx.dynamic = false;
    setupDiphthongTargets(ctx.phonStr, ctx.target2, ctx.dynamic);
    // Diphthong targets are looked up as their own independent
    // FormantParams (not derived from ctx.params), so they need the same
    // mouth/throat scaling applied separately.
    if (ctx.dynamic) {
      ctx.target2.f1 *= cfg_.mouthScale;
      ctx.target2.f2 *= cfg_.throatScale;
    }
    randomizeFormantsIfNeeded(ctx.params, ctx.phonStr);
    configureInitialFilters(ctx.params);
    // Snapshot where the previous phoneme's formants ended up (see the
    // coarticulation comment on PhonemeSynthesisContext), and size an
    // attack window to glide from there to this phoneme's own target: up
    // to ~30ms, capped at 30% of this phoneme's own duration so very short
    // stops don't spend their whole duration transitioning.
    ctx.entryF1 = lastF1_;
    ctx.entryF2 = lastF2_;
    ctx.entryF3 = lastF3_;
    float attackSec = 0.03f < ctx.duration * 0.3f ? 0.03f : ctx.duration * 0.3f;
    ctx.attackSamples = (size_t)(attackSec * sampleRate_);
    if (ctx.attackSamples < 1) ctx.attackSamples = 1;
    ctx.flags = classifyPhoneme(ctx.phonStr);
    ctx.f0 = params.pitchHz > 0.0f ? params.pitchHz : voiceF0_;
    if (ctx.params.f1 < 350.0f)
      ctx.f0 *= 1.05f;  // slight boost for closed vowels
    ctx.stressPitchRise = computeStressPitchRise(ctx.stress);
    ctx.voicing = params.voicing;
  }

  /// Prepare silence buffer and write it out if context indicates silence
  bool handleSilenceSetup(PhonemeSynthesisContext& ctx, ::Print& out) {
    if (!ctx.flags.silence) return false;

    size_t numSamples = ctx.numSamples;
    if (numSamples == 0) return true;

    // Determine overlap samples (mirror logic from applyCrossfade)
    size_t overlapSamples = static_cast<size_t>((cfg_.overlapMs / 1000.0f) * sampleRate_);
    if (overlapSamples > 8 && overlapSamples < numSamples) {
      // we'll set previousTail_ at end to zeros of this size
    } else {
      overlapSamples = 0; // treat as no tail storage
    }

    // 1. If there is a previous tail, perform crossfade (fade it out to zero)
    if (!previousTail_.empty()) {
      size_t overlap = std::min(previousTail_.size(), numSamples);
      if (overlap > 0) {
        // Create a small temporary buffer just for the overlap mix
        std::vector<int16_t> temp(overlap);
        for (size_t i = 0; i < overlap; ++i) {
          float w = (float)i / (float)(overlap - 1);
            // previous contribution fades out; silence fades in (0)
          float a = previousTail_[i] * (1.0f - w);
          int32_t v = (int32_t)a;
          if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
          temp[i] = (int16_t)v;
        }
        out.write(reinterpret_cast<const uint8_t*>(temp.data()), overlap * sizeof(int16_t));
        numSamples -= overlap;
      }
      previousTail_.clear();
    }

    // 2. Stream the remaining silence as zero chunks without allocating a giant buffer
    static int16_t zeroChunk[256] = {0};
    while (numSamples > 0) {
      size_t n = (numSamples > 256) ? 256 : numSamples;
      out.write(reinterpret_cast<const uint8_t*>(zeroChunk), n * sizeof(int16_t));
      numSamples -= n;
    }

    // 3. Prepare a zero tail for next phoneme if overlap policy applies
    if (overlapSamples > 0) {
      previousTail_.assign(overlapSamples, 0);
    }
    return true;
  }

  /// Return true if phoneme is voiced
  bool isVoicedPhoneme(const std::string& phoneme) {
    const PhonemeRule* r = findPhonemeRule(phoneme);
    return r ? ((r->flags & PF_VOICED) != 0) : false;
  }

  /// Get base duration (seconds) for phoneme
  float getPhoneDuration(const std::string& phoneme) {
    const PhonemeRule* r = findPhonemeRule(phoneme);
    return r ? (r->duration_ms / 1000.0f) : 0.10f;
  }

  /// Lookup formant parameters (fallback defaults if missing)
  FormantParams getFormantParams(const std::string& phoneme) {
    const PhonemeRule* r = findPhonemeRule(phoneme);
    if (r) return r->params;
    return {500.0f, 1500.0f, 2500.0f, 0.5f, 0.4f, 0.3f, 100.0f, 150.0f, 200.0f};
  }

  /// Generate natural speech amplitude envelope (vowel vs consonant)
  /// Natural speech envelope (different shape for vowels vs consonants)
  float generateEnvelope(float t, float duration) {
    float progress = t / duration;

    // Different envelopes for different phoneme types
    if (duration < 0.08f) {
      // Short phonemes (consonants) - quick attack/decay
      if (progress < 0.3f) {
        return progress / 0.3f;  // Quick attack
      } else if (progress > 0.7f) {
        return (1.0f - progress) / 0.3f;  // Quick decay
      } else {
        return 1.0f;  // Brief sustain
      }
    } else {
      // Longer phonemes (vowels) - more gradual envelope
      float attack = 0.15f;
      float release = 0.25f;

      if (progress < attack) {
        // Smooth attack with exponential curve
        float attack_progress = progress / attack;
        return 1.0f - expf(-5.0f * attack_progress);
      } else if (progress > (1.0f - release)) {
        // Smooth release with exponential decay
        float release_progress = (progress - (1.0f - release)) / release;
        return expf(-3.0f * release_progress);
      } else {
        // Sustain with slight amplitude variation
        float sustain_variation = 1.0f + 0.05f * sinf(2.0f * (float)M_PI * t * 4.0f);
        return 0.95f * sustain_variation;
      }
    }
  }
};
