/*
 * Audio PSOLA TTS Example
 *
 * This example demonstrates audio synthesis using the TinyTTSTools library
 * with TD-PSOLA-based synthesis for text-to-speech synthesis on microcontrollers.
 * The PSOLAVocoder plays back the same pre-recorded ARPAbet phoneme samples as
 * PhonemeVocoder, but re-synthesizes them via TD-PSOLA (Time-Domain
 * Pitch-Synchronous Overlap-Add) -- the technique the Praat phonetics
 * software is best known for -- so it can genuinely stretch/compress
 * duration and shift pitch (via PhonemeSynthesisParams::speed/pitchHz)
 * instead of only ever truncating pre-recorded audio, which is all
 * PhonemeVocoder/DiphoneVocoder can do.
 */
#include "AudioTools.h"
#include "TinyTTSTools.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"
#include "TinyTTSTools/SoundDictionary/ArpabetWAVDictionary.h"
#include "TinyTTSTools/Vocoder/PSOLAVocoder.h"

// Global TTS instance with PSOLAVocoder
G2PDictionaryAndRulesModel g2p;
PSOLAVocoder synth(ArpabetWAVDictionary); // TD-PSOLA synthesis using global dictionary
I2SStream out; // AudioTools: output via i2s
//CsvOutput<int16_t> out;  // Test with CSV to see waveforms
TinyTTSTools tts(g2p, synth, out);

// speak() bypasses tts.say()'s no-params path to thread pitch/speed
// through -- toPhonemes() + a space-joined sayPhoneme(params) call
// replicates say()'s own text->phonemes conversion.
bool speak(const char* text, const PhonemeSynthesisParams& params) {
  std::vector<std::string> phonemes = tts.toPhonemes(text);
  if (phonemes.empty()) return false;
  std::string joined;
  for (const std::string& p : phonemes) {
    if (p.empty()) continue;
    if (!joined.empty()) joined += ' ';
    joined += p;
  }
  return tts.sayPhoneme(g2p.getDefaultPhonemeType(), joined, params);
}

// Speech completion callback
void speechCompleteCallback() {
  Serial.println("Speech synthesis completed!");
  Serial.println();
}

// Error handling callback
void speechErrorCallback(const char* error) {
  Serial.print("TTS Error: ");
  Serial.println(error);
}

void setup() {
  TTSExample::waitForSerial();

  // start I2S using the TTS engine's own audio configuration
  TTSExample::beginI2S(out, tts.getConfig());

  // Initialize TTS
  if (!tts.begin()) {
    Serial.println("Failed to initialize TTS!");
    return;
  }

  // Set up callbacks
  tts.setSpeechCompleteCallback(speechCompleteCallback);
  tts.setSpeechErrorCallback(speechErrorCallback);

  Serial.println("Audio PSOLA TTS initialized successfully!");
  Serial.println("Using PSOLAVocoder (TD-PSOLA re-synthesis) with pre-recorded ARPAbet phoneme samples.");
  Serial.println();
}

void loop() {
  const char* txt = "The quick brown fox jumps over the lazy dog.";

  Serial.println("Speaking at normal pitch and speed: ");
  Serial.println(txt);
  tts.say(txt);
  delay(4000);

  Serial.println("Speaking at a higher pitch (220Hz): ");
  PhonemeSynthesisParams highPitch;
  highPitch.pitchHz = 220.0f;
  speak(txt, highPitch);
  delay(4000);

  Serial.println("Speaking at half speed (genuinely stretched, not truncated): ");
  PhonemeSynthesisParams halfSpeed;
  halfSpeed.speed = 0.5f;
  speak(txt, halfSpeed);
  delay(6000);
}
