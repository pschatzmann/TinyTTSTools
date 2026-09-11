# G2P Neural Example

Demonstrates `G2PDictionaryNeuralAndRulesModel`: dictionary lookup, a
neural GRU fallback, and letter-to-sound rules chained together, in that
order of preference.

## Why

- **Dictionary lookup** is exact but only covers words someone thought to
  list (the default curated dictionary has ~534 words; you can point it at
  the full 123k-word CMU dictionary instead via
  `g2p.getDictionaryModel().useCompactDictionary(COMPACT_CMUDICT_EN)`).
- **Letter-to-sound rules** (`G2PRuleBasedModelEN`) are a last resort with
  only a ~17% exact-match ceiling -- English spelling is fundamentally
  ambiguous (vowel reduction, stress) and can't be resolved from rules
  alone.
- **The neural model** (`G2PNeuralModel`) fills the real gap: it produces
  a plausible phonetic guess for genuinely novel words -- proper nouns,
  made-up words, technical terms -- that no dictionary, however large,
  will ever contain. Measured at ~74% exact-match accuracy against a
  sample of the full CMU dictionary (vs. ~17% for rules alone), even
  though it never memorized a dictionary itself.

It's a from-scratch, dependency-free GRU encoder-decoder (~833K params,
~970KB of flash for its weights) ported from the sibling TinyTTS project's
`DictionaryModel.h`, itself verified bit-exact against the Python `g2p_en`
reference package before being trusted. No TensorFlow Lite, no external
library -- it's a plain header, and it's fully opt-in: `G2PNeuralModel`
works correctly (falling straight through to the next model in the chain)
even if you never load its weights, so there's no cost unless you use it.

## Usage

```cpp
#include "TinyTTSTools/G2P/G2PDictionaryNeuralAndRulesModel.h"
#include "TinyTTSTools/Data/neural/G2PNeuralWeightsEN_data.h"

G2PDictionaryNeuralAndRulesModel g2p;
g2p.getNeuralModel().begin(G2P_NEURAL_MODEL_WEIGHTS_EN, G2P_NEURAL_MODEL_WEIGHTS_EN_LEN);

std::string phonemes = g2p.wordToPhonemes("zephyrion");  // a plausible guess, not silence
```

Use `G2PNeuralModel` on its own (see `G2PNeuralModel.h`) if you want the
neural model without the dictionary/rules chain, e.g. to compose it
yourself with `G2PHybridModel::addModel()`.

## Expected output

```
Before loading neural weights (dictionary + rules only):
  zephyrion -> Z EH F IY R IH AO N
  flibbertigibbet -> F L IH B ER T IH JH IH B EH T
  arduino -> AA R D UW N AO
  microcontroller -> M IH K R AO K AO N T R AO L ER
  xqzwy -> K S K Z W IY

Loading neural G2P weights (~970KB)...
Loaded.

Real words (dictionary hits -- neural model never runs):
  the -> DH AH0
  quick -> K W IH K
  brown -> B R AW N
  quietly -> K W AY AH0 T L IY

Novel words (no dictionary could ever contain these):
  zephyrion -> Z AH0 F IH1 R IY AH0 N
  flibbertigibbet -> F L IH1 B ER0 T IH JH AH0 T
  arduino -> AA R D UW IY1 N OW
  microcontroller -> M AY2 K R OW K AA1 N T R AH0 K L ER0
  xqzwy -> Z K EH1 G W IY Z
```

## Memory

- Weights: ~970KB flash (`Data/neural/G2PNeuralWeightsEN_data.h`)
- No extra RAM beyond the GRU's hidden state (256 floats) and per-call
  scratch buffers -- no tensor arena, no TensorFlow Lite runtime.

## Multi-language note

The architecture is parametric -- hidden size and vocabulary sizes are read
from the weight file, not hardcoded. The only per-language code is
`graphemeIndexFor()` (grapheme -> vocab index, UTF-8-aware for accented
letters) and `symbolForIndex()` (output-index-to-phoneme table, metadata
of that specific trained model) -- selected via the `G2PNeuralLanguage`
enum passed to `begin()`. German, French and Spanish are wired up the same
way as English:

```cpp
#include "TinyTTSTools/Data/neural/G2PNeuralWeightsDE_data.h"

g2p.getNeuralModel().begin(G2P_NEURAL_MODEL_WEIGHTS_DE, G2P_NEURAL_MODEL_WEIGHTS_DE_LEN,
                            G2PNeuralLanguage::DE);
```

`setup/neural-de/fr/es/` run the same training pipeline for
German/French/Spanish (built from the OLaPh corpus): French reached 94.8%
validation exact-match, Spanish 98.7%, and German 71.2% (German's heavier
compounding and less regular orthography make it a genuinely harder case).
See [`docs/ADDING_A_LANGUAGE.md`](../../docs/ADDING_A_LANGUAGE.md) for the
full picture, including how to add a fifth language.

## Related examples

- `G2PCustomDictionary/` -- custom phoneme dictionary usage
- `AudioPhoneme/`, `AudioDiphones/`, `AudioFormant/` -- audio synthesis
  examples (this one is phoneme conversion only, no audio output)
