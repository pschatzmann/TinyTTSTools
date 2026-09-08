/**
 * @file CompressedPhonemeDictionary.h
 * @brief Further-compressed word->phoneme dictionary: blocked offset index
 * + Huffman-coded phoneme data.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-07
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include "../Basic/Phonemes.h"
#include "PhonemeDictionaryBase.h"
#include "PhonemeHuffmanCodes.h"

/**
 * @brief Same word->phoneme lookup as CompactPhonemeDictionary, but smaller
 * on flash for large (>10k word) datasets, at the cost of some CPU work
 * per lookup (never during binary search itself -- see below).
 * @details CompactPhonemeDictionary's biggest single cost is its offset
 * arrays: two 4-byte cumulative offsets per word (8 bytes/word) just to
 * support O(1) random access during binary search. This class replaces
 * that with a blocked index instead:
 *  - one real 4-byte offset every `BLOCK_SIZE` words (a "block anchor")
 *  - a 1-byte length/count for every word in between
 * Resolving word i's position means jumping to its block's anchor, then
 * summing at most `BLOCK_SIZE-1` single-byte lengths -- a handful of
 * additions, done at every binary-search step, but far cheaper than the
 * flash saved (8 bytes/word -> ~(4/BLOCK_SIZE + 1) bytes/word).
 *
 * Phoneme data is additionally Huffman-coded (see PhonemeHuffmanCodes.h for
 * the readable code table, derived from real corpus frequencies) as a
 * packed bitstream instead of 1 byte/phoneme. Codes are only decoded
 * *after* a word is already found via binary search -- word comparisons
 * never touch the phoneme bitstream -- so this doesn't add per-comparison
 * cost, only a bounded decode-and-skip within the target word's block to
 * find where its phoneme bits start.
 * @note Memory footprint: the full 123k-word CMU dictionary
 * (`COMPACT_CMUDICT_EN`) measures ~1.74MB (measured: 1,820,804 bytes) in
 * this format -- roughly 15 bytes/word, the large majority of which is the
 * raw word text itself (Huffman coding only shrinks the phoneme data, not
 * the words). See
 * https://github.com/pschatzmann/TinyTTSTools/blob/main/docs/MEMORY.md for a
 * comparison table across all vocoders and G2P models, and
 * CompressedPhonemeDictionarySD.h to load this from SD/PSRAM instead of
 * flash.
 */
class CompressedPhonemeDictionary : public PhonemeDictionaryBase {
 public:
  static constexpr size_t BLOCK_SIZE = 8;

  CompressedPhonemeDictionary() = default;

  CompressedPhonemeDictionary(const uint32_t* wordBlockOffsets, const uint8_t* wordLengths,
                              const char* wordsBlob, const uint32_t* phonemeBlockBitOffsets,
                              const uint8_t* phonemeCounts, const uint8_t* phonemeBits,
                              size_t count) {
    begin(wordBlockOffsets, wordLengths, wordsBlob, phonemeBlockBitOffsets, phonemeCounts,
          phonemeBits, count);
  }

  /// (Re)point this dictionary at the six backing arrays. None of the
  /// pointers are copied -- they must outlive this object. Lets a loader
  /// (e.g. one that reads these arrays from a file into an owned buffer)
  /// construct this object default, then bind it once the data is ready.
  bool begin(const uint32_t* wordBlockOffsets, const uint8_t* wordLengths,
            const char* wordsBlob, const uint32_t* phonemeBlockBitOffsets,
            const uint8_t* phonemeCounts, const uint8_t* phonemeBits, size_t count) {
    wordBlockOffsets_ = wordBlockOffsets;
    wordLengths_ = wordLengths;
    wordsBlob_ = wordsBlob;
    phonemeBlockBitOffsets_ = phonemeBlockBitOffsets;
    phonemeCounts_ = phonemeCounts;
    phonemeBits_ = phonemeBits;
    count_ = count;
    return count_ > 0;
  }

  size_t size() const override { return count_; }

  bool lookup(const std::string& word, std::string& outPhonemes) const override {
    if (count_ == 0) return false;
    int32_t lo = 0, hi = static_cast<int32_t>(count_) - 1;
    while (lo <= hi) {
      int32_t mid = lo + (hi - lo) / 2;
      uint32_t start = wordByteOffset(static_cast<size_t>(mid));
      uint8_t len = wordLengths_[mid];
      int cmp = compareWordAt(start, len, word);
      if (cmp == 0) {
        decodePhonemesAt(static_cast<size_t>(mid), outPhonemes);
        return true;
      } else if (cmp < 0) {
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
    return false;
  }

  using PhonemeDictionaryBase::lookup;  // bring in the single-arg convenience overload

  std::string wordAt(size_t idx) const {
    uint32_t start = wordByteOffset(idx);
    return std::string(wordsBlob_ + start, wordLengths_[idx]);
  }

  std::string phonemesAt(size_t idx) const {
    std::string out;
    decodePhonemesAt(idx, out);
    return out;
  }

 protected:
  const uint32_t* wordBlockOffsets_ = nullptr;
  const uint8_t* wordLengths_ = nullptr;
  const char* wordsBlob_ = nullptr;
  const uint32_t* phonemeBlockBitOffsets_ = nullptr;
  const uint8_t* phonemeCounts_ = nullptr;
  const uint8_t* phonemeBits_ = nullptr;
  size_t count_ = 0;

  uint32_t wordByteOffset(size_t idx) const {
    size_t block = idx / BLOCK_SIZE;
    uint32_t off = wordBlockOffsets_[block];
    for (size_t i = block * BLOCK_SIZE; i < idx; ++i) off += wordLengths_[i];
    return off;
  }

  int compareWordAt(uint32_t start, uint8_t len, const std::string& word) const {
    const char* w = wordsBlob_ + start;
    size_t minLen = static_cast<size_t>(len) < word.size() ? len : word.size();
    int cmp = std::memcmp(w, word.data(), minLen);
    if (cmp != 0) return cmp;
    return static_cast<int>(len) - static_cast<int>(word.size());
  }

  /**
   * @brief Small runtime-built (once) canonical-Huffman decode table
   * @details Derived from the readable PHONEME_HUFFMAN_CODES array.
   */
  struct DecodeTables {
    static constexpr uint8_t kMaxLen = 16;
    uint32_t firstCode[kMaxLen + 1] = {};
    uint8_t countByLen[kMaxLen + 1] = {};
    uint8_t symbolsByLen[kMaxLen + 1][PHONEME_HUFFMAN_CODE_COUNT] = {};
    uint8_t maxLen = 0;
  };

  static const DecodeTables& decodeTables() {
    static const DecodeTables t = [] {
      DecodeTables d;
      for (size_t i = 0; i < PHONEME_HUFFMAN_CODE_COUNT; ++i) {
        const auto& e = PHONEME_HUFFMAN_CODES[i];
        if (d.countByLen[e.length] == 0) d.firstCode[e.length] = e.code;
        d.symbolsByLen[e.length][d.countByLen[e.length]] = e.packedByte;
        d.countByLen[e.length]++;
        if (e.length > d.maxLen) d.maxLen = e.length;
      }
      return d;
    }();
    return t;
  }

  uint8_t getBit(uint32_t bitIndex) const {
    uint32_t byteIdx = bitIndex >> 3;
    uint8_t bitPos = 7 - static_cast<uint8_t>(bitIndex & 7);
    return (phonemeBits_[byteIdx] >> bitPos) & 1;
  }

  /// Decode one Huffman symbol starting at `bitOffset`; returns its bit length.
  uint8_t decodeOneSymbol(uint32_t bitOffset, uint8_t& symOut) const {
    const DecodeTables& t = decodeTables();
    uint32_t code = 0;
    for (uint8_t len = 1; len <= t.maxLen; ++len) {
      code = (code << 1) | getBit(bitOffset + len - 1);
      uint8_t cnt = t.countByLen[len];
      if (cnt > 0 && code >= t.firstCode[len] && (code - t.firstCode[len]) < cnt) {
        symOut = t.symbolsByLen[len][code - t.firstCode[len]];
        return len;
      }
    }
    symOut = 0;
    return 1;  // malformed stream -- shouldn't happen with valid data
  }

  uint32_t phonemeBitOffset(size_t idx) const {
    size_t block = idx / BLOCK_SIZE;
    uint32_t bitOff = phonemeBlockBitOffsets_[block];
    for (size_t i = block * BLOCK_SIZE; i < idx; ++i) {
      uint8_t cnt = phonemeCounts_[i];
      for (uint8_t k = 0; k < cnt; ++k) {
        uint8_t sym;
        bitOff += decodeOneSymbol(bitOff, sym);
      }
    }
    return bitOff;
  }

  void decodePhonemesAt(size_t idx, std::string& out) const {
    out.clear();
    uint32_t bitOff = phonemeBitOffset(idx);
    uint8_t cnt = phonemeCounts_[idx];
    static const Phonemes phonemeTable;
    for (uint8_t k = 0; k < cnt; ++k) {
      uint8_t sym;
      bitOff += decodeOneSymbol(bitOff, sym);
      uint8_t id = sym >> 2;
      uint8_t stress = sym & 0x3;
      const PhonemeInfo* info = phonemeTable.getPhonemeById(id);
      if (!info) continue;
      if (!out.empty()) out += ' ';
      out += info->arpabet;
      if (stress != 0) out += static_cast<char>('0' + stress);
    }
  }
};
