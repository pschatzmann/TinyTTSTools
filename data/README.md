# Loadable Data

This directory is the runtime-loadable counterpart to the PROGMEM data
embedded in `src/TinyTTSTools/Data/`: the exact same audio and dictionary
data, as real files instead of compiled-in C arrays, for use with the
SD-card-loadable classes instead of the flash-embedded ones. See
[MEMORY.md](../docs/MEMORY.md#loading-runtime-data-into-psram-esp32) for
loading any of it into PSRAM on ESP32 instead of internal RAM.

## Layout

```
data/audio/arpabet/<PHONEME>.wav    -- 65 files, PCM8 unsigned, 8000Hz, mono
                                     -- (41 English ARPAbet + 23 international
                                     -- German/French/Spanish need + 1 modifier-
                                     -- tagged German long-a; a tagged phoneme's
                                     -- filename uses "-" where its tag has ":",
                                     -- e.g. "AF-.wav" -- see PHONEMES.md)
data/audio/diphones/<PH1_PH2>.wav   -- 1600 files, IMA-ADPCM, 8000Hz, mono (English only)
data/dictionary/cmudict.bin         -- full 123k-word English CMU word->phoneme dictionary
data/dictionary/olaph_{de,fr,es}.bin -- OLaPh word->phoneme dictionaries,
                                     -- filtered to each language's top 100k
                                     -- most frequent words (see FrequencyWords
                                     -- in SETUP.md) to fit ESP32 PSRAM
                                     -- (German 93k words/1.6MB, Spanish 62k/1.0MB,
                                     -- French 60k/0.9MB)
data/neural/g2p_model_{en,fr,es,de}.bin -- trained neural G2P weights (English ~970KB,
                                     -- French ~917KB, Spanish ~958KB, German ~1.1MB)
```

Each audio file is named exactly as the audio dictionary classes expect
(`basePath + name + fileExtension`), so a base path of `data/audio/arpabet/`
or `data/audio/diphones/` and extension `.wav` works with no renaming.

## Why you'd use this instead of the PROGMEM data

Trade flash for storage: nothing here occupies flash at all (no
`ArpabetWAVDictionary.h`/`DiphoneWAVDictionary.h`/`CompactCmuDictionaryEN_data.h`
compiled in), at the cost of needing an SD card (or LittleFS/SPIFFS
partition) and a little more code. Useful when flash is tighter than
storage -- see [MEMORY.md](../docs/MEMORY.md)'s discussion of
`DiphoneVocoder`'s real flash cost, and of the full CMU dictionary's
~1.74MB, for why this matters.

## Usage: audio

```cpp
#include "TinyTTSTools/SoundDictionary/AudioDictionarySD.h"
#include "TinyTTSTools/SoundDictionary/AudioEncodedDictionarySD.h"
#include "AudioTools/AudioCodecs/CodecWavIMA.h"
#include "TinyTTSTools/Memory/PsramAllocator.h"  // optional, see below

// Phonemes: plain PCM, no decoder needed.
AudioDictionarySD<> phonemeDict("/audio/arpabet/", ".wav", 8000);
phonemeDict.begin(/* SD chip-select pin */ 10);

// Diphones: IMA-ADPCM, needs a matching AudioDecoder.
WavIMADecoder decoder;
AudioEncodedDictionarySD<> diphoneDict(decoder, "/audio/diphones/", ".wav");
diphoneDict.begin(/* SD chip-select pin */ 10);
```

## Usage: word->phoneme dictionary (English)

```cpp
#include "TinyTTSTools/PhonemeDictionary/CompressedPhonemeDictionarySD.h"
#include "TinyTTSTools/G2P/G2PDictionaryAndRulesModel.h"
#include "TinyTTSTools/Memory/PsramAllocator.h"  // optional, see below

CompressedPhonemeDictionarySD<PsramAllocator<uint8_t>> cmuDict;
cmuDict.begin("/dictionary/cmudict.bin");  // ~1.74MB, loaded into PSRAM here

G2PDictionaryAndRulesModel g2p;
g2p.getDictionaryModel().useCompactDictionary(cmuDict);
```

`cmuDict` must outlive `g2p` (`useCompactDictionary()` stores a reference,
same convention as pointing it at a PROGMEM dictionary).

## Usage: word->phoneme dictionary (German/French/Spanish)

`olaph_{de,fr,es}.bin` use the widened (`uint16_t`) packed symbol (see
[PHONEMES.md](../docs/PHONEMES.md)) -- a different loader class,
`CompressedPhonemeDictionaryWideSD`, and unlike `CompressedPhonemeDictionarySD`
it needs that language's Huffman code table passed in explicitly (small
enough to stay a compiled-in flash table -- corpus-specific, so it isn't
part of the loaded file itself; see `CompressedPhonemeDictionaryWideSD.h`'s
own doc):

```cpp
#include "TinyTTSTools/PhonemeDictionary/CompressedPhonemeDictionaryWideSD.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeHuffmanCodesWideDE.h"
#include "TinyTTSTools/Memory/PsramAllocator.h"  // optional, see below

CompressedPhonemeDictionaryWideSD<PsramAllocator<uint8_t>> deDict;
deDict.begin("/dictionary/olaph_de.bin",
             PHONEME_HUFFMAN_CODES_WIDE_DE, PHONEME_HUFFMAN_CODE_WIDE_DE_COUNT);
// ~1.6MB (German) -- PSRAM recommended over internal RAM at this size

g2p.getDictionaryModel().useCompactDictionary(deDict);
```

## Usage: neural G2P

```cpp
#include "TinyTTSTools/G2P/G2PNeuralModelSD.h"
#include "TinyTTSTools/Memory/PsramAllocator.h"  // optional, see below

G2PNeuralModelSD<PsramAllocator<uint8_t>> neural;
neural.begin("/neural/g2p_model_en.bin");  // ~970KB
```

Only English (`g2p_model_en.bin`) has a matching `arpabetForIndex()`/output
table compiled into `G2PNeuralModel.h` right now -- `g2p_model_fr.bin`
(94.8% validation exact-match), `g2p_model_es.bin` (98.7%) and
`g2p_model_de.bin` (71.2%) all exist and are trained/validated, but none
is wired into any output table yet, so loading one wouldn't decode to
anything meaningful until that
table exists (see [ADDING_A_LANGUAGE.md](../docs/ADDING_A_LANGUAGE.md)).

## Placing the files

Copy `data/` onto the SD card's root (or wherever your `basePath`/file path
arguments point), or upload it to LittleFS/SPIFFS the way you would any
other filesystem data directory (Arduino IDE's/PlatformIO's filesystem
uploader tools expect a `data/` folder at the sketch root by convention --
adjust paths to match wherever you actually place it).

Every class here (`AudioDictionarySD`, `AudioEncodedDictionarySD`,
`CompressedPhonemeDictionarySD`, `CompressedPhonemeDictionaryWideSD`,
`G2PNeuralModelSD`) takes an `Allocator` template parameter (default
`std::allocator<uint8_t>`) for the buffer it loads into. Pass
`PsramAllocator<uint8_t>` (`src/TinyTTSTools/Memory/PsramAllocator.h`) to
load into PSRAM on ESP32 instead of internal RAM -- see
[MEMORY.md](../docs/MEMORY.md#loading-runtime-data-into-psram-esp32).

## Regenerating this directory

Everything here is produced automatically by the same `setup/` scripts
that generate the PROGMEM data -- there's no separate step to remember and
no risk of the two drifting apart, since both come from the same source
files in the same run. See [docs/SETUP.md](../docs/SETUP.md) for the full
pipeline; in short:

- `data/audio/arpabet/*.wav` and `data/audio/diphones/*.wav` are copied
  verbatim by each `generate_wav_dictionary.py` (in
  `setup/audio/phonemes-from-espeak/` and `setup/audio/diphones/`
  respectively) as part of its normal run -- the same invocation that
  regenerates `ArpabetWAVDictionary.h`/`DiphoneWAVDictionary.h` also
  updates these.
- `dictionary/cmudict.bin` is produced by
  `setup/dictionary-en/export_dynamic_cmudict.py`, which packs the same
  entries and Huffman codes `regen_cmudict.py` uses for
  `CompactCmuDictionaryEN_data.h` into a runtime-loadable binary file
  instead of a compiled-in C header (see
  `setup/dictionary-en/pack_compressed.py`'s `emit_binary_file()` for the
  exact format `CompressedPhonemeDictionarySD` reads). This one is a
  separate script, not folded into `regen_cmudict.py` itself, so
  regenerating the PROGMEM CMU dictionary doesn't force a `data/` rewrite
  on every run -- run it explicitly when you want `cmudict.bin` refreshed.
- `dictionary/olaph_{de,fr,es}.bin` are produced automatically by
  `setup/dictionary-common/olaph_pack_compressed.py --lang {de,fr,es}` --
  every run of that script (which also regenerates
  `CompactOlaph{DE,FR,ES}_data.h`) writes both forms, using the same
  `emit_binary_file()` as English's (the binary layout is symbol-width-
  agnostic -- see `CompressedPhonemeDictionaryWideSD.h`'s own doc).
- `neural/g2p_model_{en,de,fr,es}.bin` are produced automatically by each
  `setup/neural-{lang}/export_g2p_model.py` -- every run (after
  `train_g2p_model.py`) writes both the flash header and this binary,
  identical payload bytes either way.
