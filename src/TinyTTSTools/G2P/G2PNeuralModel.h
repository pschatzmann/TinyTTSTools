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
 * @brief Which language-specific grapheme/phoneme vocabulary a
 * G2PNeuralModel should decode with.
 * @details Each language was trained with its own grapheme set (input
 * alphabet, including that language's accented letters) and its own
 * phoneme output table (see setup/neural-{en,de,fr,es}/vocab.py) -- the
 * weights blob (Data/neural/G2PNeuralWeights{EN,DE,FR,ES}_data.h) only
 * carries the trained numbers, not which vocabulary they were trained
 * against, so the caller must say which one via begin()'s `language`
 * parameter, matching the weights file passed alongside it.
 */
enum class G2PNeuralLanguage { EN, DE, FR, ES };

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
 * The input grapheme vocabulary ('<pad>','<unk>','</s>', plus that
 * language's letters) and output phoneme vocabulary are both fixed
 * properties of the specific trained model file (see
 * setup/neural-{en,de,fr,es}/vocab.py) -- pass the matching
 * G2PNeuralLanguage to begin() alongside its weights so this class
 * decodes with the right tables (graphemeIndexFor()/arpabetForIndexFor()).
 * @note Memory footprint: ~970KB flash for the shipped weights (measured:
 * 992,716 bytes), no RAM overhead beyond a small decode buffer (no tensor
 * arena, no runtime library). See
 * https://github.com/pschatzmann/TinyTTSTools/blob/main/docs/MEMORY.md for a
 * comparison table across all vocoders and G2P models.
 */
class G2PNeuralModel : public G2PModelBase {
 public:
  /// Parses `buf` (the binary format G2PNeuralWeights{EN,DE,FR,ES}_data.h
  /// embed) without copying -- `buf` must outlive this object. `language`
  /// selects which grapheme/phoneme vocabulary to decode with and MUST
  /// match the weights file being passed (e.g. G2PNeuralLanguage::DE with
  /// G2P_NEURAL_MODEL_WEIGHTS_DE) -- mismatching the two silently produces
  /// garbage output, since the weights carry no self-describing language
  /// tag. Returns false on a malformed/truncated buffer.
  bool begin(const uint8_t* buf, size_t len, G2PNeuralLanguage language = G2PNeuralLanguage::EN) {
    language_ = language;
    size_t pos = 0;
    initialized_ = parseWeights(buf, len, pos);
    if (!initialized_) {
      TTS_LOGE("G2PNeuralModel: failed to parse weights buffer "
                "(%zu bytes) -- malformed or truncated", len);
    }
    return initialized_;
  }

  /// @param word Input word (converted to lowercase; characters outside
  /// the selected language's grapheme set fall through to <unk> handling)
  /// @return Space-separated phoneme string, or "" if not initialized or
  /// decoding produced nothing
  std::string wordToPhonemes(const std::string& word) override {
    if (!initialized_) return "";
    std::string lower = StringUtils::toLowerCase(word);
    std::vector<int> indices = predict(lower);
    std::string out;
    for (int idx : indices) {
      const char* sym = symbolForIndex(language_, idx);
      if (!sym || !sym[0]) continue;
      if (!out.empty()) out += ' ';
      out += sym;
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
    for (uint32_t cp : decodeUtf8(word)) {
      gruStep(embRow(enc_emb_, graphemeIndexFor(language_, cp)), h, enc_w_ih_, enc_w_hh_, enc_b_ih_, enc_b_hh_);
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
    // not needed here since symbolForIndex() maps straight from output
    // index to that language's phoneme table, but still present in the
    // buffer and must be skipped to validate the buffer's total length.
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

  /// Decodes a UTF-8 byte string into Unicode codepoints -- needed since
  /// DE/FR/ES grapheme sets include accented letters (2-byte UTF-8
  /// sequences), unlike EN's plain a-z. An invalid/truncated multi-byte
  /// sequence decodes to U+FFFD (falls through to <unk> in
  /// graphemeIndexFor()) rather than desyncing the byte stream.
  static std::vector<uint32_t> decodeUtf8(const std::string& s) {
    std::vector<uint32_t> out;
    size_t i = 0;
    size_t n = s.size();
    while (i < n) {
      uint8_t c = (uint8_t)s[i];
      if (c < 0x80) {
        out.push_back(c);
        i += 1;
      } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
        out.push_back(((uint32_t)(c & 0x1F) << 6) | ((uint8_t)s[i + 1] & 0x3F));
        i += 2;
      } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
        out.push_back(((uint32_t)(c & 0x0F) << 12) | (((uint8_t)s[i + 1] & 0x3F) << 6) |
                      ((uint8_t)s[i + 2] & 0x3F));
        i += 3;
      } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
        out.push_back(((uint32_t)(c & 0x07) << 18) | (((uint8_t)s[i + 1] & 0x3F) << 12) |
                      (((uint8_t)s[i + 2] & 0x3F) << 6) | ((uint8_t)s[i + 3] & 0x3F));
        i += 4;
      } else {
        out.push_back(0xFFFD);
        i += 1;
      }
    }
    return out;
  }

  /// Maps a Unicode codepoint to its grapheme-vocab index for `lang`,
  /// matching that language's vocab.py GRAPHEMES list index-for-index
  /// (['<pad>','<unk>','</s>'] then 'a'..'z' then the language's own
  /// accented letters, in the exact order vocab.py lists them -- order
  /// matters, it's what the model was trained against). Falls back to
  /// <unk> (index 1) for anything outside that set.
  static int graphemeIndexFor(G2PNeuralLanguage lang, uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return 3 + (int)(cp - 'a');
    switch (lang) {
      case G2PNeuralLanguage::DE: {
        // vocab.py: a..z + ['ä','ü','ö','ß','-']
        switch (cp) {
          case 0x00E4: return 29;  // ä
          case 0x00FC: return 30;  // ü
          case 0x00F6: return 31;  // ö
          case 0x00DF: return 32;  // ß
          case 0x002D: return 33;  // -
        }
        break;
      }
      case G2PNeuralLanguage::FR: {
        // vocab.py: a..z + ['é','â','è','ç','î','ê','û','ô','ï',"'",'œ','à']
        switch (cp) {
          case 0x00E9: return 29;  // é
          case 0x00E2: return 30;  // â
          case 0x00E8: return 31;  // è
          case 0x00E7: return 32;  // ç
          case 0x00EE: return 33;  // î
          case 0x00EA: return 34;  // ê
          case 0x00FB: return 35;  // û
          case 0x00F4: return 36;  // ô
          case 0x00EF: return 37;  // ï
          case 0x0027: return 38;  // '
          case 0x0153: return 39;  // œ
          case 0x00E0: return 40;  // à
        }
        break;
      }
      case G2PNeuralLanguage::ES: {
        // vocab.py: a..z + ['á','í','é','ó','ñ','ú','ü']
        switch (cp) {
          case 0x00E1: return 29;  // á
          case 0x00ED: return 30;  // í
          case 0x00E9: return 31;  // é
          case 0x00F3: return 32;  // ó
          case 0x00F1: return 33;  // ñ
          case 0x00FA: return 34;  // ú
          case 0x00FC: return 35;  // ü
        }
        break;
      }
      case G2PNeuralLanguage::EN:
        break;
    }
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

  /// Maps a decoder output-class index directly to its phoneme symbol for
  /// `lang`, including the stress digit TinyTTSTools uses (a trailing
  /// '1'/'2' for primary/secondary stress, or the dedicated AH0/ER0
  /// reduced-vowel phonemes for CMU's digit-0/"unstressed" AH and ER
  /// specifically -- TinyTTSTools has no distinct reduced-vowel phoneme
  /// for the other vowels, so their digit-0 case maps to the plain
  /// symbol). Each table is fixed metadata of that language's specific
  /// shipped model (Data/neural/G2PNeuralWeights{EN,DE,FR,ES}_data.h),
  /// read off setup/neural-{en,de,fr,es}/vocab.py's PHONEME_TABLE
  /// index-for-index, matching TinyTTSTools's own multi-lingual symbol
  /// table (see PhonemeDictionaryDE/FR/ES.h) -- not computed generically.
  static const char* symbolForIndex(G2PNeuralLanguage lang, int idx) {
    switch (lang) {
      case G2PNeuralLanguage::EN: {
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
      case G2PNeuralLanguage::DE: {
        static constexpr const char* kTable[159] = {
            nullptr, nullptr, nullptr, nullptr,
            "AA1", "AA2", "AA:", "AC", "AE", "AF", "AF1", "AF2",
            "AF:", "AH0", "AN", "AN:", "AO", "AO1", "AO2", "AO:",
            "AW", "AW1", "AW2", "AY", "AY1", "AY2", "B", "B1",
            "B2", "C", "C1", "C2", "CH", "CH1", "CH2", "D",
            "D1", "D2", "EC", "EH", "EH1", "EH2", "EH:", "EN",
            "EN:", "EP", "EP1", "EP2", "EP:", "F", "F1", "F2",
            "G", "G1", "G2", "GS", "GS1", "GS2", "HH", "HH1",
            "HH2", "IH", "IH1", "IH2", "IY", "IY1", "IY2", "IY:",
            "JH", "JH1", "JH2", "K", "K1", "K2", "L", "L1",
            "L2", "L=", "M", "M1", "M2", "M=", "N", "N1",
            "N2", "N=", "NG", "NG=", "OE", "OE1", "OE2", "OE:",
            "OF", "OF1", "OF2", "OF:", "ON", "ON:", "OP", "OP1",
            "OP2", "OP:", "OP~", "P", "P1", "P2", "PF", "PF1",
            "PF2", "R", "RR", "RT", "RU", "RU1", "RU2", "S",
            "S1", "S2", "SH", "SH1", "SH2", "T", "T1", "T2",
            "TH", "TS", "TS1", "TS2", "UF", "UF0", "UF01", "UF02",
            "UF1", "UF2", "UF:", "UH", "UH1", "UH2", "UW", "UW1",
            "UW2", "UW:", "V", "V1", "V2", "W", "W1", "X",
            "X1", "Y", "Y1", "Y2", "Z", "Z1", "Z2", "ZH",
            "ZH1", "ZH2", "Z_0",
        };
        if (idx < 0 || idx >= 159) return nullptr;
        return kTable[idx];
      }
      case G2PNeuralLanguage::FR: {
        static constexpr const char* kTable[42] = {
            nullptr, nullptr, nullptr, nullptr,
            "AA", "AF", "AH0", "AN", "AO", "B", "D", "EH",
            "EN", "EP", "F", "G", "HU", "IY", "K", "L",
            "M", "N", "NG", "NY", "OE", "OF", "ON", "OP",
            "P", "RU", "RU:", "S", "SH", "T", "UF", "UN",
            "UW", "V", "W", "Y", "Z", "ZH",
        };
        if (idx < 0 || idx >= 42) return nullptr;
        return kTable[idx];
      }
      case G2PNeuralLanguage::ES: {
        static constexpr const char* kTable[65] = {
            nullptr, nullptr, nullptr, nullptr,
            "AF", "AF1", "B", "B1", "BETA", "BETA1", "CH", "D",
            "D1", "DH", "DH1", "EP", "EP1", "F", "F1", "G",
            "G1", "GH", "GH1", "IY", "IY1", "JZ", "JZ1", "K",
            "K1", "L", "L1", "LY", "LY1", "M", "M1", "N",
            "N1", "NG", "NG1", "NY", "NY1", "OP", "OP1", "P",
            "P1", "RR", "RR1", "RT", "RT1", "S", "S1", "SH",
            "T", "T1", "TH", "TH1", "UW", "UW1", "W", "X",
            "X1", "Y", "Y1", "Z", "Z1",
        };
        if (idx < 0 || idx >= 65) return nullptr;
        return kTable[idx];
      }
    }
    return nullptr;
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
  G2PNeuralLanguage language_ = G2PNeuralLanguage::EN;
};
