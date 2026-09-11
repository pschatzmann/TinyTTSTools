/**
 * @file G2PRuleBasedModelBase.h
 * @brief Shared scaffolding for the per-language rule-based G2P models
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstring>
#include <string>

#include "G2PModelBase.h"
#include "../Basic/StringUtils.h"

/**
 * @brief Abstract base for G2PRuleBasedModelEN/DE/FR/ES -- the longest-
 * match rule-engine scaffolding every one of them already used
 * independently, factored out once.
 * @details None of the actual RULES are shared between languages -- each
 * subclass still owns its own rule tables and single-letter switch
 * statement, matching how these classes were designed from the start (see
 * docs/ADDING_A_LANGUAGE.md's Step 3: writing a new language's rules is
 * meant to be a self-contained, from-scratch exercise in that language's
 * own orthography, not a diff against another language's). What this base
 * class removes is the four-times-repeated boilerplate AROUND those rules:
 * the `Rule` struct shape, `matchesAt()`/`appendTokens()`, and the main
 * word-scan loop's control flow (`wordToPhonemes()` -> lowercase ->
 * `applyRules()`).
 *
 * A subclass overrides whichever rule categories it actually needs --
 * `tryWordStartRule()`/`tryWordEndRule()`/`tryDigraphRule()` default to
 * "no match", so a language with no word-start patterns (e.g. French,
 * Spanish) simply never overrides that one -- and must implement
 * `applySingleLetter()` (the always-present final fallback once no
 * multi-character rule matched at the current position). Override
 * `duplicateSkipChars()` too if a language's doubled-consonant-skip set
 * differs from the default (Spanish excludes 'l'/'r', already consumed by
 * its own "ll"/"rr" digraph rules before the skip logic would ever see
 * them).
 *
 * Every subclass is still its own from-scratch reference for that
 * language's spelling-to-sound behavior, unverified against any corpus
 * (no CMUdict equivalent exists for German/French/Spanish) except
 * G2PRuleBasedModelEN (~17% exact-match, measured against the full CMU
 * dictionary) -- see each subclass's own class doc for language-specific
 * caveats and approximations.
 */
class G2PRuleBasedModelBase : public G2PModelBase {
 public:
  G2PRuleBasedModelBase() { initialized_ = true; }

  std::string wordToPhonemes(const std::string& word) override {
    std::string lowerWord = StringUtils::toLowerCase(word);
    return applyRules(lowerWord);
  }

 protected:
  /**
   * @brief One longest-match rule
   * @details If `pattern` occurs at position i (optionally anchored to the
   * end of the word via `wordEndOnly`), emit `phonemes` and advance i past
   * it. `wordEndOnly` defaults false, so a language with no word-end rule
   * category (its rule tables never set it) can omit the field entirely
   * in an aggregate initializer, e.g. `{"ll", "Y"}`.
   */
  struct Rule {
    const char* pattern;
    const char* phonemes;
    bool wordEndOnly = false;
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

  /// Word-start rule hook -- default: no match. Override for a language
  /// with silent-letter/consonant-cluster patterns anchored to the very
  /// first letter (e.g. English "kn"->N, German "sp"->/ʃp/).
  virtual bool tryWordStartRule(const std::string& /*word*/, size_t /*i*/,
                                std::string& /*result*/, size_t& /*advance*/) {
    return false;
  }

  /// Word-end rule hook -- default: no match. Override for a language
  /// with suffix patterns anchored to the very last letter.
  virtual bool tryWordEndRule(const std::string& /*word*/, size_t /*i*/,
                              std::string& /*result*/, size_t& /*advance*/) {
    return false;
  }

  /// Digraph/trigraph rule hook -- default: no match. Override for a
  /// language's multi-character patterns checked at any position.
  virtual bool tryDigraphRule(const std::string& /*word*/, size_t /*i*/,
                              std::string& /*result*/, size_t& /*advance*/) {
    return false;
  }

  /// Single-letter fallback -- every language needs one (the last resort
  /// once no multi-character rule matched at the current position).
  virtual void applySingleLetter(const std::string& word, size_t i,
                                 std::string& result, size_t& advance) = 0;

  /// Characters eligible for the duplicate-consonant-skip step in
  /// applyRules() below -- default matches English/German/French's set.
  /// Override if a language's digraph rules already consume a doubled
  /// letter some other way.
  virtual const char* duplicateSkipChars() const { return "bcdfglmnprstz"; }

  /// Longest-match word scan: word-start, then word-end, then digraph,
  /// then single-letter, in that order at each position -- identical
  /// control flow across every language (each category is anchored/
  /// scoped differently -- word-start only fires at i==0, word-end only
  /// when the match reaches the word's last character, digraph anywhere
  /// -- so trying them in a different order would change which rule wins
  /// on an overlapping match).
  std::string applyRules(const std::string& word) {
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

      // Skip a duplicated consonant letter right after processing the
      // first occurrence (e.g. "ll", "tt", "ss") -- same sound, one
      // letter's worth.
      if (i < word.length() && i > 0 && word[i] == word[i - 1] &&
          std::strchr(duplicateSkipChars(), word[i]) != nullptr) {
        i++;
      }
    }
    return result;
  }
};
