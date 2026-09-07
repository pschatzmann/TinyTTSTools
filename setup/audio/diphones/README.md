# Diphone Audio Database for TinyTTSTools

Comprehensive diphone audio database for high-quality speech synthesis using the TinyTTSTools library. This directory contains pre-generated diphone samples and tools for creating custom diphone databases.

## Overview

**Diphones** are speech units that capture the transition between two phonemes, providing more natural-sounding speech synthesis than individual phonemes. This database contains linguistically relevant diphones for English, optimized for embedded systems and microcontrollers.

Each diphone spans roughly the **second half of phoneme 1 through the first
half of phoneme 2** (using each phoneme's own natural duration from
`src/TinyTTSTools/Basic/Phonemes.h`, halved) -- not a full phoneme 1
followed by a full phoneme 2. `DiphoneVocoder` plays consecutive diphones
back-to-back over a **sliding window across every adjacent phoneme pair**
(N phonemes -> N-1 diphones, not N/2 disjoint pairs), so this half+half
shape is what makes each shared phoneme's duration come out right once,
not doubled, at every transition.

The `adpcm/` files are real IMA-ADPCM (WAVE_FORMAT_IMA_ADPCM, 4 bits/
sample, 256-byte blocks -- verified via each file's own `fmt` chunk).
`SoundEntry`/`AudioFormatDecoder.h` decode this for real at runtime (see
`ImaAdpcmDecoder`); `generate_wav_dictionary.py` must construct diphone
`SoundEntry` values with `bits=4` for that to happen -- omitting it
silently defaults to 16 (raw PCM16), which reinterprets the compressed
bytes as noise instead of decoding them (this was shipping broken before
2025-09; see `tests/test_audio_format_decoder.cpp`). The decoder was
verified bit-exact against `sox`'s own IMA-ADPCM decode across all 877
diphones (~987k samples, 0 mismatches):
```bash
# from setup/audio/diphones/, after generating adpcm/*.wav
mkdir -p /tmp/adpcm_reference && for f in adpcm/*.wav; do
    sox "$f" -e signed-integer -b 16 "/tmp/adpcm_reference/$(basename "$f")"
done
# then compare each DIPHONES[i][j] against the matching reference file's
# PCM16 samples -- see tests/test_audio_format_decoder.cpp for the
# embedded expected values from the last such run.
```

## Quick Start

### Using Pre-generated Diphones

```bash
# The database is ready to use with the following files:
# - adpcm/     - ADPCM compressed diphones (recommended for embedded)
# - original/  - High-quality originals (22kHz 16-bit for development)
```

### Generating Custom Diphones

```bash
# Generate all diphones with default ADPCM compression
./generate_relevant_diphones.sh

# Generate with different compression options
./generate_relevant_diphones.sh --codec ulaw      # µ-law compression
./generate_relevant_diphones.sh --codec alaw      # A-law compression  
./generate_relevant_diphones.sh --codec pcm       # 8-bit PCM
./generate_relevant_diphones.sh --ultra-minimal   # 4kHz ADPCM (extreme compression)

# Convert existing originals to different format
./generate_relevant_diphones.sh --convert-existing --codec ulaw
```

### Generate C++ Header Files

```bash
# Convert ADPCM WAV files to C++ header files
python3 generate_wav_dictionary.py

# Regenerate ArpabetAltWAVDictionary.h (single-phoneme dictionary built by
# reusing each phoneme's own {P}_SIL diphone) after the diphone data above
# changes -- see generate_arpabet_alt_dictionary.py
python3 generate_arpabet_alt_dictionary.py
```

`ArpabetAltWAVDictionary.h` had the same "wrong bits value" bug as the main
diphone dictionary, but as `bits=8` (PCM8) instead of the default-to-16
bug: it was hand-written/hand-adapted (no generator referenced it) and its
own header comment claimed "PCM8 WAV files" even though it reuses the same
real IMA-ADPCM `{P}_SIL` diphone data as `DIPHONES`. Fixed by adding
`generate_arpabet_alt_dictionary.py`, which assembles it from the existing
diphone headers with `bits=4`.

## Directory Structure

```
diphones/
├── README.md                           # This documentation
├── OVERVIEW.md                         # Technical overview and theory
├── generate_relevant_diphones.sh       # Main generation script (MBROLA-based)
├── generate_wav_dictionary.py          # Convert WAV to C++ headers
├── original/                           # High-quality originals (22kHz 16-bit)
│   ├── AA_B.wav                        # Diphone: AA to B transition
│   ├── AA_CH.wav                       # Diphone: AA to CH transition
│   ├── ...                             # 1600 diphone files
│   └── Z_UW.wav
├── adpcm/                              # ADPCM compressed (8kHz, ~75% size reduction)
│   ├── AA_B.wav                        # Compressed version
│   ├── AA_CH.wav
│   ├── ...
│   └── Z_UW.wav
├── u-law/                              # µ-law compressed (created on demand)
├── a-law/                              # A-law compressed (created on demand)
└── pcm-8bit/                           # 8-bit PCM (created on demand)
```

## Diphone Coverage

The database includes **1600 diphones** for English -- complete coverage of
every phoneme-pair transition needed by the full 123k-word CMU dictionary
(verified 2025-09: 0% missing, down from an initial 4.7% gap -- see below).

### 1. Silence Transitions (Word Boundaries)
- **SIL → Vowels**: Word-initial vowels (39 diphones)
- **SIL → Consonants**: Word-initial consonants (48 diphones)  
- **Vowels → SIL**: Word-final vowels (39 diphones)
- **Consonants → SIL**: Word-final consonants (48 diphones)
- **SIL → SIL**: Pauses (1 diphone)

### 2. Core Transitions
- **Vowel → Consonant**: All combinations (375 diphones)
- **Consonant → Vowel**: All combinations (360 diphones)

### 3. Consonant Clusters (hand-picked, historical)
- **Initial clusters**: sp-, st-, str-, scr-, etc. (25 diphones)
- **Final clusters**: -nt, -mp, -st, -sk, -ks, -ps, etc. (20 diphones)

### 4. Special Cases (hand-picked, historical)
- **Vowel sequences**: Hiatus cases (7 diphones)
- **Liquid/nasal transitions**: r-l, m-n combinations (20 diphones)

### 5. Full completeness pass
Sections 1-4 above were a hand-picked "linguistically relevant" subset, not
the full consonant×consonant (24×24) or vowel×vowel (15×15) cross product.
An audit against the full CMU dictionary (`setup/dictionary/`'s
`COMPACT_CMUDICT_EN`, 123,463 words) found this left 4.7% of all diphone
instances needed (480 distinct names -- e.g. `M_B`, `R_K`, `ER_IH`, `IY_OW`)
missing, affecting 31% of words. Fixed by generating every remaining
consonant-consonant and vowel-vowel pair (`generate_relevant_diphones.sh`'s
sections 7-8); `generate_diphone()`'s skip-if-exists logic means this only
adds what sections 1-4 didn't already cover.

### ARPAbet Phoneme Set (39 phonemes)
- **Vowels (15)**: AA, AE, AH, AO, AW, AY, EH, ER, EY, IH, IY, OW, OY, UH, UW
- **Consonants (24)**: B, CH, D, DH, F, G, HH, JH, K, L, M, N, NG, P, R, S, SH, T, TH, V, W, Y, Z, ZH
- **Silence (1)**: SIL

## Audio Formats

### Original Files (original/)
- **Sample Rate**: 22kHz
- **Bit Depth**: 16-bit
- **Channels**: Mono
- **Quality**: High-quality for development and testing
- **Use Case**: Development, analysis, and re-compression

### ADPCM Compressed (adpcm/)
- **Sample Rate**: 8kHz  
- **Compression**: IMA-ADPCM (4:1 ratio)
- **Size Reduction**: ~75% smaller than original
- **Quality**: Good speech quality for embedded systems
- **Use Case**: **Recommended for embedded/microcontroller applications**

### Alternative Formats (generated on demand)
- **µ-law (u-law/)**: 8kHz µ-law, traditional telecom quality
- **A-law (a-law/)**: 8kHz A-law, European telecom standard  
- **8-bit PCM (pcm-8bit/)**: 8kHz 8-bit uncompressed
- **Ultra-minimal**: 4kHz ADPCM for extreme memory constraints

## Dependencies

### For Generation (generate_relevant_diphones.sh)
```bash
# Ubuntu/Debian
sudo apt-get install mbrola mbrola-en1 sox

# macOS  
brew install mbrola sox

# Required MBROLA voice database
sudo apt-get install mbrola-en1  # English voice
```

### For Header Generation (generate_wav_dictionary.py)
```bash
# Python 3 (usually pre-installed)
python3 --version  # Should be 3.6+
```

## Integration with TinyTTSTools

### Using DiphoneVocoder

```cpp
#include "TinyTTSTools.h"
#include "DiphoneWAVDictionary.h"  // Generated header

// Create audio dictionary from generated data
ArpabetWAVDictionary diphoneDict(DIPHONES, NUM_DIPHONES);

// Create diphone vocoder
DiphoneVocoder vocoder(diphoneDict);

// Synthesize speech
std::string phonemes = "HH EH L OW W ER L D";  // "Hello World"
vocoder.sayPhoneme(PhonemeType::ARPAbet, phonemes, audioOutput);
```

### Memory Usage

Measured on the current 1600-diphone half+half database (see the Overview
above -- these are roughly half the size of the old full-phone1+full-phone2
diphones):

| Format | Total Size | Per Diphone (avg) | Memory Footprint |
|--------|------------|-------------------|------------------|
| Original (22kHz 16-bit) | ~18MB | ~11.5KB | Development only |
| ADPCM (8kHz, `adpcm/`) | ~9.8MB | ~650 bytes | **Recommended** |
| Compiled C++ data (`Data/wav/diphones/`, ADPCM-sourced) | ~893KB raw / ~7.8MB as generated source | ~570 bytes raw | What actually ships in flash |

## Quality vs. Size Trade-offs

### ADPCM (Recommended)
- ✅ **Best balance** of quality and size
- ✅ Good speech intelligibility  
- ✅ 75% size reduction
- ✅ Hardware decoder support on many MCUs
- ❌ Slight quality loss vs. original

### µ-law/A-law
- ✅ **Traditional telecom quality**
- ✅ Well-established standards
- ✅ Simple decoding algorithm  
- ❌ Lower quality than ADPCM
- ❌ Optimized for voice, not synthesis

### Ultra-minimal (4kHz ADPCM)
- ✅ **Extreme memory savings**
- ✅ Still intelligible for simple words
- ❌ Reduced quality and naturalness
- ❌ Limited frequency range

## Advanced Usage

### Custom Phoneme Set

Modify the phoneme arrays in `generate_relevant_diphones.sh`:

```bash
# Edit vowels and consonants arrays
vowels=("AA" "AE" "AH" ...)           # Add/remove vowels
consonants=("B" "CH" "D" ...)         # Add/remove consonants
```

### Custom MBROLA Voice

```bash
# Use different MBROLA voice
VOICE="us2"  # or us1, us3, etc.
MBROLA_DATABASE="/usr/share/mbrola/us2/us2"
```

### Batch Processing

```bash
# Generate multiple formats
for codec in adpcm ulaw alaw pcm; do
    ./generate_relevant_diphones.sh --codec $codec
done
```

## Troubleshooting

### MBROLA Issues
```bash
# Check MBROLA installation
mbrola --help

# Check voice database
ls /usr/share/mbrola/

# Reinstall if needed
sudo apt-get install --reinstall mbrola mbrola-en1
```

### Audio Issues
```bash
# Check sox installation
sox --help

# Install codecs
sudo apt-get install sox libsox-fmt-all
```

### Missing Diphones
```bash
# Check generation log
./generate_relevant_diphones.sh | tee generation.log

# Verify output counts
ls -1 original/*.wav | wc -l
ls -1 adpcm/*.wav | wc -l
```

## Performance Tips

### For Embedded Systems
1. **Use ADPCM format** for best quality/size balance
2. **Pre-load frequently used diphones** into RAM
3. **Implement streaming decoder** for large vocabularies
4. **Consider vocabulary reduction** for specific applications

### For Development
1. **Use original files** for analysis and testing
2. **Generate multiple formats** for comparison
3. **Profile memory usage** with target hardware
4. **Test with real speech samples**

## Technical Notes

- **Diphone timing**: Each diphone is ~500ms duration
- **Transition quality**: Optimized for smooth phoneme transitions
- **MBROLA synthesis**: High-quality formant-based synthesis
- **Phonotactic compliance**: Only linguistically valid combinations
- **Cross-platform**: Generated files work on all platforms

## License

Generated audio files are provided under MIT License.
MBROLA voice database follows MBROLA licensing terms.

---

For technical details about diphone synthesis theory, see [OVERVIEW.md](OVERVIEW.md).
