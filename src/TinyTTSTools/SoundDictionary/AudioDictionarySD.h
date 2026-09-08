#pragma once

#include <SD.h>

#include <string>
#include <vector>

#include "AudioDictionary.h"
#include "SoundEntry.h"
#include "../Memory/PsramAllocator.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief SoundEntry implementation that reads audio data directly from SD card
 * file on-demand: The file must contain PCM 8bit unsigned or 16 bit signed audio
 * data.
 * @tparam Allocator Allocator for the loaded-file buffer (`raw_data_`),
 * which holds one phoneme's raw audio for as long as it's in use. Defaults
 * to `std::allocator<uint8_t>`; pass `PsramAllocator<uint8_t>` (see
 * Memory/PsramAllocator.h) to load it into PSRAM on ESP32 instead.
 *
 * This class extends SoundEntry to provide direct file access without any
 * caching or preloading. Files are accessed by deduced filename using Arduino
 * SD library. Data is stored in original format and converted to 16-bit on
 * access.
 */
template <typename Allocator = std::allocator<uint8_t>>
class SDSoundEntry : public SoundEntry {
 public:
  SDSoundEntry(size_t header_size = 44, int bits_per_sample = 16)
      : SoundEntry(nullptr, 0, nullptr, bits_per_sample),
        wav_header_size_(header_size),
        length_(0) {}

  /**
   * @brief Load phoneme data from the specified file path
   * @param phoneme_name The phoneme name to set
   * @param filepath The file path to load from
   */
  void loadPhoneme(const char* phoneme_name, const char* filepath) {
    name = phoneme_name;
    file_path_ = filepath;
    loadAudioData();
  }

  /**
   * @brief Get number of samples in the audio data
   * @return Number of samples
   */
  size_t samples() const override { return length_; }

  /**
   * @brief Release loaded audio data to free memory
   */
  void releaseData() {
    raw_data_.clear();
    raw_data_.shrink_to_fit();
    data = nullptr;
  }

 private:
  const char* file_path_;
  size_t wav_header_size_;
  size_t length_;
  mutable std::vector<uint8_t, Allocator>
      raw_data_;  // Vector to store raw audio data as bytes

  void loadAudioData() {
    // Reset previous data
    length_ = 0;
    size = 0;

    File file = SD.open(file_path_, FILE_READ);
    if (!file) {
      TTS_LOGE("SDSoundEntry: failed to open '%s'", file_path_);
      return;
    }

    // Calculate length from file size
    size_t file_size = file.size();
    if (file_size <= wav_header_size_) {
      TTS_LOGE("SDSoundEntry: '%s' is %zu bytes, too small for a %zu-byte "
                "header", file_path_, file_size, wav_header_size_);
      file.close();
      return;
    }

    size_t audio_data_size = file_size - wav_header_size_;
    size = audio_data_size;
    size_t bytes_per_sample = bits / 8;
    if (bytes_per_sample == 0) {
      // bits < 8 (e.g. 4 for IMA-ADPCM) isn't a raw-PCM format this class
      // supports (see class doc) -- would otherwise divide by zero below.
      TTS_LOGE("SDSoundEntry: unsupported bits=%u for '%s' (this class only "
                "reads raw PCM8/PCM16, not compressed formats)",
                static_cast<unsigned>(bits), file_path_);
      file.close();
      return;
    }
    length_ = audio_data_size / bytes_per_sample;

    if (length_ == 0) {
      TTS_LOGE("SDSoundEntry: '%s' has no audio data after the header",
                file_path_);
      file.close();
      return;
    }

    // Skip WAV header
    file.seek(wav_header_size_);

    // Calculate raw data size in bytes
    size_t raw_data_size = length_ * bytes_per_sample;

    // Resize vector to hold raw audio data
    raw_data_.resize(raw_data_size);

    // Read raw audio data as bytes
    size_t bytes_read = file.read(raw_data_.data(), raw_data_size);

    // Resize to actual bytes read if incomplete
    if (bytes_read < raw_data_size) {
      TTS_LOGW("SDSoundEntry: '%s' short read (%zu of %zu bytes)",
               file_path_, bytes_read, raw_data_size);
      raw_data_.resize(bytes_read);
      // Update length_ based on actual data read
      length_ = bytes_read / bytes_per_sample;
    }

    file.close();

    // Set data pointer for parent class access
    data = raw_data_.data();
  }
};

/**
 * @brief AudioDictionary implementation with minimal RAM usage and direct SD
 * card file access
 * @tparam Allocator Allocator for the single reusable `SDSoundEntry`'s
 * loaded-file buffer. Defaults to `std::allocator<uint8_t>`; pass
 * `PsramAllocator<uint8_t>` (see Memory/PsramAllocator.h) to load it into
 * PSRAM on ESP32 instead.
 *
 * This class extends AudioDictionary to provide audio samples from SD card
 * files with minimal memory footprint. Files are accessed directly by deduced
 * filename without any indexing or caching.
 */
template <typename Allocator = std::allocator<uint8_t>>
class AudioDictionarySD : public AudioDictionary {
 public:
  /**
   * @brief Constructor
   * @param basePath Base directory path on SD card where audio files are stored
   * @param fileExtension File extension for audio files (default: ".wav")
   * @param sampleRateHz Sample rate in Hz (default: 8000)
   * @param phonemeType Type of phonemes (default: ARPAbet)
   * @param channels Number of audio channels (default: 1)
   * @param bitsPerSample Number of bits per sample (default: 16)
   */
  AudioDictionarySD(const char* basePath = "/audio/",
                    const char* fileExtension = ".wav", int sampleRateHz = 8000,
                    PhonemeType phonemeType = PhonemeType::ARPAbet,
                    int channels = 1, int bitsPerSample = 16, int wavHeaderSize = 44, int maxPathSize = 32)
      : AudioDictionary(nullptr, 0, sampleRateHz, phonemeType, channels,
                        bitsPerSample),
        base_path_(basePath),
        file_extension_(fileExtension),
        wav_header_size_(wavHeaderSize),
        current_entry_(wavHeaderSize, bitsPerSample) {

    // reserve enough space for the file path
    file_path_.reserve(maxPathSize);
  }

  /**
   * @brief Initialize SD card
   * @param cs_pin Chip select pin for SD card
   * @return true if initialization successful, false otherwise
   */
  bool begin(int cs_pin = 10) {
    bool ok = SD.begin(cs_pin);
    if (!ok) TTS_LOGE("AudioDictionarySD: SD.begin(%d) failed", cs_pin);
    return ok;
  }

  /**
   * @brief Get the SoundEntry for a specific phoneme
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

    // Update the file path in current_entry_ and load data
    current_entry_.loadPhoneme(phoneme, file_path_.c_str());

    // Return pointer only if file exists (samples > 0)
    return (current_entry_.samples() > 0) ? &current_entry_ : nullptr;
  }

 private:
  const char* base_path_;
  const char* file_extension_;
  size_t wav_header_size_;

  // Single entry to minimize memory usage
  SDSoundEntry<Allocator> current_entry_;
  mutable std::string file_path_;
};