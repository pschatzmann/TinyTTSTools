/**
 * @file G2PRuleBasedModelDE.h
 * @brief Rule-based German G2P model for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstring>
#include <string>
#include "G2PRuleBasedModelBase.h"

/**
 * @brief Rule-based G2P model for German
 * @details Same longest-match architecture as G2PRuleBasedModelEN (see
 * G2PRuleBasedModelBase for the shared scaffolding -- word-start rule,
 * then word-end suffixes, then digraphs/trigraphs, then a single-letter
 * fallback with light context), retuned for German
 * orthography: "sch"/"ch"/"ck"/"pf"/"ng" consonant clusters, the "ie"/"ei"/
 * "eu" vowel digraphs, umlauts (ä/ö/ü) and "ß" (each handled as one
 * multi-byte UTF-8 "digraph" pattern -- see the rules table), a
 * front/back-vowel context check for "ch" (ich-Laut Phone::C vs. ach-Laut
 * Phone::X), word-initial "sp"/"st" -> /ʃp/,/ʃt/, and word-final obstruent
 * devoicing (b/d/g -> p/t/k at the absolute end of a word).
 *
 * German spelling is considerably more regular than English's, but this is
 * still a hand-written approximation, NOT measured against a corpus (no
 * German equivalent of CMUdict is bundled with this project) -- treat it
 * as a best-effort fallback for words not covered by
 * PhonemeDictionaryDE.h/COMPACT_PHONEME_DICTIONARY_DE, the same way
 * G2PRuleBasedModelEN is a fallback for COMPACT_PHONEME_DICTIONARY_EN.
 * Stress is not modeled (no MOD_STRESS_PRIMARY/digit is ever emitted).
 * @note Memory footprint: no data tables at all -- pure code, negligible
 * flash beyond the rules themselves, same as G2PRuleBasedModelEN.
 */
class G2PRuleBasedModelDE : public G2PRuleBasedModelBase {
 protected:
  /// Word-initial "sp"/"st" -> /ʃp/,/ʃt/ (standard German, not the
  /// spelled-out /sp/,/st/ a naive per-letter mapping would produce).
  bool tryWordStartRule(const std::string& word, size_t i, std::string& result,
                        size_t& advance) override {
    if (i != 0) return false;
    static const Rule rules[] = {
        {"sp", "SH P", false},
        {"st", "SH T", false},
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

  /// Common suffixes, longest-first.
  bool tryWordEndRule(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    static const Rule rules[] = {
        {"heit", "HH AY T", true}, {"keit", "K AY T", true},
        {"lich", "L IH C", true}, {"isch", "IH SH", true},
        {"ung", "UH NG", true}, {"en", "AH0 N", true},
        {"er", "AH0 RU", true},
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

  /// Vowel/consonant digraphs and trigraphs, checked anywhere in the word
  /// -- longest-match order matters for overlapping prefixes (e.g. "äu"
  /// before "ä"). "ch" is handled separately (needs a preceding-vowel
  /// context check), not via this fixed table.
  bool tryDigraphRule(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    if (tryChRule(word, i, result, advance)) return true;
    static const Rule rules[] = {
        // 2-byte UTF-8 umlaut/eszett "letters", longer sequences first.
        {"\xc3\xa4u", "OY", false},  // äu
        {"sch", "SH", false},
        {"pf", "PF", false}, {"ck", "K", false}, {"ph", "F", false},
        {"dt", "T", false},  // e.g. "Stadt" -- always pronounced as one /t/
        {"ng", "NG", false}, {"qu", "K V", false},
        {"ie", "IY", false}, {"ei", "AY", false}, {"ai", "AY", false},
        {"eu", "OY", false},
        {"ah", "AF:", false}, {"eh", "EP", false}, {"ih", "IY", false},
        {"oh", "OP", false}, {"uh", "UW", false},
        {"\xc3\xa4", "EH", false},   // ä
        {"\xc3\xb6", "OE", false},   // ö
        {"\xc3\xbc", "UF0", false},  // ü
        {"\xc3\x9f", "S", false},    // ß
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

  /// "ch": Phone::C (ich-Laut) by default, Phone::X (ach-Laut) right after
  /// a back vowel (a/o/u, including their long/umlaut forms already
  /// consumed by an earlier digraph rule -- so this only needs to check
  /// the raw preceding letter).
  bool tryChRule(const std::string& word, size_t i, std::string& result, size_t& advance) {
    if (!matchesAt(word, i, "ch")) return false;
    char prev = (i > 0) ? word[i - 1] : '\0';
    appendTokens(result, (prev == 'a' || prev == 'o' || prev == 'u') ? "X" : "C");
    advance = 2;
    return true;
  }

  /// Single-letter fallback. `wordEnd` marks the absolute last letter of
  /// the word, needed for final obstruent devoicing (b/d/g -> p/t/k) and
  /// e's reduction to schwa in a final unstressed syllable.
  void applySingleLetter(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    char c = word[i];
    bool wordEnd = (i + 1 == word.length());
    advance = 1;

    switch (c) {
      case 'a': appendTokens(result, "AF"); break;
      case 'e': appendTokens(result, wordEnd ? "AH0" : "EH"); break;
      case 'i': appendTokens(result, "IH"); break;
      case 'o': appendTokens(result, "AO"); break;
      case 'u': appendTokens(result, "UH"); break;
      case 'y': appendTokens(result, "IH"); break;
      case 'b': appendTokens(result, wordEnd ? "P" : "B"); break;
      case 'd': appendTokens(result, wordEnd ? "T" : "D"); break;
      case 'g': appendTokens(result, wordEnd ? "K" : "G"); break;
      case 'f': appendTokens(result, "F"); break;
      case 'h': appendTokens(result, "HH"); break;
      case 'j': appendTokens(result, "Y"); break;
      case 'k': appendTokens(result, "K"); break;
      case 'l': appendTokens(result, "L"); break;
      case 'm': appendTokens(result, "M"); break;
      case 'n': appendTokens(result, "N"); break;
      case 'p': appendTokens(result, "P"); break;
      case 'r': appendTokens(result, "RU"); break;
      case 's':
        appendTokens(result, (i == 0) ? "Z" : "S");
        break;
      case 't': appendTokens(result, "T"); break;
      case 'v': appendTokens(result, "F"); break;
      case 'w': appendTokens(result, "V"); break;
      case 'x': appendTokens(result, "K S"); break;
      case 'z': appendTokens(result, "TS"); break;
      default: break;  // continuation byte of a UTF-8 sequence already
                        // consumed by tryDigraphRule, or an unknown symbol
    }
  }
};
