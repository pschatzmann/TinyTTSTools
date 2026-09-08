/**
 * @file PhonemeDictionaryBase.h
 * @brief Common interface for word->phoneme lookup dictionaries
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-07
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstddef>
#include <string>

/**
 * @brief Common interface implemented by CompactPhonemeDictionary and
 * CompressedPhonemeDictionary, so callers (e.g. G2PDictionaryModel) can be
 * pointed at either without caring which storage format is behind it.
 */
class PhonemeDictionaryBase {
 public:
  virtual ~PhonemeDictionaryBase() = default;
  virtual size_t size() const = 0;
  virtual bool lookup(const std::string& word, std::string& outPhonemes) const = 0;

  std::string lookup(const std::string& word) const {
    std::string result;
    lookup(word, result);
    return result;
  }
};
