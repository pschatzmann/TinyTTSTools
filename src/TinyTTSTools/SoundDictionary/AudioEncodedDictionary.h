/**
 * @file AudioEncodedDictionary.h
 * @brief AudioDictionary implementation for encoded audio data
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-08-25
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 * 
 * This file defines the AudioEncodedDictionary class that provides support
 * for audio dictionaries containing encoded audio data (MP3, AAC, etc.)
 * with on-demand decoding capabilities.
 */

#pragma once
#include <AudioToolsConfig.h>
#include <AudioTools/AudioCodecs/AudioCodecsBase.h>
#include <vector>
#include <memory>

#include "AudioDictionary.h"
#include "SoundEntry.h"
#include "../Memory/PsramAllocator.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief Audio dictionary for encoded audio data with on-demand decoding
 * @tparam Allocator Allocator for the decoded-PCM buffer (`decoded_buffer_`),
 * which holds one phoneme's fully-decoded audio at a time. Defaults to
 * `std::allocator<uint8_t>` (ordinary heap/internal RAM); pass
 * `PsramAllocator<uint8_t>` (see Memory/PsramAllocator.h) to place it in
 * PSRAM on ESP32 instead.
 *
 * This class provides an AudioDictionary implementation that can handle
 * encoded audio data (MP3, AAC, etc.) and decode it on-demand using
 * the provided AudioDecoder. The phoneme data is typically provided as
 * an array of SoundEntry objects stored in PROGMEM to conserve RAM.
 */
template <typename Allocator = std::allocator<uint8_t>>
class AudioEncodedDictionary : public AudioDictionary, public AudioOutput {
 public:
  /**
   * @brief Constructor for encoded audio dictionary
   * @param decoder Reference to AudioDecoder for decoding
   * @param phonemes Array of SoundEntry objects containing encoded data (typically stored in PROGMEM)
   * @param numPhonemes Number of phoneme entries in the array
   * @param phonemeType Type of phonemes (default: ARPAbet)
   */
  AudioEncodedDictionary(AudioDecoder& decoder,
                        const SoundEntry* phonemes,
                        size_t numPhonemes,
                        PhonemeType phonemeType = PhonemeType::ARPAbet)
      : AudioDictionary(nullptr, 0, 0, phonemeType, 0, 0),
        decoder_(decoder),
        phonemes_(phonemes),
        num_entries_(numPhonemes),
        current_sound_entry_("", 0, nullptr, 16) {
  }

  /**
   * @brief Get the SoundEntry for a specific phoneme
   * @param phoneme The phoneme name to look up
   * @return Pointer to SoundEntry if found, nullptr otherwise
   */
  SoundEntry* getSoundEntry(const char* phoneme) override {
    if (!phoneme) return nullptr;
    
    for (size_t i = 0; i < num_entries_; i++) {
      const SoundEntry& entry = phonemes_[i];
      if (std::strcmp(entry.name, phoneme) == 0) {
        return decode(entry.data, entry.size);
      }
    }
    return nullptr;
  }

  /**
   * @brief Get the sample rate of the audio data from decoder
   * @return Sample rate in Hz
   */
  int sampleRate() const override {
    return decoder_.audioInfo().sample_rate;
  }

  /**
   * @brief Get the number of audio channels from decoder
   * @return Number of audio channels
   */
  int channels() const override {
    return decoder_.audioInfo().channels;
  }

  /**
   * @brief Get the number of bits per sample from decoder
   * @return Number of bits per sample
   */
  int bitsPerSample() const override {
    return decoder_.audioInfo().bits_per_sample;
  }

  /**
   * @brief Release all decoded data to free memory
   * This can be called to free memory after synthesis is complete
   */
  virtual void releaseAllDecodedData() {
    decoded_buffer_.clear();
    decoded_buffer_.shrink_to_fit();
  }

  /**
   * @brief Write decoded PCM data to the internal buffer
   * This method is called by the AudioDecoder during decoding
   * @param decoded_data Pointer to decoded PCM data
   * @param size Number of bytes to write
   * @return Number of bytes written
   */
  size_t write(const uint8_t* decoded_data, size_t size) override {
    decoded_buffer_.insert(decoded_buffer_.end(), decoded_data, decoded_data + size);
    return size;
  }

 protected:
  /**
   * @brief Decode encoded audio data
   * @param data Pointer to encoded audio data
   * @param size Size of encoded data in bytes
   * @return Pointer to SoundEntry if successful, nullptr otherwise
   */
  SoundEntry* decode(const uint8_t* data, size_t size) {
    // Clear any previous decoded data
    decoded_buffer_.clear();

    // Set this as output for the decoder
    decoder_.setOutput(*this);

    // Initialize decoder
    if (!decoder_.begin()) {
      TTS_LOGE("AudioEncodedDictionary: decoder.begin() failed");
      return nullptr;
    }

    // Decode the encoded data
    size_t written = decoder_.write(data, size);
    if (written != size) {
      TTS_LOGE("AudioEncodedDictionary: decoder accepted %zu of %zu bytes",
                written, size);
      decoder_.end();
      return nullptr;
    }

    // Finalize decoding
    decoder_.end();

    // Update current SoundEntry with decoded data
    current_sound_entry_.data = decoded_buffer_.data();
    current_sound_entry_.size = decoded_buffer_.size();
    
    return &current_sound_entry_;
  }

 private:
  AudioDecoder& decoder_;
  const SoundEntry* phonemes_;
  size_t num_entries_;
  std::vector<uint8_t, Allocator> decoded_buffer_;
  SoundEntry current_sound_entry_;
};
