/**
 * @file G2PDictionaryNeuralAndRulesModel.h
 * @brief Combined dictionary, neural and rules-based G2P model for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <string>
#include "G2PHybridModel.h"
#include "G2PDictionaryModel.h"
#include "G2PNeuralModel.h"
#include "G2PRuleBasedModelEN.h"

/**
 * @brief G2P model chaining dictionary lookup, a neural fallback and rules
 * @details Same shape as G2PDictionaryAndRulesModel, with the neural model
 * (G2PNeuralModel) inserted between the dictionary and the rule-based
 * fallback:
 * 1. Dictionary lookup (exact, for known words -- default is the small
 *    527-word curated table; call getDictionaryModel().useCompactDictionary()
 *    to use a larger one, e.g. the full CMU dictionary)
 * 2. Neural GRU model (a plausible phonetic guess for genuinely novel
 *    words -- proper nouns, made-up words -- that no dictionary will ever
 *    contain), IF weights have been loaded via getNeuralModel().begin()
 * 3. Letter-to-sound rules (final safety net; also what's used if the
 *    neural model's weights were never loaded, since begin() must be
 *    called explicitly -- see below)
 *
 * The neural model's weights (~970KB) are NOT loaded automatically -- this
 * class works correctly (falling straight through to rules) even if you
 * never call begin() on it, so the cost is opt-in:
 * @code
 * #include "TinyTTSTools/G2P/G2PDictionaryNeuralAndRulesModel.h"
 * #include "TinyTTSTools/Data/neural/G2PNeuralWeightsEN_data.h"
 *
 * G2PDictionaryNeuralAndRulesModel g2p;
 * g2p.getNeuralModel().begin(G2P_NEURAL_MODEL_WEIGHTS_EN, G2P_NEURAL_MODEL_WEIGHTS_EN_LEN);
 * @endcode
 * @note Memory footprint: ~10KB flash (dictionary + rules) if begin() is
 * never called; +~970KB once the neural fallback's weights are loaded. See
 * https://github.com/pschatzmann/TinyTTSTools/blob/main/docs/MEMORY.md for a
 * comparison table across all vocoders and G2P models.
 */
class G2PDictionaryNeuralAndRulesModel : public G2PHybridModel {
 public:
  G2PDictionaryNeuralAndRulesModel() {
    // Add the default models in order of preference
    addModel(g2pDictionaryModel_);
    addModel(g2pNeuralModel_);
    addModel(g2pRuleBasedModel_);
  }

  G2PDictionaryModel& getDictionaryModel() { return g2pDictionaryModel_; }

  G2PNeuralModel& getNeuralModel() { return g2pNeuralModel_; }

  G2PRuleBasedModelEN& getRuleBasedModel() { return g2pRuleBasedModel_; }

  /**
   * @brief Convert word to phonemes using dictionary, neural model and rules
   * @param word Input word
   * @return Space-separated phoneme string
   */
  std::string wordToPhonemes(const std::string& word) override {
    return G2PHybridModel::wordToPhonemes(word);
  }

 protected:
  G2PDictionaryModel g2pDictionaryModel_;  ///< Dictionary-based model
  G2PNeuralModel g2pNeuralModel_;          ///< Neural GRU fallback (weights opt-in via begin())
  G2PRuleBasedModelEN g2pRuleBasedModel_;    ///< Rule-based model
};
