#pragma once
#include <stddef.h>
#include <stdint.h>
#include "TTSTypes.h"

#if !defined(ARDUINO) && !defined(IS_MIN_DESKTOP)
/**
 * @brief Mock Print class for non-Arduino environments
 * @details Provides compatibility when Arduino's Print class is not available
 */
class Print {
 public:
  /**
   * @brief Pure virtual write method
   * @param data Pointer to data buffer to write
   * @param size Size of data in bytes
   * @return Number of bytes written
   */
  virtual size_t write(const uint8_t* data, size_t size) = 0;
};
#endif

/**
 * @brief TTS audio output using callback function
 * @details Allows custom audio handling through user-provided callback functions
 */
class TTSAudioOutputCallback : public Print {
 public:
  /**
   * @brief Constructor with audio callback
   * @param callback Function to handle audio data output
   */
  TTSAudioOutputCallback(AudioDataCallback callback)
      : audioCallback_(callback) {}
  /**
   * @brief Set audio output callback
   * @param callback Function to call with audio data
   * @details The callback receives PCM audio data, sample count, and sample
   * rate. This is the primary method for audio output.
   */
  void setAudioCallback(AudioDataCallback callback) {
    audioCallback_ = callback;
  }

  /**
   * @brief Write audio data using the configured callback
   * @param data Pointer to audio data buffer
   * @param size Size of data in bytes
   * @return Number of bytes processed (currently returns 0)
   * @note Currently commented out - implementation needs to be completed
   */
  size_t write(const uint8_t* data, size_t size) override {
    audioCallback_((const int16_t*)data, size/sizeof(int16_t));
    return size;
  }

 protected:
  AudioDataCallback audioCallback_ = nullptr; ///< Callback function for audio output
};

