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
    for (unsigned char c : result) {
      if (c >= 0x80) {
        // Continuation/lead byte of a multi-byte UTF-8 sequence -- never
        // a valid ASCII letter/space/punctuation on its own, so <cctype>
        // (ASCII-only) can't classify it. Passing it to isalpha()/
        // isspace() as a plain (possibly signed) char is also undefined
        // behavior per the standard (they only accept values representable
        // as unsigned char or EOF). Pass it through untouched instead of
        // running it through <cctype> -- dropping it would corrupt any
        // accented word (see PhonemeDictionaryFR/DE/ES.h, whose entries
        // are keyed by their UTF-8-accented spelling, e.g. "café", "chéri").
        normalized += static_cast<char>(c);
      } else if (std::isalpha(c) || std::isspace(c)) {
        normalized += static_cast<char>(c);
      } else if (c == '.' || c == '!' || c == '?') {
        normalized += " . ";  // Convert sentence endings to pauses
      }
    }

    return StringUtils::trim(normalized);
  }
};
