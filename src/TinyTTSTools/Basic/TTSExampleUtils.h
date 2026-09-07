/**
 * @file TTSExampleUtils.h
 * @brief Small helpers to remove boilerplate duplicated across example
 * sketches (Serial startup, I2S output setup from a TTSConfig).
 * @details Optional and not pulled in by TinyTTSTools.h -- include it only
 * from a sketch that already includes AudioTools.h, so the core library
 * headers stay free of any AudioTools-specific dependency.
 *
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include "TTSTypes.h"

namespace TTSExample {

/// Start Serial and block until a host is connected (as every example did by
/// hand): `Serial.begin(baud); while (!Serial) delay(10);`
inline void waitForSerial(long baud = 115200) {
  Serial.begin(baud);
  while (!Serial) {
    delay(10);
  }
}

/// Configure and begin an I2S-like AudioTools output stream (I2SStream,
/// MiniAudioStream, ...) from explicit sample rate/bit depth/channel count.
template <typename I2SStreamT>
inline void beginI2S(I2SStreamT& out, uint32_t sampleRate,
                     uint8_t bitsPerSample, uint8_t channels) {
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sample_rate = sampleRate;
  cfg.bits_per_sample = bitsPerSample;
  cfg.channels = channels;
  out.begin(cfg);
}

/// Configure and begin an I2S-like AudioTools output stream from a TTSConfig
/// (e.g. `tts.getConfig()`).
template <typename I2SStreamT>
inline void beginI2S(I2SStreamT& out, const TTSConfig& config) {
  beginI2S(out, config.sample_rate, config.bits_per_sample, config.channels);
}

}  // namespace TTSExample
