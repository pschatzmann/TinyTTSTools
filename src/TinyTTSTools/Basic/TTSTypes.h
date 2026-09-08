/**
 * @file TTSTypes.h
 * @brief Core types and structures for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdint>
#include <string>

#define TTS_DEFAULT_SAMPLE_RATE 8000

/**
 * @brief Supported phoneme representation types
 */
enum class PhonemeType {
  ARPAbet,  ///< ARPAbet phoneme representation (e.g., "AA", "B", "CH")
  IPA,      ///< International Phonetic Alphabet representation (e.g., "ɑ", "b", "tʃ")
  XSAMPA    ///< X-SAMPA phoneme representation (e.g., "A", "b", "tS")
};

/**
 * @brief Audio data callback function type
 * @param pcmData Pointer to 16-bit PCM audio samples
 * @param numSamples Number of samples in the buffer
 * @param sample_rate Sample rate of the audio data in Hz
 * @details This callback is called whenever audio data is ready for output.
 *          Implement this to send audio to your DAC, PWM, or audio codec.
 */
typedef void (*AudioDataCallback)(const int16_t* pcmData, size_t numSamples);

/**
 * @brief Speech completion callback function type
 * @details Called when speech synthesis is completed
 */
typedef void (*SpeechCompleteCallback)();

/**
 * @brief Speech error callback function type
 * @param error Error message string
 * @details Called when an error occurs during speech synthesis
 */
typedef void (*SpeechErrorCallback)(const char* error);

/**
 * @brief Phoneme Dictionary Entry (stored in PROGMEM)
 * @details Structure for storing word-to-phoneme mappings in program memory
 */
struct PhonemeEntry {
  const char* word;      ///< Word string
  const char* phonemes;  ///< Phoneme sequence string
};


/**
 * @brief TTS Configuration structure
 * @details Main configuration for the TTS engine
 */
struct TTSConfig {
  uint32_t sample_rate = TTS_DEFAULT_SAMPLE_RATE;  ///< Audio sample rate in Hz
  uint8_t bits_per_sample = 16;                    ///< Audio bit depth
  uint8_t channels = 1;  ///< Number of audio channels (1=mono, 2=stereo)
};


/**
 * @brief Phoneme classification (not representation)
 * @details Distinguishes between regular phonemes and special silence markers.
 */
enum class PhonemeClass { Phoneme, Silence };

/**
 * @brief Structure for phoneme representation mapping
 * @details Maps a single phoneme across three different representation
 * formats
 */
struct PhonemeInfo {
  const char* arpabet;   ///< ARPAbet representation (ASCII-based)
  const char* ipa;       ///< IPA representation (Unicode symbols)
  const char* xsampa;    ///< X-SAMPA representation (ASCII-based IPA approximation)
  uint16_t duration_ms;  ///< Default rendering time in milliseconds
  PhonemeClass category; ///< Classification (phoneme vs silence)
  uint16_t id;           ///< Sequential ID
};
