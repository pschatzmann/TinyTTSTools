/**
 * @file Phonemes.h
 * @brief Phoneme information for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-13
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

#include "TTSTypes.h"

/**
 * @brief Sequential ARPAbet phoneme ids as a byte-sized enum.
 * @details Mirrors PhonemeInfo::id / Phonemes::getPhonemeById() in the
 * phoneme_map table below one-to-one, so a phoneme can be referred to
 * symbolically (e.g. `Phone::AH0`) instead of by a raw index or string.
 * `static_cast<uint8_t>(Phone::X)` matches `getPhonemeID(PhonemeType::ARPAbet, "X")`,
 * and `getPhonemeById(static_cast<uint8_t>(Phone::X))` returns that entry.
 */
enum class Phone : uint8_t {
  SIL = 0,
  SP = 1,

  AA = 2,
  AE = 3,
  AH = 4,
  AH0 = 5,
  AO = 6,
  AW = 7,
  AY = 8,
  EH = 9,
  ER = 10,
  ER0 = 11,
  EY = 12,
  IH = 13,
  IY = 14,
  OW = 15,
  OY = 16,
  UH = 17,
  UW = 18,

  B = 19,
  D = 20,
  G = 21,
  K = 22,
  P = 23,
  T = 24,

  DH = 25,
  F = 26,
  HH = 27,
  S = 28,
  SH = 29,
  TH = 30,
  V = 31,
  Z = 32,
  ZH = 33,

  CH = 34,
  JH = 35,

  L = 36,
  R = 37,
  W = 38,
  Y = 39,

  M = 40,
  N = 41,
  NG = 42,

  // Stressed vowel variants (primary/secondary), for dictionary authoring
  // (see PH_WORD() in CompactPhonemeDictionaryBuilder.h) -- these are NOT
  // phoneme_map/getPhonemeById() indices like the values above. Only AH and
  // ER don't need their own "0" member here: that slot is already the
  // dedicated AH0/ER0 id above (a genuinely different, reduced-vowel
  // timbre, not just "AH/ER with no stress").
  AA1 = 43, AA2 = 44,
  AE1 = 45, AE2 = 46,
  AH1 = 47, AH2 = 48,
  AO1 = 49, AO2 = 50,
  AW1 = 51, AW2 = 52,
  AY1 = 53, AY2 = 54,
  EH1 = 55, EH2 = 56,
  ER1 = 57, ER2 = 58,
  EY1 = 59, EY2 = 60,
  IH1 = 61, IH2 = 62,
  IY1 = 63, IY2 = 64,
  OW1 = 65, OW2 = 66,
  OY1 = 67, OY2 = 68,
  UH1 = 69, UH2 = 70,
  UW1 = 71, UW2 = 72,
};

/**
 * @brief Phoneme representation translator and timing information
 * @details Translates phonemes between different representation formats:
 *          ARPAbet, IPA (International Phonetic Alphabet), and X-SAMPA.
 *          Also provides default duration information for each phoneme to
 *          assist with speech synthesis timing.
 *
 *          This is useful when working with different speech synthesis
 *          systems or datasets that use different phoneme notations, and
 *          when you need realistic timing for phoneme playback.
 */
class Phonemes {
 public:
  /**
   * @brief Get the complete PhonemeInfo entry for a phoneme
   * @param phonemeType The phoneme representation format to search by
   * @param phoneme The phoneme to find
   * @return Pointer to PhonemeInfo entry, or nullptr if not found
   *
   * @example
   * // Get complete phoneme info for ARPAbet "AA"
   * const PhonemeInfo* entry = phonemes.getPhoneme(PhonemeType::ARPAbet, "AA");
   * if (entry) {
   *     const char* ipa = entry->ipa;        // "ɑ"
   *     uint16_t duration = entry->duration_ms; // 150
   * }
   */
  const PhonemeInfo* getPhoneme(PhonemeType phonemeType,
                                const std::string& phoneme) const {
    for (const auto& entry : phoneme_map) {
      const char* targetPhoneme = nullptr;

      // Get the phoneme representation from the mapping entry
      switch (phonemeType) {
        case PhonemeType::ARPAbet:
          targetPhoneme = entry.arpabet;
          break;
        case PhonemeType::IPA:
          targetPhoneme = entry.ipa;
          break;
        case PhonemeType::XSAMPA:
          targetPhoneme = entry.xsampa;
          break;
      }

      // Check if we found a match
      if (targetPhoneme && targetPhoneme == phoneme) {
        return &entry;
      }
    }
    return nullptr;  // Phoneme not found
  }

  /**
   * @brief True if the given phoneme is a silence/pause.
   * @param phonemeType Representation of the input symbol.
   * @param phoneme Phoneme symbol (may be null).
   * @return true if mapped to SIL/SP or if symbol is unknown; otherwise false.
   * @note Unknown symbols default to silence to avoid unintended audio.
   */
  bool isSilence(PhonemeType phonemeType, const char* phoneme) const {
    const PhonemeInfo* entry = getPhoneme(phonemeType, phoneme);
    if (entry == nullptr) return true;
    return entry->category == PhonemeClass::Silence;
  }

  /**
   * @brief Translate a single phoneme between representation formats
   * @param sourceType Source phoneme representation format
   * @param phoneme The phoneme to translate
   * @param targetType Target phoneme representation format
   * @return Translated phoneme string, or nullptr if not found
   *
   * @example
   * // Translate ARPAbet "AA" to IPA
   * const char* ipa = translator.translatePhoneme(PhonemeType::ARPAbet, "AA",
   * PhonemeType::IPA);
   * // Result: "ɑ"
   */
  const char* translatePhoneme(PhonemeType sourceType,
                               const std::string& phoneme,
                               PhonemeType targetType) const {
    // Use getPhoneme to find the entry
    const PhonemeInfo* entry = getPhoneme(sourceType, phoneme);
    if (!entry) {
      return nullptr;  // Phoneme not found in mapping table
    }

    // Return the target representation
    switch (targetType) {
      case PhonemeType::ARPAbet:
        return entry->arpabet;
      case PhonemeType::IPA:
        return entry->ipa;
      case PhonemeType::XSAMPA:
        return entry->xsampa;
    }
    return nullptr;
  }

  /**
   * @brief Translate a space-separated sequence of phonemes
   * @param sourceType Source phoneme representation format
   * @param phonemes Space-separated string of phonemes to translate
   * @param targetType Target phoneme representation format
   * @return Space-separated string of translated phonemes
   *
   * @details This method translates a sequence of phonemes from one
   representation
   *          to another. Each phoneme in the sequence can also be used with
   *          getPhonemeDuration() to get timing information for speech
   synthesis.
   *
   * @example
   * // Translate ARPAbet sequence to X-SAMPA
   * std::string xsampa = translator.translatePhonemes(
   *     PhonemeType::ARPAbet, "HH EH L OW", PhonemeType::XSAMPA);
   * // Result: "h E l oU"
   *
   * @example
   * // Get both translation and timing information
   * Phonemes phonemes;
   * std::string input = "HH EH L OW";
   * std::string translated = phonemes.translatePhonemes(
   *     PhonemeType::ARPAbet, input, PhonemeType::IPA);
   *
   * // Calculate total duration for the phoneme sequence
   * uint32_t totalDuration = 0;
    if (id >= 43) {
   * std::string phoneme;
   * while (iss >> phoneme) {
   *     totalDuration += phonemes.getPhonemeDuration(PhonemeType::ARPAbet,
   phoneme);
   * }
   * // totalDuration now contains the sum of all phoneme durations in ms
   */
  std::string translatePhonemes(PhonemeType sourceType,
                                const std::string& phonemes,
                                PhonemeType targetType) const {
    std::string result;
    std::string currentPhoneme;
    bool firstPhoneme = true;

    // Parse the input string character by character
    for (size_t i = 0; i <= phonemes.length(); ++i) {
      // Check if we've reached a space or the end of string
      if (i == phonemes.length() || phonemes[i] == ' ') {
        if (!currentPhoneme.empty()) {
          // Translate the current phoneme
          const char* translatedPhoneme =
              translatePhoneme(sourceType, currentPhoneme, targetType);
          if (translatedPhoneme) {
            // Add space separator between phonemes (except for the first one)
            if (!firstPhoneme) {
              result += " ";
            }
            result += translatedPhoneme;
            firstPhoneme = false;
          }
          // Reset for next phoneme
          currentPhoneme.clear();
        }
      } else {
        // Build up the current phoneme
        currentPhoneme += phonemes[i];
      }
    }

    return result;
  }

  std::vector<const char*> getPhonemes(PhonemeType type) const {
    std::vector<const char*> phonemes;
    for (const auto& entry : phoneme_map) {
      switch (type) {
        case PhonemeType::ARPAbet:
          phonemes.push_back(entry.arpabet);
          break;
        case PhonemeType::IPA:
          phonemes.push_back(entry.ipa);
          break;
        case PhonemeType::XSAMPA:
          phonemes.push_back(entry.xsampa);
          break;
      }
    }
    return phonemes;
  }

  /**
   * @brief Get the default duration for a phoneme
   * @param phonemeType The phoneme representation format
   * @param phoneme The phoneme to get duration for
   * @return Duration in milliseconds, or 100ms default if not found
   *
   * @example
   * // Get duration for ARPAbet "AA"
   * uint16_t duration = translator.getPhonemeDuration(PhonemeType::ARPAbet,
   * "AA");
   * // Result: 150 (ms)
   */
  uint16_t getPhonemeDuration(PhonemeType phonemeType,
                              const std::string& phoneme) const {
    const PhonemeInfo* entry = getPhoneme(phonemeType, phoneme);
    return entry ? entry->duration_ms
                 : 100;  // Default duration if phoneme not found
  }

  /**
   * @brief Get the phoneme id for a phoneme
   * @param phonemeType The phoneme representation format to search by
   * @param phoneme The phoneme to find
   * @return Pointer to PhonemeInfo entry, or nullptr if not found
   */
  const uint16_t getPhonemeID(PhonemeType phonemeType,
                              const std::string& phoneme) const {
    const PhonemeInfo* entry = getPhoneme(phonemeType, phoneme);
    return entry ? entry->id : 0;
  }

  /**
   * @brief Get the complete PhonemeInfo entry by sequential ID
   * @param id The sequential ID of the phoneme (0-42)
   * @return Pointer to PhonemeInfo entry, or nullptr if ID is out of range
   *
   * @example
   * // Get phoneme info for ID 0 (SIL silence phoneme)
   * const PhonemeInfo* entry = phonemes.getPhonemeById(0);
   * if (entry) {
   *     const char* arpabet = entry->arpabet;    // "SIL"
   *     const char* ipa = entry->ipa;            // "∅"
   *     uint16_t duration = entry->duration_ms;  // 400
   *     uint16_t id = entry->id;                 // 0
   * }
   */
  const PhonemeInfo* getPhonemeById(uint16_t id) const {
    if (id >= 43) {
      return nullptr;  // ID out of range -- table has 43 entries (0-42, NG last)
    }
    return &phoneme_map[id];
  }

  /**
   * @brief Convert a Phone enum value to its ARPAbet string form
   * @param phone Phoneme enum value, e.g. Phone::AA or a stressed authoring
   *        variant like Phone::IH1 (see Phone's declaration)
   * @return ARPAbet symbol (e.g. "AA", "IH1"), or empty string if invalid
   * @details Ids 0-42 map 1:1 to phoneme_map via getPhonemeById(). Ids
   *          43-72 are stress-authoring variants (Phone::AA1, Phone::AA2,
   *          ...) with no phoneme_map entry of their own -- they resolve to
   *          the base vowel's ARPAbet symbol with the stress digit appended
   *          (e.g. Phone::IH1 -> "IH1"), matching the stress-suffix
   *          convention stripStressMarker() parses elsewhere.
   */
  std::string toArpabetString(Phone phone) const {
    uint8_t id = static_cast<uint8_t>(phone);
    if (id < 43) {
      const PhonemeInfo* info = getPhonemeById(id);
      return info ? std::string(info->arpabet) : std::string();
    }
    if (id > 72) return std::string();
    // Stress-variant ids: 15 vowels x 2 (primary/secondary), in the same
    // order as declared in Phone (43=AA1, 44=AA2, 45=AE1, ..., 72=UW2).
    static const char* const kStressVowels[15] = {
        "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER",
        "EY", "IH", "IY", "OW", "OY", "UH", "UW"};
    size_t offset = id - 43;
    size_t vowelIndex = offset / 2;
    int stress = static_cast<int>(offset % 2) + 1;  // 1 or 2
    return std::string(kStressVowels[vowelIndex]) + static_cast<char>('0' + stress);
  }

 protected:
  /**
   * @brief Static mapping table for phoneme representations
   * @details Contains 43 entries (41 common English phonemes plus SIL/SP
   *          silence markers) with their equivalents across ARPAbet, IPA,
   *          and X-SAMPA formats, plus default durations and sequential
   *          IDs for indexing.
   *
   * @note This table covers the most common English phonemes used in
   *       speech synthesis. Additional phonemes can be added as needed.
   *
   * Format examples:
   * - ARPAbet: Two-letter codes like "AA", "B", "CH"
   * - IPA: Unicode phonetic symbols like "ɑ", "b", "tʃ"
   * - X-SAMPA: ASCII approximations like "A", "b", "tS"
   * - Duration: Typical phoneme length in milliseconds for natural speech
   * - ID: Sequential identifier from 0-42 for direct array access
   *
   * Duration guidelines:
   * - Silence: 400ms full pause (SIL), 30ms short pause (SP, between words)
   * - Vowels: 120-180ms (longer for diphthongs)
   * - Stops: 60-80ms (short burst sounds)
   * - Fricatives: 90-130ms (sustained sounds)
   * - Nasals/Liquids: 90-120ms (resonant sounds)
   * - Unstressed vowels: 80-90ms (reduced duration)
   */
  const PhonemeInfo phoneme_map[43] = {
      // Silence/Pause
      {"SIL", "∅", "_", 400, PhonemeClass::Silence, 0},  // silence, pause
      {"SP", "∅", "_", 30, PhonemeClass::Silence, 1},    // short pause

      // Vowels (longer duration: 120-180ms)
      {"AA", "ɑ", "A", 150, PhonemeClass::Phoneme, 2},    // father, hot
      {"AE", "æ", "{", 130, PhonemeClass::Phoneme, 3},    // cat, hat
      {"AH", "ʌ", "V", 120, PhonemeClass::Phoneme, 4},    // cut, but
      {"AH0", "ə", "@", 80, PhonemeClass::Phoneme, 5},    // schwa (unstressed)
      {"AO", "ɔ", "O", 160, PhonemeClass::Phoneme, 6},    // caught, saw
      {"AW", "aʊ", "aU", 180, PhonemeClass::Phoneme, 7},  // how, now
      {"AY", "aɪ", "aI", 180, PhonemeClass::Phoneme, 8},  // my, eye
      {"EH", "ɛ", "E", 130, PhonemeClass::Phoneme, 9},    // bet, red
      {"ER", "ɝ", "3\\", 140, PhonemeClass::Phoneme, 10},  // bird, hurt (stressed)
      {"ER0", "ɚ", "@\\", 90, PhonemeClass::Phoneme, 11},  // butter (unstressed)
      {"EY", "eɪ", "eI", 170, PhonemeClass::Phoneme, 12}, // bay, say
      {"IH", "ɪ", "I", 110, PhonemeClass::Phoneme, 13},   // bit, hit
      {"IY", "i", "i", 140, PhonemeClass::Phoneme, 14},   // beat, see
      {"OW", "oʊ", "oU", 170, PhonemeClass::Phoneme, 15}, // boat, show
      {"OY", "ɔɪ", "OI", 180, PhonemeClass::Phoneme, 16}, // boy, toy
      {"UH", "ʊ", "U", 120, PhonemeClass::Phoneme, 17},   // book, good
      {"UW", "u", "u", 150, PhonemeClass::Phoneme, 18},   // boot, two

      // Stops (short: 60-80ms)
      {"B", "b", "b", 70, PhonemeClass::Phoneme, 19},  // big, job
      {"D", "d", "d", 60, PhonemeClass::Phoneme, 20},  // dog, bad
      {"G", "ɡ", "g", 70, PhonemeClass::Phoneme, 21},  // go, big
      {"K", "k", "k", 80, PhonemeClass::Phoneme, 22},  // cat, back
      {"P", "p", "p", 80, PhonemeClass::Phoneme, 23},  // put, cup
      {"T", "t", "t", 70, PhonemeClass::Phoneme, 24},  // tea, cat

      // Fricatives (longer: 90-130ms)
      {"DH", "ð", "D", 100, PhonemeClass::Phoneme, 25}, // this, mother
      {"F", "f", "f", 120, PhonemeClass::Phoneme, 26},  // fish, if
      {"HH", "h", "h", 90, PhonemeClass::Phoneme, 27},  // house, ahead
      {"S", "s", "s", 130, PhonemeClass::Phoneme, 28},  // see, yes
      {"SH", "ʃ", "S", 120, PhonemeClass::Phoneme, 29},  // she, fish
      {"TH", "θ", "T", 110, PhonemeClass::Phoneme, 30},  // think, math
      {"V", "v", "v", 100, PhonemeClass::Phoneme, 31},  // very, have
      {"Z", "z", "z", 110, PhonemeClass::Phoneme, 32},  // zero, his
      {"ZH", "ʒ", "Z", 110, PhonemeClass::Phoneme, 33}, // measure, vision

      // Affricates (medium: 100-120ms)
      {"CH", "tʃ", "tS", 120, PhonemeClass::Phoneme, 34}, // chair, match
      {"JH", "dʒ", "dZ", 110, PhonemeClass::Phoneme, 35}, // jump, bridge

      // Liquids and Glides (medium: 80-110ms)
      {"L", "l", "l", 100, PhonemeClass::Phoneme, 36},  // like, bell
      {"R", "ɹ", "r\\", 90, PhonemeClass::Phoneme, 37}, // red, car
      {"W", "w", "w", 80, PhonemeClass::Phoneme, 38},   // we, away
      {"Y", "j", "j", 70, PhonemeClass::Phoneme, 39},   // yes, you

      // Nasals (medium: 90-120ms)
      {"M", "m", "m", 100, PhonemeClass::Phoneme, 40},  // man, home
      {"N", "n", "n", 90, PhonemeClass::Phoneme, 41},   // no, pen
      {"NG", "ŋ", "N", 110, PhonemeClass::Phoneme, 42}, // sing, ring
  };
};
