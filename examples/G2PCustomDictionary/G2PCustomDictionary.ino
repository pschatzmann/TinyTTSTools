/*
 * G2P Custom Dictionary Example
 *
 * This example demonstrates how to use custom phoneme dictionaries
 * with the TinyTTSTools library to convert text to phonemes without audio output.
 *
 * The setPhonemeDictionary() method allows you to:
 * - Use a custom vocabulary optimized for your application
 * - Add specialized pronunciations for technical terms
 * - Support multiple languages or dialects
 */

// AudioTools.h is not used for audio here -- on real Arduino, Serial/delay
// are already globally available, but on a desktop build they only exist
// via AudioTools' Arduino-compatibility emulation layer.
#include "AudioTools.h"
#include "TinyTTSTools/G2P/G2PDictionaryModel.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"


// Example: Custom phoneme dictionary with only a few words
// Note: Dictionary must be sorted alphabetically by word for binary search to work
const PhonemeEntry CUSTOM_DICTIONARY[] = {
    {"arduino", "AA R D UW IY N OW"},
    {"microcontroller", "M AY K R OW K AH N T R OW L ER"},
    {"sensor", "S EH N S ER"},
    {"temperature", "T EH M P ER AH CH ER"},
    {"voltage", "V OW L T IH JH"}};

const size_t CUSTOM_DICTIONARY_SIZE =
    sizeof(CUSTOM_DICTIONARY) / sizeof(PhonemeEntry);

// Global G2P instance
G2PDictionaryModel g2p;

void demonstrateCustomDictionary() {
  // EXAMPLE: Use custom dictionary
  Serial.println("\n--- Using Custom Dictionary ---");
  g2p.setPhonemeDictionary((PhonemeEntry*)CUSTOM_DICTIONARY,
                           CUSTOM_DICTIONARY_SIZE, PhonemeType::ARPAbet);

  Serial.print("Current dictionary size: ");
  Serial.println(g2p.getPhonemeDictionarySize());

  // Test words from custom dictionary
  const char* customWords[] = {"arduino", "sensor", "temperature", "voltage", "microcontroller"};
  
  Serial.println("\nCustom Dictionary Phoneme Conversions:");
  for (int i = 0; i < 5; i++) {
    auto phonemes = g2p.wordToPhonemes(customWords[i]);
    Serial.print("Word: \"");
    Serial.print(customWords[i]);
    Serial.print("\" -> Phonemes: ");
    Serial.println(phonemes.c_str());
  }

  delay(1000);

  // EXAMPLE 2: Reset to default dictionary
  Serial.println("\n--- Resetting to Default Dictionary ---");
  g2p.resetPhonemeDictionary();

  Serial.print("Default dictionary size: ");
  Serial.println(g2p.getPhonemeDictionarySize());

  // Test with words from default dictionary
  const char* defaultWords[] = {"hello", "world", "test", "example"};
  
  Serial.println("\nDefault Dictionary Phoneme Conversions:");
  for (int i = 0; i < 4; i++) {
    auto phonemes = g2p.wordToPhonemes(defaultWords[i]);
    Serial.print("Word: \"");
    Serial.print(defaultWords[i]);
    Serial.print("\" -> Phonemes: ");
    Serial.println(phonemes.c_str());
  }
}

void setup() {
  TTSExample::waitForSerial();

  Serial.println("G2P Custom Dictionary Example");
  Serial.println("==============================");
    
  // Demonstrate custom dictionary usage
  demonstrateCustomDictionary();
}


void loop() {
  // This example runs once in setup() and then does nothing
  // The G2P custom dictionary demonstration is complete
  delay(1000);
}
