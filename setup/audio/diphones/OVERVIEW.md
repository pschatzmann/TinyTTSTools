# Diphone Speech Synthesis: Technical Overview

## Introduction

**Diphone synthesis** is a concatenative speech synthesis technique that uses speech units called "diphones" to generate natural-sounding speech. Unlike phoneme-based synthesis, which concatenates individual phonemes, diphone synthesis captures the acoustic transitions between phonemes, resulting in more fluid and natural speech output.

## What are Diphones?

A **diphone** is a speech unit that spans from the center of one phoneme to the center of the next phoneme, effectively capturing the complete acoustic transition between the two sounds.

### Diphone Structure

```
Phoneme 1:  [----***----]
Diphone:         [--------]
Phoneme 2:           [----***----]

Where *** represents the phoneme center (steady state)
And --- represents the transition regions
```

### Example: "CAT" (/K AE T/)

```
Word: "CAT"
Phonemes: /K/ - /AE/ - /T/
Diphones: 
  1. SIL_K  (silence to K)
  2. K_AE   (K to AE transition)  
  3. AE_T   (AE to T transition)
  4. T_SIL  (T to silence)
```

## Advantages of Diphone Synthesis

### 1. **Natural Transitions**
- Captures real coarticulation effects between phonemes
- Preserves acoustic continuity at phoneme boundaries
- Reduces audible "clicks" and discontinuities

### 2. **Linguistic Accuracy**
- Reflects actual speech production patterns
- Accounts for phonotactic constraints of the language
- Models context-dependent phoneme variations

### 3. **Compact Representation**
- Fewer units than triphones or larger contexts
- Manageable database size for embedded systems
- Good coverage with ~800-1200 diphones for English

### 4. **Quality vs. Efficiency**
- Better quality than phoneme synthesis
- More efficient than word-based or unit selection synthesis
- Suitable for real-time applications

## Diphone Database Design

### Complete Coverage Strategy

For comprehensive English speech synthesis, a diphone database must include:

#### 1. **Core Phoneme Transitions** (~750 diphones)
```
Vowel → Consonant:    15 × 25 = 375 diphones
Consonant → Vowel:    24 × 15 = 360 diphones
```

#### 2. **Silence Boundaries** (~175 diphones)
```
Silence → Phoneme:    39 × 1 = 39 diphones (word beginnings)
Phoneme → Silence:    39 × 1 = 39 diphones (word endings)
Silence → Silence:    1 × 1 = 1 diphone (pauses)
```

#### 3. **Consonant Clusters** (~50 diphones)
```
Initial clusters: sp-, st-, str-, scr-, pl-, pr-, etc.
Final clusters:   -nt, -mp, -st, -ks, -ps, etc.
```

#### 4. **Special Cases** (~25 diphones)
```
Vowel sequences:      Hiatus cases (piano, create)
Liquid transitions:   r-l, l-r, m-n, n-m combinations
```

### Total Database Size
- **Theoretical maximum**: 39² = 1,521 diphones
- **Linguistically relevant**: ~800-1,000 diphones  
- **Core essential**: ~600-700 diphones
- **Minimal functional**: ~400-500 diphones

## Audio Characteristics

### Temporal Properties
- **Duration**: 80-200ms per diphone (varies by phoneme type)
- **Boundary placement**: Center-to-center of adjacent phonemes
- **Overlap handling**: Smooth transitions without gaps or overlaps

### Spectral Properties  
- **Formant continuity**: Preserved across phoneme boundaries
- **Voicing transitions**: Natural voice onset/offset timing
- **Fricative characteristics**: Proper turbulence and spectral shape

### Quality Factors
- **Coarticulation**: Natural phoneme interaction effects
- **Timing**: Appropriate segment durations for perception
- **Amplitude**: Consistent energy levels across diphones

## Synthesis Process

### 1. **Text Processing**
```
Input Text: "Hello"
↓
Phonemic Transcription: /HH EH L OW/
↓
Diphone Sequence: [SIL_HH, HH_EH, EH_L, L_OW, OW_SIL]
```

### 2. **Diphone Lookup**
```cpp
// Pseudocode for diphone synthesis
std::vector<std::string> phonemes = {"HH", "EH", "L", "OW"};
std::vector<AudioSegment> diphones;

// Add initial silence-to-phoneme diphone
diphones.push_back(findDiphone("SIL_" + phonemes[0]));

// Add phoneme-to-phoneme diphones  
for (int i = 0; i < phonemes.size() - 1; i++) {
    std::string diphoneName = phonemes[i] + "_" + phonemes[i+1];
    diphones.push_back(findDiphone(diphoneName));
}

// Add final phoneme-to-silence diphone
diphones.push_back(findDiphone(phonemes.back() + "_SIL"));
```

### 3. **Audio Concatenation**
```
Diphone 1: [SIL_HH]     |------HH------|
Diphone 2: [HH_EH]              |---HH---EH---|
Diphone 3: [EH_L]                      |---EH---L---|
Diphone 4: [L_OW]                             |---L---OW---|
Diphone 5: [OW_SIL]                                  |---OW------|

Result:    |------HH---EH---L---OW------|
```

### 4. **Signal Processing**
- **Amplitude normalization**: Consistent volume levels
- **Duration adjustment**: Prosodic timing control
- **Pitch modification**: Intonation patterns (optional)
- **Spectral blending**: Smooth transitions (advanced)

## Implementation Considerations

### Memory Optimization

#### Storage Formats
```cpp
// Efficient diphone storage
struct DiphoneEntry {
    const char* name;        // "AA_B"
    uint16_t size;          // Size in bytes
    const uint8_t* data;    // Compressed audio data
    uint16_t duration_ms;   // Expected duration
};
```

#### Compression Strategies
1. **ADPCM**: 4:1 compression, good quality
2. **µ-law/A-law**: 2:1 compression, telecom quality  
3. **Spectral coding**: Higher compression, more complex
4. **Hybrid approaches**: Critical diphones uncompressed

### Real-time Considerations

#### Streaming Synthesis
```cpp
class DiphoneVocoder {
    void synthesizeStream(const std::vector<std::string>& phonemes, 
                         AudioOutputStream& output) {
        for (int i = 0; i < phonemes.size() - 1; i++) {
            std::string diphoneName = phonemes[i] + "_" + phonemes[i+1];
            AudioSegment diphone = loadDiphone(diphoneName);
            output.write(diphone.data(), diphone.size());
        }
    }
};
```

#### Buffering Strategies
- **Double buffering**: Decode next diphone while playing current
- **Predictive loading**: Pre-load common diphones
- **Cache management**: LRU cache for frequently used diphones

### Quality Enhancement

#### Prosodic Control
```cpp
struct ProsodyParams {
    float duration_scale;   // Tempo control (0.5-2.0)
    float pitch_scale;      // Pitch shifting (0.7-1.3)  
    float volume_scale;     // Volume adjustment (0.0-1.0)
};
```

#### Advanced Processing
- **PSOLA**: Pitch-Synchronous Overlap-Add for prosody
- **Spectral smoothing**: Reduce concatenation artifacts
- **Formant tracking**: Ensure smooth formant transitions
- **Voice conversion**: Adapt speaker characteristics

## Linguistic Considerations

### English Phonotactics

#### Valid Initial Clusters
```
/sp/ /st/ /sk/ /sm/ /sn/ /sl/ /sw/
/pl/ /pr/ /bl/ /br/ /tr/ /dr/ /kr/ /gr/
/fl/ /fr/ /θr/ /ʃr/ /tw/ /dw/ /kw/ /gw/
```

#### Valid Final Clusters  
```
/mp/ /nt/ /nd/ /ŋk/ /pt/ /kt/
/ps/ /ts/ /ks/ /fs/ /θs/
/lp/ /lt/ /ld/ /lk/ /lf/ /ls/
/rp/ /rt/ /rd/ /rk/ /rf/ /rs/
```

#### Cross-Linguistic Adaptation
- **Phoneme inventory**: Adjust for target language
- **Cluster patterns**: Language-specific combinations
- **Allophonic variation**: Context-dependent realizations

### Evaluation Metrics

#### Objective Measures
- **Spectral distortion**: Mel-cepstral distance
- **Fundamental frequency**: F0 continuity and naturalness
- **Segmental quality**: Phoneme recognition accuracy
- **Temporal accuracy**: Duration and timing precision

#### Subjective Measures  
- **Mean Opinion Score (MOS)**: Overall quality rating
- **Intelligibility**: Word/sentence recognition rates
- **Naturalness**: Perceptual quality assessment
- **Preference tests**: Comparison with other methods

## Future Directions

### Deep Learning Integration
- **Neural vocoders**: WaveNet, HiFi-GAN for high-quality synthesis
- **Learned representations**: End-to-end diphone learning
- **Style transfer**: Emotional and speaker adaptation
- **Data efficiency**: Few-shot learning for new voices

### Embedded Optimization
- **Hardware acceleration**: DSP and FPGA implementations
- **Quantization**: 8-bit and 16-bit neural networks
- **Model compression**: Pruning and knowledge distillation
- **Edge computing**: On-device synthesis optimization

### Applications
- **IoT devices**: Smart speakers and assistants
- **Automotive**: In-car navigation and alerts
- **Accessibility**: Screen readers and communication aids
- **Gaming**: Real-time character voice generation
- **Robotics**: Human-robot interaction

## References

### Classical Diphone Synthesis
- Klatt, D. H. (1987). "Review of text-to-speech conversion for English"
- Moulines, E. & Charpentier, F. (1990). "Pitch-synchronous waveform processing"
- Hunt, A. J. & Black, A. W. (1996). "Unit selection in a concatenative speech synthesis system"

### Modern Approaches
- Zen, H. et al. (2009). "Statistical parametric speech synthesis based on HMMs"
- van den Oord, A. et al. (2016). "WaveNet: A generative model for raw audio"  
- Kumar, K. et al. (2019). "MelGAN: Generative adversarial networks for conditional waveform synthesis"

### Implementation Guides
- Taylor, P. (2009). "Text-to-Speech Synthesis"
- Dutoit, T. (1997). "An Introduction to Text-to-Speech Synthesis"
- Lemmetty, S. (1999). "Review of speech synthesis technology"

---

This overview provides the theoretical foundation for understanding and implementing diphone-based speech synthesis in the TinyTTSTools framework.
