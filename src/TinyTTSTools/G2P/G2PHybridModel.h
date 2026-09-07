/**
 * @file G2PHybridModel.h
 * @brief Convert word to phonemes using multiple models in priority order
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <string>
#include <vector>

#include "G2PModelBase.h"
#include "../Basic/StringUtils.h"

/**
 * @brief Hybrid G2P model combining multiple approaches
 * @details add the models via addModel()
 */
class G2PHybridModel : public G2PModelBase {
 public:
  /**
   * @brief Constructor
   * @param config G2P configuration settings
   */
  G2PHybridModel() = default;
  G2PHybridModel(G2PModelBase& model) { addModel(model); };
  G2PHybridModel(G2PModelBase& model1, G2PModelBase& model2) {
    addModel(model1);
    addModel(model2);
  };
  G2PHybridModel(G2PModelBase& model1, G2PModelBase& model2,
                 G2PModelBase& model3) {
    addModel(model1);
    addModel(model2);
    addModel(model3);
  };

  void addModel(G2PModelBase& model) { models_.push_back(&model); }

  /**
   * @brief Convert word to phonemes using hybrid approach
   * @param word Input word
   * @return Space-separated phoneme string
   */
  std::string wordToPhonemes(const std::string& word) override {
    std::string lowerWord = StringUtils::toLowerCase(word);

    for (auto& model : models_) {
      std::string phonemes = model->wordToPhonemes(lowerWord);
      if (phonemes.empty() == false) {
        return phonemes;  // Return first successful conversion
      }
    }

    return "";
  }

 protected:
  std::vector<G2PModelBase*> models_;  ///< List of G2P models to use
};
