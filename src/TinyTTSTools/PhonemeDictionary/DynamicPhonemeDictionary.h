/**
 * @file DynamicPhonemeDictionary.h
 * @brief Runtime-mutable word->phoneme dictionary
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "PhonemeDictionaryBase.h"

/**
 * @brief A word->phoneme dictionary you can add() to at runtime, one entry
 * at a time.
 * @tparam Allocator Allocator for the backing `std::vector<Entry>`.
 * Defaults to `std::allocator<Entry>`; pass `PsramAllocator<Entry>` (see
 * Memory/PsramAllocator.h) to place the vector's storage in PSRAM on
 * ESP32. Note this only covers the vector's own array of `Entry` structs
 * -- each entry's two `std::string`s still allocate through the default
 * allocator for anything past small-string-optimization length (typically
 * ~15 characters on libstdc++), so this isn't a full PSRAM solution for
 * very large dynamic dictionaries the way CompressedPhonemeDictionarySD is
 * for a bulk-loaded one.
 * @details Every other dictionary in this library is built for read-only,
 * bulk-loaded data -- compiled into flash
 * (CompactPhonemeDictionary/CompressedPhonemeDictionary), or loaded whole
 * from a file (CompressedPhonemeDictionarySD) -- and
 * `G2PDictionaryModel::setPhonemeDictionary()` only accepts a fixed,
 * already-sorted `PhonemeEntry` array. None of them support adding one
 * word at a time after construction (e.g. words learned from user input,
 * a config file parsed line-by-line, or a growing custom vocabulary). This
 * class fills that gap: entries are kept sorted internally (binary search
 * for lookup, same as the read-only dictionaries), but add()/remove() can
 * be called at any time.
 *
 * @code
 * DynamicPhonemeDictionary<> dict;
 * dict.add("gizmo", "G IH Z M OW");
 * dict.add("gizmo", "G IH Z M OW2");  // updates the existing entry
 *
 * g2p.getDictionaryModel().useCompactDictionary(dict);
 * @endcode
 *
 * `dict` must outlive the G2P model it's pointed at (same convention as
 * every other PhonemeDictionaryBase), and adding/removing entries after
 * `useCompactDictionary()` is safe -- lookups always see the current
 * contents, there's no separate "compile"/"rebuild" step.
 */
template <typename Allocator = std::allocator<std::pair<std::string, std::string>>>
class DynamicPhonemeDictionary : public PhonemeDictionaryBase {
 public:
  using Entry = std::pair<std::string, std::string>;

  size_t size() const override { return entries_.size(); }

  /// Exact-match lookup. `word` must already be in the same case entries
  /// were added with -- like every other dictionary here, this class does
  /// no case-folding of its own (callers, e.g. G2PDictionaryModel, already
  /// lowercase before calling in).
  bool lookup(const std::string& word, std::string& outPhonemes) const override {
    auto it = find(word);
    if (it == entries_.end() || it->first != word) return false;
    outPhonemes = it->second;
    return true;
  }

  using PhonemeDictionaryBase::lookup;  // bring in the single-arg convenience overload

  /// Add a new word, or update an existing word's pronunciation.
  /// @return true if a new entry was inserted, false if an existing
  /// word's phonemes were overwritten instead.
  bool add(const std::string& word, const std::string& phonemes) {
    auto it = find(word);
    if (it != entries_.end() && it->first == word) {
      it->second = phonemes;
      return false;
    }
    entries_.insert(it, Entry(word, phonemes));
    return true;
  }

  /// Remove one entry by word.
  /// @return true if it existed and was removed, false otherwise.
  bool remove(const std::string& word) {
    auto it = find(word);
    if (it == entries_.end() || it->first != word) return false;
    entries_.erase(it);
    return true;
  }

  /// Remove every entry.
  void clear() { entries_.clear(); }

  /// Word at sequential index `idx` (0..size()-1), for iterating the whole
  /// dictionary rather than looking one up by name.
  const std::string& wordAt(size_t idx) const { return entries_[idx].first; }

  /// Phonemes at sequential index `idx` (0..size()-1).
  const std::string& phonemesAt(size_t idx) const { return entries_[idx].second; }

 private:
  std::vector<Entry, Allocator> entries_;

  typename std::vector<Entry, Allocator>::iterator find(const std::string& word) {
    return std::lower_bound(entries_.begin(), entries_.end(), word,
                            [](const Entry& e, const std::string& w) { return e.first < w; });
  }
  typename std::vector<Entry, Allocator>::const_iterator find(const std::string& word) const {
    return std::lower_bound(entries_.begin(), entries_.end(), word,
                            [](const Entry& e, const std::string& w) { return e.first < w; });
  }
};
