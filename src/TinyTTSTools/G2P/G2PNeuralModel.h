/**
 * @file G2PNeuralModel.h
 * @brief Neural (GRU) grapheme-to-phoneme fallback model for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "G2PModelBase.h"
#include "../Basic/StringUtils.h"
#include "../Basic/TTSLogger.h"

/**
 * @brief Neural grapheme-to-phoneme fallback for out-of-dictionary words
 * @details A small (~833K-param) single-layer GRU encoder-decoder,
 * greedy-decoded. Ported from the sibling TinyTTS project's
 * DictionaryModel.h -- a from-scratch, dependency-free reimplementation of
 * that project's own g2p_predict.js (itself a port of the Python `g2p_en`
 * package's `G2p.predict()`), verified bit-exact against both there before
 * being trusted (see TinyTTS's research/g2p_reference_numpy.py,
 * docs/research.md).
 *
 * Unlike G2PRuleBasedModelEN (letter-to-sound rules, ~17% exact-match
 * ceiling -- English spelling can't be resolved from rules alone without a
 * stress/pronunciation model) or a static exception dictionary (only
 * covers words someone thought to list), this generalizes: it produces a
 * plausible phonetic guess for genuinely novel words -- proper nouns, made
 * up words, technical terms -- that no dictionary, however large, will
 * ever contain. Use it as the last fallback in a G2PHybridModel chain,
 * after real dictionary lookups.
 *
 * Weights (~970KB) come from an in-memory buffer (a flash-embedded const
 * array -- see Data/neural/G2PNeuralWeightsEN_data.h -- or one read from
 * LittleFS/SD at startup) passed to begin(); the caller keeps it alive for
 * as long as this object is used (no copy, same convention as the compact
 * dictionaries).
 *
 * The four GRU weight matrices (enc_w_ih/enc_w_hh/dec_w_ih/dec_w_hh, ~94%
 * of the model's params) are INT8-quantized (symmetric, per output row);
 * everything else (the two small embedding tables, biases, the output
 * projection) stays float32. Plain hand-written dequantize-on-the-fly
 * arithmetic -- no TensorFlow Lite dependency.
 *
 * The input grapheme vocabulary ('<pad>','<unk>','</s>','a'..'z') and
 * output phoneme vocabulary (see kArpabetTable) are both fixed properties
 * of this specific English model file -- a differently-trained model
 * (another language, or a retrained English one) would need its own
 * graphemeIndex() and output table, not just a different weights blob.
 * @note Memory footprint: ~970KB flash for the shipped weights (measured:
 * 992,716 bytes), no RAM overhead beyond a small decode buffer (no tensor
 * arena, no runtime library). See
 * https://github.com/pschatzmann/TinyTTSTools/blob/main/docs/MEMORY.md for a
 * comparison table across all vocoders and G2P models.
 */
class G2PNeuralModel : public G2PModelBase {
 public:
  /// Parses `buf` (the binary format G2PNeuralWeightsEN_data.h embeds)
  /// without copying -- `buf` must outlive this object. Returns false on a
  /// malformed/truncated buffer.
  bool begin(const uint8_t* buf, size_t len) {
    size_t pos = 0;
    initialized_ = parseWeights(buf, len, pos);
    if (!initialized_) {
      TTS_LOGE("G2PNeuralModel: failed to parse weights buffer "
                "(%zu bytes) -- malformed or truncated", len);
    }
    return initialized_;
  }

  /// @param word Input word (converted to lowercase; non a-z characters
  /// fall through to <unk> handling in graphemeIndex())
  /// @return Space-separated ARPAbet phoneme string, or "" if not
  /// initialized or decoding produced nothing
  std::string wordToPhonemes(const std::string& word) override {
    if (!initialized_) return "";
    std::string lower = StringUtils::toLowerCase(word);
    std::vector<int> indices = predict(lower);
    std::string out;
    for (int idx : indices) {
      const char* arp = arpabetForIndex(idx);
      if (!arp || !arp[0]) continue;
      if (!out.empty()) out += ' ';
      out += arp;
    }
    return out;
  }

 protected:
  /**
   * @brief One [rows, hidden_dim_] weight matrix, symmetric per-row INT8
   * @details Dequantized value = data[r*cols+c] * row_scale[r].
   */
  struct QuantizedWeight {
    const int8_t* data = nullptr;
    const float* row_scale = nullptr;
    int rows = 0;
  };

  /// Greedy-decoded raw output-class indices (0..num_phonemes_-1) for
  /// `word`; empty if decoding somehow ran the full 20-step cap without
  /// emitting the end-of-sequence class (not observed in practice, but not
  /// assumed impossible).
  std::vector<int> predict(const std::string& word) const {
    std::vector<float> h(hidden_dim_, 0.0f);
    for (char c : word) {
      gruStep(embRow(enc_emb_, graphemeIndex(c)), h, enc_w_ih_, enc_w_hh_, enc_b_ih_, enc_b_hh_);
    }
    gruStep(embRow(enc_emb_, 2 /* </s> */), h, enc_w_ih_, enc_w_hh_, enc_b_ih_, enc_b_hh_);

    std::vector<int> out;
    const float* dec = embRow(dec_emb_, 2 /* <s> */);
    for (int step = 0; step < 20; step++) {
      gruStep(dec, h, dec_w_ih_, dec_w_hh_, dec_b_ih_, dec_b_hh_);
      int idx = argmaxLogits(h);
      if (idx == 3 /* </s> */) break;
      out.push_back(idx);
      dec = embRow(dec_emb_, idx);
    }
    return out;
  }

  bool parseWeights(const uint8_t* buf, size_t len, size_t& pos) {
    if (!readHeader(buf, len, pos)) return false;

    enc_emb_ = readFloats(buf, len, pos, (size_t)num_graphemes_ * hidden_dim_);
    dec_emb_ = readFloats(buf, len, pos, (size_t)num_dec_symbols_ * hidden_dim_);
    if (!enc_emb_ || !dec_emb_) return false;

    if (!readQuantized(buf, len, pos, &enc_w_ih_)) return false;
    if (!readQuantized(buf, len, pos, &enc_w_hh_)) return false;
    if (!readQuantized(buf, len, pos, &dec_w_ih_)) return false;
    if (!readQuantized(buf, len, pos, &dec_w_hh_)) return false;

    enc_b_ih_ = readFloats(buf, len, pos, 3 * (size_t)hidden_dim_);
    enc_b_hh_ = readFloats(buf, len, pos, 3 * (size_t)hidden_dim_);
    dec_b_ih_ = readFloats(buf, len, pos, 3 * (size_t)hidden_dim_);
    dec_b_hh_ = readFloats(buf, len, pos, 3 * (size_t)hidden_dim_);
    fc_w_ = readFloats(buf, len, pos, (size_t)num_phonemes_ * hidden_dim_);
    fc_b_ = readFloats(buf, len, pos, (size_t)num_phonemes_);
    if (!enc_b_ih_ || !enc_b_hh_ || !dec_b_ih_ || !dec_b_hh_ || !fc_w_ || !fc_b_) return false;

    // The original format also stores a (symbol_id, tone) pair per output
    // class, resolved against TinyTTS's own multi-lingual symbol table --
    // not needed here since arpabetForIndex() maps straight from output
    // index to ARPAbet, but still present in the buffer and must be
    // skipped to validate the buffer's total length.
    if (pos + 2 * (size_t)num_phonemes_ > len) return false;
    pos += 2 * (size_t)num_phonemes_;
    return true;
  }

  bool readHeader(const uint8_t* buf, size_t len, size_t& pos) {
    if (pos + 16 > len) return false;
    int32_t hidden_dim, num_graphemes, num_dec_symbols, num_phonemes;
    std::memcpy(&hidden_dim, buf + pos, 4);
    std::memcpy(&num_graphemes, buf + pos + 4, 4);
    std::memcpy(&num_dec_symbols, buf + pos + 8, 4);
    std::memcpy(&num_phonemes, buf + pos + 12, 4);
    pos += 16;
    if (hidden_dim <= 0 || num_graphemes <= 0 || num_dec_symbols <= 0 || num_phonemes <= 0) return false;
    hidden_dim_ = hidden_dim;
    num_graphemes_ = num_graphemes;
    num_dec_symbols_ = num_dec_symbols;
    num_phonemes_ = num_phonemes;
    return true;
  }

  static const float* readFloats(const uint8_t* buf, size_t len, size_t& pos, size_t count) {
    size_t bytes = count * sizeof(float);
    if (pos + bytes > len) return nullptr;
    const float* p = reinterpret_cast<const float*>(buf + pos);
    pos += bytes;
    return p;
  }

  bool readQuantized(const uint8_t* buf, size_t len, size_t& pos, QuantizedWeight* w) const {
    int rows = 3 * hidden_dim_;
    size_t data_bytes = (size_t)rows * hidden_dim_;
    if (pos + data_bytes > len) return false;
    w->data = reinterpret_cast<const int8_t*>(buf + pos);
    pos += data_bytes;
    w->row_scale = readFloats(buf, len, pos, (size_t)rows);
    w->rows = rows;
    return w->row_scale != nullptr;
  }

  // Fixed vocab order matching the exported model: ['<pad>','<unk>','</s>','a'..'z'].
  static int graphemeIndex(char c) {
    if (c >= 'a' && c <= 'z') return 3 + (c - 'a');
    return 1;  // <unk>
  }

  const float* embRow(const float* emb, int idx) const { return emb + (size_t)idx * hidden_dim_; }

  // in-place GRU cell: h is both the input hidden state and the output.
  void gruStep(const float* x, std::vector<float>& h, const QuantizedWeight& w_ih, const QuantizedWeight& w_hh,
               const float* b_ih, const float* b_hh) const {
    int hidden = hidden_dim_;
    std::vector<float> rzn_ih(3 * hidden), rzn_hh(3 * hidden);
    // dequantized_weight[i][j] == row[j] * row_scale[i], so
    // dot(x, dequantized_row) == row_scale[i] * dot(x, row) -- scale the
    // int8 dot product once at the end rather than dequantizing every
    // element first.
    for (int i = 0; i < 3 * hidden; i++) {
      const int8_t* row = w_ih.data + (size_t)i * hidden;
      float dot = 0.0f;
      for (int j = 0; j < hidden; j++) dot += x[j] * (float)row[j];
      rzn_ih[i] = b_ih[i] + dot * w_ih.row_scale[i];
    }
    for (int i = 0; i < 3 * hidden; i++) {
      const int8_t* row = w_hh.data + (size_t)i * hidden;
      float dot = 0.0f;
      for (int j = 0; j < hidden; j++) dot += h[j] * (float)row[j];
      rzn_hh[i] = b_hh[i] + dot * w_hh.row_scale[i];
    }
    for (int i = 0; i < hidden; i++) {
      float r = 1.0f / (1.0f + std::exp(-(rzn_ih[i] + rzn_hh[i])));
      float z = 1.0f / (1.0f + std::exp(-(rzn_ih[hidden + i] + rzn_hh[hidden + i])));
      float n = std::tanh(rzn_ih[2 * hidden + i] + r * rzn_hh[2 * hidden + i]);
      h[i] = (1 - z) * n + z * h[i];
    }
  }

  int argmaxLogits(const std::vector<float>& h) const {
    int best = 0;
    float best_val = -1e30f;
    for (int j = 0; j < num_phonemes_; j++) {
      float logit = fc_b_[j];
      const float* row = fc_w_ + (size_t)j * hidden_dim_;
      for (int k = 0; k < hidden_dim_; k++) logit += h[k] * row[k];
      if (logit > best_val) {
        best_val = logit;
        best = j;
      }
    }
    return best;
  }

  /// Maps a decoder output-class index directly to its ARPAbet symbol,
  /// including the stress digit TinyTTSTools uses (a trailing '1'/'2' for
  /// primary/secondary stress, or the dedicated AH0/ER0 reduced-vowel
  /// phonemes for CMU's digit-0/"unstressed" AH and ER specifically --
  /// TinyTTSTools has no distinct reduced-vowel phoneme for the other
  /// vowels, so their digit-0 case maps to the plain symbol). This table
  /// is fixed metadata of the specific shipped model
  /// (G2PNeuralWeightsEN_data.h): it was read directly off that binary's own
  /// embedded (symbol, tone) pairs, cross-referenced against TinyTTS's
  /// multi-lingual symbol table, not computed generically -- a
  /// differently-trained model would need a different table here.
  static const char* arpabetForIndex(int idx) {
    static constexpr const char* kTable[74] = {
        nullptr, nullptr, nullptr, nullptr,  // 0-3: pad/unk/<s>/</s> -- never emitted
        "AA", "AA1", "AA2", "AE", "AE1", "AE2", "AH0", "AH1", "AH2",
        "AO", "AO1", "AO2", "AW", "AW1", "AW2", "AY", "AY1", "AY2",
        "B", "CH", "D", "DH",
        "EH", "EH1", "EH2", "ER0", "ER1", "ER2", "EY", "EY1", "EY2",
        "F", "G", "HH",
        "IH", "IH1", "IH2", "IY", "IY1", "IY2",
        "JH", "K", "L", "M", "N", "NG",
        "OW", "OW1", "OW2", "OY", "OY1", "OY2",
        "P", "R", "S", "SH", "T", "TH",
        "UH", "UH1", "UH2", "UW", "UW", "UW1", "UW2",
        "V", "W", "Y", "Z", "ZH",
    };
    if (idx < 0 || idx >= 74) return nullptr;
    return kTable[idx];
  }

  const float* enc_emb_ = nullptr;
  const float* dec_emb_ = nullptr;
  QuantizedWeight enc_w_ih_, enc_w_hh_, dec_w_ih_, dec_w_hh_;
  const float* enc_b_ih_ = nullptr;
  const float* enc_b_hh_ = nullptr;
  const float* dec_b_ih_ = nullptr;
  const float* dec_b_hh_ = nullptr;
  const float* fc_w_ = nullptr;
  const float* fc_b_ = nullptr;
  int hidden_dim_ = 0;
  int num_graphemes_ = 0;
  int num_dec_symbols_ = 0;
  int num_phonemes_ = 0;
};
