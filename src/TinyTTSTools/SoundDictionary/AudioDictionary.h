/**
 * @file AudioDictionary.h
 * @brief Abstract base class for audio dictionaries in TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-08-23
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 * 
 * This file defines the AudioDictionary interface that provides a common
 * abstraction for different types of audio dictionaries used in speech synthesis.
 * Audio dictionaries are responsible for storing and retrieving phoneme audio data.
 */

#pragma once
#include <cstddef>
#include <cstring>
#include <string>

#include "../Basic/PhonemeModifiers.h"
#include "../Basic/TTSLogger.h"
#include "../Basic/TTSTypes.h"
#include "SoundEntry.h"

/**
 * @brief Base class for audio dictionaries
 * 
 * This class provides a common interface for different types of audio dictionaries
 * that can output PCM audio data for given phonemes or sound entries.
 * Can be used directly with phoneme data arrays or extended for specialized implementations.
 * The phoneme data is typically provided as an array of SoundEntry objects stored in PROGMEM to conserve RAM.
 */
class AudioDictionary {
 public:
  
  /**
   * @brief Default constructor for abstract use
   */
  AudioDictionary() = default;

  /**
   * @brief Constructor with phoneme data
   * @param phonemes Array of SoundEntry objects containing phoneme data (typically stored in PROGMEM)
   * @param numPhonemes Number of phonemes in the array
   * @param sampleRateHz Sample rate in Hz
   * @param phonemeType Type of phonemes (ARPAbet, IPA, etc.)
   * @param channels Number of audio channels (default: 1)
   * @param bitsPerSample Number of bits per sample (default: 16)
   */
  AudioDictionary(const SoundEntry* phonemes, size_t numPhonemes, int sampleRateHz, 
                  PhonemeType phonemeType, int channels = 1, int bitsPerSample = 16)
      : phonemes_(phonemes), numPhonemes_(numPhonemes), sampleRate_(sampleRateHz),
        phonemeType_(phonemeType), channels_(channels), bitsPerSample_(bitsPerSample) {
  }
  
  /**
   * @brief Get the sample rate of the audio data
   * @return Sample rate in Hz
   */
  virtual int sampleRate() const {
    return sampleRate_;
  }

  /**
   * @brief Get the number of audio channels
   * @return Number of audio channels (1 for mono, 2 for stereo, etc.) - default 1
   */
  virtual int channels() const {
    return channels_;
  }

  /**
   * @brief Get the number of bits per sample
   * @return Number of bits per sample (8, 16, 24, 32)
   */
  virtual int bitsPerSample() const {
    return bitsPerSample_;
  }

  /**
   * @brief Get the phoneme type used by this dictionary
   * @return PhonemeType indicating the phoneme format (ARPAbet, IPA, etc.)
   */
  virtual PhonemeType phonemeType() const {
    return phonemeType_;
  }

  /**
   * @brief Get the SoundEntry for a phoneme, falling back to its base
   * (modifier-stripped) symbol if a recording tagged with modifiers isn't
   * available.
   * @param phoneme The phoneme (or diphone, e.g. "AA B") name to look up --
   * plain, or tagged with modifiers like "AF_k" (creaky /a/).
   * @return Pointer to SoundEntry if found (either the exact recording or
   * its modifier-stripped base), nullptr if neither exists.
   * @details Lookup order:
   *   1. Exact match on `phoneme` as given -- lets a target voice ship a
   *      dedicated recording for a specific phoneme+modifier combination
   *      (see PhonemeModifier' own @note on when that's worth doing,
   *      e.g. breathy/creaky voice or the non-pulmonic Phone symbols).
   *   2. If that fails and `phoneme` actually carried modifier tags
   *      (parsePhonemeModifiers(), see PhonemeModifiers.h), retry with the
   *      bare base symbol -- accepting whatever quality loss comes from
   *      using an unmodified recording as a stand-in (the caller is
   *      expected to apply DSP approximation, e.g. PSOLA for length/
   *      stress, on top of it; this class only ever returns unmodified
   *      recorded audio).
   * A miss on step 1 with no modifier tags present (parsing left the
   * string unchanged) skips step 2 entirely -- there's no different key
   * left to try.
   *
   * Diphone dictionaries (DiphoneWAVDictionary.h-style two-phoneme keys,
   * e.g. "AA G") share this same lookup safely: their keys use a SPACE
   * between the two phoneme symbols specifically because no
   * PhonemeModifiers.h tag is or ever will be a bare space (they're all
   * `_`/`:`/`~`/etc. prefixed or suffixed instead) -- so
   * parsePhonemeModifiers() can never mistake a diphone's second half for
   * a modifier tag and corrupt the lookup.
   */
  virtual SoundEntry* getSoundEntry(const char* phoneme) {
    if (!phonemes_) return nullptr;

    for (size_t i = 0; i < numPhonemes_; i++) {
      if (std::strcmp(phonemes_[i].name, phoneme) == 0) {
        return const_cast<SoundEntry*>(&phonemes_[i]);
      }
    }

    PhonemeModifier modifier = PhonemeModifier::MOD_NONE;
    std::string base = parsePhonemeModifiers(phoneme, modifier);
    if (base == phoneme) {
      // No modifier tags were present at all -- the exact-match miss
      // above already covers this key, nothing new to try.
      TTS_LOGE("AudioDictionary: no sound entry for '%s'", phoneme);
      return nullptr;
    }

    TTS_LOGW(
        "AudioDictionary: no recording for '%s', falling back to base '%s' "
        "(modifiers approximated, not recorded)",
        phoneme, base.c_str());
    SoundEntry* fallback = getSoundEntry(base.c_str());
    if (!fallback) {
      TTS_LOGE("AudioDictionary: no sound entry for '%s' or its base '%s'",
                phoneme, base.c_str());
    }
    return fallback;
  }

 protected:
  const SoundEntry* phonemes_ = nullptr;  ///< Array of phoneme sound entries
  size_t numPhonemes_ = 0;               ///< Number of phonemes in the array
  int sampleRate_ = 8000;                ///< Sample rate in Hz
  PhonemeType phonemeType_ = PhonemeType::ARPAbet;  ///< Type of phonemes
  int channels_ = 1;                     ///< Number of audio channels
  int bitsPerSample_ = 16;              ///< Number of bits per sample

};
