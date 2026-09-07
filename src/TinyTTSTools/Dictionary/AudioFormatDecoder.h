/**
 * @file AudioFormatDecoder.h
 * @brief Pluggable sample-format decoders used by SoundEntry
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-07
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Strategy interface: decode raw bytes in some sample format into
 * individual 16-bit signed PCM samples.
 * @details SoundEntry delegates to one of these instead of hardcoding a
 * bits-per-sample switch, so a new format (e.g. a future MEL-spectrogram-
 * driven vocoder unit) only needs a new decoder here, not a SoundEntry
 * rewrite. Implementations are stateless singletons (see getAudioFormatDecoder()) --
 * no per-SoundEntry-instance memory cost regardless of how many of the
 * (potentially thousands of) SoundEntry values in a dictionary array exist.
 */
class AudioFormatDecoder {
 public:
  virtual ~AudioFormatDecoder() = default;

  /// Number of decodable samples in `data` (byteSize bytes).
  virtual size_t sampleCount(const uint8_t* data, size_t byteSize) const = 0;

  /// Decodes and returns the sample at `index` (0-based).
  virtual int16_t sampleAt(const uint8_t* data, size_t byteSize, size_t index) const = 0;
};

/// 8-bit unsigned PCM -> 16-bit signed (O(1) random access).
class Pcm8Decoder : public AudioFormatDecoder {
 public:
  size_t sampleCount(const uint8_t* data, size_t byteSize) const override {
    (void)data;
    return byteSize;
  }
  int16_t sampleAt(const uint8_t* data, size_t byteSize, size_t index) const override {
    (void)byteSize;
    uint8_t sample8 = data[index];
    return static_cast<int16_t>((static_cast<int32_t>(sample8) - 128) * 256);
  }
};

/// 16-bit signed PCM, little-endian, stored as-is (O(1) random access).
class Pcm16Decoder : public AudioFormatDecoder {
 public:
  size_t sampleCount(const uint8_t* data, size_t byteSize) const override {
    (void)data;
    return byteSize / 2;
  }
  int16_t sampleAt(const uint8_t* data, size_t byteSize, size_t index) const override {
    (void)byteSize;
    return reinterpret_cast<const int16_t*>(data)[index];
  }
};

/**
 * @brief IMA-ADPCM (WAVE_FORMAT_IMA_ADPCM / DVI-ADPCM), 4 bits/sample,
 * matching what `sox -e ima-adpcm` produces (the diphone/phoneme data
 * pipelines under setup/audio/*).
 * @details Blocked format: every kBlockAlign-byte block starts with a
 * 4-byte header (initial predictor: int16 LE, initial step index: uint8,
 * reserved: uint8) whose predictor value IS the block's first sample,
 * followed by (kBlockAlign-4)*2 nibble-coded samples (low nibble of each
 * byte decoded before the high nibble). kBlockAlign is a sox encoding
 * parameter, not stored in the stripped-down data-chunk-only byte arrays
 * this project embeds -- it's fixed at 256 here because every WAV in
 * setup/audio/{diphones,phonemes-from-espeak} was encoded the same way
 * (verified via each file's own fmt chunk before stripping); confirm this
 * still holds if the encoding command ever changes.
 *
 * Unlike Pcm8Decoder/Pcm16Decoder, decoding sample N requires replaying
 * every nibble in its block from the start (the predictor/step state is
 * cumulative) -- O(index within block) per call, not O(1). Diphone/phoneme
 * units are short (a few hundred samples), so this is not a practical
 * bottleneck, but it's why this class exists separately rather than as a
 * third case bolted onto Pcm8Decoder/Pcm16Decoder's O(1) pattern.
 */
class ImaAdpcmDecoder : public AudioFormatDecoder {
 public:
  static constexpr size_t kBlockAlign = 256;
  static constexpr size_t kSamplesPerBlock = (kBlockAlign - 4) * 2 + 1;

  size_t sampleCount(const uint8_t* data, size_t byteSize) const override {
    (void)data;
    size_t fullBlocks = byteSize / kBlockAlign;
    size_t remBytes = byteSize % kBlockAlign;
    size_t count = fullBlocks * kSamplesPerBlock;
    if (remBytes >= 4) {
      count += (remBytes - 4) * 2 + 1;
    }
    return count;
  }

  int16_t sampleAt(const uint8_t* data, size_t byteSize, size_t index) const override {
    (void)byteSize;
    size_t block = index / kSamplesPerBlock;
    size_t withinBlock = index % kSamplesPerBlock;
    const uint8_t* blockData = data + block * kBlockAlign;

    int32_t predictor = static_cast<int16_t>(
        static_cast<uint16_t>(blockData[0]) | (static_cast<uint16_t>(blockData[1]) << 8));
    if (withinBlock == 0) return static_cast<int16_t>(predictor);

    int8_t stepIndex = static_cast<int8_t>(blockData[2]);
    for (size_t i = 1; i <= withinBlock; ++i) {
      size_t nibbleIndex = i - 1;
      uint8_t byte = blockData[4 + nibbleIndex / 2];
      uint8_t code = (nibbleIndex % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
      predictor = decodeStep(code, predictor, stepIndex);
    }
    return static_cast<int16_t>(predictor);
  }

 private:
  static int32_t decodeStep(uint8_t code, int32_t predictor, int8_t& stepIndex) {
    static constexpr int16_t kStepTable[89] = {
        7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,
        21,    23,    25,    28,    31,    34,    37,    41,    45,    50,    55,
        60,    66,    73,    80,    88,    97,    107,   118,   130,   143,   157,
        173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,
        494,   544,   598,   658,   724,   796,   876,   963,   1060,  1166,  1282,
        1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,  3660,
        4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442,
        11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
        32767};
    static constexpr int8_t kIndexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8,
                                               -1, -1, -1, -1, 2, 4, 6, 8};

    int32_t step = kStepTable[stepIndex];
    int32_t diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    if (code & 8) predictor -= diff;
    else predictor += diff;

    if (predictor > 32767) predictor = 32767;
    if (predictor < -32768) predictor = -32768;

    stepIndex = static_cast<int8_t>(stepIndex + kIndexTable[code]);
    if (stepIndex < 0) stepIndex = 0;
    if (stepIndex > 88) stepIndex = 88;

    return predictor;
  }
};

/// Returns the stateless singleton decoder for `bits` (8, 16, or 4 for
/// IMA-ADPCM -- 4 matches the real bitsPerSample IMA-ADPCM WAV files
/// declare in their own fmt chunk, so it's not an arbitrary sentinel).
/// Returns nullptr for an unrecognized value.
inline const AudioFormatDecoder* getAudioFormatDecoder(uint8_t bits) {
  static const Pcm8Decoder pcm8;
  static const Pcm16Decoder pcm16;
  static const ImaAdpcmDecoder adpcm;
  switch (bits) {
    case 8: return &pcm8;
    case 16: return &pcm16;
    case 4: return &adpcm;
    default: return nullptr;
  }
}
