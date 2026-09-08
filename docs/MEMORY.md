# Memory Usage

Flash/RAM cost of each vocoder and G2P model, so you can pick a combination
that fits your target device before writing any code. All "flash" figures
are the size of the read-only data table each class needs (PROGMEM on
Arduino/ESP boards); code size itself is small and not broken out here.
Figures marked *measured* come from `sizeof()`/binary-length checks against
the data actually shipped in this repo; others are as documented elsewhere
(see [TUTORIAL.md](TUTORIAL.md)).

## Vocoders (phoneme -> audio)

The "Audio data" column is the raw compressed sample table alone (confirmed
via `nm` on a linked binary -- see below). It is **not** the total flash a
sketch needs: `DiphoneVocoder`/`PhonemeVocoder` also pull in
`ConcatenatedAudioVocoder`'s cross-fade/coarticulation logic and an audio
decoder (`ImaAdpcmDecoder`/`Pcm8Decoder`), which add real code size on top
of the data. The "Example flash (approx.)" column is the actual difference
in compiled+linked binary size between that vocoder's example and
`AudioFormant` (which needs neither data nor a decoder), so it reflects
data + the code that data requires:

| Class | Data required | Audio data (measured) | Example flash vs. `AudioFormant` | RAM cost | Notes |
|---|---|---|---|---|---|
| [`FormantVocoder`](https://pschatzmann.github.io/TinyTTSTools/classFormantVocoder.html) | None (procedural synthesis) | ~0 (no data table) | baseline | Small, fixed (a handful of filter-state floats per instance) | Smallest footprint of the three; more robotic sound |
| [`PhonemeVocoder`](https://pschatzmann.github.io/TinyTTSTools/classPhonemeVocoder.html) | `ArpabetWAVDictionary` | ~35KB *(measured: 35,000 bytes)* | ~35KB+ (decoder/concatenation code adds a small amount) | Small, streaming (decodes one unit at a time) | Individual phoneme samples; no coarticulation between sounds |
| [`DiphoneVocoder`](https://pschatzmann.github.io/TinyTTSTools/classDiphoneVocoder.html) | `DiphoneWAVDictionary` | ~893KB *(measured: 913,664 bytes)* | **+~1.0MB** (desktop build; a real board with less aggressive dead-code elimination can be noticeably higher -- an ESP32 build of `AudioBiphones` has overflowed the default partition at ~1.5MB) | Small, streaming | Diphone samples; most natural of the three, but budget flash for the code this data needs, not just the data itself |

## G2P models (text -> phonemes)

| Class | Data required | Flash cost | Notes |
|---|---|---|---|
| [`G2PRuleBasedModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PRuleBasedModel.html) | None | ~0 (no data table, pure code) | ~17% exact-match on words outside a dictionary |
| [`G2PDictionaryModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryModel.html) (default dictionary) | Built-in 535-word `CompactPhonemeDictionary` | ~10KB *(measured: 10,136 bytes)* | 100% for the words it contains, 0% otherwise |
| [`G2PDictionaryModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryModel.html) + full CMU dictionary | `COMPACT_CMUDICT_EN` (123k words, `CompressedPhonemeDictionary`) | ~1.74MB *(measured: 1,820,804 bytes)* | ~100% for nearly all real English words |
| [`G2PDictionaryAndRulesModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryAndRulesModel.html) | Default dictionary + rules | ~10KB | The default in every audio example |
| [`G2PNeuralModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PNeuralModel.html) | GRU weights (`G2PNeuralWeights_data.h`) | ~970KB *(measured: 992,716 bytes)* | No RAM overhead beyond a small decode buffer (no tensor arena, no runtime library); ~74% exact-match on words outside a dictionary |
| [`G2PDictionaryNeuralAndRulesModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryNeuralAndRulesModel.html) | Default dictionary + rules, neural weights opt-in | ~10KB, +~970KB once `getNeuralModel().begin()` is called | Neural weights are never loaded automatically -- zero cost unless you opt in |

## Loading runtime data into PSRAM (ESP32)

Everything in the tables above is PROGMEM-embedded (compiled into flash), so
PSRAM doesn't apply to it -- there's nothing to allocate. It matters for the
*runtime-loaded* buffers used by the SD-card classes, which read a file (or
a decoded/parsed copy of one) into a heap buffer:
[`AudioDictionarySD`](https://pschatzmann.github.io/TinyTTSTools/classAudioDictionarySD.html)'s
`SDSoundEntry` (one phoneme's raw audio per lookup),
[`AudioEncodedDictionarySD`](https://pschatzmann.github.io/TinyTTSTools/classAudioEncodedDictionarySD.html)'s
file-read and decoded-PCM buffers (same, one lookup at a time), and
[`CompressedPhonemeDictionarySD`](https://pschatzmann.github.io/TinyTTSTools/classCompressedPhonemeDictionarySD.html)'s
whole loaded dictionary file -- the entire ~1.74MB CMU dictionary in the
`data/dictionary/cmudict.bin` case, held for as long as the dictionary is
in use, not per lookup.

All three classes (and `SDSoundEntry`/`AudioEncodedDictionary`) take an
`Allocator` template parameter, defaulted to `std::allocator<uint8_t>` so
existing code is unaffected. Pass `PsramAllocator<uint8_t>` (see
`src/TinyTTSTools/Memory/PsramAllocator.h`) to move those buffers into PSRAM
on ESP32 instead of internal RAM:

```cpp
#include "TinyTTSTools/SoundDictionary/AudioEncodedDictionarySD.h"
#include "TinyTTSTools/PhonemeDictionary/CompressedPhonemeDictionarySD.h"
#include "TinyTTSTools/Memory/PsramAllocator.h"

AudioEncodedDictionarySD<PsramAllocator<uint8_t>> dict(decoder, "/audio/", ".mp3");

CompressedPhonemeDictionarySD<PsramAllocator<uint8_t>> cmuDict;
cmuDict.begin("/dictionary/cmudict.bin");  // ~1.74MB, in PSRAM instead of flash
```

`PsramAllocator` falls back to internal RAM if PSRAM isn't present or is
exhausted, and to plain `malloc` off ESP32, so it's safe to use in portable
code. PSRAM is slower to access than internal RAM: a good fit for a whole
file read/decode per lookup or a dictionary held for the sketch's whole
lifetime, a poor fit for small, frequently-touched buffers like a
vocoder's synthesis buffer.

## Dictionary storage formats

| Class | Per-word overhead | When to use |
|---|---|---|
| [`CompactPhonemeDictionary`](https://pschatzmann.github.io/TinyTTSTools/classCompactPhonemeDictionary.html) | ~8 bytes/word index overhead + 1 byte/phoneme + raw word text | Small/medium dictionaries (hundreds to a few thousand words); O(1) random access during binary search |
| [`CompressedPhonemeDictionary`](https://pschatzmann.github.io/TinyTTSTools/classCompressedPhonemeDictionary.html) | ~15 bytes/word (blocked index + raw word text + Huffman-coded phonemes -- the word text itself, not the index, is the largest single piece) | Large (>10k word) dictionaries, e.g. the full CMU dictionary; trades a bounded amount of per-lookup CPU work for less flash |

## Where these numbers come from

- Rule-model figures: pure code, no data table, verified by inspection.
- "Audio data (measured)" figures come from `nm -S` on the compiled desktop
  `AudioPhoneme`/`AudioBiphones` example binaries, summing the size of every
  linked `phoneme_data_*`/`sound_data_*` symbol -- the true linked size of
  the sample data, not just the source `.h` file's byte count.
- "Example flash vs. `AudioFormant`" figures come from comparing `size`
  (text+data) across the three desktop example binaries built by this
  repo's own CMake/`ctest` setup.
- Dictionary and neural-model figures marked *measured* were obtained directly from this
  repo's shipped data: `sizeof(COMPACT_PHONEME_DICTIONARY_ENData)` for the default
  dictionary, the summed array sizes in `CompactCmuDictionaryEN_data.h` for the full CMU
  dictionary, and `G2P_NEURAL_MODEL_WEIGHTS_LEN` for the neural model's weights.
- See [SETUP.md](SETUP.md) for how each of these data files is generated.

**Caveat**: these are desktop (x86-64 gcc) measurements, useful for relative
comparison between vocoders/models, not a substitute for measuring your own
actual target build -- microcontroller toolchains (avr-gcc, xtensa-esp32)
optimize, pad and dead-code-eliminate differently, and real-world numbers
can come in higher, as the ESP32 `AudioBiphones` case above shows.
