/**
 * @file FallbackPhonemeDictionary.h
 * @brief Tries one PhonemeDictionaryBase, then a second on miss.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstddef>
#include <string>

#include "PhonemeDictionaryBase.h"

/**
 * @brief Composes two PhonemeDictionaryBase-implementing dictionaries into
 * one: `lookup()` tries `primary` first, then `secondary` if `primary`
 * doesn't have the word.
 * @details Typical use: pair a compile-time, flash-resident
 * CompactPhonemeDictionary (built from PH_WORD()/Seg() entries -- see
 * CompactPhonemeDictionaryBuilder.h) with a small runtime
 * RuntimePhonemeDictionary (see RuntimePhonemeDictionary.h) holding a
 * handful of words authored via the extended phoneme-string syntax, so
 * both authoring styles resolve through a single dictionary passed to
 * G2PDictionaryModel::useCompactDictionary(). Neither operand is copied --
 * both must outlive this object.
 */
class FallbackPhonemeDictionary : public PhonemeDictionaryBase {
 public:
  FallbackPhonemeDictionary(const PhonemeDictionaryBase& primary,
                            const PhonemeDictionaryBase& secondary)
      : primary_(primary), secondary_(secondary) {}

  /// Sum of both dictionaries' sizes -- an upper bound, not deduplicated,
  /// if the same word happens to appear in both (primary always wins the
  /// actual lookup in that case).
  size_t size() const override { return primary_.size() + secondary_.size(); }

  bool lookup(const std::string& word, std::string& outPhonemes) const override {
    return primary_.lookup(word, outPhonemes) || secondary_.lookup(word, outPhonemes);
  }

  using PhonemeDictionaryBase::lookup;  // bring in the single-arg convenience overload

 private:
  const PhonemeDictionaryBase& primary_;
  const PhonemeDictionaryBase& secondary_;
};
