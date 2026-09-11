/**
 * @file G2PRuleBasedModelFR.h
 * @brief Rule-based French G2P model for TinyTTSTools
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
 * @brief Rule-based G2P model for French
 * @details Same longest-match architecture as G2PRuleBasedModelEN (see
 * G2PRuleBasedModelBase for the shared scaffolding -- word-end suffixes,
 * then digraphs/trigraphs, then a single-letter fallback with light
 * context; French has no word-start rule category, unlike English/German),
 * retuned for French orthography: the four nasal-vowel
 * spelling families (an/am/en/em -> Phone::AN, in/im/ain/aim/ein ->
 * Phone::EN, on/om -> Phone::ON, un/um -> Phone::UN), "ch"/"gn"/"qu"/"ph"
 * consonant digraphs, the "oi"/"ou"/"eu"/"eau" vowel spellings, and
 * accented letters (é/è/ê/à/ô/î/û/ç/œ, each a 2-byte UTF-8 "digraph"
 * pattern -- see the rules table).
 *
 * French spelling is notoriously irregular around SILENT final consonants
 * (most final b/d/p/s/t/x/z after a vowel are silent, but with many
 * exceptions -- "avec", "bus", "sac" all pronounce theirs); this model
 * applies the common-case default (silent) at the word's last letter, so
 * it will mispronounce the exceptions. Treat it as a best-effort fallback
 * for words not covered by PhonemeDictionaryFR.h/
 * COMPACT_PHONEME_DICTIONARY_FR, not measured against any corpus (no
 * French equivalent of CMUdict is bundled with this project). Stress is
 * not modeled (no MOD_STRESS_PRIMARY/digit is ever emitted).
 * @note Memory footprint: no data tables at all -- pure code, negligible
 * flash beyond the rules themselves, same as G2PRuleBasedModelEN.
 */
class G2PRuleBasedModelFR : public G2PRuleBasedModelBase {
 protected:
  /// Common verb/adjective endings where the default silent-final-
  /// consonant rule (see applySingleLetter) isn't enough on its own.
  bool tryWordEndRule(const std::string& word, size_t i, std::string& result,
                      size_t& advance) override {
    static const Rule rules[] = {
        {"tion", "S Y ON", true},
        {"ez", "EP", true}, {"er", "EP", true},
        {"es", "", true},  // silent plural -es (adjectives/nouns)
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
  /// -- longest-match order matters for overlapping prefixes (e.g. "ain"
  /// before "in", "eau" before "au").
  bool tryDigraphRule(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    static const Rule rules[] = {
        // Nasal-vowel trigraphs (longest match first).
        {"ain", "EN", false}, {"aim", "EN", false}, {"ein", "EN", false},
        {"eau", "OP", false},
        // Nasal-vowel digraphs.
        {"an", "AN", false}, {"am", "AN", false},
        {"en", "AN", false}, {"em", "AN", false},
        {"in", "EN", false}, {"im", "EN", false},
        {"on", "ON", false}, {"om", "ON", false},
        {"un", "UN", false}, {"um", "UN", false},
        // Other vowel digraphs.
        {"oi", "W AF", false}, {"ou", "UW", false}, {"au", "OP", false},
        {"eu", "OF", false}, {"ui", "HU IH", false},
        // Consonant digraphs.
        {"ch", "SH", false}, {"gn", "NY", false}, {"qu", "K", false},
        {"ph", "F", false},
        // Ligature + trailing vowel (must precede the bare "œ" rule below).
        {"\xc5\x93u", "OF", false},  // œu, e.g. "cœur", "sœur"
        // Accented/ligature letters (2-byte UTF-8 sequences).
        {"\xc3\xa9", "EP", false},   // é
        {"\xc3\xa8", "EH", false},   // è
        {"\xc3\xaa", "EH", false},   // ê
        {"\xc3\xa0", "AF", false},   // à
        {"\xc3\xb4", "OP", false},   // ô
        {"\xc3\xae", "IY", false},   // î
        {"\xc3\xbb", "UW", false},   // û
        {"\xc3\xa7", "S", false},    // ç
        {"\xc5\x93", "OF", false},   // œ
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

  /// Single-letter fallback. `wordEnd` marks the absolute last letter,
  /// needed for the silent-final-consonant default and mute final "e".
  void applySingleLetter(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    char c = word[i];
    char next = (i + 1 < word.length()) ? word[i + 1] : '\0';
    bool wordEnd = (i + 1 == word.length());
    advance = 1;

    switch (c) {
      case 'a': appendTokens(result, "AF"); break;
      case 'e': appendTokens(result, wordEnd ? "" : "AH0"); break;
      case 'i': appendTokens(result, "IY"); break;
      case 'o': appendTokens(result, "AO"); break;
      case 'u': appendTokens(result, "UF"); break;
      case 'y': appendTokens(result, "IY"); break;
      case 'b': appendTokens(result, "B"); break;
      case 'c':
        appendTokens(result, (next == 'e' || next == 'i' || next == 'y') ? "S" : "K");
        break;
      case 'd': appendTokens(result, wordEnd ? "" : "D"); break;
      case 'f': appendTokens(result, "F"); break;
      case 'g':
        appendTokens(result, (next == 'e' || next == 'i' || next == 'y') ? "ZH" : "G");
        break;
      case 'h': break;  // always silent in French
      case 'j': appendTokens(result, "ZH"); break;
      case 'k': appendTokens(result, "K"); break;
      case 'l': appendTokens(result, "L"); break;
      case 'm': appendTokens(result, "M"); break;
      case 'n': appendTokens(result, "N"); break;
      case 'p': appendTokens(result, wordEnd ? "" : "P"); break;
      case 'q': appendTokens(result, "K"); break;
      case 'r': appendTokens(result, "RU"); break;
      case 's':
        if (wordEnd) break;  // silent final s (default case)
        appendTokens(result, (i > 0 && next != '\0') ? "Z" : "S");
        break;
      case 't': appendTokens(result, wordEnd ? "" : "T"); break;
      case 'v': appendTokens(result, "V"); break;
      case 'w': appendTokens(result, "W"); break;
      case 'x': appendTokens(result, wordEnd ? "" : "K S"); break;
      case 'z': appendTokens(result, "Z"); break;
      default: break;  // continuation byte of a UTF-8 sequence already
                        // consumed by tryDigraphRule, or an unknown symbol
    }
  }
};
