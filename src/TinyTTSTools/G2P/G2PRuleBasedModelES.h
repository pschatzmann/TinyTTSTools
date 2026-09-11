/**
 * @file G2PRuleBasedModelES.h
 * @brief Rule-based Spanish G2P model for TinyTTSTools
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
 * @brief Rule-based G2P model for Spanish
 * @details Same longest-match architecture as G2PRuleBasedModelEN (see
 * G2PRuleBasedModelBase for the shared scaffolding -- digraphs/trigraphs,
 * then a single-letter fallback with light context; Spanish has no
 * word-start or word-end rule category, unlike English/German/French),
 * retuned for Spanish orthography -- by far the most regular of the three international
 * languages added alongside this one (G2PRuleBasedModelDE/FR), since
 * Spanish spelling is close to a 1:1 letter-to-sound mapping. Handles
 * "ll"/"rr"/"ch"/"qu"/"gu" digraphs (the latter two with a following-e/i
 * context check, silencing the "u"), the ñ/á/é/í/ó/ú/ü accented letters
 * (each a 2-byte UTF-8 "digraph" pattern), Latin American seseo (z, soft
 * c -> Phone::S, not a dedicated /θ/), and yeísmo (ll/y -> Phone::Y, not
 * the traditional Phone::LY).
 *
 * Even Spanish's regularity has real exceptions this simple a model can't
 * resolve (single "r" is a trill word-initially/after l-n-s, a tap
 * elsewhere -- approximated here as word-initial-only trill); treat this
 * as a best-effort fallback for words not covered by
 * PhonemeDictionaryES.h/COMPACT_PHONEME_DICTIONARY_ES, not measured
 * against any corpus (no Spanish equivalent of CMUdict is bundled with
 * this project). Stress is not modeled (no MOD_STRESS_PRIMARY/digit is
 * ever emitted, even for an accented vowel that marks irregular stress).
 * @note Memory footprint: no data tables at all -- pure code, negligible
 * flash beyond the rules themselves, same as G2PRuleBasedModelEN.
 */
class G2PRuleBasedModelES : public G2PRuleBasedModelBase {
 protected:
  /// "gu"/"qu" before e/i: the "u" is silent (guerra, que) -- otherwise
  /// "gu" is /gw/ and "qu" doesn't occur outside that context in Spanish.
  bool tryGuQuRule(const std::string& word, size_t i, std::string& result, size_t& advance) {
    char next2 = (i + 2 < word.length()) ? word[i + 2] : '\0';
    bool beforeFront = (next2 == 'e' || next2 == 'i');
    if (matchesAt(word, i, "gu") && beforeFront) {
      appendTokens(result, "G");
      advance = 2;
      return true;
    }
    if (matchesAt(word, i, "qu") && beforeFront) {
      appendTokens(result, "K");
      advance = 2;
      return true;
    }
    return false;
  }

  /// Digraphs checked anywhere in the word (after the gu/qu context
  /// check above, which must run first since "gu"/"qu" would otherwise
  /// never be reached by a plain g/q single-letter rule).
  bool tryDigraphRule(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    if (tryGuQuRule(word, i, result, advance)) return true;
    static const Rule rules[] = {
        {"ll", "Y"}, {"rr", "RR"}, {"ch", "CH"},
        {"\xc3\xb1", "NY"},   // ñ
        {"\xc3\xa1", "AF"},   // á
        {"\xc3\xa9", "EP"},   // é
        {"\xc3\xad", "IY"},   // í
        {"\xc3\xb3", "OP"},   // ó
        {"\xc3\xba", "UW"},   // ú
        {"\xc3\xbc", "UW"},   // ü
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

  /// Single-letter fallback with light context (c/g soft-vs-hard, r
  /// trill-vs-tap).
  void applySingleLetter(const std::string& word, size_t i, std::string& result, size_t& advance) override {
    char c = word[i];
    char next = (i + 1 < word.length()) ? word[i + 1] : '\0';
    advance = 1;

    switch (c) {
      case 'a': appendTokens(result, "AF"); break;
      case 'e': appendTokens(result, "EP"); break;
      case 'i': appendTokens(result, "IY"); break;
      case 'o': appendTokens(result, "OP"); break;
      case 'u': appendTokens(result, "UW"); break;
      case 'y': appendTokens(result, "Y"); break;
      case 'b': appendTokens(result, "B"); break;
      case 'c':
        appendTokens(result, (next == 'e' || next == 'i') ? "S" : "K");
        break;
      case 'd': appendTokens(result, "D"); break;
      case 'f': appendTokens(result, "F"); break;
      case 'g':
        appendTokens(result, (next == 'e' || next == 'i') ? "X" : "G");
        break;
      case 'h': break;  // always silent in Spanish
      case 'j': appendTokens(result, "X"); break;
      case 'k': appendTokens(result, "K"); break;
      case 'l': appendTokens(result, "L"); break;
      case 'm': appendTokens(result, "M"); break;
      case 'n': appendTokens(result, "N"); break;
      case 'p': appendTokens(result, "P"); break;
      case 'q': appendTokens(result, "K"); break;
      case 'r': appendTokens(result, (i == 0) ? "RR" : "RT"); break;
      case 's': appendTokens(result, "S"); break;
      case 't': appendTokens(result, "T"); break;
      case 'v': appendTokens(result, "B"); break;
      case 'w': appendTokens(result, "W"); break;
      case 'x': appendTokens(result, "K S"); break;
      case 'z': appendTokens(result, "S"); break;
      default: break;  // continuation byte of a UTF-8 sequence already
                        // consumed by tryDigraphRule, or an unknown symbol
    }
  }

  /// Excludes 'l'/'r' from the base default -- "ll"/"rr" are already
  /// handled as their own digraphs above (tryDigraphRule runs first) and
  /// never reach the duplicate-letter-skip step in applyRules() at all.
  const char* duplicateSkipChars() const override { return "bcdfgmnpstz"; }
};
