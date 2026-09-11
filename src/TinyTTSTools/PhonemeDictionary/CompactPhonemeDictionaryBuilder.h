/**
 * @file CompactPhonemeDictionaryBuilder.h
 * @brief Compile-time builder that packs a readable, enum-typed
 * {"word", Phone::X, Phone::Y, ...} table into the flat arrays
 * CompactPhonemeDictionary reads.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-07
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "../Basic/PhonemeModifiers.h"
#include "CompactPhonemeDictionary.h"

/**
 * @brief One human-readable dictionary source entry
 * @details A word and a pointer to its already-packed phoneme sequence
 * (see PH_WORD() below -- entries are built with that macro, never
 * written out by hand).
 */
struct PhonemeWordSource {
  const char* word;
  const uint16_t* phonemeData;
  size_t phonemeCount;
};

namespace tts_compact_dict_detail {

constexpr size_t cxStrLen(const char* s) {
  size_t n = 0;
  while (s[n] != '\0') ++n;
  return n;
}

/// true if a is strictly less than b, comparing byte-by-byte (matches the
/// std::memcmp-based ordering CompactPhonemeDictionary's binary search uses).
constexpr bool cxWordLess(const char* a, const char* b) {
  size_t i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i]) return static_cast<unsigned char>(a[i]) < static_cast<unsigned char>(b[i]);
    ++i;
  }
  return a[i] == '\0' && b[i] != '\0';
}

/// Packs one `Phone` value into the low bits of a dictionary symbol:
/// `base_id << 2`. `Phone` itself carries no stress information (see
/// Phonemes.h) -- Seg() below ORs the stress bits in separately. Every
/// `Phone` id (0-120, ARPAbet and the international/IPA extension alike)
/// is valid here: the packed symbol is a plain `uint16_t` with 7 bits of
/// id, so nothing overflows the way the old 1-byte `(id << 2) | stress`
/// format would have past id 63.
constexpr uint16_t cxPackPhone(Phone p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p) << 2);
}

template <size_t N>
constexpr size_t cxTotalWordBytes(const PhonemeWordSource (&table)[N]) {
  size_t total = 0;
  for (size_t i = 0; i < N; ++i) total += cxStrLen(table[i].word);
  return total;
}

template <size_t N>
constexpr size_t cxTotalPhonemeCount(const PhonemeWordSource (&table)[N]) {
  size_t total = 0;
  for (size_t i = 0; i < N; ++i) total += table[i].phonemeCount;
  return total;
}

/// Normalizes one PH_WORD() argument to a packed symbol: a bare `Phone`
/// (e.g. `Phone::AA`, no stress/modifier -- the plain-list authoring
/// style) packs via cxPackPhone() directly; an already-packed `uint16_t`
/// (a Seg() result, carrying stress and/or a modifier) passes through
/// unchanged. Overload resolution picks the right one per-argument, so a
/// single PH_WORD() call can freely mix both styles (see Seg()'s own doc).
constexpr uint16_t cxNormalizeSymbol(Phone p) { return cxPackPhone(p); }
constexpr uint16_t cxNormalizeSymbol(uint16_t s) { return s; }

/**
 * @brief Backing storage for one word's packed phoneme sequence
 * @details One instantiation per unique `<Symbols...>` combination (so
 * identical pronunciations, e.g. true homophones, automatically share
 * storage). `Symbols` is `auto...` (not `uint16_t...`) so each argument can
 * be either a bare `Phone` (plain-list style, no stress/modifier) or an
 * already-packed `uint16_t` from Seg() (stress and/or a modifier) --
 * cxNormalizeSymbol() converts whichever was given to the stored
 * `uint16_t` form.
 */
template <auto... Symbols>
struct PhonemeSeqHolder {
  static constexpr uint16_t values[sizeof...(Symbols)] = {cxNormalizeSymbol(Symbols)...};
};
template <auto... Symbols>
constexpr uint16_t PhonemeSeqHolder<Symbols...>::values[sizeof...(Symbols)];

template <auto... Symbols>
constexpr PhonemeWordSource makeWord(const char* word) {
  return PhonemeWordSource{word, PhonemeSeqHolder<Symbols...>::values, sizeof...(Symbols)};
}

}  // namespace tts_compact_dict_detail

/**
 * @brief Pack one PH_WORD() segment: a `Phone` plus an optional modifier.
 * @details `Seg(Phone::AA)` (bare, no second argument) packs identically to
 * the pre-Seg() `cxPackPhone(Phone::AA)`. `MOD_STRESS_PRIMARY`/
 * `MOD_STRESS_SECONDARY` (see PhonemeModifiers.h) fold into the packed
 * symbol's dedicated 2-bit stress field, exactly as before; every other
 * modifier now has its own 5-bit field in the widened symbol (bits 9-13,
 * above the 7-bit id field in bits 2-8), so e.g. `Seg(Phone::UF,
 * PhonemeModifier::MOD_LONG)` (German long "über")
 * or `Seg(Phone::T, PhonemeModifier::MOD_PALATALIZED)` are both fully
 * captured -- no modifier is silently dropped here anymore.
 * `Phone::AA1`/`AA2`-style stress-variant enum members no longer exist;
 * write `Seg(Phone::AA, PhonemeModifier::MOD_STRESS_PRIMARY)` where those
 * used to be spelled `Phone::AA1`.
 */
constexpr uint16_t Seg(Phone base,
                        PhonemeModifier modifier = PhonemeModifier::MOD_NONE) {
  return static_cast<uint16_t>(
      tts_compact_dict_detail::cxPackPhone(base) |
      (modifier == PhonemeModifier::MOD_STRESS_PRIMARY
           ? 1
           : modifier == PhonemeModifier::MOD_STRESS_SECONDARY
                 ? 2
                 : (static_cast<uint16_t>(modifier) << 9)));
}

/// Builds one dictionary table row. Each argument after the word is either
/// a bare `Phone` (plain-list style, no stress/modifier -- e.g. `PH_WORD
/// ("able", Phone::EY, Phone::B, Phone::AH, Phone::L)`) or a `Seg(...)`
/// call for a segment that needs stress and/or a modifier -- the two styles
/// freely mix in one call, e.g. `PH_WORD("stressed", Seg(Phone::AA,
/// PhonemeModifier::MOD_STRESS_PRIMARY), Phone::T)`. An unrecognized
/// phoneme name is simply a compiler error (no `Phone` enumerator of that
/// kind exists), so there's no separate "unknown token" check to run at
/// dictionary-build time.
#define PH_WORD(word, ...) tts_compact_dict_detail::makeWord<__VA_ARGS__>(word)

/**
 * @brief Compile-time-computed compact dictionary data for a readable,
 * PH_WORD()-built `PhonemeWordSource` table.
 * @tparam N Number of table entries (deduced)
 * @tparam WordBytes Total word-character count (pass
 *         `tts_compact_dict_detail::cxTotalWordBytes(table)`)
 * @tparam PhonemeCount Total phoneme count (pass
 *         `tts_compact_dict_detail::cxTotalPhonemeCount(table)`)
 * @details Building this object is a compile-time constant expression --
 * the packed arrays end up in read-only memory exactly as if they'd been
 * generated by an offline script, but the source stays a plain table of
 * `PH_WORD(...)` rows with no separate build step. An out-of-alphabetical-
 * order word fails the *build* (binary search requires sorted entries)
 * rather than corrupting the dictionary silently at runtime; an
 * unrecognized phoneme name is simply not a valid `Phone` enumerator, so
 * it's already a compiler error at the PH_WORD() call site.
 * Use TTS_COMPACT_DICTIONARY() below instead of naming this type directly.
 */
template <size_t N, size_t WordBytes, size_t PhonemeCount>
struct CompactPhonemeDictionaryData {
  std::array<uint32_t, N + 1> wordOffsets{};
  std::array<char, WordBytes> wordsBlob{};
  std::array<uint32_t, N + 1> phonemeOffsets{};
  std::array<uint16_t, PhonemeCount> phonemeData{};

  constexpr CompactPhonemeDictionaryData(const PhonemeWordSource (&table)[N]) {
    using namespace tts_compact_dict_detail;
    size_t wPos = 0, pPos = 0;
    wordOffsets[0] = 0;
    phonemeOffsets[0] = 0;
    for (size_t i = 0; i < N; ++i) {
      if (i > 0 && !cxWordLess(table[i - 1].word, table[i].word)) {
        throw "CompactPhonemeDictionaryData: source table is not sorted "
              "(words must be strictly ascending for binary search)";
      }
      size_t wlen = cxStrLen(table[i].word);
      for (size_t j = 0; j < wlen; ++j) wordsBlob[wPos + j] = table[i].word[j];
      wPos += wlen;
      wordOffsets[i + 1] = static_cast<uint32_t>(wPos);

      for (size_t j = 0; j < table[i].phonemeCount; ++j) {
        phonemeData[pPos++] = table[i].phonemeData[j];
      }
      phonemeOffsets[i + 1] = static_cast<uint32_t>(pPos);
    }
  }
};

/// Declares `name##Data` (the compile-time-packed storage) and `name`
/// (a ready-to-use CompactPhonemeDictionary bound to it) from a
/// `static constexpr PhonemeWordSource table[] = { PH_WORD(...), ... };`.
#define TTS_COMPACT_DICTIONARY(name, table)                                        \
  constexpr CompactPhonemeDictionaryData<                                          \
      sizeof(table) / sizeof((table)[0]),                                          \
      tts_compact_dict_detail::cxTotalWordBytes(table),                            \
      tts_compact_dict_detail::cxTotalPhonemeCount(table)>                         \
      name##Data(table);                                                           \
  const CompactPhonemeDictionary name(                                             \
      name##Data.wordOffsets.data(), name##Data.wordsBlob.data(),                  \
      name##Data.phonemeOffsets.data(), name##Data.phonemeData.data(),             \
      sizeof(table) / sizeof((table)[0]))
