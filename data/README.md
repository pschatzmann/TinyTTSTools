# Loadable Data

This directory is the runtime-loadable counterpart to the PROGMEM data
embedded in `src/TinyTTSTools/Data/`: the exact same audio and dictionary
data, as real files instead of compiled-in C arrays, for use with the
SD-card-loadable classes instead of the flash-embedded ones. See
[MEMORY.md](../docs/MEMORY.md#loading-runtime-data-into-psram-esp32) for
loading any of it into PSRAM on ESP32 instead of internal RAM.

## Layout

```
data/audio/arpabet/<PHONEME>.wav    -- 41 files, PCM8 unsigned, 8000Hz, mono
data/audio/diphones/<PH1_PH2>.wav   -- 1600 files, IMA-ADPCM, 8000Hz, mono
data/dictionary/cmudict.bin         -- full 123k-word CMU word->phoneme dictionary
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

## Usage: word->phoneme dictionary

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

## Placing the files

Copy `data/` onto the SD card's root (or wherever your `basePath`/file path
arguments point), or upload it to LittleFS/SPIFFS the way you would any
other filesystem data directory (Arduino IDE's/PlatformIO's filesystem
uploader tools expect a `data/` folder at the sketch root by convention --
adjust paths to match wherever you actually place it).

Every class here (`AudioDictionarySD`, `AudioEncodedDictionarySD`,
`CompressedPhonemeDictionarySD`) takes an `Allocator` template parameter
(default `std::allocator<uint8_t>`) for the buffer it loads into. Pass
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
  `setup/dictionary/export_dynamic_cmudict.py`, which packs the same
  entries and Huffman codes `regen_cmudict.py` uses for
  `CompactCmuDictionaryEN_data.h` into a runtime-loadable binary file
  instead of a compiled-in C header (see
  `setup/dictionary/pack_compressed.py`'s `emit_binary_file()` for the
  exact format `CompressedPhonemeDictionarySD` reads). This one is a
  separate script, not folded into `regen_cmudict.py` itself, so
  regenerating the PROGMEM CMU dictionary doesn't force a `data/` rewrite
  on every run -- run it explicitly when you want `cmudict.bin` refreshed.
