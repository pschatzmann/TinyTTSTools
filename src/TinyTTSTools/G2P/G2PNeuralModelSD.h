/**
 * @file G2PNeuralModelSD.h
 * @brief Runtime-loadable (SD card / LittleFS) neural G2P model
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <SD.h>

#include <cstdint>
#include <vector>

#include "G2PNeuralModel.h"
#include "../Memory/PsramAllocator.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief Loads a `G2PNeuralModel`'s weights from a single binary file (SD
 * card, LittleFS, SPIFFS -- anything the Arduino `SD`-style API can open)
 * into an owned, `Allocator`-backed buffer, instead of compiling them into
 * flash (as `Data/neural/G2PNeuralWeightsEN_data.h` does) -- same role for
 * `G2PNeuralModel` that `CompressedPhonemeDictionarySD` already plays for
 * `CompressedPhonemeDictionary`.
 * @tparam Allocator Allocator for the loaded-file buffer. Defaults to
 * `std::allocator<uint8_t>`; pass `PsramAllocator<uint8_t>` (see
 * Memory/PsramAllocator.h) to load the weights into PSRAM on ESP32 instead
 * of internal RAM -- worthwhile at ~970KB (the shipped English model; see
 * docs/MEMORY.md).
 * @details The file is exactly `export_g2p_model.py`'s `--output-bin`
 * output (`g2p_model.bin`) -- the same raw binary
 * `G2PNeuralModel::begin(buf, len)` already parses, just read from a file
 * instead of a compiled-in array. This class owns the loaded buffer and
 * points an internal `G2PNeuralModel` at it (no further copy), so the
 * buffer and the model share one lifetime -- same zero-copy convention
 * `G2PNeuralModel::begin()` itself already documents.
 *
 * Whichever weights file you load must already have a matching
 * `arpabetForIndex()`/output table compiled into `G2PNeuralModel.h` --
 * that table is fixed metadata of one specific trained model, not
 * something a weights file carries or this class infers (see
 * `G2PNeuralModel`'s own class doc, and docs/ADDING_A_LANGUAGE.md for a
 * non-English model needing its own table).
 *
 * @code
 * G2PNeuralModelSD<PsramAllocator<uint8_t>> neural;
 * neural.begin("/neural/g2p_model_en.bin");
 * g2p.getNeuralModel(); // or use `neural` directly as a G2PModelBase
 * @endcode
 */
template <typename Allocator = std::allocator<uint8_t>>
class G2PNeuralModelSD : public G2PModelBase {
 public:
  /**
   * @brief Load the model's weights from `filePath`
   * @param filePath Path to a file written by `export_g2p_model.py`'s
   * `--output-bin` (default `g2p_model.bin`)
   * @return true if the file was read and its header is valid; false on a
   * missing/short/truncated file or a malformed header (in which case this
   * model behaves as uninitialized -- `wordToPhonemes()` always returns
   * "", matching a never-`begin()`'d `G2PNeuralModel`)
   */
  bool begin(const char* filePath) {
    buffer_.clear();
    model_ = G2PNeuralModel();
    initialized_ = false;  // G2PModelBase's flag -- distinct from model_'s own

    File file = SD.open(filePath, FILE_READ);
    if (!file) {
      TTS_LOGE("G2PNeuralModelSD: failed to open '%s'", filePath);
      return false;
    }

    size_t fileSize = file.size();
    buffer_.resize(fileSize);
    size_t bytesRead = file.read(buffer_.data(), fileSize);
    file.close();
    if (bytesRead != fileSize) {
      TTS_LOGE("G2PNeuralModelSD: '%s' short read (%zu of %zu bytes)",
                filePath, bytesRead, fileSize);
      buffer_.clear();
      return false;
    }

    if (!model_.begin(buffer_.data(), buffer_.size())) {
      // G2PNeuralModel::begin() already logged the specific parse failure.
      buffer_.clear();
      return false;
    }
    initialized_ = true;
    return true;
  }

  std::string wordToPhonemes(const std::string& word) override {
    return model_.wordToPhonemes(word);
  }

 private:
  std::vector<uint8_t, Allocator> buffer_;
  G2PNeuralModel model_;
};
