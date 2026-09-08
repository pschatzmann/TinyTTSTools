# TinyTTSTools Setup Tools

`setup/` holds the development tools that generate the audio/dictionary
data files shipped under `src/TinyTTSTools/Data/`, `src/TinyTTSTools/PhonemeDictionary/` and `src/TinyTTSTools/SoundDictionary/`.
None of this is needed to *use* the library (it's header-only, just
`#include "TinyTTSTools.h"`) -- it's only needed to *regenerate* that data,
e.g. after changing a duration table, adding words, or extending coverage.

This file is the entry point; each subdirectory below has its own more
detailed README for the specifics of that pipeline.

## Structure

```
setup/
├── audio/
│   ├── phonemes-from-espeak/   # Isolated single-phoneme audio (ArpabetWAVDictionary)
│   └── diphones/               # Phoneme-to-phoneme transition audio (DiphoneWAVDictionary)
├── dictionary/                 # Word -> phoneme dictionaries (CMU + exceptions)
└── neural/                     # Neural G2P model training/export (see "Neural G2P" below)
```

## Audio: isolated phonemes (`audio/phonemes-from-espeak/`)

Generates `ArpabetWAVDictionary.h` (used by `PhonemeVocoder`, e.g.
`examples/AudioPhoneme`) -- one real recording per phoneme, PCM8 @ 8kHz.

**Current pipeline**: MBROLA, not espeak, despite the directory's name
(historical; it started as an espeak-based pipeline that turned out to
produce unusably long, non-representative recordings -- see below).

```bash
cd setup/audio/phonemes-from-espeak
./generate_arpabet_phonemes_mbrola.sh   # synthesize original/*.wav + pcm8bit/*.wav
python3 generate_wav_dictionary.py --input pcm8bit --force
```

`generate_arpabet_phonemes_mbrola.sh` renders each phoneme via MBROLA at
*exactly* its `src/TinyTTSTools/Basic/Phonemes.h` table duration (the same
approach already proven for diphones -- MBROLA reliably renders within a
few ms of the requested duration) and verifies the real rendered duration
against what was requested. `generate_wav_dictionary.py` converts the
resulting `pcm8bit/*.wav` files into `Data/wav/arpabet/*.h` + the umbrella
`Data/wav/arpabet.h` + `SoundDictionary/ArpabetWAVDictionary.h`, and also
copies each source WAV verbatim into `../data/audio/arpabet/` (the
runtime-loadable counterpart, see "Loadable data" below) -- both are
produced from the same source files in the same run, so they can't drift
out of sync with each other.

**Why not espeak (history)**: the directory previously used espeak-ng's
isolated `[[phoneme]]` notation at a slow, deliberate speech rate, which
produced abnormally sustained recordings (every vowel hit an arbitrary
1-second crop). Played back at each phoneme's real 60-200ms table
duration, that mostly captured an unrepresentative attack/onset fragment
of an artificially held tone -- the direct cause of `AudioPhoneme`
sounding unintelligible before this was fixed (2025-09). The old
espeak/gTTS scripts and their stale docs have been removed; MBROLA is now
the only pipeline here.

## Audio: diphones (`audio/diphones/`)

Generates `DiphoneWAVDictionary.h` (used by `DiphoneVocoder`, e.g.
`examples/AudioDiphones`) -- one real MBROLA recording per phoneme-pair
transition (each diphone spans roughly the second half of phoneme 1
through the first half of phoneme 2), IMA-ADPCM @ 8kHz. Full coverage: a
2025-09 audit confirmed 0% missing diphones against the entire 123k-word
CMU dictionary (1,600 diphones total -- every vowel-vowel and
consonant-consonant pair, not just a hand-picked "common" subset).

```bash
cd setup/audio/diphones
./generate_relevant_diphones.sh          # synthesize original/*.wav + adpcm/*.wav
python3 generate_wav_dictionary.py       # emit Data/wav/diphones/*.h + DiphoneWAVDictionary.h
                                          # + copy adpcm/*.wav to ../data/audio/diphones/
```

See [`../setup/audio/diphones/README.md`](../setup/audio/diphones/README.md)
for the full diphone-timing rationale and the ADPCM format details, and
[`OVERVIEW.md`](../setup/audio/diphones/OVERVIEW.md) for the synthesis
theory.

(Removed 2025-09: `generate_arpabet_alt_dictionary.py` used to produce
`ArpabetAltWAVDictionary.h`, a public, `AudioDictionary`-shaped class that
existed only as an ADPCM-decoder regression-test fixture -- its audio was
only ~half a phoneme's duration by diphone-generation design, so it looked
usable but wasn't. The regression it protected (a manually-tagged
`SoundEntry`'s `bits` value getting IMA-ADPCM decoding wrong) is now
checked directly and more simply in `tests/test_audio_format_decoder.cpp`,
without a separate generated class in the public API surface.)

## Loadable data (`../data/`)

[`../data/`](../data/) ships the same phoneme/diphone audio and the full
CMU word->phoneme dictionary as real files -- for use with the SD-card
classes (`AudioDictionarySD`, `AudioEncodedDictionarySD`,
`CompressedPhonemeDictionarySD`) instead of the PROGMEM ones, when you'd
rather spend SD/LittleFS storage than flash. See
[`../data/README.md`](../data/README.md) for layout and usage, and
[MEMORY.md](MEMORY.md#loading-runtime-data-into-psram-esp32) for loading it
into PSRAM. The audio files are a straight copy of the same
`pcm8bit/`/`adpcm/` sources above; `dictionary/cmudict.bin` is produced by
`export_dynamic_cmudict.py` (see the "Word -> phoneme dictionaries" section
below) -- neither is a separately maintained artifact.

## Word -> phoneme dictionaries (`dictionary/`)

Generates the two big compact/compressed dictionaries
(`CompactCmuDictionaryEN_data.h` -- full 123k-word CMU dictionary,
`PhonemeExceptionDictionaryEN_data.h` -- the subset `G2PRuleBasedModel`'s
rules get wrong) and their shared Huffman code table
(`PhonemeHuffmanCodes.h`).

`export_dynamic_cmudict.py` packs the same CMU dictionary entries into
`../data/dictionary/cmudict.bin` instead -- the runtime-loadable form for
`CompressedPhonemeDictionarySD` (SD card/LittleFS, optionally PSRAM on
ESP32), rather than a compiled-in flash array:

```bash
cd setup/dictionary
python3 export_dynamic_cmudict.py
```

The small, hand-maintained ~534-word default dictionary
(`src/TinyTTSTools/PhonemeDictionary/PhonemeDictionaryEN.h`) is unrelated to this
pipeline -- it's a readable `PH_WORD(...)` source table compiled at build
time, not generated by a script; edit it directly (keeping entries sorted
alphabetically) to add or correct specific words.

See [`../setup/dictionary/README.md`](../setup/dictionary/README.md) for
the full 4-step pipeline order (dump -> failures -> Huffman -> pack) and
verification procedure.

## Neural G2P (`neural/`)

A dependency-free (no TensorFlow Lite) GRU-based neural grapheme-to-phoneme
fallback (`src/TinyTTSTools/G2P/G2PNeuralModel.h`), for words no dictionary
covers -- see `examples/G2PNeural` and [TUTORIAL.md](TUTORIAL.md)'s
"Choosing a G2P model" section.

The currently-shipped English weights
(`src/TinyTTSTools/Data/neural/G2PNeuralWeights_data.h`, ~970KB) were
ported from the sibling TinyTTS project's own pretrained model -- not
trained by anything in this repository.

*(Training pipeline: see below -- added as part of the same work that
added this section.)*

