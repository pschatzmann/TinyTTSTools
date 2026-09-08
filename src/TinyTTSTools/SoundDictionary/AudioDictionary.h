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
   * @brief Get the SoundEntry for a specific phoneme
   * @param phoneme The phoneme name to look up
   * @return Pointer to SoundEntry if found, nullptr otherwise
   */
  virtual SoundEntry* getSoundEntry(const char* phoneme) {
    if (!phonemes_) return nullptr;
    
    for (size_t i = 0; i < numPhonemes_; i++) {
      if (std::strcmp(phonemes_[i].name, phoneme) == 0) {
        return const_cast<SoundEntry*>(&phonemes_[i]);
      }
    }
    return nullptr;
  }

 protected:
  const SoundEntry* phonemes_ = nullptr;  ///< Array of phoneme sound entries
  size_t numPhonemes_ = 0;               ///< Number of phonemes in the array
  int sampleRate_ = 8000;                ///< Sample rate in Hz
  PhonemeType phonemeType_ = PhonemeType::ARPAbet;  ///< Type of phonemes
  int channels_ = 1;                     ///< Number of audio channels
  int bitsPerSample_ = 16;              ///< Number of bits per sample

};
