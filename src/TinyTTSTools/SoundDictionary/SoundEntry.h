/**
 * @file SoundEntry.h
 * @brief Sound entry structure for audio dictionary storage
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-08-23
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 * 
 * This file defines the SoundEntry structure used for storing audio data
 * in phoneme and diphone dictionaries with efficient sample access methods.
 */

#pragma once
#include <cstdint>
#include "AudioFormatDecoder.h"

/**
 * @brief Structure for storing audio data for phonemes or diphones
 * @details Encapsulates the name, size, and data pointer for a single audio
 *          entry in a dictionary. Used for both phoneme and diphone storage.
 *          Provides efficient access to audio samples with automatic format
 *          conversion to 16-bit signed PCM regardless of the underlying
 *          storage format (see AudioFormatDecoder.h).
 *
 * @note The audio data is stored as a pointer to const uint8_t, making it
 *       suitable for ROM storage on microcontrollers. The structure includes
 *       virtual methods for transparent sample access regardless of the
 *       underlying format.
 */
struct SoundEntry {
  /**
   * @brief Constructor to initialize a SoundEntry
   *
   * @param n Pointer to the name string
   * @param s Size of the audio data in bytes
   * @param d Pointer to the audio data array
   * @param bits Sample format selector, matching the real bitsPerSample a
   *        WAV file of that format would declare in its own fmt chunk: 8
   *        (unsigned PCM8), 16 (signed PCM16, the default), or 4
   *        (IMA-ADPCM -- see AudioFormatDecoder.h for the block layout
   *        this assumes).
   */
  SoundEntry(const char* name, uint16_t size, const uint8_t* audioData, int bits = 16)
      : name(name), size(size), data(audioData), bits(bits) {}


  /**
   * @brief Name identifier for the sound entry
   *
   * A null-terminated string that uniquely identifies this sound entry.
   * For diphones, this follows the format "PHONEME1 PHONEME2" (a space
   * between the two symbols, e.g. "AA B", "SIL T") -- a space, not an
   * underscore, specifically so a diphone key can never collide with a
   * PhonemeModifiers.h modifier tag (they're all `_`/`:`/`~`/etc. prefixed
   * or suffixed, never a bare space); see AudioDictionary::getSoundEntry()
   * for the modifier-stripping fallback this keeps unambiguous.
   */
  const char* name;

  /**
   * @brief Size of the audio data in bytes
   *
   * The total number of bytes contained in the audio data array.
   * This is used to determine the length of the audio sample.
   */
  uint16_t size;

  /**
   * @brief Pointer to the raw audio data
   *
   * Points to an array of bytes containing the raw audio data.
   * The format depends on the implementation (e.g., PCM, ADPCM).
   * The data is const to ensure it can be stored in ROM.
   */
  const uint8_t* data;

  /**
   * @brief Number of bits per audio sample
   *
   * Specifies the bit depth of the audio data (typically 8 or 16 bits).
   * Used to determine how to interpret and convert the raw audio data.
   */
  uint8_t bits;

  /**
   * @brief Get the number of audio samples in the entry
   * @return Number of samples, computed by the format's own decoder (see
   *         AudioFormatDecoder.h) -- 0 if `bits` isn't a recognized format.
   */
  virtual size_t samples() const {
    const AudioFormatDecoder* decoder = getAudioFormatDecoder(bits);
    return decoder ? decoder->sampleCount(data, size) : 0;
  }

  /**
   * @brief Access audio sample at specified index as 16-bit signed PCM
   * @param index Zero-based index of the sample to retrieve
   * @return 16-bit signed PCM sample value
   * @details Provides transparent access to audio samples regardless of the
   *          underlying storage format -- delegates to the matching
   *          AudioFormatDecoder (PCM8/PCM16/IMA-ADPCM today; a new format
   *          only needs a new decoder there, not a change here).
   */
  virtual int16_t operator[](size_t index) const {
    const AudioFormatDecoder* decoder = getAudioFormatDecoder(bits);
    return decoder ? decoder->sampleAt(data, size, index) : 0;
  }

};
