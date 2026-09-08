# TinyTTSTools - Text-to-Speech Tools for Microcontrollers

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue?logo=arduino&logoColor=white)](https://www.arduino.cc/reference/en/libraries/)
[![CMake](https://img.shields.io/badge/CMake-supported-blue?logo=cmake&logoColor=white)](CMakeLists.txt)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue)](LICENSE)

A flexible, header-only text-to-speech (TTS) library designed specifically for microcontrollers and embedded systems. TinyTTSTools provides high-quality speech synthesis with minimal memory footprint and optional machine learning capabilities.

## Overview

Unfortunately, microcontrollers do not have enough RAM and computational resources to implement high-quality TTS using machine learning directly. Therefore, we generate audio in the following three steps:

1. **Grapheme to Phoneme (G2P)** - Converting text to phonetic representations
2. **Phoneme to Audio** - Converting phonemes to PCM data
3. **Output of Audio** - Output of PCM data to I2S, Analog Pins, PWM, PDM etc

For each of these steps, we provide different implementations with varying quality and resource requirements. 

## Grapheme to Phoneme (G2P)

All implementations are based on the common [`G2PModelBase`](https://pschatzmann.github.io/TinyTTSTools/classG2PModelBase.html) class and provide different approaches to convert text to phonemes:

### Available Models

- **[`G2PDictionaryModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryModel.html)** - The simplest method using a dictionary lookup to translate words to phonemes
- **[`G2PRuleBasedModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PRuleBasedModel.html)** - Rule-based implementation for English phoneme conversion
- **[`G2PNeuralModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PNeuralModel.html)** - Dependency-free (no TensorFlow Lite) GRU neural network fallback for out-of-dictionary words, ported from the sibling TinyTTS project

### Hybrid Approaches

Models can be combined using the [`G2PHybridModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PHybridModel.html) class for improved accuracy:
- **[`G2PDictionaryAndRulesModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryAndRulesModel.html)** - Predefined combination of dictionary and rule-based models
- **[`G2PDictionaryNeuralAndRulesModel`](https://pschatzmann.github.io/TinyTTSTools/classG2PDictionaryNeuralAndRulesModel.html)** - Predefined combination of dictionary, neural fallback and rule-based models (see `examples/G2PNeural`)

## Phoneme to Audio

In the second step, we generate audio data from phonemes. All implementations inherit from the [`VocoderBase`](https://pschatzmann.github.io/TinyTTSTools/classVocoderBase.html) class.

### Available Vocoders

#### Rule-Based Synthesis
- **[`FormantVocoder`](https://pschatzmann.github.io/TinyTTSTools/classFormantVocoder.html)** - Generates PCM audio using formant synthesis rules
  - **Pros**: Minimal memory usage, no external audio data required
  - **Cons**: Lower audio quality, more robotic sound

#### Sample-Based Synthesis
The following classes use pre-recorded (compressed) audio in PROGMEM:

- **[`PhonemeVocoder`](https://pschatzmann.github.io/TinyTTSTools/classPhonemeVocoder.html)** - Uses individual phoneme samples
  - **Pros**: Smaller dictionary, predictable output
  - **Cons**: Less natural transitions between phonemes
- **[`DiphoneVocoder`](https://pschatzmann.github.io/TinyTTSTools/classDiphoneVocoder.html)** - Uses diphone samples (combinations of two phonemes)
  - **Pros**: More natural speech with smooth phoneme transitions
  - **Cons**: Larger dictionary required

#### Sample-Based Re-synthesis
- **[`PSOLAVocoder`](https://pschatzmann.github.io/TinyTTSTools/classPSOLAVocoder.html)** - Re-synthesizes the same phoneme samples as `PhonemeVocoder` via TD-PSOLA (Time-Domain Pitch-Synchronous Overlap-Add), the technique the Praat phonetics software is best known for
  - **Pros**: Genuinely shifts pitch and stretches/compresses duration (`PhonemeSynthesisParams::pitchHz`/`speed`), instead of only ever truncating pre-recorded audio
  - **Cons**: Higher CPU cost than plain playback; same dictionary size as `PhonemeVocoder`

## Audio Output

Any subclass of [Print](https://pschatzmann.github.io/TinyTTSTools/classPrint.html) can be used to output the audio. We recommend that you use the output classes provided by the Arduino Audio Tools:

- I2SStream for i2s
- AnalogAudioStream using the internal DAC
- PWMAudioStream using PWM

Alternatively you can also use the [TTSAudioOutputCallback](https://pschatzmann.github.io/TinyTTSTools/classTTSAudioOutputCallback.html) class to output the audio with the help of a callback or the audio output classes provided by your microcontroller.

## Documentation

- [Tutorial](docs/TUTORIAL.md) - Full walkthrough: choosing a vocoder and G2P model, tuning synthesis, audio output
- [Building on Desktop](docs/BUILDING.md) - CMake build instructions, including how to build and run the test suite
- [Setup Tools](docs/SETUP.md) - Regenerating the audio/dictionary data files (`setup/`), including the neural G2P training pipeline
- [Loadable Data](data/README.md) - The same audio data as real `.wav` files, for `AudioDictionarySD`/`AudioEncodedDictionarySD` (SD card/LittleFS) instead of PROGMEM
- [Phonemes](docs/PHONEMES.md) - The ARPAbet phoneme set used throughout the library
- [Memory Usage](docs/MEMORY.md) - Flash/RAM cost of each vocoder, G2P model and dictionary format
- [Class Documentation](https://pschatzmann.github.io/TinyTTSTools/annotated.html)

## Features

- **Header-only library** - Easy integration, no separate compilation
- **Multiple synthesis methods** - Choose based on memory/quality requirements
- **Compressed audio storage** - Codecs reduce memory footprint
- **Extensible architecture** - Easy to add custom models and synthesizers
- **Machine learning support** - Dependency-free neural G2P fallback (`G2PNeuralModel`)
- **Cross-platform** - Works on Arduino, ESP32, and other microcontrollers

## Examples

See the `examples/` directory for complete usage examples:
- `AudioFormant/` - Text-to-speech using `FormantVocoder` (no audio data required)
- `AudioPhoneme/` - Text-to-speech using `PhonemeVocoder` (pre-recorded phoneme samples)
- `AudioBiphones/` - Text-to-speech using `DiphoneVocoder` (pre-recorded diphone samples)
- `AudioPSOLA/` - Text-to-speech using `PSOLAVocoder` (TD-PSOLA re-synthesis with pitch/speed control)
- `G2PCustomDictionary/` - Adding custom pronunciations, phoneme conversion only (no audio)
- `G2PNeural/` - Dictionary + neural + rule-based G2P fallback chain, phoneme conversion only (no audio)


## Installation

### Arduino IDE
1. Download ZIP from GitHub or clone: `git clone https://github.com/pschatzmann/TinyTTSTools.git`
2. Arduino IDE: `Sketch` → `Include Library` → `Add .ZIP Library...`
3. Optionally install dependency: "Arduino Audio Tools" 

### PlatformIO
```ini
lib_deps = 
    https://github.com/pschatzmann/TinyTTSTools.git
    https://github.com/pschatzmann/arduino-audio-tools.git
```




