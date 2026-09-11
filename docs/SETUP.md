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
│   ├── phonemes-from-espeak/   # Isolated single-phoneme audio (ArpabetWAVDictionary) --
│   │                           # both the English ARPAbet set and the international
│   │                           # phonemes DE/FR/ES need (see "International audio" below)
│   └── diphones/               # Phoneme-to-phoneme transition audio (DiphoneWAVDictionary,
│                               # English only)
├── dictionary-en/               # English: CMU dictionary + exceptions (Huffman-compressed,
│                                 # uint8_t packed symbol -- ARPAbet only)
├── dictionary-common/           # OLaPh IPA-corpus parser/packer, shared by DE/FR/ES
│                                 # (parameterized by --lang, not per-language code)
├── dictionary-de/, dictionary-fr/, dictionary-es/   # Each just its own OLaPh corpus
│                                                     # (olaph_{lang}.txt)
├── neural-en/                   # Neural G2P training/export, English (trained, weights shipped)
└── neural-de/, neural-fr/, neural-es/   # Same pipeline, scaffolded but NOT YET TRAINED
                                          # (see "Neural G2P" below)
```

See [ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md) for the end-to-end guide
tying all of this together (which pipeline to use for what, and how to
add a fifth language).

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

### International audio (German/French/Spanish phonemes)

`generate_international_phonemes_mbrola.sh` (same directory) generates
the additional international-id/modifier phonemes the German/French/Spanish
dictionaries and rule-based models actually use (currently 24 phonemes:
23 international `Phone` ids plus a dedicated German long-a recording,
`"AF:"`) -- `ArpabetWAVDictionary` now has 65 entries total, not just the
original 41 ARPAbet ones. Requires the `mbrola-de6`, `mbrola-fr4`, and
`mbrola-es1` voice databases in addition to `mbrola-en1`
(`apt-get install mbrola-de6 mbrola-fr4 mbrola-es1`):

```bash
cd setup/audio/phonemes-from-espeak
./generate_international_phonemes_mbrola.sh   # adds to the same original/ and pcm8bit/
python3 generate_wav_dictionary.py --input pcm8bit --force
```

Each phoneme is sourced from whichever voice's own documented SAMPA
inventory actually defines it (no single voice covers all three
languages) -- see the script's own header comment for the full per-phoneme
voice/SAMPA-symbol table and the two known approximations (`BETA`/`GH`
reuse `es1`'s plain stops, since that database has no fricative-allophone
pair; `RU` is rendered once from German and reused by French, both being
the same uvular /ʁ/). A modifier-tagged phoneme like `"AF:"` can't use a
literal colon in its filename (illegal on FAT32 SD cards, unsafe in a
`#include` path) -- `generate_wav_dictionary.py`'s `DISPLAY_NAME_OVERRIDES`
maps the safe filename (`AF-.wav`) back to the real runtime lookup key
(`"AF:"`) in the generated C++ only.

This does NOT give `PhonemeVocoder`/`PSOLAVocoder` full international
coverage -- only the ~23 phonemes the bundled DE/FR/ES dictionaries/rules
reference have recordings; `FormantVocoder` (procedural) remains the only
vocoder with complete coverage, and `DiphoneVocoder` has none at all for
non-English (no international diphone pairs exist). See
[ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md) for the full picture.

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

[`../data/`](../data/) ships the same phoneme/diphone audio, word->phoneme
dictionaries (English CMU + German/French/Spanish OLaPh), and neural G2P
weights as real files -- for use with the SD-card classes
(`AudioDictionarySD`, `AudioEncodedDictionarySD`,
`CompressedPhonemeDictionarySD`, `CompressedPhonemeDictionaryWideSD`,
`G2PNeuralModelSD`) instead of the PROGMEM ones, when you'd rather spend
SD/LittleFS/PSRAM storage than flash -- the natural choice for the OLaPh
dictionaries (German ~1.6MB, Spanish ~1.0MB, French ~0.9MB after
frequency-filtering to fit PSRAM, see below), which are still too large
for comfortable flash embedding on most boards. See [`../data/README.md`](../data/README.md) for
layout and usage, and [MEMORY.md](MEMORY.md#loading-runtime-data-into-psram-esp32)
for loading it into PSRAM. Every file here is produced automatically as
part of the same `setup/` script run that produces its PROGMEM
counterpart -- neither is a separately maintained artifact (see each
section below).

## Word -> phoneme dictionaries: English (`dictionary-en/`)

Generates the two big compact/compressed dictionaries
(`CompactCmuDictionaryEN_data.h` -- full 123k-word CMU dictionary,
`PhonemeExceptionDictionaryEN_data.h` -- the subset `G2PRuleBasedModelEN`'s
rules get wrong) and their shared Huffman code table
(`PhonemeHuffmanCodes.h`). ARPAbet only (`uint8_t` packed symbol, ids 0-42)
-- see below for the international pipeline.

`export_dynamic_cmudict.py` packs the same CMU dictionary entries into
`../data/dictionary/cmudict.bin` instead -- the runtime-loadable form for
`CompressedPhonemeDictionarySD` (SD card/LittleFS, optionally PSRAM on
ESP32), rather than a compiled-in flash array:

```bash
cd setup/dictionary-en
python3 export_dynamic_cmudict.py
```

The small, hand-maintained ~534-word default dictionary
(`src/TinyTTSTools/PhonemeDictionary/PhonemeDictionaryEN.h`) is unrelated to this
pipeline -- it's a readable `PH_WORD(...)` source table compiled at build
time, not generated by a script; edit it directly (keeping entries sorted
alphabetically) to add or correct specific words. The equivalent DE/FR/ES
tables (below) work the same way.

See [`../setup/dictionary-en/README.md`](../setup/dictionary-en/README.md)
for the full 4-step pipeline order (dump -> failures -> Huffman -> pack)
and verification procedure.

## Word -> phoneme dictionaries: German/French/Spanish (`dictionary-common/`, `dictionary-de/fr/es/`)

Same idea as the English pipeline -- a full-corpus, Huffman-compressed
dictionary -- but built from the
[OLaPh](https://huggingface.co/datasets/cstr/g2p-dicts) pronunciation
corpus (real IPA transcriptions, not ARPAbet) instead of CMU dict, and
using the WIDENED packed symbol (`uint16_t`: base id << 2 | stress, with a
non-stress `PhonemeModifier` OR'd in at bit 9) needed to cover the
international `Phone` id range plus modifier tags. This is a completely
separate class (`CompressedPhonemeDictionaryWide.h`) from English's
`CompressedPhonemeDictionary` -- the English pipeline/data is untouched.

```bash
cd setup/dictionary-common
python3 olaph_build_huffman.py --lang de    # emits PhonemeHuffmanCodesWideDE.h
python3 olaph_pack_compressed.py --lang de  # emits Data/dictionary/CompactOlaphDE_data.h
```

(substitute `fr`/`es` for the other two languages). Coverage against the
real corpora: German 92.2% (1,123,259 lines; most of the remainder is a
deliberate skip of multi-word/digit compound entries, not parser failure),
French 98.8%, Spanish 100.0%. Parsed at full size, the resulting
Huffman-compressed dictionaries are large: German 19.1MB (878,007 words),
Spanish 11.1MB (600,248 words), French 4.3MB (255,759 words) -- too big
for ESP32 PSRAM (typically 2-8MB total, shared with everything else the
sketch needs). The shipped dictionaries are instead filtered down to each
language's top 100,000 most frequent words (via
[hermitdave/FrequencyWords](https://github.com/hermitdave/FrequencyWords)'
`content/2016/{xx}/{xx}_full.txt`) before packing -- see
[ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md) Step 6.6 for the exact filter
script -- bringing them down to German 1.6MB (93,226 words), Spanish
1.0MB (62,394 words), French 0.9MB (59,906 words). This tier still
targets desktop or SD-card/PSRAM loading, not flash-resident embedding,
unlike the small built-in `PhonemeDictionaryDE/FR/ES.h` (~970-990 words
each, generated from this same corpus's top-1000-frequency-word slice --
see `setup/dictionary-common/generate_small_dictionary.py` and
[ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md)).

See [ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md) for the IPA-to-`Phone`
mapping design (`olaph_ipa_map.py`) and how to point this pipeline at a
different corpus/language.

## Neural G2P (`neural-en/`, `neural-de/fr/es/`)

A dependency-free (no TensorFlow Lite) GRU-based neural grapheme-to-phoneme
fallback (`src/TinyTTSTools/G2P/G2PNeuralModel.h`), for words no dictionary
covers -- see `examples/G2PNeural` and [TUTORIAL.md](TUTORIAL.md)'s
"Choosing a G2P model" section.

**English (`neural-en/`)**: the currently-shipped weights
(`src/TinyTTSTools/Data/neural/G2PNeuralWeightsEN_data.h`, ~970KB) were
ported from the sibling TinyTTS project's own pretrained model -- not
trained by anything in this repository, though `neural-en/`'s own pipeline
(`prepare_data.py` -> `train_g2p_model.py` -> `export_g2p_model.py` ->
`validate_export.py`) can retrain a drop-in replacement from
`dictionary-en/cmudict_dump.txt`. See `neural-en/README.md` for the full
4-step walkthrough.

**German/French/Spanish (`neural-de/`, `neural-fr/`, `neural-es/`)**: same
architecture, `model.py`/`train_g2p_model.py`/`validate_export.py` copied
verbatim (fully generic, driven entirely by `vocab.py`);
`export_g2p_model.py` is copied but with its default output path and
symbol names made language-specific (a real mistake to watch for -- see
[ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md)'s Step 7). Each language's
`vocab.py` was generated from its own parsed OLaPh corpus (not
hand-picked): the grapheme set is every letter appearing at least 50 times
in the filtered word set, the phoneme set is every phoneme text token
(matching `CompressedPhonemeDictionaryWide`'s own decode format) appearing
at least 50 times -- German 155 phoneme classes/31 letters, French 38/12,
Spanish 61/7. `prepare_data.py` pulls from the same OLaPh corpus + parser
(`dictionary-common/olaph_parse.py`) the compressed dictionary is built
from, so the two cover identical words with identical transcriptions.
**All three are trained**: French 94.8%, Spanish 98.7%, German 71.2%
validation exact-match (German trained on the full unfiltered 878k-word
corpus, not the top-100k-filtered dictionary tier -- see
`setup/neural-de/README.md` for why its accuracy trails the other two).
All exported and validated -- `setup/neural-{fr,es,de}/g2p_model.bin`,
`data/neural/g2p_model_{fr,es,de}.bin` -- but none are wired into
`G2PNeuralModel.h` yet (no matching `PHONEME_TABLE`). See each directory's
own `README.md`, and [ADDING_A_LANGUAGE.md](ADDING_A_LANGUAGE.md) for what
finishing this would involve.

