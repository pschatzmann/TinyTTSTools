# Audio Phoneme TTS Example

This example demonstrates audio synthesis using the TinyTTSTools library with phoneme-based synthesis for text-to-speech synthesis on microcontrollers.

## Overview

The AudioPhoneme example shows how to:
- Initialize the TinyTTSTools system with PhonemeVocoder
- Configure I2S audio output for real audio playback
- Synthesize speech using pre-recorded phoneme audio samples
- Output high-quality, natural-sounding audio

## Key Features

### PhonemeVocoder
- **Sample-based Synthesis**: Uses pre-recorded ARPAbet phoneme samples
- **High Quality**: Natural-sounding speech with realistic phoneme timing
- **ARPAbet Dictionary**: Standard phoneme set for English pronunciation
- **ADPCM Compression**: Efficient storage of audio samples

### Audio Output
- **I2S Stream**: Direct digital audio output via I2S interface
- **16-bit Audio**: High-quality 16-bit audio samples
- **Configurable Sample Rate**: Matches phoneme sample rate for optimal quality
- **Real-time Playback**: Seamless phoneme concatenation for fluent speech

## Hardware Requirements

- **ESP32** (recommended) - Built-in I2S and sufficient memory for phoneme storage
- **Arduino with I2S shield** - Alternative with external memory for phoneme data
- **Speaker or headphones** connected to I2S output pins
- Sufficient memory for phoneme audio dictionary (varies by dictionary size)

## Software Requirements

- **Arduino Audio Tools** library for I2S output
- **TinyTTSTools** library with PhonemeVocoder support
- **ARPAbet WAV Dictionary** with phoneme audio samples
- Compatible microcontroller environment (Arduino IDE, PlatformIO)

## Code Structure

### Main Components
```cpp
G2PDictionaryAndRulesModel g2p;        // Text-to-phoneme conversion
ArpabetWAVDictionary<WAVDecoder> dictionary;  // Phoneme audio samples
PhonemeVocoder synth(dictionary);      // Phoneme-based synthesis
I2SStream out;                         // I2S audio output
TinyTTSTools tts(g2p, synth, out);         // Complete TTS system
```

### Audio Configuration
```cpp
auto audio_cfg = tts.getConfig();
auto cfg = out.defaultConfig(TX_MODE);
cfg.sample_rate = audio_cfg.sample_rate;
cfg.bits_per_sample = audio_cfg.bits_per_sample;
cfg.channels = audio_cfg.channels;
out.begin(cfg);
```

### Speech Synthesis
```cpp
tts.say("This is an audio phoneme synthesis test.");
```

## Expected Audio Output

The example will generate spoken audio saying:
- "This is an audio phoneme synthesis test."
- "Audio phoneme synthesis sounds great!" (on Arduino)

The audio will have characteristics typical of phoneme synthesis:
- Natural-sounding speech patterns
- Smooth phoneme transitions
- Realistic pronunciation timing
- Human-like voice characteristics

## Advantages of Phoneme Synthesis

1. **Natural Sound**: Uses real human speech samples for authentic pronunciation
2. **High Quality**: Superior audio quality compared to mathematical synthesis
3. **Accurate Pronunciation**: Precise phoneme representation for clear speech
4. **Flexible Dictionary**: Can use different phoneme sets or voice samples
5. **Proven Technology**: Well-established approach used in commercial TTS systems

## Phoneme Dictionary

The example uses the ARPAbet phoneme set:
- **44 Phonemes**: Complete English phoneme coverage
- **WAV Format**: High-quality audio samples
- **ADPCM Compression**: Efficient storage for embedded systems
- **Timing Information**: Proper phoneme duration for natural speech

## I2S Wiring (ESP32)

Default I2S connections for ESP32:
- **BCLK (Bit Clock)**: GPIO 26
- **LRCLK (Word Select)**: GPIO 25  
- **DOUT (Data Out)**: GPIO 22
- **GND**: Ground connection

Connect to an I2S DAC or amplifier for audio output.

## Memory Considerations

### Phoneme Storage
- **Dictionary Size**: Varies by phoneme set and compression
- **RAM Usage**: Active phonemes loaded into memory during playback
- **Flash Storage**: Complete dictionary stored in program memory
- **Optimization**: Use ADPCM compression to reduce memory footprint

### Performance Tips
- Pre-load frequently used phonemes into RAM
- Use external memory (SD card, SPI RAM) for large dictionaries
- Implement phoneme caching for better performance
- Monitor memory usage during development

## Troubleshooting

### No Audio Output
- Check I2S wiring and connections
- Verify phoneme dictionary is properly loaded
- Monitor serial output for initialization messages
- Ensure sufficient memory for phoneme storage

### Audio Quality Issues
- Verify phoneme sample quality and format
- Check for memory corruption or insufficient RAM
- Monitor phoneme loading and playback timing
- Ensure proper I2S configuration

### Memory Issues
- Reduce dictionary size or use compression
- Implement external memory storage
- Optimize phoneme caching strategy
- Monitor memory usage with diagnostic tools

### Compilation Issues
- Install Arduino Audio Tools library
- Verify TinyTTSTools library with phoneme support
- Check phoneme dictionary file inclusion
- Ensure compatible Arduino IDE/PlatformIO version

## Next Steps

To extend this example:
1. **Custom Phonemes**: Record and integrate custom phoneme samples
2. **Multiple Voices**: Support different voice characteristics and speakers
3. **Language Support**: Add phoneme sets for other languages
4. **Dynamic Loading**: Load phonemes from external storage on demand
5. **Voice Effects**: Apply audio processing for different vocal characteristics
