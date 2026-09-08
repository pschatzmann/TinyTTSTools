#pragma once

#include <SD.h>
#include <AudioToolsConfig.h>
#include <AudioTools/AudioCodecs/AudioCodecsBase.h>

#include <string>
#include <vector>

#include "AudioEncodedDictionary.h"
#include "../Memory/PsramAllocator.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief AudioDictionary implementation for encoded audio files on SD card
 * @tparam Allocator Allocator for both the raw file-read buffer
 * (`sd_buffer_`) and the inherited decoded-PCM buffer. Defaults to
 * `std::allocator<uint8_t>`; pass `PsramAllocator<uint8_t>` (see
 * Memory/PsramAllocator.h) to keep both buffers off internal RAM -- the
 * natural choice here, since a whole encoded file plus its fully-decoded
 * PCM can be sizeable, and this class already reloads/redecodes on every
 * phoneme lookup rather than caching, so PSRAM's extra access latency is
 * incurred once per lookup, not per sample.
 *
 * This class extends AudioEncodedDictionary to load encoded audio data from
 * SD card files on-demand. Files are accessed by constructing filename from
 * phoneme name using the base path and file extension.
 */
template <typename Allocator = std::allocator<uint8_t>>
class AudioEncodedDictionarySD : public AudioEncodedDictionary<Allocator> {
 public:
  /**
   * @brief Constructor
   * @param decoder Reference to the AudioDecoder instance for decoding
   * @param basePath Base directory path on SD card where audio files are stored
   * @param fileExtension File extension for audio files
   * @param phonemeType Type of phonemes (default: ARPAbet)
   * @param maxPathSize Maximum file path size (default: 32)
   */
  AudioEncodedDictionarySD(AudioDecoder& decoder,
                          const char* basePath = "/audio/",
                          const char* fileExtension = ".mp3",
                          PhonemeType phonemeType = PhonemeType::ARPAbet,
                          int maxPathSize = 32)
      : AudioEncodedDictionary<Allocator>(decoder, nullptr, 0, phonemeType),
        base_path_(basePath),
        file_extension_(fileExtension) {
    // Reserve enough space for the file path
    file_path_.reserve(maxPathSize);
  }

  /**
   * @brief Initialize SD card
   * @param cs_pin Chip select pin for SD card
   * @return true if initialization successful, false otherwise
   */
  bool begin(int cs_pin = 10) {
    bool ok = SD.begin(cs_pin);
    if (!ok) TTS_LOGE("AudioEncodedDictionarySD: SD.begin(%d) failed", cs_pin);
    return ok;
  }

  /**
   * @brief Get the SoundEntry for a specific phoneme (loads from SD card)
   * @param phoneme The phoneme name to look up
   * @return Pointer to SoundEntry if found, nullptr otherwise
   */
  SoundEntry* getSoundEntry(const char* phoneme) override {
    if (!phoneme) return nullptr;

    // Build file path efficiently - clear first to reuse capacity
    file_path_.clear();
    file_path_ += base_path_;
    file_path_ += phoneme;
    file_path_ += file_extension_;

    // Load encoded data from SD card file and decode it
    if (loadFromSD(file_path_.c_str())) {
      return this->decode(sd_buffer_.data(), sd_buffer_.size());
    }

    return nullptr;
  }

  /**
   * @brief Release all loaded data to free memory
   */
  void releaseAllDecodedData() override {
    AudioEncodedDictionary<Allocator>::releaseAllDecodedData();
    sd_buffer_.clear();
    sd_buffer_.shrink_to_fit();
  }

 private:
  const char* base_path_;
  const char* file_extension_;
  mutable std::string file_path_;
  std::vector<uint8_t, Allocator> sd_buffer_;  ///< Buffer for SD card file data

  /**
   * @brief Load encoded audio data from SD card file
   * @param filePath Path to the file to load
   * @return true if loading was successful, false otherwise
   */
  bool loadFromSD(const char* filePath) {
    // Clear previous SD buffer data
    sd_buffer_.clear();
    
    // Open the file
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
      TTS_LOGE("AudioEncodedDictionarySD: failed to open '%s'", filePath);
      return false;
    }

    // Get file size
    size_t fileSize = file.size();
    if (fileSize == 0) {
      TTS_LOGE("AudioEncodedDictionarySD: '%s' is empty", filePath);
      file.close();
      return false;
    }

    // Reserve space in buffer
    sd_buffer_.reserve(fileSize);

    // Read file data into buffer
    while (file.available()) {
      sd_buffer_.push_back(file.read());
    }

    file.close();
    if (sd_buffer_.size() != fileSize) {
      TTS_LOGE("AudioEncodedDictionarySD: '%s' short read (%zu of %zu bytes)",
                filePath, sd_buffer_.size(), fileSize);
      return false;
    }
    return true;
  }
};