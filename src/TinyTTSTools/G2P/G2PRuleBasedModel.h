/**
 * @file G2PRuleBasedModel.h
 * @brief Rule-based G2P model for TinyTTSTools
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
#include "../Basic/StringUtils.h"

/**
 * @brief Rule-based G2P model with enhanced linguistic rules
 * @details Uses letter-to-sound rules for English pronunciation without a
 * dictionary or neural network -- a best-effort fallback for words the
 * dictionary doesn't cover. English spelling is highly irregular, so this
 * can never be exact; it's a longest-match rule engine (word-end suffixes,
 * then trigraphs/digraphs, then single letters) covering the common vowel
 * digraphs, silent letters, doubled consonants and suffixes that a naive
 * per-letter mapping misses. Stress is not modeled (no digits are ever
 * emitted) -- callers needing precise stress should prefer a dictionary
 * lookup and treat this purely as a last resort for out-of-vocabulary
 * words.
 *
 * Measured against the full CMU dictionary (~123k words, stress-stripped
 * exact match), the rules alone get ~17% of words exactly right -- English
 * spelling-to-sound is fundamentally ambiguous (vowel reduction to schwa
 * in unstressed syllables can't be predicted from letters alone without a
 * stress model), so this is a hard ceiling for a pure letter-rule engine.
 * To push measured accuracy higher, don't extend this class -- compose it:
 * `G2PDictionaryAndRulesModel` already chains a dictionary lookup before
 * this rule fallback (via G2PHybridModel), so pointing its dictionary at
 * `PHONEME_EXCEPTION_DICTIONARY_EN` (see PhonemeExceptionDictionaryEN_data.h,
 * every CMU word these rules get wrong) via
 * `g2p.getDictionaryModel().useCompactDictionary(PHONEME_EXCEPTION_DICTIONARY_EN)`
 * pushes accuracy over 99% at the cost of ~2.2MB flash, with no change to
 * this class at all.
 */
class G2PRuleBasedModel : public G2PModelBase {
 public:
  G2PRuleBasedModel() { initialized_ = true; }

  std::string wordToPhonemes(const std::string& word) override {
    std::string lowerWord = StringUtils::toLowerCase(word);
    return applyEnhancedG2PRules(lowerWord);
  }

 protected:

  /**
   * @brief One longest-match rule
   * @details If `pattern` occurs at position i (optionally anchored to the
   * end of the word), emit `phonemes` and advance i past it.
   */
  struct Rule {
    const char* pattern;
    const char* phonemes;
    bool wordEndOnly;
  };

  static bool matchesAt(const std::string& word, size_t i, const char* pattern) {
    size_t len = std::strlen(pattern);
    if (i + len > word.length()) return false;
    return word.compare(i, len, pattern) == 0;
  }

  void appendTokens(std::string& result, const char* phonemes) {
    if (phonemes[0] == '\0') return;
    if (!result.empty()) result += ' ';
    result += phonemes;
  }

  /// Word-start silent-letter patterns: emit only the sounded remainder.
  bool tryWordStartRule(const std::string& word, size_t i, std::string& result, size_t& advance) {
    if (i != 0) return false;
    static const Rule rules[] = {
        {"kn", "N", false}, {"gn", "N", false}, {"wr", "R", false},
        {"ps", "S", false}, {"pn", "N", false}, {"wh", "W", false},
    };
    for (const auto& r : rules) {
      if (matchesAt(word, i, r.pattern)) {
        appendTokens(result, r.phonemes);
        advance = std::strlen(r.pattern);
        return true;
      }
    }
    return false;
  }

  /// Word-end patterns (suffixes and silent-letter clusters).
  bool tryWordEndRule(const std::string& word, size_t i, std::string& result, size_t& advance) {
    static const Rule rules[] = {
        // Silent-consonant clusters
        {"mb", "M", true}, {"bt", "T", true}, {"mn", "M", true},
        // Common suffixes (checked longest-first)
        {"tion", "SH AH N", true}, {"sion", "ZH AH N", true},
        {"cious", "SH AH S", true}, {"tious", "SH AH S", true},
        {"ious", "IY AH S", true}, {"eous", "IY AH S", true},
        {"ture", "CH ER", true}, {"sure", "ZH ER", true},
        {"tial", "SH AH L", true}, {"cial", "SH AH L", true},
        {"ally", "AH L IY", true}, {"ing", "IH NG", true},
        {"ness", "N AH S", true}, {"ment", "M AH N T", true},
        {"ful", "F AH L", true}, {"ous", "AH S", true},
        {"tle", "T AH L", true}, {"dle", "D AH L", true},
        {"ble", "B AH L", true}, {"gle", "G AH L", true},
        {"kle", "K AH L", true}, {"ple", "P AH L", true},
        {"zle", "Z AH L", true}, {"fle", "F AH L", true},
        {"sle", "S AH L", true},
    };
    for (const auto& r : rules) {
      size_t len = std::strlen(r.pattern);
      if (i + len == word.length() && matchesAt(word, i, r.pattern)) {
        appendTokens(result, r.phonemes);
        advance = len;
        return true;
      }
    }
    return false;
  }

  /// Vowel/consonant digraphs and trigraphs, checked anywhere in the word.
  bool tryDigraphRule(const std::string& word, size_t i, std::string& result, size_t& advance) {
    static const Rule rules[] = {
        // Trigraphs (longest match first)
        {"tch", "CH", false}, {"dge", "JH", false}, {"igh", "AY", false},
        {"augh", "AO", false}, {"ough", "AH F", false},
        // Vowel digraphs
        {"aa", "AA", false},
        {"ee", "IY", false}, {"ea", "IY", false}, {"oa", "OW", false},
        {"oo", "UW", false}, {"ou", "AW", false}, {"ow", "OW", false},
        {"oi", "OY", false}, {"oy", "OY", false}, {"au", "AO", false},
        {"aw", "AO", false}, {"ay", "EY", false}, {"ai", "EY", false},
        {"ue", "UW", false}, {"ui", "UW", false}, {"ei", "EY", false},
        {"ey", "IY", false}, {"ie", "IY", false},
        // Consonant digraphs
        {"th", "TH", false}, {"sh", "SH", false}, {"ch", "CH", false},
        {"ph", "F", false}, {"ck", "K", false}, {"ng", "NG", false},
        {"wh", "W", false}, {"qu", "K W", false}, {"gh", "", false},
    };
    for (const auto& r : rules) {
      size_t len = std::strlen(r.pattern);
      if (matchesAt(word, i, r.pattern)) {
        appendTokens(result, r.phonemes);
        advance = len;
        return true;
      }
    }
    return false;
  }

  /// Single-letter fallback with light context (vowel+r rhotic vowels,
  /// c/g soft-vs-hard, y).
  void applySingleLetter(const std::string& word, size_t i, std::string& result, size_t& advance) {
    char c = word[i];
    char next = (i + 1 < word.length()) ? word[i + 1] : '\0';
    advance = 1;

    switch (c) {
      case 'a':
        if (next == 'r') { appendTokens(result, "AA"); }
        else { appendTokens(result, "AE"); }
        break;
      case 'e':
        if (word.length() > 1 && i == word.length() - 1) {
          // silent final e -- emit nothing
        } else if (next == 'r') {
          appendTokens(result, "ER");
          advance = 2;  // "ER" covers the r-coloring -- skip the 'r' itself
        } else {
          appendTokens(result, "EH");
        }
        break;
      case 'i':
        if (next == 'r') {
          appendTokens(result, "ER");
          advance = 2;
        } else {
          appendTokens(result, "IH");
        }
        break;
      case 'o':
        if (next == 'r') {
          appendTokens(result, "AO R");
          advance = 2;
        } else {
          appendTokens(result, "AO");
        }
        break;
      case 'u':
        if (next == 'r') {
          appendTokens(result, "ER");
          advance = 2;
        } else {
          appendTokens(result, "AH");
        }
        break;
      case 'y':
        if (i == 0) appendTokens(result, "Y");
        else appendTokens(result, "IY");
        break;
      case 'c':
        if (next == 'e' || next == 'i' || next == 'y') appendTokens(result, "S");
        else appendTokens(result, "K");
        break;
      case 'g':
        if (next == 'e' || next == 'i' || next == 'y') appendTokens(result, "JH");
        else appendTokens(result, "G");
        break;
      case 'b': appendTokens(result, "B"); break;
      case 'd': appendTokens(result, "D"); break;
      case 'f': appendTokens(result, "F"); break;
      case 'h': appendTokens(result, "HH"); break;
      case 'j': appendTokens(result, "JH"); break;
      case 'k': appendTokens(result, "K"); break;
      case 'l': appendTokens(result, "L"); break;
      case 'm': appendTokens(result, "M"); break;
      case 'n': appendTokens(result, "N"); break;
      case 'p': appendTokens(result, "P"); break;
      case 'q': appendTokens(result, "K"); break;
      case 'r': appendTokens(result, "R"); break;
      case 's': appendTokens(result, "S"); break;
      case 't': appendTokens(result, "T"); break;
      case 'v': appendTokens(result, "V"); break;
      case 'w': appendTokens(result, "W"); break;
      case 'x': appendTokens(result, "K S"); break;
      case 'z': appendTokens(result, "Z"); break;
      default: break;
    }
  }

  /**
   * @brief Apply enhanced rule-based G2P with context awareness
   * @param word Word for which to generate phonemes
   * @return Enhanced phoneme approximation
   */
  std::string applyEnhancedG2PRules(const std::string& word) {
    std::string result;
    size_t i = 0;
    while (i < word.length()) {
      size_t advance = 0;
      if (tryWordStartRule(word, i, result, advance)) {
        i += advance;
        continue;
      }
      if (tryWordEndRule(word, i, result, advance)) {
        i += advance;
        continue;
      }
      if (tryDigraphRule(word, i, result, advance)) {
        i += advance;
        continue;
      }
      applySingleLetter(word, i, result, advance);
      i += advance;

      // Skip a duplicated consonant letter right after processing the first
      // occurrence (e.g. "ll", "tt", "ss") -- same sound, one letter's worth.
      if (i < word.length() && i > 0 && word[i] == word[i - 1] &&
          std::strchr("bcdfglmnprstz", word[i]) != nullptr) {
        i++;
      }
    }
    return result;
  }
};
