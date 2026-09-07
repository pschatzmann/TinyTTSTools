# Audio Formant TTS Example

This example demonstrates audio synthesis using the TinyTTSTools library with formant-based synthesis for text-to-speech synthesis on microcontrollers.

## Overview

The AudioFormant example shows how to:
- Initialize the TinyTTSTools system with FormantVocoder
- Configure I2S audio output for real audio playback
- Synthesize speech using mathematical formant synthesis
- Output high-quality audio with compact memory usage

## Key Features

### FormantVocoder
- **Mathematical Synthesis**: Uses formant equations to generate speech
- **Low Memory**: Compact implementation requiring minimal storage
- **Real-time**: Generates audio samples on-the-fly
- **Configurable**: Adjustable formant parameters for different voices

### Audio Output
- **I2S Stream**: Direct digital audio output via I2S interface
- **16-bit Audio**: High-quality 16-bit audio samples
- **Configurable Sample Rate**: Default 8kHz for efficient processing
- **Real-time Playback**: Immediate audio output to connected speakers

## Hardware Requirements

- **ESP32** (recommended) - Built-in I2S and sufficient processing power
- **Arduino with I2S shield** - Alternative if ESP32 is not available
- **Speaker or headphones** connected to I2S output pins
- Sufficient processing power for real-time formant calculation

## Software Requirements

- **Arduino Audio Tools** library for I2S output
- **TinyTTSTools** library with FormantVocoder support
- Compatible microcontroller environment (Arduino IDE, PlatformIO)

## Code Structure

### Main Components
```cpp
G2PDictionaryAndRulesModel g2p;        // Text-to-phoneme conversion
FormantVocoder synth(8000);            // Formant synthesis at 8kHz
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
tts.say("This is an audio formant synthesis test.");
```

## Expected Audio Output

The example will generate spoken audio saying:
- "This is an audio formant synthesis test."
- "Audio formant synthesis is working great!" (on Arduino)

The audio will have characteristics typical of formant synthesis:
- Clear, robotic-sounding speech
- Consistent pronunciation
- Efficient processing with minimal latency

## Advantages of Formant Synthesis

1. **Memory Efficient**: No audio samples needed, pure mathematical generation
2. **Consistent Quality**: Predictable output regardless of text complexity
3. **Real-time Processing**: Low latency audio generation
4. **Configurable**: Can adjust formant parameters for different voice characteristics
5. **Compact Code**: Small footprint suitable for embedded systems

## I2S Wiring (ESP32)

Default I2S connections for ESP32:
- **BCLK (Bit Clock)**: GPIO 26
- **LRCLK (Word Select)**: GPIO 25  
- **DOUT (Data Out)**: GPIO 22
- **GND**: Ground connection

Connect to an I2S DAC or amplifier for audio output.

## Troubleshooting

### No Audio Output
- Check I2S wiring and connections
- Verify speaker/headphone functionality
- Monitor serial output for initialization messages
- Ensure sufficient power supply for audio components

### Audio Quality Issues
- Adjust sample rate in FormantVocoder constructor
- Check for electromagnetic interference
- Verify I2S timing and clock settings
- Monitor system load and processing capabilities

### Compilation Issues
- Install Arduino Audio Tools library
- Verify TinyTTSTools library installation
- Check board selection and pin definitions
- Ensure compatible Arduino IDE/PlatformIO version

## Next Steps

To extend this example:
1. **Voice Customization**: Adjust formant parameters for different voice characteristics
2. **Interactive Input**: Accept text from Serial input for real-time speech
3. **Audio Effects**: Add reverb, echo, or other digital signal processing
4. **Multiple Voices**: Implement voice switching with different formant settings
