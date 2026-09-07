/**
 * @file G2PDictionaryAndRulesModel.h
 * @brief Combined dictionary and rules-based G2P model for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <string>
#include "G2PHybridModel.h"
#include "G2PDictionaryModel.h"
#include "G2PRuleBasedModel.h"

/**
 * @brief Default G2P model using basic lookup and rule-based methods
 * @details Combines dictionary lookup and rule-based phoneme generation
 *          for efficient and accurate conversion.
 */
class G2PDictionaryAndRulesModel : public G2PHybridModel {
 public:
  /**
   * @brief Constructor
   * @details Initializes with the default G2P models
   */
  G2PDictionaryAndRulesModel() {
    // Add the default models in order of preference
    addModel(g2pDictionaryModel_);
    addModel(g2pRuleBasedModel_);
  }

  G2PDictionaryModel& getDictionaryModel() { return g2pDictionaryModel_; }

  G2PRuleBasedModel& getRuleBasedModel() { return g2pRuleBasedModel_; }

  /**
   * @brief Convert word to phonemes using dictionary and rules
   * @param word Input word
   * @return Space-separated phoneme string
   */
  std::string wordToPhonemes(const std::string& word) override {
    return G2PHybridModel::wordToPhonemes(word);
  }

 protected:
  G2PDictionaryModel g2pDictionaryModel_;  ///< Dictionary-based model
  G2PRuleBasedModel g2pRuleBasedModel_;    ///< Rule-based model
};
