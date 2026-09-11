// Tests for PSOLAVocoder.h, focused on its PhonemeModifier support (see
// PSOLAVocoder's own class doc for what it can and can't realize --
// duration and pitch-contour modifiers are fully supported, voicing is
// approximated one-directionally, secondary-articulation/place-shifting
// modifiers aren't implemented at all).
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/SoundDictionary/ArpabetWAVDictionary.h"
#include "TinyTTSTools/Vocoder/PSOLAVocoder.h"

class CapturePrint : public Print {
 public:
  std::vector<int16_t> samples;
  size_t write(const uint8_t* data, size_t len) override {
    size_t n = len / sizeof(int16_t);
    const int16_t* p = reinterpret_cast<const int16_t*>(data);
    samples.insert(samples.end(), p, p + n);
    return len;
  }
};

static size_t countZeroCrossings(const std::vector<int16_t>& samples) {
  size_t count = 0;
  for (size_t i = 1; i < samples.size(); i++) {
    if ((samples[i - 1] <= 0 && samples[i] > 0) ||
        (samples[i - 1] >= 0 && samples[i] < 0))
      count++;
  }
  return count;
}

static void testBasicSynthesisStillWorks() {
  PSOLAVocoder synth(ArpabetWAVDictionary);
  CapturePrint out;
  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out);
  CHECK(ok);
  CHECK(!out.samples.empty());
}

static void testModifierLongVowelExtendsDuration() {
  PSOLAVocoder synthPlain(ArpabetWAVDictionary);
  PSOLAVocoder synthLong(ArpabetWAVDictionary);
  CapturePrint outPlain, outLong;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "AA", outPlain);
  synthLong.sayPhoneme(PhonemeType::ARPAbet, "AA:", outLong);
  CHECK(!outPlain.samples.empty());
  CHECK(outLong.samples.size() > outPlain.samples.size());
}

static void testModifierAspiratedExtendsDuration() {
  PSOLAVocoder synthPlain(ArpabetWAVDictionary);
  PSOLAVocoder synthAspirated(ArpabetWAVDictionary);
  CapturePrint outPlain, outAspirated;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "P", outPlain);
  synthAspirated.sayPhoneme(PhonemeType::ARPAbet, "P_h", outAspirated);
  CHECK(outAspirated.samples.size() > outPlain.samples.size());
}

static void testModifierSyllabicExtendsDuration() {
  PSOLAVocoder synthPlain(ArpabetWAVDictionary);
  PSOLAVocoder synthSyllabic(ArpabetWAVDictionary);
  CapturePrint outPlain, outSyllabic;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "L", outPlain);
  synthSyllabic.sayPhoneme(PhonemeType::ARPAbet, "L=", outSyllabic);
  CHECK(outSyllabic.samples.size() > outPlain.samples.size());
}

static void testStressTagExtendsDurationLikeDigitSuffix() {
  // "IH_1"-equivalent via the X-SAMPA prefix convention ("AA on its own
  // has no stress; "\"AA" prefixes primary stress) should fold into the
  // same stress-duration boost getPhonemeWithStressDuration() already
  // gives a digit-suffixed "AA1" -- same target duration either way.
  PSOLAVocoder synthDigit(ArpabetWAVDictionary);
  PSOLAVocoder synthTag(ArpabetWAVDictionary);
  CapturePrint outDigit, outTag;
  synthDigit.sayPhoneme(PhonemeType::ARPAbet, "AA1", outDigit);
  synthTag.sayPhoneme(PhonemeType::ARPAbet, "\"AA", outTag);
  CHECK_EQ(outDigit.samples.size(), outTag.samples.size());
}

static void testModifierDevoicedIncreasesZeroCrossings() {
  // Devoicing blends in generated broadband noise, which crosses zero far
  // more often than a real recorded vowel.
  PSOLAVocoder synthVoiced(ArpabetWAVDictionary);
  PSOLAVocoder synthDevoiced(ArpabetWAVDictionary);
  CapturePrint outVoiced, outDevoiced;
  synthVoiced.sayPhoneme(PhonemeType::ARPAbet, "IY", outVoiced);
  synthDevoiced.sayPhoneme(PhonemeType::ARPAbet, "IY_0", outDevoiced);
  CHECK(!outVoiced.samples.empty());
  CHECK(!outDevoiced.samples.empty());
  CHECK(countZeroCrossings(outDevoiced.samples) >
        countZeroCrossings(outVoiced.samples));
}

static void testVoicingParamNowHonored() {
  // Regression check: PhonemeSynthesisParams::voicing existed on the
  // struct before this change but PSOLAVocoder never read it -- setting
  // it to 0 must now audibly matter.
  PSOLAVocoder synthFull(ArpabetWAVDictionary);
  PSOLAVocoder synthWhisper(ArpabetWAVDictionary);
  CapturePrint outFull, outWhisper;
  PhonemeSynthesisParams pFull;
  PhonemeSynthesisParams pWhisper;
  pWhisper.voicing = 0.0f;
  synthFull.sayPhoneme(PhonemeType::ARPAbet, "IY", outFull, pFull);
  synthWhisper.sayPhoneme(PhonemeType::ARPAbet, "IY", outWhisper, pWhisper);
  CHECK(countZeroCrossings(outWhisper.samples) >
        countZeroCrossings(outFull.samples));
}

static void testTonePitchContourRisingIncreasesFrequencyOverTime() {
  // TONE_RISING (PhonemeModifier) varies each PSOLA synthesis mark's own
  // target period across the phoneme's duration -- second half should
  // have measurably more zero crossings than the first.
  PSOLAVocoder synth(ArpabetWAVDictionary, /*defaultPitchHz=*/150.0f);
  CapturePrint out;
  PhonemeSynthesisParams p;
  p.durationMs = 300;
  p.setPhonemeModifier(PhonemeModifier::TONE_RISING);
  synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out, p);
  CHECK(!out.samples.empty());

  size_t half = out.samples.size() / 2;
  std::vector<int16_t> firstHalf(out.samples.begin(), out.samples.begin() + half);
  std::vector<int16_t> secondHalf(out.samples.begin() + half, out.samples.end());
  CHECK(countZeroCrossings(secondHalf) > countZeroCrossings(firstHalf));
}

static void testToneCheckedShortensDuration() {
  PSOLAVocoder synthPlain(ArpabetWAVDictionary);
  PSOLAVocoder synthChecked(ArpabetWAVDictionary);
  CapturePrint outPlain, outChecked;
  PhonemeSynthesisParams pChecked;
  pChecked.setPhonemeModifier(PhonemeModifier::TONE_CHECKED);
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "AA", outPlain);
  synthChecked.sayPhoneme(PhonemeType::ARPAbet, "AA", outChecked, pChecked);
  CHECK(outChecked.samples.size() < outPlain.samples.size());
}

static void testUnimplementedFormantModifierStillSynthesizes() {
  // MOD_PALATALIZED isn't implemented for PSOLA (needs a spectral shift
  // this purely time-domain technique doesn't have) -- the token must
  // still resynthesize successfully via AudioDictionary's modifier
  // fallback, just without any audible formant change.
  PSOLAVocoder synth(ArpabetWAVDictionary);
  CapturePrint out;
  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet, "T_j", out);
  CHECK(ok);
  CHECK(!out.samples.empty());
}

int main() {
  testBasicSynthesisStillWorks();
  testModifierLongVowelExtendsDuration();
  testModifierAspiratedExtendsDuration();
  testModifierSyllabicExtendsDuration();
  testStressTagExtendsDurationLikeDigitSuffix();
  testModifierDevoicedIncreasesZeroCrossings();
  testVoicingParamNowHonored();
  testTonePitchContourRisingIncreasesFrequencyOverTime();
  testToneCheckedShortensDuration();
  testUnimplementedFormantModifierStillSynthesizes();
  return testSummary();
}
