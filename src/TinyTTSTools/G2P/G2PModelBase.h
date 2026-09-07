/**
 * @file G2PModelBase.h
 * @brief Base class for Grapheme-to-Phoneme conversion models
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <string>
#include "../Basic/TTSTypes.h"

/**
 * @brief Base class for Grapheme-to-Phoneme conversion models
 * @details Abstract base class defining the interface for all G2P models
 */
class G2PModelBase {
 public:
  /**
   * @brief Virtual destructor
   */
  virtual ~G2PModelBase() = default;

  /**
   * @brief Convert word to phonemes
   * @param word Input word (will be converted to lowercase)
   * @return Space-separated phoneme string
   */
  virtual std::string wordToPhonemes(const std::string& word) = 0;

  /**
   * @brief Check if model/data is loaded and ready
   * @return true if model is ready for inference
   */
  virtual operator bool() const { return initialized_; }
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

 protected:
  AudioDataCallback audioCallback_ =
      nullptr;  ///< Audio output callback function
  SpeechCompleteCallback completeCallback_ =
      nullptr;                                   ///< Speech completion callback
  SpeechErrorCallback errorCallback_ = nullptr;  ///< Error callback function

  bool initialized_ = false;  ///< Initialization status flag
};
