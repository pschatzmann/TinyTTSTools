/**
 * @file CompressedPhonemeDictionaryWide.h
 * @brief Compressed word->phoneme dictionary for the widened (uint16_t,
 * international-id + modifier capable) packed symbol -- a sibling to
 * CompressedPhonemeDictionary.h, not a replacement.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../Basic/PhonemeModifiers.h"
#include "../Basic/Phonemes.h"
#include "PhonemeDictionaryBase.h"

/**
 * @brief One packed phoneme symbol's canonical Huffman code, for the
 * widened uint16_t packing CompactPhonemeDictionaryBuilder.h's Seg() uses:
 * `(phone_id << 2) | stress`, with a non-stress modifier OR'd in at bit 9
 * (see Seg()'s own doc). Unlike PhonemeHuffmanCodes.h's `PhonemeHuffmanCode`
 * (which backs ONLY the English CMU-derived CompressedPhonemeDictionary,
 * deliberately untouched by this), a table of these is corpus-specific --
 * every language gets its own array of real frequency-derived codes, and
 * CompressedPhonemeDictionaryWide takes that array as a constructor
 * argument instead of assuming a single global table.
 */
struct PhonemeHuffmanCodeWide {
  uint16_t packedSymbol;
  uint32_t code;  // wider than PhonemeHuffmanCode's uint16_t -- a richer,
                  // multi-hundred-symbol alphabet (international ids +
                  // modifiers) can need codewords past 16 bits, unlike
                  // English's 71-symbol ARPAbet-only table
  uint8_t length;
};

/**
 * @brief Same blocked-offset-index + Huffman-coded-phonemes design as
 * CompressedPhonemeDictionary (see that file's own doc for the rationale),
 * but for the widened uint16_t packed symbol instead of the original
 * uint8_t `(id << 2) | stress` -- needed for a large (100k+ word),
 * non-English corpus that uses the international/IPA Phone id extension
 * (43-120) and/or PhonemeModifier tags, e.g. a German/French/Spanish
 * dictionary built from the OLaPh corpus (see setup/dictionary/
 * olaph_parse.py, olaph_build_huffman.py, olaph_pack_compressed.py).
 *
 * The English CMU pipeline (CompressedPhonemeDictionary.h,
 * PhonemeHuffmanCodes.h, COMPACT_CMUDICT_EN) is completely untouched by
 * this class -- it keeps its own, separate, uint8_t-symbol format and
 * single shared/global Huffman table. This class's Huffman table is NOT
 * global: since a different corpus has a different real symbol-frequency
 * distribution, each language needs its own canonical codes, passed to
 * the constructor rather than assumed -- the small per-instance decode
 * table (see DecodeTables) is therefore built once at construction time,
 * not as a shared function-local static the way
 * CompressedPhonemeDictionary::decodeTables() does it.
 * @note This class is not intended to be flash-resident/constexpr the way
 * CompactPhonemeDictionary's small hand-authored tables are -- a
 * full-corpus dictionary this size (tens of MB even compressed) is meant
 * for a desktop build or an SD-card/PSRAM-loaded embedded target, not
 * flat compiled-in flash.
 */
class CompressedPhonemeDictionaryWide : public PhonemeDictionaryBase {
 public:
  static constexpr size_t BLOCK_SIZE = 8;

  CompressedPhonemeDictionaryWide() = default;

  CompressedPhonemeDictionaryWide(const uint32_t* wordBlockOffsets, const uint8_t* wordLengths,
                                  const char* wordsBlob, const uint32_t* phonemeBlockBitOffsets,
                                  const uint8_t* phonemeCounts, const uint8_t* phonemeBits,
                                  size_t count, const PhonemeHuffmanCodeWide* huffmanCodes,
                                  size_t huffmanCodeCount) {
    begin(wordBlockOffsets, wordLengths, wordsBlob, phonemeBlockBitOffsets, phonemeCounts,
          phonemeBits, count, huffmanCodes, huffmanCodeCount);
  }

  /// (Re)point this dictionary at the six backing arrays plus this
  /// corpus's own Huffman code table. None of the pointers are copied --
  /// they must outlive this object. Builds the small runtime decode table
  /// from `huffmanCodes` immediately (not lazily), since unlike
  /// CompressedPhonemeDictionary's shared static table, this one is
  /// per-instance.
  bool begin(const uint32_t* wordBlockOffsets, const uint8_t* wordLengths,
            const char* wordsBlob, const uint32_t* phonemeBlockBitOffsets,
            const uint8_t* phonemeCounts, const uint8_t* phonemeBits, size_t count,
            const PhonemeHuffmanCodeWide* huffmanCodes, size_t huffmanCodeCount) {
    wordBlockOffsets_ = wordBlockOffsets;
    wordLengths_ = wordLengths;
    wordsBlob_ = wordsBlob;
    phonemeBlockBitOffsets_ = phonemeBlockBitOffsets;
    phonemeCounts_ = phonemeCounts;
    phonemeBits_ = phonemeBits;
    count_ = count;
    buildDecodeTables(huffmanCodes, huffmanCodeCount);
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

  /// Runtime-built (once, at begin()) canonical-Huffman decode table --
  /// per-instance, not shared, since a different corpus's table differs
  /// (see the class's own doc). Sized dynamically (std::vector), not a
  /// fixed compile-time bound: unlike English's 71-symbol ARPAbet-only
  /// alphabet, a corpus using the international/modifier id space can
  /// have a much larger, data-dependent symbol count.
  struct DecodeTables {
    static constexpr uint8_t kMaxLen = 24;
    uint32_t firstCode[kMaxLen + 1] = {};
    uint8_t countByLen[kMaxLen + 1] = {};
    std::vector<uint16_t> symbolsByLen[kMaxLen + 1];
    uint8_t maxLen = 0;
  };
  DecodeTables decodeTables_;

  void buildDecodeTables(const PhonemeHuffmanCodeWide* codes, size_t codeCount) {
    decodeTables_ = DecodeTables{};
    for (size_t i = 0; i < codeCount; ++i) {
      const auto& e = codes[i];
      if (e.length > DecodeTables::kMaxLen) continue;  // shouldn't happen; see generator
      if (decodeTables_.countByLen[e.length] == 0) decodeTables_.firstCode[e.length] = e.code;
      decodeTables_.symbolsByLen[e.length].push_back(e.packedSymbol);
      decodeTables_.countByLen[e.length]++;
      if (e.length > decodeTables_.maxLen) decodeTables_.maxLen = e.length;
    }
  }

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

  uint8_t getBit(uint32_t bitIndex) const {
    uint32_t byteIdx = bitIndex >> 3;
    uint8_t bitPos = 7 - static_cast<uint8_t>(bitIndex & 7);
    return (phonemeBits_[byteIdx] >> bitPos) & 1;
  }

  /// Decode one Huffman symbol starting at `bitOffset`; returns its bit length.
  uint8_t decodeOneSymbol(uint32_t bitOffset, uint16_t& symOut) const {
    uint32_t code = 0;
    for (uint8_t len = 1; len <= decodeTables_.maxLen; ++len) {
      code = (code << 1) | getBit(bitOffset + len - 1);
      uint8_t cnt = decodeTables_.countByLen[len];
      if (cnt > 0 && code >= decodeTables_.firstCode[len] &&
          (code - decodeTables_.firstCode[len]) < cnt) {
        symOut = decodeTables_.symbolsByLen[len][code - decodeTables_.firstCode[len]];
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
        uint16_t sym;
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
      uint16_t sym;
      bitOff += decodeOneSymbol(bitOff, sym);
      uint8_t id = (sym >> 2) & 0x7F;
      uint8_t stress = sym & 0x3;
      PhonemeModifier modifier = static_cast<PhonemeModifier>((sym >> 9) & 0x1F);
      const PhonemeInfo* info = phonemeTable.getPhonemeById(id);
      if (!info) continue;
      if (!out.empty()) out += ' ';
      std::string token = info->arpabet;
      if (stress != 0) token += static_cast<char>('0' + stress);
      else if (modifier != PhonemeModifier::MOD_NONE) token = applyModifierTag(token, modifier);
      out += token;
    }
  }
};
