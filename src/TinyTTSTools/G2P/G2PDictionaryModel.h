/**
 * @file G2PDictionaryModel.h
 * @brief Dictionary-based G2P model for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstring>
#include <string>
#include "G2PModelBase.h"
#include "../Dictionary/PhonemeDictionaryBase.h"
#include "../Dictionary/PhonemeDictionaryEN.h"
#include "../Basic/StringUtils.h"

/**
 * @brief Dictionary-based G2P model (fastest, smallest memory)
 * @details Uses binary search through the built-in phoneme dictionary
 *          with basic rule-based fallback for unknown words.
 *
 * The built-in default dictionary is a CompactPhonemeDictionary (see
 * CompactPhonemeDictionary.h) -- four flat arrays with no per-entry
 * pointers and 1 byte per phoneme, compiled at build time from the
 * readable table in PhonemeDictionaryEN.h (see
 * CompactPhonemeDictionaryBuilder.h), instead of the old
 * `PhonemeEntry{const char* word, const char* phonemes}` table. A custom
 * dictionary can still be supplied via setPhonemeDictionary() using the
 * simple PhonemeEntry array format, which is more convenient for a sketch
 * defining a handful of custom pronunciations.
 */
class G2PDictionaryModel : public G2PModelBase {
 public:
  /**
   * @brief Constructor
   */
  G2PDictionaryModel() {
    initialized_ = true;
    resetPhonemeDictionary();
  }

  /**
   * @brief Set custom phoneme dictionary
   * @param dictionary Pointer to array of PhonemeEntry structures
   * @param size Number of entries in the dictionary
   * @details Allows using a custom phoneme dictionary instead of the default
   * one. The dictionary should remain valid for the lifetime of the TinyTTSTools
   * object. Dictionary entries should be sorted alphabetically by word for
   * optimal lookup performance.
   */
  void setPhonemeDictionary(PhonemeEntry* dictionary, size_t size, PhonemeType type) {
    if (dictionary != nullptr && size > 0) {
      custom_dictionary_ = dictionary;
      custom_dictionary_size_ = size;
      setDefaultPhonemeType(type);
    }
  }

  /**
   * @brief Get current custom phoneme dictionary
   * @return Pointer to the custom phoneme dictionary, or nullptr if the
   *         built-in default (compact) dictionary is active
   */
  PhonemeEntry* getPhonemeDictionary() { return custom_dictionary_; }

  /**
   * @brief Get current phoneme dictionary size
   * @return Number of entries in the currently active phoneme dictionary
   *         (custom dictionary if set, otherwise the built-in default)
   */
  size_t getPhonemeDictionarySize() {
    return custom_dictionary_ != nullptr ? custom_dictionary_size_
                                          : default_dictionary_->size();
  }

  /**
   * @brief Use any PhonemeDictionaryBase-implementing dictionary as the
   * default lookup table (CompactPhonemeDictionary or
   * CompressedPhonemeDictionary)
   * @param dictionary The dictionary to use; must outlive this object
   * @details Lets you swap in a bigger bundled dataset -- e.g. the full
   * ~123k-word CMU dictionary at
   * TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h -- instead of
   * the small built-in default, without needing the PhonemeEntry format:
   * `g2p.useCompactDictionary(COMPACT_CMUDICT_EN);`
   * Clears any custom PhonemeEntry dictionary previously set.
   */
  void useCompactDictionary(const PhonemeDictionaryBase& dictionary) {
    custom_dictionary_ = nullptr;
    custom_dictionary_size_ = 0;
    default_dictionary_ = &dictionary;
    setDefaultPhonemeType(PhonemeType::ARPAbet);
  }

  /**
   * @brief Reset to default phoneme dictionary
   * @details Restores the built-in default (compact) phoneme dictionary
   */
  void resetPhonemeDictionary() {
    custom_dictionary_ = nullptr;
    custom_dictionary_size_ = 0;
    default_dictionary_ = &COMPACT_PHONEME_DICTIONARY_EN;
    setDefaultPhonemeType(PhonemeType::ARPAbet);
  }

  /**
   * @brief Convert word to phonemes using dictionary lookup
   * @param word Input word
   * @return Space-separated phoneme string
   */
  std::string wordToPhonemes(const std::string& word) override {
    std::string lowerWord = StringUtils::toLowerCase(word);
    if (custom_dictionary_ != nullptr) {
      return customDictionaryLookup(lowerWord);
    }
    return default_dictionary_->lookup(lowerWord);
  }

 protected:
  const PhonemeDictionaryBase* default_dictionary_ =
      &COMPACT_PHONEME_DICTIONARY_EN;              ///< Built-in compact dictionary
  PhonemeEntry* custom_dictionary_ = nullptr;     ///< Optional user-supplied override
  size_t custom_dictionary_size_ = 0;

  /**
   * @brief Binary search over a custom PhonemeEntry dictionary
   * @param word Lowercase word to look up
   * @return Phoneme string if found, empty string if not found
   */
  std::string customDictionaryLookup(const std::string& word) {
    int left = 0;
    int right = static_cast<int>(custom_dictionary_size_) - 1;

    while (left <= right) {
      int mid = left + (right - left) / 2;
      int comparison = strcmp(word.c_str(), custom_dictionary_[mid].word);

      if (comparison == 0) {
        return std::string(custom_dictionary_[mid].phonemes);
      } else if (comparison < 0) {
        right = mid - 1;
      } else {
        left = mid + 1;
      }
    }
    return "";  // Not found
  }
};
