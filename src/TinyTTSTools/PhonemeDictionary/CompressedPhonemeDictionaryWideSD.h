/**
 * @file CompressedPhonemeDictionaryWideSD.h
 * @brief Runtime-loadable (SD card / LittleFS) word->phoneme dictionary,
 * widened (uint16_t) packed symbol -- a sibling to
 * CompressedPhonemeDictionarySD.h, not a replacement.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <SD.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "CompressedPhonemeDictionaryWide.h"
#include "../Memory/PsramAllocator.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief Loads a `CompressedPhonemeDictionaryWide`'s six backing arrays
 * from a single binary file (SD card, LittleFS, SPIFFS) into an owned,
 * `Allocator`-backed buffer, instead of compiling them into flash (as
 * `CompactOlaphDE/FR/ES_data.h` does) -- same role for
 * `CompressedPhonemeDictionaryWide` that `CompressedPhonemeDictionarySD`
 * already plays for `CompressedPhonemeDictionary`.
 * @tparam Allocator Allocator for the loaded-file buffer. Defaults to
 * `std::allocator<uint8_t>`; pass `PsramAllocator<uint8_t>` (see
 * Memory/PsramAllocator.h) to load the whole dictionary into PSRAM on
 * ESP32 instead of internal RAM -- the natural choice at the size of a
 * full OLaPh dictionary (German ~19MB, Spanish ~11MB, French ~4.3MB; see
 * docs/MEMORY.md).
 * @details The file is produced by
 * `setup/dictionary-common/olaph_pack_compressed.py` and uses the exact
 * same binary layout `setup/dictionary-en/pack_compressed.py`'s
 * `emit_binary_file()` documents (the format is symbol-width-agnostic --
 * `phoneme_bits` is an opaque Huffman-coded bitstream either way, so one
 * file format serves both `CompressedPhonemeDictionary` and
 * `CompressedPhonemeDictionaryWide`; only the DECODER needs to know the
 * symbol width and which canonical codes to use).
 *
 * Unlike `CompressedPhonemeDictionarySD`, the Huffman code table is NOT
 * part of the loaded file -- each language's codes are corpus-specific
 * (see `CompressedPhonemeDictionaryWide`'s own doc) and small enough to
 * stay a compiled-in flash table (`PhonemeHuffmanCodesWideDE/FR/ES.h`,
 * a few KB), so `begin()` takes it as a separate argument rather than
 * expecting it inside the loaded file:
 *
 * @code
 * #include "TinyTTSTools/PhonemeDictionary/PhonemeHuffmanCodesWideDE.h"
 *
 * CompressedPhonemeDictionaryWideSD<PsramAllocator<uint8_t>> deDict;
 * deDict.begin("/dictionary/olaph_de.bin",
 *              PHONEME_HUFFMAN_CODES_WIDE_DE, PHONEME_HUFFMAN_CODE_WIDE_DE_COUNT);
 * g2p.getDictionaryModel().useCompactDictionary(deDict);
 * @endcode
 */
template <typename Allocator = std::allocator<uint8_t>>
class CompressedPhonemeDictionaryWideSD : public PhonemeDictionaryBase {
 public:
  /**
   * @brief Load the dictionary from `filePath`
   * @param filePath Path to a file written by
   * `olaph_pack_compressed.py`'s `emit_binary_file()` call
   * @param huffmanCodes This language's canonical Huffman code table (see
   * `PhonemeHuffmanCodesWideDE/FR/ES.h`) -- must outlive this object, same
   * as `filePath`'s loaded data
   * @param huffmanCodeCount Number of entries in `huffmanCodes`
   * @return true if the file was read and its header is valid; false on a
   * missing/short/truncated file or a magic/version mismatch (in which
   * case this dictionary behaves as empty -- lookup() always returns
   * false, matching a default-constructed CompressedPhonemeDictionaryWide)
   */
  bool begin(const char* filePath, const PhonemeHuffmanCodeWide* huffmanCodes,
            size_t huffmanCodeCount) {
    buffer_.clear();
    dict_ = CompressedPhonemeDictionaryWide();

    File file = SD.open(filePath, FILE_READ);
    if (!file) {
      TTS_LOGE("CompressedPhonemeDictionaryWideSD: failed to open '%s'", filePath);
      return false;
    }

    size_t fileSize = file.size();
    if (fileSize < kHeaderBytes) {
      TTS_LOGE("CompressedPhonemeDictionaryWideSD: '%s' is %zu bytes, too small "
                "for a %zu-byte header", filePath, fileSize, kHeaderBytes);
      file.close();
      return false;
    }

    buffer_.resize(fileSize);
    size_t bytesRead = file.read(buffer_.data(), fileSize);
    file.close();
    if (bytesRead != fileSize) {
      TTS_LOGE("CompressedPhonemeDictionaryWideSD: '%s' short read (%zu of %zu "
                "bytes)", filePath, bytesRead, fileSize);
      buffer_.clear();
      return false;
    }

    if (!parse(huffmanCodes, huffmanCodeCount)) {
      TTS_LOGE("CompressedPhonemeDictionaryWideSD: '%s' failed to parse (bad "
                "magic/version, or truncated data)", filePath);
      return false;
    }
    return true;
  }

  size_t size() const override { return dict_.size(); }

  bool lookup(const std::string& word, std::string& outPhonemes) const override {
    return dict_.lookup(word, outPhonemes);
  }

  using PhonemeDictionaryBase::lookup;  // bring in the single-arg convenience overload

 private:
  static constexpr size_t kHeaderBytes = 6 * sizeof(uint32_t);
  static constexpr uint32_t kMagic = 0x31445054;  // "TPD1", little-endian
  static constexpr uint32_t kVersion = 1;

  std::vector<uint8_t, Allocator> buffer_;
  CompressedPhonemeDictionaryWide dict_;

  static size_t align4(size_t n) { return (n + 3) & ~static_cast<size_t>(3); }

  /// Parses the header and re-points `dict_` at slices of `buffer_`. See
  /// emit_binary_file()'s docstring for the exact layout this must match
  /// (identical to CompressedPhonemeDictionarySD's -- see that class).
  bool parse(const PhonemeHuffmanCodeWide* huffmanCodes, size_t huffmanCodeCount) {
    const uint8_t* p = buffer_.data();
    uint32_t magic, version, count, numBlocks, wordsBlobLen, phonemeBitsLen;
    std::memcpy(&magic, p + 0, 4);
    std::memcpy(&version, p + 4, 4);
    std::memcpy(&count, p + 8, 4);
    std::memcpy(&numBlocks, p + 12, 4);
    std::memcpy(&wordsBlobLen, p + 16, 4);
    std::memcpy(&phonemeBitsLen, p + 20, 4);
    if (magic != kMagic || version != kVersion || count == 0) {
      buffer_.clear();
      return false;
    }

    size_t offset = kHeaderBytes;
    const uint32_t* wordBlockOffsets = reinterpret_cast<const uint32_t*>(p + offset);
    offset += static_cast<size_t>(numBlocks) * 4;

    const uint8_t* wordLengths = p + offset;
    offset += count;
    offset = align4(offset);

    const char* wordsBlob = reinterpret_cast<const char*>(p + offset);
    offset += wordsBlobLen;
    offset = align4(offset);

    const uint32_t* phonemeBlockBitOffsets = reinterpret_cast<const uint32_t*>(p + offset);
    offset += static_cast<size_t>(numBlocks) * 4;

    const uint8_t* phonemeCounts = p + offset;
    offset += count;
    offset = align4(offset);

    const uint8_t* phonemeBits = p + offset;
    offset += phonemeBitsLen;

    if (offset > buffer_.size()) {
      // Truncated/corrupt file -- the header claims more data than we have.
      buffer_.clear();
      return false;
    }

    dict_.begin(wordBlockOffsets, wordLengths, wordsBlob, phonemeBlockBitOffsets,
                phonemeCounts, phonemeBits, count, huffmanCodes, huffmanCodeCount);
    return true;
  }
};
