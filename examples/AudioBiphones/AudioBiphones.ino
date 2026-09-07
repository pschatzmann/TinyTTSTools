/*
 * Audio Diphone TTS Example
 *
 * This example demonstrates audio synthesis using the TinyTTSTools library
 * with diphone-based synthesis for text-to-speech synthesis on microcontrollers.
 * The DiphoneVocoder generates speech using pre-recorded diphone audio samples
 * (phoneme-to-phoneme transitions), which provides natural-sounding speech with
 * smooth transitions. It relies purely on its default concatenation settings.
 */
#include "AudioTools.h"
#include "TinyTTSTools.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"
#include "TinyTTSTools/Dictionary/DiphoneWAVDictionary.h"
#include "TinyTTSTools/Vocoder/DiphoneVocoder.h"

// Global TTS instance with DiphoneVocoder
G2PDictionaryAndRulesModel g2p;
AudioDictionary dict(DIPHONES, NUM_DIPHONES, 8000, PhonemeType::ARPAbet, 1, 16);
DiphoneVocoder synth(dict); // Diphone-based synthesis using global dictionary
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

  Serial.println("Audio Diphone TTS initialized successfully!");
  Serial.println("Using DiphoneVocoder with pre-recorded ARPAbet diphone samples for audio output.");
  Serial.println();
}

void loop() {
  const char* txt = "The quick brown fox jumps over the lazy dog, then quietly whispers: 'Really?'";
  Serial.println("Speaking: ");
  Serial.println(txt);
  tts.say(txt);
  delay(5000);
}
