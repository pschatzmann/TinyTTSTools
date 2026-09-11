# TinyTTSTools Phoneme Reference

A comprehensive guide to phonemes used in the TinyTTSTools library, including their representations in different notation systems and timing information.

## Phoneme Notation Systems

TinyTTSTools supports three phoneme notation systems:

- **ARPAbet**: ASCII-based notation developed for American English (e.g., AA, B, CH)
- **IPA**: International Phonetic Alphabet using Unicode symbols (e.g., ɑ, b, tʃ)
- **X-SAMPA**: ASCII-based approximation of IPA (e.g., A, b, tS)

## Phoneme Classification

TinyTTSTools categorizes phonemes into two main classes:

- **PhonemeClass::Phoneme**: Standard speech sounds (vowels, consonants)
- **PhonemeClass::Silence**: Silence and pause markers (SIL, SP)

## Two Id Bands: ARPAbet (0-42) and International/IPA Extension (43-120)

The `Phone` enum (`src/TinyTTSTools/Basic/Phonemes.h`) has **121 entries
total, ids 0-120**. The first 43 (below) are the original ARPAbet set,
sufficient for English on their own. Ids 43-120 are an international/IPA
extension covering everything German, French, and Spanish need that
ARPAbet has no symbol for -- front rounded vowels (ü/ö), nasal vowels,
dorsal fricatives (ich-Laut/ach-Laut/jota), Spanish rhotics and palatals,
plus the rest of the IPA consonant chart and vowel quadrilateral (clicks,
ejectives, implosives, retroflex/uvular/pharyngeal series, ...). See
`Phonemes.h`'s own enum comments for the full id-by-id list with IPA
symbols and example words -- it's the authoritative reference, this file
covers the commonly-used subset.

Both bands share one lookup mechanism: `Phonemes::getPhonemeById()`,
`translatePhoneme()`, `isSilence()`, etc. all work identically regardless
of which band an id falls in. The only place the two bands are treated
differently is `CompactPhonemeDictionaryBuilder.h`'s `Seg()`/`PH_WORD()`
compile-time authoring helper, which accepts the full 0-120 range (see
"Authoring dictionary entries" below).

## Modifiers: diacritics carried alongside a base Phone id

A base `Phone` id (either band) can carry ONE diacritic/modifier --
stress, length, nasalization, palatalization, aspiration, tone, and more
-- via `PhonemeModifier` (`src/TinyTTSTools/Basic/PhonemeModifiers.h`).
This is a single-valued `uint8_t` enum, not an OR-able bitmask: a segment
that would linguistically need two simultaneous modifiers (e.g. a
palatalized AND aspirated consonant) can only keep one. This was a
deliberate memory/simplicity tradeoff -- modifiers are rare in practice,
so a `Segment{phonemeId, modifier}` costs 1 extra byte always rather than
the 4 bytes an OR-able `uint32_t` bitmask would cost for every segment,
most of which never use it.

### Writing a modifier in plain text

A phoneme string (the space-separated token format `PhonemeDictionaryBase`
returns and every vocoder's `sayPhoneme(PhonemeType, std::string, ...)`
accepts) carries a modifier as a real X-SAMPA diacritic tag attached to
its base symbol -- verified against the
[espeak-ng X-SAMPA reference](https://github.com/espeak-ng/espeak-ng/blob/master/docs/phonemes/xsampa.md),
not an invented convention. Every diacritic except stress is a **suffix**;
stress is a **prefix** on the stressed segment (X-SAMPA's own convention):

| Tag | Modifier | Meaning | Example |
|---|---|---|---|
| `"` (prefix) | `MOD_STRESS_PRIMARY` | Primary stress | `"AA` |
| `%` (prefix) | `MOD_STRESS_SECONDARY` | Secondary stress | `%AA` |
| `:` | `MOD_LONG` | Long vowel/geminate consonant | `AA:` |
| `:\` | `MOD_HALF_LONG` | Half-long | `AA:\` |
| `~` | `MOD_NASALIZED` | Nasalized | `AF~` |
| `_j` | `MOD_PALATALIZED` | Palatalized ("soft") | `T_j` |
| `_w` | `MOD_LABIALIZED` | Labialized | `K_w` |
| `_G` | `MOD_VELARIZED` | Velarized ("dark") | `L_G` |
| `_?\` | `MOD_PHARYNGEALIZED` | Pharyngealized/emphatic | `T_?\` |
| `_h` | `MOD_ASPIRATED` | Aspirated | `P_h` |
| `_}` | `MOD_UNRELEASED` | Unreleased | `K_}` |
| `_0` | `MOD_DEVOICED` | Devoiced | `Z_0` |
| `_v` | `MOD_VOICED` | Allophonically voiced | `B_v` |
| `_t` | `MOD_BREATHY` | Breathy voice | `AF_t` |
| `_k` | `MOD_CREAKY` | Creaky voice | `AF_k` |
| `=` | `MOD_SYLLABIC` | Syllabic consonant | `L=` |
| `` ` `` | `MOD_RHOTACIZED` | Rhotacized (r-colored) vowel | `` AF` `` |

A single token normally carries at most one tag; `parsePhonemeModifiers()`
strips prefix and suffix tags repeatedly (so `"AA:` is fully handled), and
if more than one distinct tag is found, the last one found wins (logged
via `TTS_LOGW`, since `PhonemeModifier` can't keep both). Example
sentence-level string: `B AH0 "N AE N AH0` ("banana" -- primary stress on
the 2nd syllable).

Lexical tone (`TONE_LEVEL_HIGH/MID/LOW`, `TONE_RISING/FALLING/DIPPING/
PEAKING/CHECKED`) has no X-SAMPA text tag -- it's set programmatically via
`PhonemeSynthesisParams::setPhonemeModifier()` or a `Seg()` call, not
written into a phoneme string, since tone is a pitch-contour effect rather
than a segmental diacritic.

### What each vocoder actually does with a modifier

- **`FormantVocoder`** honors the full list -- formant-frequency shifts
  (palatalized/labialized/velarized/pharyngealized), duration changes
  (long/aspirated/syllabic), voicing overrides (devoiced/voiced/breathy),
  and the tone/pitch-contour values, since it's procedural and can just
  compute a different result.
- **`PSOLAVocoder`** honors duration and pitch-contour modifiers fully (it
  re-synthesizes the recording at a new duration/pitch anyway), and
  approximates voicing (devoiced/breathy) by blending in noise -- but
  can't realize formant-shifting modifiers (palatalized, labialized, ...)
  at all, since TD-PSOLA is a purely time-domain technique with no
  spectral filtering step.
- **`PhonemeVocoder`/`DiphoneVocoder`** play back a fixed recording and
  don't read a segment's modifier; `AudioDictionary::getSoundEntry()`
  still tries the modifier-tagged key first (e.g. a dedicated `"AF:"`
  recording, if one exists -- see the German dictionary's long-a
  recording, `setup/audio/phonemes-from-espeak/
  generate_international_phonemes_mbrola.sh`), falling back to the bare
  base symbol's recording (logged via `TTS_LOGW`) when no dedicated
  recording exists.

## Complete Phoneme Set (43 Total)

TinyTTSTools includes 43 ARPAbet phonemes with unique IDs (0-42), duration information, and classification -- see the section above for the additional 43-120 international/IPA extension.

### Silence/Pause Markers

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example |
|----|---------|-----|---------|---------------|-------------|---------|
| 0 | **SIL** | ∅ | _ | 400 | Silence, pause | Long pause between sentences |
| 1 | **SP** | ∅ | _ | 100 | Short pause | Brief pause within phrases |

### Vowels

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example Words |
|----|---------|-----|---------|---------------|-------------|---------------|
| 2 | **AA** | ɑ | A | 150 | Open back unrounded | father, hot |
| 3 | **AE** | æ | { | 130 | Near-open front unrounded | cat, hat |
| 4 | **AH** | ʌ | V | 120 | Open-mid back unrounded | cut, but |
| 5 | **AH0** | ə | @ | 80 | Mid central (schwa, unstressed) | about, sofa |
| 6 | **AO** | ɔ | O | 160 | Open-mid back rounded | caught, saw |
| 7 | **AW** | aʊ | aU | 180 | Diphthong | how, now |
| 8 | **AY** | aɪ | aI | 180 | Diphthong | my, eye |
| 9 | **EH** | ɛ | E | 130 | Open-mid front unrounded | bet, red |
| 10 | **ER** | ɝ | 3` | 140 | Mid central rhotacized (stressed) | bird, hurt |
| 11 | **ER0** | ɚ | @` | 90 | Mid central rhotacized (unstressed) | butter, father |
| 12 | **EY** | eɪ | eI | 170 | Diphthong | bay, say |
| 13 | **IH** | ɪ | I | 110 | Near-close near-front unrounded | bit, hit |
| 14 | **IY** | i | i | 140 | Close front unrounded | beat, see |
| 15 | **OW** | oʊ | oU | 170 | Diphthong | boat, show |
| 16 | **OY** | ɔɪ | OI | 180 | Diphthong | boy, toy |
| 17 | **UH** | ʊ | U | 120 | Near-close near-back rounded | book, good |
| 18 | **UW** | u | u | 150 | Close back rounded | boot, two |

### Consonants

#### Stops (Plosives)

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example Words |
|----|---------|-----|---------|---------------|-------------|---------------|
| 19 | **B** | b | b | 70 | Voiced bilabial plosive | big, job |
| 20 | **D** | d | d | 60 | Voiced alveolar plosive | dog, bad |
| 21 | **G** | ɡ | g | 70 | Voiced velar plosive | go, big |
| 22 | **K** | k | k | 80 | Voiceless velar plosive | cat, back |
| 23 | **P** | p | p | 80 | Voiceless bilabial plosive | put, cup |
| 24 | **T** | t | t | 70 | Voiceless alveolar plosive | tea, cat |

#### Fricatives

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example Words |
|----|---------|-----|---------|---------------|-------------|---------------|
| 25 | **DH** | ð | D | 100 | Voiced dental fricative | this, mother |
| 26 | **F** | f | f | 120 | Voiceless labiodental fricative | fish, if |
| 27 | **HH** | h | h | 90 | Voiceless glottal fricative | house, ahead |
| 28 | **S** | s | s | 130 | Voiceless alveolar fricative | see, yes |
| 29 | **SH** | ʃ | S | 120 | Voiceless postalveolar fricative | she, fish |
| 30 | **TH** | θ | T | 110 | Voiceless dental fricative | think, math |
| 31 | **V** | v | v | 100 | Voiced labiodental fricative | very, have |
| 32 | **Z** | z | z | 110 | Voiced alveolar fricative | zero, his |
| 33 | **ZH** | ʒ | Z | 110 | Voiced postalveolar fricative | measure, vision |

#### Affricates

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example Words |
|----|---------|-----|---------|---------------|-------------|---------------|
| 34 | **CH** | tʃ | tS | 120 | Voiceless postalveolar affricate | chair, match |
| 35 | **JH** | dʒ | dZ | 110 | Voiced postalveolar affricate | jump, bridge |

#### Liquids and Glides

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example Words |
|----|---------|-----|---------|---------------|-------------|---------------|
| 36 | **L** | l | l | 100 | Alveolar lateral approximant | like, bell |
| 37 | **R** | ɹ | r\\ | 90 | Alveolar approximant | red, car |
| 38 | **W** | w | w | 80 | Labial-velar approximant | we, away |
| 39 | **Y** | j | j | 70 | Palatal approximant | yes, you |

#### Nasals

| ID | ARPAbet | IPA | X-SAMPA | Duration (ms) | Description | Example Words |
|----|---------|-----|---------|---------------|-------------|---------------|
| 40 | **M** | m | m | 100 | Bilabial nasal | man, home |
| 41 | **N** | n | n | 90 | Alveolar nasal | no, pen |
| 42 | **NG** | ŋ | N | 110 | Velar nasal | sing, ring |

## Duration Guidelines

Phoneme durations are based on natural speech patterns and optimized for speech synthesis:

- **Silence**: 400ms (long pause), 100ms (short pause)
- **Vowels**: 120-180ms (longer for diphthongs)
- **Stops**: 60-80ms (short burst sounds)
- **Fricatives**: 90-130ms (sustained sounds)
- **Affricates**: 100-120ms (burst + friction)
- **Liquids/Glides**: 70-100ms (smooth transitions)
- **Nasals**: 90-110ms (resonant sounds)
- **Unstressed vowels**: 80-90ms (reduced duration)

## Usage in TinyTTSTools

### Phoneme Translation

```cpp
Phonemes phonemes;

// Translate between formats
const char* ipa = phonemes.translatePhoneme(PhonemeType::ARPAbet, "AA", PhonemeType::IPA);
// Result: "ɑ"

// Translate phoneme sequences
std::string xsampa = phonemes.translatePhonemes(PhonemeType::ARPAbet, "HH EH L OW", PhonemeType::XSAMPA);
// Result: "h E l oU"
```

### Duration Information

```cpp
// Get phoneme duration
uint16_t duration = phonemes.getPhonemeDuration(PhonemeType::ARPAbet, "AA");
// Result: 150 (ms)

// Get phoneme by ID
const PhonemeInfo* info = phonemes.getPhonemeById(2); // AA phoneme
// info->duration_ms = 150, info->arpabet = "AA", info->ipa = "ɑ"
```

### Silence Detection

```cpp
// Check if phoneme is silence
bool isSil = phonemes.isSilence(PhonemeType::ARPAbet, "SIL");
// Result: true

bool isPhoneme = phonemes.isSilence(PhonemeType::ARPAbet, "AA");
// Result: false
```

### Authoring dictionary entries

`CompactPhonemeDictionaryBuilder.h`'s `PH_WORD()`/`Seg()` build a
compile-time, flash-resident dictionary from a readable source table (see
`src/TinyTTSTools/PhonemeDictionary/PhonemeDictionaryEN/DE/FR/ES.h`).
Three styles freely mix within one `PH_WORD()` call:

```cpp
PH_WORD("cat", Phone::K, Phone::AE, Phone::T),                       // (a) bare Phone list
PH_WORD("stressed", Seg(Phone::AA, PhonemeModifier::MOD_STRESS_PRIMARY), Phone::T), // (b) Seg() for a modifier
```

A third style, the extended phoneme-string syntax (`"T_j EN AA:"`) used
for words needing several modifier tags, can't share the same
compile-time table in C++17 (no string-literal non-type template
parameter) -- see `RuntimePhonemeDictionary.h` + `FallbackPhonemeDictionary.h`
for that path: a small runtime-parsed dictionary composed with the
compile-time one via `FallbackPhonemeDictionary`.

## Technical Notes

- **Sequential IDs**: Each phoneme has a unique ID (0-120: 0-42 ARPAbet, 43-120 international/IPA extension) for array indexing
- **Thread Safety**: The phoneme mapping table is immutable and thread-safe
- **Memory Efficient**: Static lookup table with minimal memory footprint
- **Unicode Support**: Full IPA Unicode character support for international use
- **Default Fallback**: Unknown phonemes default to 100ms duration and silence classification
- **Modifiers**: A base id's diacritic (stress, length, nasalization, ...) is carried separately via `PhonemeModifier` (`Segment{phonemeId, modifier}`), not baked into the id space -- see "Modifiers" above


