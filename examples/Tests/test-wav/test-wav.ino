/*
 * Test WAV Dictionary with I2S Output using Phonemes Class
 *
 * This test demonstrates the combination of Phonemes class for duration
 * and ArpabetWAVDictionary for audio data output to I2S.
 * This test validates that:
 * - Phonemes class can provide duration information
 * - ArpabetWAVDictionary can provide audio data correctly
 * - I2S audio output functions correctly
 * - Proper timing integration between classes
 */

#include "AudioTools.h"
#include "TinyTTSTools.h"  // also aliases I2SStream to MiniAudioStream on desktop builds
#include "TinyTTSTools/Dictionary/ArpabetWAVDictionary.h"
#include "TinyTTSTools/Vocoder/PhonemeVocoder.h"
#include "TinyTTSTools/Basic/Phonemes.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"

// Audio configuration
Phonemes phonemes;
PhonemeVocoder vocoder(ArpabetWAVDictionary);
I2SStream out;
//CsvOutput<int16_t> out(Serial, 1);

void setup() {
  TTSExample::waitForSerial();
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  Serial.println("WAV Dictionary I2S Test using Phonemes Class");
  Serial.println("============================================");

  // Configure I2S output from the dictionary's own audio format
  Serial.println("Configuring I2S output...");
  TTSExample::beginI2S(out, ArpabetWAVDictionary.sampleRate(), 16,
                       ArpabetWAVDictionary.channels());

  Serial.println("I2S output...");
  auto phonem_type = ArpabetWAVDictionary.phonemeType();
  for (auto& phoneme : phonemes.getPhonemes(phonem_type)) {    
    Serial.print("Phoneme: ");
    Serial.println(phoneme);
    int duration = phonemes.getPhonemeDuration(phonem_type, phoneme);
    vocoder.sayPhoneme(phonem_type, std::string(phoneme), out);
    delay(3000);
  }
}

void loop() { delay(1000); }
