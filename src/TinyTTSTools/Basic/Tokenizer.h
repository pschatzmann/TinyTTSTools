/**
 * @file Tokenizer.h
 * @brief Text preprocessing and tokenization for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cctype>
#include <string>
#include <vector>

#include "StringUtils.h"

/**
 * @brief Text preprocessing class
 * @details Handles text normalization and tokenization for TTS processing
 */
class Tokenizer {
 public:
  /**
   * @brief Tokenize text into words
   * @param text Input text string
   * @return Vector of word tokens
   * @details Preprocesses text and splits into individual words
   */
  static std::vector<std::string> tokenize(const std::string& text) {
    return StringUtils::split(preprocessText(text), ' ');
  }

 protected:
  /**
   * @brief Preprocess text for TTS synthesis
   * @param text Input text string
   * @return Normalized text ready for phoneme conversion
   * @details Converts to lowercase, handles punctuation, and normalizes
   * whitespace
   */
  static std::string preprocessText(const std::string& text) {
    std::string result = StringUtils::toLowerCase(text);

    // Remove punctuation and normalize
    std::string normalized;
    for (char c : result) {
      if (std::isalpha(c) || std::isspace(c)) {
        normalized += c;
      } else if (c == '.' || c == '!' || c == '?') {
        normalized += " . ";  // Convert sentence endings to pauses
      }
    }

    return StringUtils::trim(normalized);
  }
};
