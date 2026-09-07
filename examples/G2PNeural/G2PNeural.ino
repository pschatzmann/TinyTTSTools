/*
 * G2P Neural Example
 *
 * Demonstrates G2PDictionaryNeuralAndRulesModel: dictionary lookup, a
 * neural GRU fallback, and letter-to-sound rules chained together.
 *
 * Unlike the library's dictionaries (however large) or its rule-based
 * fallback (~17% exact-match ceiling -- English spelling can't be resolved
 * from rules alone), the neural model generalizes: it produces a plausible
 * phonetic guess for genuinely novel words -- proper nouns, made-up words,
 * technical terms -- that no dictionary will ever contain. It's a
 * dependency-free (no TensorFlow Lite) GRU encoder-decoder ported from the
 * sibling TinyTTS project, ~970KB of flash for its weights, fully opt-in:
 * this class works correctly even if you never load the weights, falling
 * straight through to rules.
 *
 * This example focuses on phoneme conversion only and does not include
 * audio synthesis -- see AudioPhoneme/AudioBiphones/AudioFormant for that.
 */

// AudioTools.h is not used for audio here -- on real Arduino, Serial/delay
// are already globally available, but on a desktop build they only exist
// via AudioTools' Arduino-compatibility emulation layer.
#include "AudioTools.h"
#include "TinyTTSTools/G2P/G2PDictionaryNeuralAndRulesModel.h"
#include "TinyTTSTools/Data/neural/G2PNeuralWeights_data.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"

G2PDictionaryNeuralAndRulesModel g2p;

// Real dictionary words (resolved by the small curated dictionary, never
// reach the neural model) alongside words no dictionary would ever
// contain, to show the neural fallback actually doing work.
const char* dictionaryWords[] = {"the", "quick", "brown", "quietly"};
const char* novelWords[] = {"zephyrion", "flibbertigibbet", "arduino",
                            "microcontroller", "xqzwy"};

void printConversions(const char* label, const char* const* words, size_t count) {
  Serial.println(label);
  for (size_t i = 0; i < count; i++) {
    Serial.print("  ");
    Serial.print(words[i]);
    Serial.print(" -> ");
    Serial.println(g2p.wordToPhonemes(words[i]).c_str());
  }
  Serial.println();
}

void setup() {
  TTSExample::waitForSerial();

  Serial.println("G2P Neural Example");
  Serial.println("===================");
  Serial.println();

  printConversions("Before loading neural weights (dictionary + rules only):",
                    novelWords, 5);

  Serial.println("Loading neural G2P weights (~970KB)...");
  if (!g2p.getNeuralModel().begin(G2P_NEURAL_MODEL_WEIGHTS, G2P_NEURAL_MODEL_WEIGHTS_LEN)) {
    Serial.println("Failed to load neural G2P weights!");
    return;
  }
  Serial.println("Loaded.");
  Serial.println();

  printConversions("Real words (dictionary hits -- neural model never runs):",
                    dictionaryWords, 4);
  printConversions("Novel words (no dictionary could ever contain these):",
                    novelWords, 5);
}

void loop() {
  // This example runs once in setup() and then does nothing.
  delay(1000);
}
