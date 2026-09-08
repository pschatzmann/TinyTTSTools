# Audio PSOLA TTS Example

This example demonstrates audio synthesis using the TinyTTSTools library with TD-PSOLA-based synthesis for text-to-speech synthesis on microcontrollers.

## Overview

The AudioPSOLA example shows how to:
- Initialize the TinyTTSTools system with PSOLAVocoder
- Configure I2S audio output for real audio playback
- Synthesize speech using the same pre-recorded ARPAbet phoneme samples as PhonemeVocoder
- Genuinely shift pitch and stretch/compress duration via `PhonemeSynthesisParams`, instead of only ever truncating pre-recorded audio

## Key Features

### PSOLAVocoder
- **TD-PSOLA Re-synthesis**: Time-Domain Pitch-Synchronous Overlap-Add -- the technique the Praat phonetics software is best known for
- **Genuine Pitch Shifting**: `PhonemeSynthesisParams::pitchHz` actually changes the fundamental frequency, not just playback speed
- **Genuine Time Stretching**: `PhonemeSynthesisParams::speed` can slow speech down by *stretching* audio, not just truncating it (the only option for PhonemeVocoder/DiphoneVocoder)
- **Same Audio Data**: Reuses the same ARPAbet WAV phoneme dictionary as PhonemeVocoder -- no extra storage cost

### Audio Output
- **I2S Stream**: Direct digital audio output via I2S interface
- **16-bit Audio**: High-quality 16-bit audio samples
- **Configurable Sample Rate**: Matches phoneme sample rate for optimal quality

## Hardware Requirements

- **ESP32** (recommended) - Built-in I2S and sufficient memory for phoneme storage
- **Arduino with I2S shield** - Alternative with external memory for phoneme data
- **Speaker or headphones** connected to I2S output pins

## Software Requirements

- **Arduino Audio Tools** library for I2S output
- **TinyTTSTools** library with PSOLAVocoder support
- **ARPAbet WAV Dictionary** with phoneme audio samples
- Compatible microcontroller environment (Arduino IDE, PlatformIO)

## Code Structure

### Main Components
```cpp
G2PDictionaryAndRulesModel g2p;         // Text-to-phoneme conversion
PSOLAVocoder synth(ArpabetWAVDictionary); // TD-PSOLA re-synthesis
I2SStream out;                          // I2S audio output
TinyTTSTools tts(g2p, synth, out);          // Complete TTS system
```

### Plain Speech (default pitch/speed)
```cpp
tts.say("The quick brown fox jumps over the lazy dog.");
```

### Pitch-Shifted Speech
`tts.say()` has no params-accepting overload, so a non-default pitch/speed
bypasses it: convert text to phonemes, join them the same way `say()` does
internally, and call the params-accepting `sayPhoneme()` overload directly.
```cpp
PhonemeSynthesisParams highPitch;
highPitch.pitchHz = 220.0f;  // 0 = vocoder's own default pitch
speak(txt, highPitch);       // see the .ino for the speak() helper
```

### Time-Stretched (slowed down) Speech
```cpp
PhonemeSynthesisParams halfSpeed;
halfSpeed.speed = 0.5f;  // 1.0 = normal, 2.0 = twice as fast, 0.5 = half speed
speak(txt, halfSpeed);
```

## Expected Audio Output

The example speaks the same sentence three times:
1. At normal pitch and speed
2. At a higher pitch (220Hz) -- the same words, noticeably higher voice
3. At half speed -- genuinely slower and longer, not just cut short

## PSOLAVocoder vs. PhonemeVocoder/DiphoneVocoder

| | PhonemeVocoder / DiphoneVocoder | PSOLAVocoder |
|---|---|---|
| Audio source | Pre-recorded phoneme/diphone samples | Same pre-recorded phoneme samples |
| Duration change | Truncates only (never stretches) | Genuinely stretches or compresses |
| Pitch change | Not supported | Genuinely shifts pitch |
| CPU cost | Lower (mostly playback + cross-fade) | Higher (per-phoneme analysis + overlap-add) |
| Best for | Natural-sounding speech at a fixed rate/pitch | Speech that needs a different rate or pitch than the recordings |

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
- Verify the ARPAbet phoneme dictionary is properly loaded
- Monitor serial output for initialization messages

### Pitch/Speed Has No Audible Effect
- Confirm you're calling `speak()` (or `tts.sayPhoneme(..., params)` directly) rather than `tts.say()`, which always uses default params
- `speed` is ignored whenever `PhonemeSynthesisParams::durationMs` is also set -- an explicit fixed duration wins outright

### Compilation Issues
- Install Arduino Audio Tools library
- Verify TinyTTSTools library with PSOLAVocoder support
- Ensure compatible Arduino IDE/PlatformIO version

## Next Steps

To extend this example:
1. **Per-Phoneme Volume/Pitch**: Use `TinyTTSTools::sayPhonemesWithParams()` to vary volume, pitch, or speed independently for each phoneme in a sequence
2. **Prosody**: Sweep pitch gradually across a sentence for more natural-sounding intonation
3. **Voice Effects**: Combine pitch-shifting with `PhonemeSynthesisParams::voicing` for whispered or robotic effects
