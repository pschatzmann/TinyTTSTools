/**
 * @file RuntimePhonemeDictionary.h
 * @brief Small runtime word->phoneme dictionary for the extended
 * (X-SAMPA modifier-tag) phoneme-string authoring syntax.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "PhonemeDictionaryBase.h"

/**
 * @brief One human-readable {word, phoneme-string} entry, e.g.
 * `{"ueber", "UF: B ER0"}`. The phoneme string uses the same X-SAMPA
 * modifier-tag/stress-digit syntax PSOLAVocoder/FormantVocoder already
 * parse at synthesis time (see PhonemeModifiers.h's parsePhonemeModifiers()/
 * stripStressMarker()) -- entries are stored and returned verbatim, no
 * packing or re-parsing happens here.
 */
struct PhonemeStringSource {
  const char* word;
  const char* phonemes;
};

/**
 * @brief Small runtime-built word->phoneme dictionary for words authored
 * via the extended phoneme-string syntax.
 * @details A companion to CompactPhonemeDictionary/PH_WORD(), not a
 * replacement. PH_WORD()'s Seg()/bare-`Phone` styles (see
 * CompactPhonemeDictionaryBuilder.h) stay fully compile-time and
 * flash-resident, which matters for a dictionary with many entries; this
 * class exists for the handful of words where writing the phoneme string
 * directly -- e.g. one needing several modifier tags, or ported from
 * another X-SAMPA-based source -- is more convenient than a long Seg()/
 * Phone argument list.
 *
 * Entries are sorted once at construction and binary-searched, same
 * approach as CompactPhonemeDictionary, but built and stored at RUNTIME
 * (static-init time, before main): C++17 has no string-literal non-type
 * template parameter (that lands in C++20), so there is no way to give
 * PH_WORD()'s compile-time storage trick a string argument the way it
 * already does for Seg()/Phone arguments -- a string-authored entry can
 * only be parsed at, and only be backed by storage allocated at, runtime.
 * Keep this dictionary small; each entry costs two heap-allocated
 * std::strings, unlike CompactPhonemeDictionary's ~2 bytes/phoneme,
 * flash-resident cost.
 *
 * Use alongside a compile-time PH_WORD() dictionary via
 * FallbackPhonemeDictionary.h, e.g.:
 * @code
 * static constexpr PhonemeStringSource extra[] = {
 *     {"ueber", "UF: B ER0"},
 *     {"tja", "T_j AA"},
 * };
 * RuntimePhonemeDictionary extraDict(extra);
 * FallbackPhonemeDictionary combined(COMPACT_PHONEME_DICTIONARY_EN, extraDict);
 * g2p.useCompactDictionary(combined);
 * @endcode
 */
class RuntimePhonemeDictionary : public PhonemeDictionaryBase {
 public:
  template <size_t N>
  explicit RuntimePhonemeDictionary(const PhonemeStringSource (&table)[N]) {
    entries_.reserve(N);
    for (size_t i = 0; i < N; ++i) {
      entries_.emplace_back(table[i].word, table[i].phonemes);
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.word < b.word; });
  }

  size_t size() const override { return entries_.size(); }

  bool lookup(const std::string& word, std::string& outPhonemes) const override {
    auto it = std::lower_bound(
        entries_.begin(), entries_.end(), word,
        [](const Entry& e, const std::string& w) { return e.word < w; });
    if (it == entries_.end() || it->word != word) return false;
    outPhonemes = it->phonemes;
    return true;
  }

  using PhonemeDictionaryBase::lookup;  // bring in the single-arg convenience overload

 private:
  struct Entry {
    std::string word;
    std::string phonemes;
    Entry(const char* w, const char* p) : word(w), phonemes(p) {}
  };
  std::vector<Entry> entries_;
};
