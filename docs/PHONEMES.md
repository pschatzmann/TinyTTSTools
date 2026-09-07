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

## Complete Phoneme Set (43 Total)

TinyTTSTools includes 43 phonemes with unique IDs (0-42), duration information, and classification.

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

## Technical Notes

- **Sequential IDs**: Each phoneme has a unique ID (0-42) for array indexing
- **Thread Safety**: The phoneme mapping table is immutable and thread-safe
- **Memory Efficient**: Static lookup table with minimal memory footprint
- **Unicode Support**: Full IPA Unicode character support for international use
- **Default Fallback**: Unknown phonemes default to 100ms duration and silence classification


