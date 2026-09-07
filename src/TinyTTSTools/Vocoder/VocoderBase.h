/**
 * @file VocoderBase.h
 * @brief Base class for audio synthesis/vocoder implementations
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../Basic/TTSAudioOutput.h"
#include "../Basic/Phonemes.h"
#include "../Basic/TTSTypes.h"
#include "../Basic/StringUtils.h"


/**
 * @brief Optional per-call synthesis attributes for sayPhoneme()
 * @details All fields are optional overrides of the vocoder's own defaults;
 * a value of 0 (or 1.0 for volume) means "use the vocoder's normal
 * behavior" so existing call sites are unaffected by this struct's
 * addition.
 */
struct PhonemeSynthesisParams {
  float volume = 1.0f;      ///< 1.0 = full/original volume, 0.0 = silent
  uint16_t durationMs = 0;  ///< Requested duration in ms; 0 = automatically determined
  float pitchHz = 0.0f;     ///< Requested pitch/fundamental frequency in Hz; 0 = default voice pitch
  float voicing = 1.0f;     ///< 1.0 = normal/fully voiced, 0.0 = fully unvoiced (whispered);
                            ///< only affects phonemes that are naturally voiced (vowels,
                            ///< voiced consonants) -- naturally unvoiced phonemes are
                            ///< unaffected. Only honored by procedural vocoders
                            ///< (e.g. FormantVocoder); concatenative vocoders play back
                            ///< fixed pre-recorded audio and have no source to devoice.
};

/**
 * @brief Base class for audio synthesis implementations
 * @details Abstract base class defining the interface for all vocoder types
 */
class VocoderBase {
 public:
  /**
   * @brief Virtual destructor
   */
  virtual ~VocoderBase() = default;

  /**
   * @brief Say/synthesize audio for a phoneme with optional synthesis overrides
   * @param phonemeType The phoneme representation type (ARPAbet, IPA, X-SAMPA)
   * @param phoneme Phoneme symbol to synthesize
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @return true if synthesis successful, false otherwise
   */
  virtual bool sayPhoneme(PhonemeType phonemeType, const std::string& phoneme, ::Print &out,
                          const PhonemeSynthesisParams& params) {
    std::string translatedPhoneme = phoneme;

    // Translate phoneme if input type differs from default type
    if (phonemeType != defaultPhonemeType) {
      const char* translated = translator_.translatePhoneme(phonemeType, phoneme, defaultPhonemeType);
      if (translated) {
        translatedPhoneme = translated;
      }
      // If translation fails, use original phoneme and continue
    }

    // Call the implementation-specific synthesis method
    return synthesizePhoneme(translatedPhoneme, out, params);
  }

  /**
   * @brief Say/synthesize audio for a phoneme
   * @param phonemeType The phoneme representation type (ARPAbet, IPA, X-SAMPA)
   * @param phoneme Phoneme symbol to synthesize
   * @param out Output stream for audio data
   * @return true if synthesis successful, false otherwise
   */
  virtual bool sayPhoneme(PhonemeType phonemeType, const std::string& phoneme, ::Print &out) {
    return sayPhoneme(phonemeType, phoneme, out, PhonemeSynthesisParams{});
  }

  /**
   * @brief Say/synthesize audio for a phoneme given as a Phone enum value
   * @param phone Phoneme, e.g. Phone::AA or a stressed variant like Phone::IH1
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @return true if synthesis successful, false otherwise
   * @details Phone always represents ARPAbet symbols (see its declaration
   * in Phonemes.h), so this converts via Phonemes::toArpabetString() and
   * delegates to sayPhoneme(PhonemeType::ARPAbet, ...) -- translation to
   * the vocoder's own defaultPhonemeType (if different) still applies.
   */
  virtual bool sayPhoneme(Phone phone, ::Print &out, const PhonemeSynthesisParams& params) {
    std::string phoneme = translator_.toArpabetString(phone);
    if (phoneme.empty()) return false;
    return sayPhoneme(PhonemeType::ARPAbet, phoneme, out, params);
  }

  /**
   * @brief Say/synthesize audio for a phoneme given as a Phone enum value
   * @param phone Phoneme, e.g. Phone::AA or a stressed variant like Phone::IH1
   * @param out Output stream for audio data
   * @return true if synthesis successful, false otherwise
   */
  virtual bool sayPhoneme(Phone phone, ::Print &out) {
    return sayPhoneme(phone, out, PhonemeSynthesisParams{});
  }

  /**
   * @brief Check if vocoder is ready for synthesis
   * @return true if ready, false otherwise
   */
  virtual bool isReady() const { return true; }

  /**
   * @brief Get vocoder type name
   * @return String identifying the vocoder type
   */
  virtual std::string getType() const = 0;

  /**
   * @brief Get the default phoneme type
   * @return Current default phoneme type
   */
  PhonemeType getDefaultPhonemeType() const { return defaultPhonemeType; }

  /**
   * @brief Set the default phoneme type
   * @param type Phoneme type to set as default
   */
  void setDefaultPhonemeType(PhonemeType type) { defaultPhonemeType = type; }

  protected:
  PhonemeType defaultPhonemeType = PhonemeType::ARPAbet;  ///< Default phoneme type
  Phonemes translator_;  ///< Phoneme translator for converting between representation types

  /**
   * @brief Output silence (zero values) for a specified duration
   * @param sampleRateHz Sample rate in Hz
   * @param durationMs Duration in milliseconds
   * @param out Output stream for audio data
   */
  void outputSilence(int sampleRateHz, uint16_t durationMs, ::Print& out) {
    if (durationMs == 0) return;
    
    // Calculate number of samples needed for the duration
    size_t numSamples = (static_cast<size_t>(durationMs) * sampleRateHz) / 1000;
    
    // Output zero values (16-bit signed PCM silence)
    int16_t silence = 0;
    const uint8_t* silenceBytes = reinterpret_cast<const uint8_t*>(&silence);
    
    for (size_t i = 0; i < numSamples; i++) {
      out.write(silenceBytes, sizeof(int16_t));
    }
  }

  /**
   * @brief Implementation-specific phoneme synthesis (to be overridden by subclasses)
   * @param phoneme Phoneme symbol in the default phoneme type format (may be sequence)
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @return true if synthesis successful, false otherwise
   */
  virtual bool synthesizePhoneme(const std::string& phoneme, ::Print &out,
                                 const PhonemeSynthesisParams& params) {
    // Default implementation uses common sequence processing
    return processPhonemeSequence(phoneme, out, params);
  }

  protected:

  /**
   * @brief Parse phoneme string and process as sequence
   * @param phoneme Phoneme string (may contain multiple space-separated phonemes)
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides, applied to every phoneme in the sequence
   * @return true if synthesis successful, false otherwise
   */
  bool processPhonemeSequence(const std::string& phoneme, ::Print& out,
                              const PhonemeSynthesisParams& params) {
    if (!isReady()) {
      return false;
    }

    // If input contains multiple phonemes (space-separated), process them as a sequence
    std::vector<std::string> phonemes = StringUtils::split(phoneme, ' ');

    if (phonemes.empty()) {
      return false;
    }

    // Process each phoneme individually
    bool success = true;
    for (const std::string& singlePhoneme : phonemes) {
      if (!processSinglePhoneme(singlePhoneme, out, params)) {
        success = false;
      }
    }

    return success;
  }

  /**
   * @brief Process a single phoneme (handles silence detection and duration lookup)
   * @param phoneme Single phoneme symbol
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @return true if synthesis successful, false otherwise
   */
  virtual bool processSinglePhoneme(const std::string& phoneme, ::Print& out,
                                    const PhonemeSynthesisParams& params) {
    // Check if this phoneme is marked as silence
    if (translator_.isSilence(defaultPhonemeType, phoneme.c_str())) {
      // Output silence (zero values) for the indicated duration
      uint16_t durationMs = resolveDuration(
          translator_.getPhonemeDuration(defaultPhonemeType, phoneme), params);
      outputSilence(sampleRate(), durationMs, out);
      return true;
    } else {
      // Delegate to implementation-specific synthesis
      return synthesizePhoneme(phoneme, out, params);
    }
  }

  /**
   * @brief Resolve an explicit duration override, falling back to a computed default
   * @param defaultDurationMs Duration computed by normal lookup rules
   * @param params Synthesis params that may carry an explicit override (0 = no override)
   * @return params.durationMs if non-zero, otherwise defaultDurationMs
   */
  static uint16_t resolveDuration(uint16_t defaultDurationMs, const PhonemeSynthesisParams& params) {
    return params.durationMs > 0 ? params.durationMs : defaultDurationMs;
  }

  /**
   * @brief Get sample rate for silence generation (must be implemented by subclasses)
   * @return Sample rate in Hz
   */
  virtual int sampleRate() const = 0;

  /**
   * @brief Strip stress markers (1, 2) from phoneme and return stress level
   * @param phoneme Input phoneme (may have trailing stress digit)
   * @param stress Output parameter for stress level (0, 1, or 2)
   * @return Phoneme string without stress marker
   */
  std::string stripStressMarker(const std::string& phoneme, int& stress) {
    std::string p = phoneme;
    stress = 0;
    if (!p.empty()) {
      char c = p.back();
      if (c == '1' || c == '2') {
        stress = (c == '1') ? 1 : 2;
        p.pop_back();
      }
    }
    return p;
  }

  /**
   * @brief Get phoneme duration with optional stress adjustment
   * @param phoneme Phoneme symbol
   * @param stress Stress level (0=none, 1=primary, 2=secondary)  
   * @return Duration in milliseconds
   */
  uint16_t getPhonemeWithStressDuration(const std::string& phoneme, int stress = 0) {
    uint16_t baseDuration = translator_.getPhonemeDuration(defaultPhonemeType, phoneme);
    
    // Apply stress duration modification (stressed syllables are typically longer)
    if (stress == 1) {
      return static_cast<uint16_t>(baseDuration * 1.2f);  // 20% longer for primary stress
    } else if (stress == 2) {
      return static_cast<uint16_t>(baseDuration * 1.1f);  // 10% longer for secondary stress
    }
    
    return baseDuration;
  }

};

