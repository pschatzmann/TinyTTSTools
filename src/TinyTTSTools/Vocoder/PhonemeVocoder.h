/**
 * @file PhonemeVocoder.h
 * @brief Phoneme-based speech synthesizer for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-13
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 * 
 * @note This module requires the Arduino Audio Tools library by Phil Schatzmann
 *       for audio decoding functionality. Install via Arduino Library Manager
 *       or from: https://github.com/pschatzmann/arduino-audio-tools
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Dictionary/AudioDictionary.h"
#include "ConcatenatedAudioVocoder.h"
#include "../Dictionary/ArpabetWAVDictionary.h"

/**
 * @brief Phoneme-based speech synthesizer with advanced concatenation
 * @details Generates speech audio using pre-recorded individual phoneme
 * samples from an AudioDictionary with sophisticated concatenation techniques 
 * for natural-sounding output. Each phoneme is synthesized and intelligently 
 * combined with neighboring phonemes using configurable enhancement features.
 * 
 * This class inherits all advanced concatenation capabilities from 
 * ConcatenatedAudioVocoder including phase alignment, cross-fade, fade-out,
 * and coarticulation effects. See ConcatenatedAudioVocoder documentation
 * for detailed feature descriptions and usage examples.
 * 
 * ## Dictionary Integration
 * 
 * PhonemeVocoder uses an AudioDictionary to access pre-recorded phoneme samples:
 * - Supports any AudioDictionary implementation
 * - Automatically handles phoneme type translation
 * - Provides duration lookup for each phoneme
 * - Manages audio sample access for concatenation processing
 * 
 * ## Usage Examples
 *
 * ### Default (recommended) -- just construct and use:
 * ```cpp
 * PhonemeVocoder synth(ArpabetWAVDictionary);
 * ```
 *
 * ### Tuning for tight memory/CPU budgets:
 * ```cpp
 * synth.setCrossFade(false);  // skip the ~128 byte tail buffer + blending cost
 * synth.setBatchSize(128);    // smaller batches for responsiveness
 * ```
 *
 * @note This class depends on the Arduino Audio Tools library for audio decoding.
 *       The AudioDecoder interface is provided by the Audio Tools library.
 */
class PhonemeVocoder : public ConcatenatedAudioVocoder {
 public:
  /**
   * @brief Constructor
   * @param dictionary Audio dictionary containing phoneme audio data
   * @param batchSize Maximum number of samples to write in a single batch (default: 256)
   */
  PhonemeVocoder(AudioDictionary& dictionary, size_t batchSize = 256)
      : ConcatenatedAudioVocoder(batchSize), dictionary_(dictionary) {
  }

  /**
   * @brief Get vocoder type name
   * @return "PhonemeVocoder"
   */
  std::string getType() const override { return "PhonemeVocoder"; }

  /**
   * @brief Check if vocoder is ready for synthesis
   * @return true if dictionary is available, false otherwise
   */
  bool isReady() const override {
    return true; // Dictionary manages its own readiness
  }

  /**
   * @brief Get the sample rate from the dictionary
   * @return Sample rate in Hz
   */
  int sampleRate() const override {
    return dictionary_.sampleRate();
  }


 protected:
  AudioDictionary& dictionary_;  ///< Reference to the audio dictionary

  // No synthesizePhoneme() override here (deliberately): a previous version
  // treated its whole `phoneme` argument as ONE literal dictionary key,
  // which only worked because every caller used to pass exactly one
  // phoneme per call. Once TinyTTSTools::sayPhonemes() was fixed to pass a
  // whole multi-phoneme utterance in a single call (so DiphoneVocoder could
  // form real diphones), that assumption broke: dictionary_.getSoundEntry()
  // was asked to look up e.g. "DH AH SP K W IH K" as a single name, which
  // never matches anything, so every multi-token utterance silently
  // produced zero audio. ConcatenatedAudioVocoder's own synthesizePhoneme()
  // already does the right thing generically -- splits the sequence,
  // handles SIL/SP, provides next-unit lookahead for cross-fade, and
  // flushes the batch at the end (the old override never did any of the
  // last three, even for a single phoneme) -- so inheriting it is strictly
  // better, not just simpler.

  /**
   * @brief Get audio entry by phoneme identifier (implements ConcatenatedAudioVocoder interface)
   * @param identifier Phoneme identifier
   * @return Pointer to SoundEntry or nullptr if not found
   */
  const SoundEntry* getAudioEntry(const std::string& identifier) override {
    return dictionary_.getSoundEntry(identifier.c_str());
  }

  /**
   * @brief Get next audio entry in sequence (implements ConcatenatedAudioVocoder interface)
   * @param currentId Current phoneme identifier
   * @param nextId Next phoneme identifier  
   * @return Pointer to next SoundEntry or nullptr if not found
   */
  const SoundEntry* getNextAudioEntry(const std::string& currentId, const std::string& nextId) override {
    // For phonemes, next entry is simply the next phoneme
    return dictionary_.getSoundEntry(nextId.c_str());
  }
};
