// Regression test for a critical bug in PhonemeVocoder.h (2025-09):
// PhonemeVocoder had its own synthesizePhoneme() override that treated its
// entire `phoneme` argument as ONE literal dictionary key
// (dictionary_.getSoundEntry(phoneme.c_str())). That only worked because
// every caller used to pass exactly one phoneme per call. Once
// TinyTTSTools::sayPhonemes() was fixed to join a whole utterance into one
// multi-phoneme sequence and call sayPhoneme() ONCE (so DiphoneVocoder
// could form real diphones -- see test_diphones.cpp), PhonemeVocoder
// started receiving strings like "DH AH SP K W IH K" and tried to look
// that up as a single dictionary entry, which never matches anything --
// every multi-token utterance silently produced ZERO audio output (the
// exact bug report: "AudioPhoneme does not produce any sound any more").
// Fixed by removing the override entirely: ConcatenatedAudioVocoder's own
// synthesizePhoneme() already splits sequences, handles SIL/SP, and
// flushes the batch correctly.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/Vocoder/PhonemeVocoder.h"
#include "TinyTTSTools/Dictionary/ArpabetWAVDictionary.h"

class CapturePrint : public Print {
 public:
  size_t total = 0;
  size_t write(const uint8_t* data, size_t len) override {
    total += len;
    return len;
  }
};

static void testSinglePhonemeProducesAudio() {
  PhonemeVocoder synth(ArpabetWAVDictionary);
  CapturePrint out;
  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out);
  CHECK(ok);
  CHECK(out.total > 0);
}

static void testMultiPhonemeSequenceProducesAudio() {
  PhonemeVocoder synth(ArpabetWAVDictionary);
  CapturePrint out;
  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet, "AA B", out);
  CHECK(ok);
  CHECK(out.total > 0);
}

static void testSequenceWithEmbeddedPauseProducesAudio() {
  PhonemeVocoder synth(ArpabetWAVDictionary);
  CapturePrint out;
  // Mirrors what TinyTTSTools::toPhonemes() actually emits: phonemes with
  // "SP" pauses between words, all in ONE sequence.
  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet, "AA SP B", out);
  CHECK(ok);
  CHECK(out.total > 0);
}

static void testWholeUtteranceProducesSubstantialAudio() {
  PhonemeVocoder synth(ArpabetWAVDictionary);
  CapturePrint out;
  // Roughly what "The quick brown fox" becomes after G2P + toPhonemes().
  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet,
                             "DH AH SP K W IH K SP B R OW N SP F AO K S SP", out);
  CHECK(ok);
  // A handful of phonemes at 8kHz should be at least a few thousand bytes;
  // the pre-fix behavior was exactly 0.
  CHECK(out.total > 5000);
}

int main() {
  testSinglePhonemeProducesAudio();
  testMultiPhonemeSequenceProducesAudio();
  testSequenceWithEmbeddedPauseProducesAudio();
  testWholeUtteranceProducesSubstantialAudio();
  return testSummary();
}
