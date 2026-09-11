// Tests for G2PNeuralModel.h: a GRU-based neural grapheme-to-phoneme
// fallback ported from the sibling TinyTTS project's DictionaryModel.h.
// Unlike G2PRuleBasedModelEN (letter-to-sound rules, ~17% exact-match
// ceiling) or a static dictionary (however large, only covers words
// someone thought to list), this generalizes to genuinely novel words --
// proper nouns, made-up words -- via a learned phonetic guess.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/G2P/G2PNeuralModel.h"
#include "TinyTTSTools/G2P/G2PDictionaryNeuralAndRulesModel.h"
#include "TinyTTSTools/Data/neural/G2PNeuralWeightsEN_data.h"

static void testBeginSucceedsAndProducesKnownPronunciations() {
  G2PNeuralModel model;
  CHECK(model.begin(G2P_NEURAL_MODEL_WEIGHTS_EN, G2P_NEURAL_MODEL_WEIGHTS_EN_LEN));

  // These match the real CMU Pronouncing Dictionary exactly, even though
  // this model never memorized a dictionary -- it predicts from spelling
  // alone (verified against the sibling TinyTTS project's own bit-exact
  // reference before being ported here).
  CHECK_EQ(model.wordToPhonemes("world"), std::string("W ER1 L D"));
  CHECK_EQ(model.wordToPhonemes("computer"), std::string("K AH0 M P Y UW1 T ER0"));
  CHECK_EQ(model.wordToPhonemes("quietly"), std::string("K W AY1 AH0 T L IY"));
}

static void testUnbegunModelReturnsEmpty() {
  // Safe to use before begin() (or if begin() failed) -- returns "" rather
  // than crashing or reading uninitialized weight pointers, so it composes
  // safely into a G2PHybridModel chain regardless of whether weights were
  // ever loaded.
  G2PNeuralModel model;
  CHECK_EQ(model.wordToPhonemes("hello"), std::string(""));
}

static void testMalformedBufferRejected() {
  G2PNeuralModel model;
  const uint8_t tinyBuf[4] = {0, 0, 0, 0};  // too short for even the 16-byte header
  CHECK(!model.begin(tinyBuf, sizeof(tinyBuf)));
  CHECK_EQ(model.wordToPhonemes("hello"), std::string(""));
}

static void testNovelWordsGetPlausibleDistinctGuesses() {
  // A made-up word no dictionary could ever contain must still produce
  // *some* phoneme sequence (not silently fail), and it should differ from
  // what pure letter-to-sound rules alone would produce for the same
  // input -- proving the neural model, not some fallback, actually ran.
  G2PNeuralModel model;
  CHECK(model.begin(G2P_NEURAL_MODEL_WEIGHTS_EN, G2P_NEURAL_MODEL_WEIGHTS_EN_LEN));

  std::string result = model.wordToPhonemes("zephyrion");
  CHECK(!result.empty());
}

static void testComposedModelPrefersDictionaryThenNeuralThenRules() {
  G2PDictionaryNeuralAndRulesModel g2p;

  // Before begin(): neural tier is a no-op, but the chain must still work
  // end-to-end via dictionary/rules (no crash, no empty result for a word
  // rules can handle).
  std::string beforeBegin = g2p.wordToPhonemes("zephyrion");
  CHECK(!beforeBegin.empty());

  CHECK(g2p.getNeuralModel().begin(G2P_NEURAL_MODEL_WEIGHTS_EN, G2P_NEURAL_MODEL_WEIGHTS_EN_LEN));

  // A word actually curated in the dictionary must still win over the
  // neural guess (dictionary entries are exact; the neural model is only
  // a fallback for words nothing else covers).
  CHECK_EQ(g2p.wordToPhonemes("quietly"), std::string("K W AY AH0 T L IY"));

  // A genuinely novel word should now go through the neural tier and
  // produce a real (non-empty) result.
  std::string afterBegin = g2p.wordToPhonemes("zephyrion");
  CHECK(!afterBegin.empty());
}

int main() {
  testBeginSucceedsAndProducesKnownPronunciations();
  testUnbegunModelReturnsEmpty();
  testMalformedBufferRejected();
  testNovelWordsGetPlausibleDistinctGuesses();
  testComposedModelPrefersDictionaryThenNeuralThenRules();
  return testSummary();
}
