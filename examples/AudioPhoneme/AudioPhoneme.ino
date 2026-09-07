/*
 * Audio Phoneme TTS Example
 * 
 * This example demonstrates audio synthesis using the TinyTTSTools library
 * with phoneme-based synthesis for text-to-speech synthesis on microcontrollers.
 * The PhonemeVocoder generates speech using pre-recorded phoneme audio samples,
 * which provides higher quality speech synthesis with realistic phoneme timing and audio output.
 */
#include "AudioTools.h"
#include "TinyTTSTools.h"
#include "TinyTTSTools/Dictionary/ArpabetWAVDictionary.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"

// Global TTS instance with PhonemeVocoder
G2PDictionaryAndRulesModel g2p;
PhonemeVocoder synth(ArpabetWAVDictionary); // Phoneme-based synthesis using global dictionary
I2SStream out; // AudioTools: output via i2s
//CsvOutput<int16_t> out;  // Test with CSV to see waveforms
TinyTTSTools tts(g2p, synth, out);


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
  
  Serial.println("Audio Phoneme TTS initialized successfully!");
  Serial.println("Using PhonemeVocoder with pre-recorded ARPAbet phoneme samples for audio output.");
  Serial.println();
}

void loop() {
  const char* txt = "The quick brown fox jumps over the lazy dog, then quietly whispers: ‘Really?’";
  Serial.println("Speaking: ");
  Serial.println(txt);
  tts.say(txt);
  delay(5000);
}
