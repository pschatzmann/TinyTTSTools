# Test WAV Dictionary with Phonemes Class

This test validates the combination of the Phonemes class and ArpabetWAVDictionary by playing all available phonemes through I2S output. It demonstrates a simple integration between phoneme information and audio data output.

## Overview

The test demonstrates:
- **Complete Phoneme Set**: Iterates through all phonemes supported by the dictionary
- **Phonemes class integration** for duration and phoneme enumeration
- **ArpabetWAVDictionary** for phoneme audio data output
- **AudioDictionary interface** for direct PCM output to I2S
- **Automatic phoneme type detection** from the dictionary

## Test Components

### Phonemes Class Usage
```cpp
Phonemes phonemes;
uint16_t duration = phonemes.getPhonemeDuration(PhonemeType::ARPAbet, phoneme);
```
- **Duration Information**: Provides realistic phoneme timing
- **ARPAbet Support**: Uses standard ARPAbet phoneme notation
- **No Initialization Required**: Class is ready to use immediately

### AudioDictionary Interface
```cpp
ArpabetWAVDictionary<WAVDecoder> dictionary;
dictionary.outputPCM(phoneme, duration, out);
```
- **Direct PCM Output**: Streams audio data directly to I2S
- **Duration-based Playback**: Uses duration from Phonemes class
- **Automatic Decoding**: Handles WAV/ADPCM decompression internally

## Test Sequence

The test automatically plays all available phonemes in the dictionary:
1. **Auto-detection**: Uses dictionary.phonemeType() to determine the phoneme type
2. **Complete enumeration**: Iterates through phonemes.getPhonemes(phonem_type)
3. **Direct output**: Each phoneme is played with its appropriate duration
4. **Continuous loop**: Repeats the full phoneme set for ongoing testing

This provides a comprehensive test of all phonemes without manual selection, allowing verification of the complete phoneme audio dictionary.

## Hardware Requirements

- **ESP32** (recommended) - Built-in I2S support
- **Arduino with I2S capability** - Alternative platform
- **Speaker or headphones** connected to I2S output
- Sufficient memory for phoneme dictionary storage

## Expected Behavior

### Console Output
```
WAV Dictionary I2S Test using Phonemes Class
============================================
Playing all phonemes with their durations:
Playing phoneme: AA (duration: 120ms)
Playing phoneme: AE (duration: 100ms)  
Playing phoneme: AH (duration: 90ms)
Playing phoneme: AO (duration: 110ms)
...
All phonemes played. Repeating...
```

### Audio Output
- Complete sequence of all available phonemes
- Each phoneme played with proper duration timing
- Continuous loop for comprehensive testing
- Clear demonstration of dictionary coverage

## Test Validation

The test validates several critical components:

### Dictionary Functionality
- ✅ Dictionary initialization and loading
- ✅ Phoneme lookup and data retrieval
- ✅ Memory management for audio samples
- ✅ Error handling for missing phonemes

### Audio Processing
- ✅ WAV/ADPCM decoding accuracy
- ✅ Audio format conversion (to 16-bit PCM)
- ✅ Sample rate handling (16kHz typical)
- ✅ Audio buffer management

### I2S Output
- ✅ I2S stream configuration and initialization
- ✅ Real-time audio data streaming
- ✅ Proper audio timing and synchronization
- ✅ Hardware interface functionality

## Technical Details

### Audio Configuration
```cpp
AudioInfo info(16000, 1, 16);  // 16kHz mono, 16-bit
```
- **Sample Rate**: 16kHz (optimal for speech phonemes)
- **Channels**: Mono (phonemes are typically single-channel)
- **Bit Depth**: 16-bit (high quality audio)

### Dictionary Usage
```cpp
ArpabetWAVDictionary<WAVDecoder> dictionary;
auto phonem_type = dictionary.phonemeType();
for (auto& phoneme : phonemes.getPhonemes(phonem_type)) {
    int duration = phonemes.getPhonemeDuration(phonem_type, phoneme);
    dictionary.outputPCM(phoneme, duration, out);
}
```

## I2S Wiring (ESP32)

Default I2S pin connections:
- **BCLK (Bit Clock)**: GPIO 26
- **LRCLK (Word Select)**: GPIO 25
- **DOUT (Data Out)**: GPIO 22
- **GND**: Ground reference

Connect to an I2S DAC, amplifier, or compatible audio device.

## Troubleshooting

### No Audio Output
- Check I2S wiring and connections
- Verify speaker/headphone connections
- Monitor serial output for initialization errors
- Ensure sufficient memory for dictionary loading

### Audio Quality Issues
- Check phoneme dictionary integrity
- Verify WAV format compatibility
- Monitor for memory corruption or buffer underruns
- Ensure proper sample rate configuration

### Dictionary Loading Errors
- Verify phoneme dictionary is properly included
- Check available memory for dictionary storage
- Ensure WAV decoder is properly initialized
- Monitor for file format or compression issues

### Timing or Synchronization Issues
- Check I2S clock configuration
- Verify audio buffer sizes
- Monitor for real-time processing delays
- Ensure proper phoneme boundary detection

## Performance Monitoring

The test includes built-in diagnostics:
- Dictionary size reporting
- Phoneme lookup validation
- Audio stream status monitoring
- Error detection and reporting

## Next Steps

This test can be extended for:
1. **Stress Testing**: Rapid phoneme switching and memory management
2. **Quality Analysis**: Audio output measurement and verification
3. **Performance Benchmarking**: Timing and resource usage analysis
4. **Integration Testing**: Combined with TTS pipeline components
5. **Custom Dictionary Testing**: Validation with different phoneme sets
