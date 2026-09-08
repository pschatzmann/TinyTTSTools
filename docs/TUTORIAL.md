# TinyTTSTools Tutorial

TinyTTSTools turns text into audio in three independent steps, each with
its own set of interchangeable implementations:

1. **Grapheme to Phoneme (G2P)** -- text -> phoneme symbols (e.g. "hello" -> `HH AH0 L OW1`)
2. **Phoneme to Audio** -- phoneme symbols -> PCM samples (the "vocoder")
3. **Audio Output** -- PCM samples -> I2S / DAC / PWM / a callback

Picking a combination of one G2P model and one vocoder is the main design
decision; everything downstream (the `TinyTTSTools` class, audio output)
stays the same regardless of which you pick.

## Your first sketch

The smallest complete example uses `FormantVocoder`, which needs no
pre-recorded audio data at all (pure procedural synthesis) -- the cheapest
option to get started with:

```cpp
#include "AudioTools.h"
#include "TinyTTSTools.h"

G2PDictionaryAndRulesModel g2p;   // text -> phonemes
FormantVocoder synth(16000);      // phonemes -> PCM, 16kHz
I2SStream out;                    // PCM -> I2S hardware output
TinyTTSTools tts(g2p, synth, out);

void setup() {
  Serial.begin(115200);
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = 16;
  cfg.channels = 1;
  out.begin(cfg);

  tts.begin();
}

void loop() {
  tts.say("Hello world");
  delay(5000);
}
```

That's the whole shape every other example in this library follows: one
G2P model, one vocoder, one `Print`-compatible output, wired together by
`TinyTTSTools`.

## Choosing a vocoder (phoneme -> audio)

| Vocoder | Data needed | Quality | When to use |
|---|---|---|---|
| `FormantVocoder` | None (procedural) | Robotic but intelligible | Tightest flash budgets; no audio assets to ship |
| `PhonemeVocoder` | `ArpabetWAVDictionary` (~35KB audio data) | Better than formant, no coarticulation between sounds | Small flash budget, some real recorded audio |
| `DiphoneVocoder` | `DiphoneWAVDictionary` (~893KB audio data, +~1MB total flash once its decoder/concatenation code is linked in -- see [MEMORY.md](MEMORY.md)) | Most natural of the three -- real phoneme-to-phoneme transitions | Flash isn't tight (ESP32-class), best quality wanted |

See `examples/AudioFormant`, `examples/AudioPhoneme`, `examples/AudioBiphones`
for a complete, runnable version of each. Swapping vocoders is a two-line
change (the constructor and its `#include`) -- the rest of a sketch is
identical.

### Tuning a phoneme's synthesis

Every vocoder accepts an optional `PhonemeSynthesisParams` on `sayPhoneme()`
for per-call overrides, without changing any global configuration:

```cpp
PhonemeSynthesisParams params;
params.volume = 0.5f;      // 1.0 = normal; quieter here
params.durationMs = 300;   // 0 = automatic; force a specific length
params.pitchHz = 180.0f;   // 0 = default voice pitch
params.voicing = 0.0f;     // 1.0 = normal voiced, 0.0 = whispered (FormantVocoder only)

synth.sayPhoneme(Phone::AA, out, params);
```

`Phone` is a type-safe enum alternative to raw ARPAbet strings (`Phone::AA`
instead of `"AA"`), covering every phoneme plus the stress-marked variants
(`Phone::IH1`, `Phone::AA2`, ...) used internally by the dictionaries.

## Choosing a G2P model (text -> phonemes)

| Model | Coverage | Exact-match accuracy | Flash cost |
|---|---|---|---|
| `G2PDictionaryModel` (default 534-word dictionary) | Common/curated words only | 100% for words it contains, 0% otherwise | Small |
| + `G2PRuleBasedModel` fallback (`G2PDictionaryAndRulesModel`) | Any word | ~17% for words outside the dictionary (English spelling is fundamentally ambiguous without a stress model) | Small |
| + `G2PNeuralModel` fallback (`G2PDictionaryNeuralAndRulesModel`) | Any word, including genuinely novel ones (proper nouns, made-up words) | ~74% for words outside the dictionary | +~970KB |
| Full CMU dictionary (`COMPACT_CMUDICT_EN`, 123k words) | Nearly all real English words | ~100% for words it contains | +~1.74MB |

Start with `G2PDictionaryAndRulesModel` (the default in every audio
example). Reach for the others as needed:

**More vocabulary, still small**: point the dictionary tier at the full
CMU dictionary instead of the small curated one:

```cpp
#include "TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h"

g2p.getDictionaryModel().useCompactDictionary(COMPACT_CMUDICT_EN);
```

**Better guesses for words no dictionary will ever contain** (proper
nouns, made-up words, technical terms): add the neural fallback --
see `examples/G2PNeural` for the full picture, including exact-match
numbers measured against the real CMU dictionary:

```cpp
#include "TinyTTSTools/G2P/G2PDictionaryNeuralAndRulesModel.h"
#include "TinyTTSTools/Data/neural/G2PNeuralWeights_data.h"

G2PDictionaryNeuralAndRulesModel g2p;
g2p.getNeuralModel().begin(G2P_NEURAL_MODEL_WEIGHTS, G2P_NEURAL_MODEL_WEIGHTS_LEN);
```

This is a from-scratch, dependency-free GRU model (no TensorFlow Lite) --
it costs flash for its weights but nothing else (no tensor arena, no
runtime library).

**A handful of specific words wrong or missing**: add them directly to
`PhonemeDictionaryEN.h`'s `PH_WORD(...)` table (entries must stay sorted
alphabetically) rather than reaching for a bigger dictionary -- this is
how, for example, `"quietly"` and `"whispers"` were fixed for the
`examples/AudioBiphones`/`AudioPhoneme` demo phrase.

**Your own vocabulary only** (embedded product with a fixed, known set of
words -- device names, commands, units): see `examples/G2PCustomDictionary`
for `setPhonemeDictionary()` (a fixed, pre-sorted array, set once). If you
need to add or correct pronunciations at runtime instead -- learned from
user input, a config file, or growing incrementally -- use
`DynamicPhonemeDictionary` (`PhonemeDictionary/DynamicPhonemeDictionary.h`) via
`useCompactDictionary()`, and call `add()`/`remove()` any time after.

## Multi-word text and pauses

`TinyTTSTools::say()` tokenizes text into words, converts each via the G2P
model, and joins the whole utterance into one phoneme sequence with a
short pause phoneme (`SP`) between words before handing it to the vocoder
in a single call -- vocoders that need multi-phoneme context (`DiphoneVocoder`
for real diphone transitions, `PhonemeVocoder` for cross-unit blending)
depend on seeing the whole sequence at once, not one phoneme at a time.
You don't need to manage this yourself; it's what `say()` does.

If you're driving a vocoder directly with `sayPhoneme()` (bypassing
`TinyTTSTools`/G2P entirely, e.g. to test specific phoneme sequences),
pass the whole sequence as one space-separated string for the same reason:

```cpp
synth.sayPhoneme(PhonemeType::ARPAbet, "HH EH L OW SP W ER L D", out);  // "hello world"
```

## Audio output

Any `Print`-compatible sink works. The Arduino Audio Tools classes cover
the common hardware targets:

```cpp
I2SStream out;           // I2S DAC/codec
AnalogAudioStream out;   // internal DAC pin
PWMAudioStream out;      // PWM output
```

Or use `TTSAudioOutputCallback` to receive PCM data via a callback instead
of writing to a stream directly -- useful if your output path doesn't fit
the `Print` interface (e.g. handing samples to a separate audio library or
task).

## Where to go next

- [BUILDING.md](BUILDING.md) -- build and test everything on desktop, no board required
- `examples/` -- one complete, runnable sketch per vocoder and G2P combination
- [PHONEMES.md](PHONEMES.md) -- the ARPAbet phoneme set this library uses throughout
- [../README.md](../README.md) -- architecture overview and installation
