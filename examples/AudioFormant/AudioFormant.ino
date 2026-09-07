/*
 * Audio Formant TTS Example
 * 
 * This example demonstrates audio synthesis using the TinyTTSTools library
 * with formant-based synthesis for text-to-speech synthesis on microcontrollers.
 * The FormantVocoder generates speech using mathematical formant synthesis,
 * which provides compact, low-memory speech synthesis with audio output.
 */
#include "AudioTools.h"
#include "TinyTTSTools.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"

// Global TTS instance with FormantVocoder
G2PDictionaryAndRulesModel g2p;
FormantVocoder synth(8000);  // Formant-based synthesis at 8kHz
I2SStream out; // AudioTools: output via i2s
//CsvOutput<int16_t> out;  // Test with CSV to see waveforms
TinyTTSTools tts(g2p, synth, out);


// Speech completion callback
void speechCompleteCallback() {
  Serial.println("Speech synthesis completed!");
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
  
  Serial.println("Audio Formant TTS initialized successfully!");
  Serial.println("Using FormantVocoder for compact, mathematical speech synthesis with audio output.");
  Serial.println();
}

void loop() {
  // Output speech using formant synthesis with audio
  const char* txt = "The quick brown fox jumps over the lazy dog, then quietly whispers: ‘Really?’";
  Serial.println("Speaking: ");
  Serial.println(txt);
  tts.say(txt);
  delay(5000);
  
}

