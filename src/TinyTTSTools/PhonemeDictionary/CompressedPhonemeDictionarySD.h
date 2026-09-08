/**
 * @file CompressedPhonemeDictionarySD.h
 * @brief Runtime-loadable (SD card / LittleFS) word->phoneme dictionary
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <SD.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "CompressedPhonemeDictionary.h"
#include "../Memory/PsramAllocator.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief Loads a CompressedPhonemeDictionary's six backing arrays from a
 * single binary file (SD card, LittleFS, SPIFFS -- anything the Arduino
 * `SD`-style API can open) into an owned, `Allocator`-backed buffer,
 * instead of compiling them into flash (as `CompactCmuDictionaryEN_data.h`
 * does).
 * @tparam Allocator Allocator for the loaded-file buffer. Defaults to
 * `std::allocator<uint8_t>`; pass `PsramAllocator<uint8_t>` (see
 * Memory/PsramAllocator.h) to load the whole dictionary into PSRAM on
 * ESP32 instead of internal RAM -- the natural choice for something the
 * size of the full CMU dictionary (~880KB, see docs/MEMORY.md).
 * @details The file format is produced by
 * `setup/dictionary/pack_compressed.py`'s `emit_binary_file()` -- see that
 * function's docstring for the exact byte layout. This class owns the
 * loaded buffer and re-points an internal CompressedPhonemeDictionary at
 * slices of it (no further copies), so the buffer and the dictionary share
 * one lifetime.
 *
 * Once loaded, use it exactly like any other PhonemeDictionaryBase:
 * @code
 * CompressedPhonemeDictionarySD<PsramAllocator<uint8_t>> dict;
 * dict.begin("/dictionary/cmudict.bin");
 * g2p.getDictionaryModel().useCompactDictionary(dict);
 * @endcode
 */
template <typename Allocator = std::allocator<uint8_t>>
class CompressedPhonemeDictionarySD : public PhonemeDictionaryBase {
 public:
  /**
   * @brief Load the dictionary from `filePath`
   * @param filePath Path to a file written by
   * `pack_compressed.emit_binary_file()`
   * @return true if the file was read and its header is valid; false on a
   * missing/short/truncated file or a magic/version mismatch (in which
   * case this dictionary behaves as empty -- lookup() always returns
   * false, matching a default-constructed CompressedPhonemeDictionary)
   */
  bool begin(const char* filePath) {
    buffer_.clear();
    dict_ = CompressedPhonemeDictionary();

    File file = SD.open(filePath, FILE_READ);
    if (!file) {
      TTS_LOGE("CompressedPhonemeDictionarySD: failed to open '%s'", filePath);
      return false;
    }

    size_t fileSize = file.size();
    if (fileSize < kHeaderBytes) {
      TTS_LOGE("CompressedPhonemeDictionarySD: '%s' is %zu bytes, too small "
                "for a %zu-byte header", filePath, fileSize, kHeaderBytes);
      file.close();
      return false;
    }

    buffer_.resize(fileSize);
    size_t bytesRead = file.read(buffer_.data(), fileSize);
    file.close();
    if (bytesRead != fileSize) {
      TTS_LOGE("CompressedPhonemeDictionarySD: '%s' short read (%zu of %zu "
                "bytes)", filePath, bytesRead, fileSize);
      buffer_.clear();
      return false;
    }

    if (!parse()) {
      TTS_LOGE("CompressedPhonemeDictionarySD: '%s' failed to parse (bad "
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
  CompressedPhonemeDictionary dict_;

  static size_t align4(size_t n) { return (n + 3) & ~static_cast<size_t>(3); }

  /// Parses the header and re-points `dict_` at slices of `buffer_`. See
  /// emit_binary_file()'s docstring for the exact layout this must match.
  bool parse() {
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
                phonemeCounts, phonemeBits, count);
    return true;
  }
};
