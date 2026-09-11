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

  // ==================== International / IPA extension (43-120) ====================
  // Everything ARPAbet has no symbol for, covering German/French/Spanish
  // plus the rest of the IPA pulmonic/non-pulmonic consonant charts and
  // vowel quadrilateral -- formerly a separate `PhoneIntl` enum, merged
  // here so there is only ever one phoneme id space. These ARE valid
  // PH_WORD()/CompactPhonemeDictionary phonemes (see
  // CompactPhonemeDictionaryBuilder.h's Seg()/cxPackPhone() -- the packed
  // symbol was widened to a uint16_t specifically to fit this id range
  // plus a modifier; see PhonemeDictionaryDE.h/FR.h/ES.h for real
  // examples). They are NOT valid for the separate, Huffman-compressed
  // CompressedPhonemeDictionary pipeline (CompressedPhonemeDictionary.h +
  // PhonemeHuffmanCodes.h) that backs the bundled ~123k-word English CMU
  // dictionary -- that Huffman table is derived from a real English corpus
  // and has no codeword for anything past NG.

  // -- Front rounded vowels (German ü/ö, French u/eu) ----------------------
  UF = 43,   // /y/  - German "über", French "tu"      (long/tense)
  UF0 = 44,  // /ʏ/  - German "hübsch"                 (short/lax variant of UF)
  OF = 45,   // /ø/  - German "schön", French "deux"   (long/tense)
  OE = 46,   // /œ/  - German "können", French "sœur"  (short/lax, open-mid)

  // -- French nasal vowels ---------------------------------------------------
  AN = 47,  // /ɑ̃/ - French "dans", "temps"
  EN = 48,  // /ɛ̃/ - French "vin", "pain"
  ON = 49,  // /ɔ̃/ - French "bon", "nom"
  UN = 50,  // /œ̃/ - French "un" (merging with EN in most modern dialects)

  // -- German dorsal fricatives -----------------------------------------------
  C = 51,  // /ç/  - German ich-Laut ("ich", "München")
  X = 52,  // /x/  - German ach-Laut ("Bach") AND Spanish jota ("jota", "rojo")

  // -- German affricates (beyond ARPAbet's CH=/tʃ/, JH=/dʒ/) -----------------
  TS = 53,  // /ts/ - German "z" ("Zeit"), "tz"
  PF = 54,  // /pf/ - German "pf" ("Pferd", "Apfel")

  // -- Spanish rhotics (ARPAbet's R is an American retroflex approximant,
  //    acoustically wrong for either Spanish rhotic) --------------------------
  RT = 55,  // /ɾ/  - Spanish tap  ("pero", "cara")
  RR = 56,  // /r/  - Spanish trill ("perro", word-initial "rosa")

  // -- Spanish palatals --------------------------------------------------------
  NY = 57,  // /ɲ/  - Spanish "ñ" ("año")
  LY = 58,  // /ʎ/  - traditional Spanish "ll" ("calle"); most dialects have
            //        merged this into /ʝ/ -- reuse Phone::Y where that's
            //        the target accent instead of this symbol

  // -- Pulmonic consonants: bilabial ------------------------------------------
  PHI = 59,   // /ɸ/  voiceless bilabial fricative (Japanese "fu")
  BETA = 60,  // /β/  voiced bilabial fricative (Spanish intervocalic "b/v")
  BR = 61,    // /ʙ/  bilabial trill

  // Labiodental
  MV = 62,  // /ɱ/  labiodental nasal (allophone, e.g. "symphony")
  VV = 63,  // /ʋ/  labiodental approximant (Dutch/Hindi "w")

  // Alveolar / lateral
  LH = 64,  // /ɬ/  voiceless lateral fricative (Welsh "ll")
  LZ = 65,  // /ɮ/  voiced lateral fricative
  LF = 66,  // /ɺ/  lateral flap

  // Retroflex (Hindi, Dravidian languages, Mandarin zh/ch/sh/r)
  TR = 67,  // /ʈ/  voiceless retroflex stop
  DR = 68,  // /ɖ/  voiced retroflex stop
  NR = 69,  // /ɳ/  retroflex nasal
  SR = 70,  // /ʂ/  voiceless retroflex fricative
  ZR = 71,  // /ʐ/  voiced retroflex fricative
  RA = 72,  // /ɻ/  retroflex approximant
  LR = 73,  // /ɭ/  retroflex lateral approximant
  RF = 74,  // /ɽ/  retroflex flap

  // Palatal
  CJ = 75,  // /c/  voiceless palatal stop
  JJ = 76,  // /ɟ/  voiced palatal stop
  JZ = 77,  // /ʝ/  voiced palatal fricative

  // Velar
  GH = 78,  // /ɣ/  voiced velar fricative
  WV = 79,  // /ɰ/  velar approximant
  LL = 80,  // /ʟ/  velar lateral approximant

  // Uvular
  QQ = 81,   // /q/  voiceless uvular stop
  GU = 82,   // /ɢ/  voiced uvular stop
  NU = 83,   // /ɴ/  uvular nasal
  CU = 84,   // /χ/  voiceless uvular fricative
  RU = 85,   // /ʁ/  voiced uvular fricative (French/German "r")
  RT2 = 86,  // /ʀ/  uvular trill

  // Pharyngeal / glottal
  HP = 87,  // /ħ/  voiceless pharyngeal fricative (Arabic ح)
  AP = 88,  // /ʕ/  voiced pharyngeal fricative/approximant (Arabic ع)
  GS = 89,  // /ʔ/  glottal stop (Arabic ء, Hawaiian ʻokina, German
            //      vowel-initial onset)
  HV = 90,  // /ɦ/  voiced glottal fricative (Hindi/Czech "h")

  // Co-articulated approximants
  WH = 91,  // /ʍ/  voiceless labial-velar approximant ("wh" in some
            //      English dialects)
  HU = 92,  // /ɥ/  labial-palatal approximant (French "huit")

  // -- Non-pulmonic consonants -------------------------------------------------
  // Clicks (Zulu, Xhosa, Khoisan languages)
  CLB = 93,  // /ʘ/  bilabial click
  CLD = 94,  // /ǀ/  dental click
  CLA = 95,  // /ǃ/  (post)alveolar click
  CLP = 96,  // /ǂ/  palatoalveolar click
  CLL = 97,  // /ǁ/  lateral click

  // Implosives (Sindhi, Vietnamese, Swahili, Hausa)
  IMB = 98,   // /ɓ/  bilabial implosive
  IMD = 99,   // /ɗ/  dental/alveolar implosive
  IMJ = 100,  // /ʄ/  palatal implosive
  IMG = 101,  // /ɠ/  velar implosive
  IMQ = 102,  // /ʛ/  uvular implosive

  // Ejectives (Amharic, Georgian, Quechua, Hausa) -- glottalized
  // counterparts of existing stops/fricatives
  EJP = 103,  // /pʼ/ ejective bilabial stop
  EJT = 104,  // /tʼ/ ejective alveolar stop
  EJK = 105,  // /kʼ/ ejective velar stop
  EJS = 106,  // /sʼ/ ejective alveolar fricative
  EJC = 107,  // /tʃʼ/ ejective postalveolar affricate

  // -- Vowels (IPA vowel quadrilateral) -----------------------------------------
  IB = 108,   // /ɨ/  close central unrounded (Polish "y", Welsh "u")
  UB = 109,   // /ʉ/  close central rounded (Swedish "u")
  UM = 110,   // /ɯ/  close back unrounded (Japanese "u", Korean "eu")
  EP = 111,   // /e/  close-mid front unrounded, pure monophthong (Spanish/
              //      Italian/Japanese "e", vs. English's diphthongal EY)
  OP = 112,   // /o/  close-mid back rounded, pure monophthong (Spanish/
              //      Italian/Japanese "o", vs. English's diphthongal OW)
  EB = 113,   // /ɘ/  close-mid central unrounded
  OB = 114,   // /ɵ/  close-mid central rounded
  OM = 115,   // /ɤ/  close-mid back unrounded (Vietnamese "ơ")
  EC = 116,   // /ɜ/  open-mid central unrounded, non-rhotic (British "bird",
              //      vs. American English's rhotacized ER)
  AC = 117,   // /ɐ/  near-open central
  AF = 118,   // /a/  open front unrounded (French/Spanish/Italian/German
              //      "a", vs. ARPAbet AA which is open BACK /ɑ/)
  OER = 119,  // /ɶ/  open front rounded (rare: Swedish dialects)
  OB2 = 120,  // /ɒ/  open back rounded (British English "lot")
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
   * @param id The sequential ID of the phoneme (0-42 ARPAbet, 43-120
   *        international/IPA extension -- see Phone's declaration)
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
    if (id >= 121) {
      return nullptr;  // ID out of range -- table has 121 entries (0-120)
    }
    return &phoneme_map[id];
  }

  /**
   * @brief Convert a Phone enum value to its ARPAbet string form
   * @param phone Phoneme enum value, e.g. Phone::AA
   * @return ARPAbet symbol (e.g. "AA"), or empty string if invalid
   * @details Every Phone id maps 1:1 to phoneme_map via getPhonemeById().
   *          Stress is not part of Phone -- author it as a stress-suffixed
   *          ARPAbet string (e.g. "IH1") instead, using the same
   *          stress-suffix convention stripStressMarker() parses elsewhere.
   */
  std::string toArpabetString(Phone phone) const {
    const PhonemeInfo* info = getPhonemeById(static_cast<uint8_t>(phone));
    return info ? std::string(info->arpabet) : std::string();
  }

 protected:
  /**
   * @brief Static mapping table for phoneme representations
   * @details Contains 121 entries: ids 0-42 are the 41 common English
   *          (ARPAbet) phonemes plus SIL/SP silence markers; ids 43-120
   *          are the international/IPA extension (German/French/Spanish
   *          plus the rest of the IPA charts -- see Phone's declaration).
   *          Each entry gives equivalents across ARPAbet, IPA, and X-SAMPA
   *          formats, plus a default duration and sequential id for
   *          indexing.
   *
   * @note Ids 43-120 are NOT valid PH_WORD()/compact-dictionary phonemes
   *       -- see CompactPhonemeDictionaryBuilder.h's cxPackPhone().
   *
   * Format examples:
   * - ARPAbet: Two-letter codes like "AA", "B", "CH"
   * - IPA: Unicode phonetic symbols like "ɑ", "b", "tʃ"
   * - X-SAMPA: ASCII approximations like "A", "b", "tS"
   * - Duration: Typical phoneme length in milliseconds for natural speech
   * - ID: Sequential identifier from 0-120 for direct array access
   *
   * Duration guidelines:
   * - Silence: 400ms full pause (SIL), 30ms short pause (SP, between words)
   * - Vowels: 120-180ms (longer for diphthongs)
   * - Stops: 60-80ms (short burst sounds)
   * - Fricatives: 90-130ms (sustained sounds)
   * - Nasals/Liquids: 90-120ms (resonant sounds)
   * - Unstressed vowels: 80-90ms (reduced duration)
   */
  const PhonemeInfo phoneme_map[121] = {
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

      // ============ International / IPA extension (43-120) ============
      // See the Phone enum's own comment above this range for scope and
      // the PH_WORD()/compact-dictionary caveat. Formant frequencies for
      // the vowels/nasals here are literature-informed male-voice starting
      // points (Peterson & Barney-style charts for German/French/Spanish,
      // nearest-ARPAbet-analogue estimates otherwise) meant to be tuned by
      // ear once rendered, not calibrated data. Durations are coarse
      // defaults by manner class (stops/clicks/ejectives short,
      // fricatives/vowels longer).

      // Front rounded vowels (140-160ms, matching ARPAbet's long-vowel range)
      {"UF", "y", "y", 150, PhonemeClass::Phoneme, 43},
      {"UF0", "ʏ", "Y", 110, PhonemeClass::Phoneme, 44},
      {"OF", "ø", "2", 150, PhonemeClass::Phoneme, 45},
      {"OE", "œ", "9", 140, PhonemeClass::Phoneme, 46},

      // French nasal vowels (170ms -- nasalization runs slightly longer than
      // the oral vowels it's derived from)
      {"AN", "ɑ̃", "A~", 170, PhonemeClass::Phoneme, 47},
      {"EN", "ɛ̃", "E~", 170, PhonemeClass::Phoneme, 48},
      {"ON", "ɔ̃", "O~", 170, PhonemeClass::Phoneme, 49},
      {"UN", "œ̃", "9~", 170, PhonemeClass::Phoneme, 50},

      // German dorsal fricatives (100-110ms, matching ARPAbet's fricative range)
      {"C", "ç", "C", 100, PhonemeClass::Phoneme, 51},
      {"X", "x", "x", 110, PhonemeClass::Phoneme, 52},

      // Affricates (90ms -- shorter than CH/JH since these are single onset
      // bursts rather than a full stop+frication sequence)
      {"TS", "ts", "ts", 90, PhonemeClass::Phoneme, 53},
      {"PF", "pf", "pf", 90, PhonemeClass::Phoneme, 54},

      // Spanish rhotics: a tap is brief (single contact), a trill is longer
      // (multiple contacts) -- durations reflect that directly
      {"RT", "ɾ", "4", 40, PhonemeClass::Phoneme, 55},
      {"RR", "r", "r", 150, PhonemeClass::Phoneme, 56},

      // Spanish palatals (matching ARPAbet's N/L duration range)
      {"NY", "ɲ", "J", 120, PhonemeClass::Phoneme, 57},
      {"LY", "ʎ", "L", 110, PhonemeClass::Phoneme, 58},

      // Bilabial
      {"PHI", "ɸ", "p\\", 100, PhonemeClass::Phoneme, 59},
      {"BETA", "β", "B", 100, PhonemeClass::Phoneme, 60},
      {"BR", "ʙ", "b\\", 120, PhonemeClass::Phoneme, 61},

      // Labiodental
      {"MV", "ɱ", "F", 100, PhonemeClass::Phoneme, 62},
      {"VV", "ʋ", "P", 90, PhonemeClass::Phoneme, 63},

      // Alveolar lateral
      {"LH", "ɬ", "K", 110, PhonemeClass::Phoneme, 64},
      {"LZ", "ɮ", "K\\", 110, PhonemeClass::Phoneme, 65},
      {"LF", "ɺ", "l\\", 60, PhonemeClass::Phoneme, 66},

      // Retroflex
      {"TR", "ʈ", "t`", 80, PhonemeClass::Phoneme, 67},
      {"DR", "ɖ", "d`", 70, PhonemeClass::Phoneme, 68},
      {"NR", "ɳ", "n`", 120, PhonemeClass::Phoneme, 69},
      {"SR", "ʂ", "s`", 120, PhonemeClass::Phoneme, 70},
      {"ZR", "ʐ", "z`", 110, PhonemeClass::Phoneme, 71},
      {"RA", "ɻ", "r`", 90, PhonemeClass::Phoneme, 72},
      {"LR", "ɭ", "l`", 100, PhonemeClass::Phoneme, 73},
      {"RF", "ɽ", "r`", 50, PhonemeClass::Phoneme, 74},

      // Palatal
      {"CJ", "c", "c", 80, PhonemeClass::Phoneme, 75},
      {"JJ", "ɟ", "J\\", 70, PhonemeClass::Phoneme, 76},
      {"JZ", "ʝ", "j\\", 100, PhonemeClass::Phoneme, 77},

      // Velar
      {"GH", "ɣ", "G", 100, PhonemeClass::Phoneme, 78},
      {"WV", "ɰ", "M\\", 90, PhonemeClass::Phoneme, 79},
      {"LL", "ʟ", "L\\", 100, PhonemeClass::Phoneme, 80},

      // Uvular
      {"QQ", "q", "q", 80, PhonemeClass::Phoneme, 81},
      {"GU", "ɢ", "G\\", 70, PhonemeClass::Phoneme, 82},
      {"NU", "ɴ", "N\\", 120, PhonemeClass::Phoneme, 83},
      {"CU", "χ", "X", 110, PhonemeClass::Phoneme, 84},
      {"RU", "ʁ", "R", 100, PhonemeClass::Phoneme, 85},
      {"RT2", "ʀ", "R\\", 150, PhonemeClass::Phoneme, 86},

      // Pharyngeal / glottal
      {"HP", "ħ", "X\\", 110, PhonemeClass::Phoneme, 87},
      {"AP", "ʕ", "?\\", 100, PhonemeClass::Phoneme, 88},
      {"GS", "ʔ", "?", 50, PhonemeClass::Phoneme, 89},
      {"HV", "ɦ", "h\\", 90, PhonemeClass::Phoneme, 90},

      // Co-articulated approximants
      {"WH", "ʍ", "W", 80, PhonemeClass::Phoneme, 91},
      {"HU", "ɥ", "H", 80, PhonemeClass::Phoneme, 92},

      // Clicks
      {"CLB", "ʘ", "O\\", 60, PhonemeClass::Phoneme, 93},
      {"CLD", "ǀ", "|\\", 60, PhonemeClass::Phoneme, 94},
      {"CLA", "ǃ", "!\\", 60, PhonemeClass::Phoneme, 95},
      {"CLP", "ǂ", "=\\", 60, PhonemeClass::Phoneme, 96},
      {"CLL", "ǁ", "|\\|\\", 60, PhonemeClass::Phoneme, 97},

      // Implosives
      {"IMB", "ɓ", "b_<", 80, PhonemeClass::Phoneme, 98},
      {"IMD", "ɗ", "d_<", 80, PhonemeClass::Phoneme, 99},
      {"IMJ", "ʄ", "J\\_<", 80, PhonemeClass::Phoneme, 100},
      {"IMG", "ɠ", "g_<", 80, PhonemeClass::Phoneme, 101},
      {"IMQ", "ʛ", "G\\_<", 80, PhonemeClass::Phoneme, 102},

      // Ejectives
      {"EJP", "pʼ", "p_>", 80, PhonemeClass::Phoneme, 103},
      {"EJT", "tʼ", "t_>", 80, PhonemeClass::Phoneme, 104},
      {"EJK", "kʼ", "k_>", 80, PhonemeClass::Phoneme, 105},
      {"EJS", "sʼ", "s_>", 100, PhonemeClass::Phoneme, 106},
      {"EJC", "tʃʼ", "tS_>", 100, PhonemeClass::Phoneme, 107},

      // Vowels
      {"IB", "ɨ", "1", 130, PhonemeClass::Phoneme, 108},
      {"UB", "ʉ", "}", 130, PhonemeClass::Phoneme, 109},
      {"UM", "ɯ", "M", 130, PhonemeClass::Phoneme, 110},
      {"EP", "e", "e", 130, PhonemeClass::Phoneme, 111},
      {"OP", "o", "o", 130, PhonemeClass::Phoneme, 112},
      {"EB", "ɘ", "@\\", 120, PhonemeClass::Phoneme, 113},
      {"OB", "ɵ", "8", 120, PhonemeClass::Phoneme, 114},
      {"OM", "ɤ", "7", 130, PhonemeClass::Phoneme, 115},
      {"EC", "ɜ", "3", 130, PhonemeClass::Phoneme, 116},
      {"AC", "ɐ", "6", 110, PhonemeClass::Phoneme, 117},
      {"AF", "a", "a", 140, PhonemeClass::Phoneme, 118},
      {"OER", "ɶ", "&", 140, PhonemeClass::Phoneme, 119},
      {"OB2", "ɒ", "Q", 140, PhonemeClass::Phoneme, 120},
  };
};
