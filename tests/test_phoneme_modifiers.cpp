// Tests for PhonemeModifiers.h's kModifierTags-driven parser
// (parsePhonemeModifiers()), PhonemeSynthesisParams::setPhonemeModifier(),
// AudioDictionary::getSoundEntry()'s modifier-stripping fallback, and
// FormantVocoder's actual synthesis-level wiring of modifier tags
// (preparePhonemeSynthesis()/applyModifierFormantShift() in
// FormantVocoder.h -- formant shifts, duration effects, voicing
// overrides, and the f0StartRatio/MidRatio/EndRatio pitch contour).
//
// PhonemeModifier is a single-valued enum (not an OR-able bitmask -- see
// its own file-level @note) -- these tests reflect that: a token carrying
// two modifier tags keeps only the last one found (with a logged
// warning), it never combines both.
#include <cmath>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/Basic/PhonemeModifiers.h"
#include "TinyTTSTools/SoundDictionary/ArpabetWAVDictionary.h"
#include "TinyTTSTools/SoundDictionary/DiphoneWAVDictionary.h"
#include "TinyTTSTools/Vocoder/FormantVocoder.h"
#include "TinyTTSTools/Vocoder/VocoderBase.h"

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

#define CHECK_NEAR(a, b) CHECK(std::fabs((a) - (b)) < 1e-4f)

static void testNoModifierPassesThrough() {
  PhonemeModifier mod = PhonemeModifier::MOD_CREAKY;  // deliberately non-NONE
  std::string base = parsePhonemeModifiers("AA", mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK(mod == PhonemeModifier::MOD_NONE);
}

static void testSingleSuffixTag() {
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("P_h", mod);
  CHECK_EQ(base, std::string("P"));
  CHECK(mod == PhonemeModifier::MOD_ASPIRATED);
}

static void testLongVowelSuffix() {
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("AA:", mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK(mod == PhonemeModifier::MOD_LONG);
}

static void testHalfLongDoesNotMatchLongPrefix() {
  // ":\" (half-long) must not be misparsed as ":" (long) leaving a stray "\".
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("AA:\\", mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK(mod == PhonemeModifier::MOD_HALF_LONG);
}

static void testChainedSuffixesKeepsLastOneFound() {
  // "T_j_h" carries two tags (palatalized, aspirated). PhonemeModifier is
  // single-valued, so only one survives -- the base symbol is still fully
  // stripped either way. This documents the accepted lossy behavior
  // (logged as a warning inside parsePhonemeModifiers), not a bug.
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("T_j_h", mod);
  CHECK_EQ(base, std::string("T"));
  CHECK(mod == PhonemeModifier::MOD_PALATALIZED ||
        mod == PhonemeModifier::MOD_ASPIRATED);
}

static void testPrimaryStressPrefix() {
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("\"AA", mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK(mod == PhonemeModifier::MOD_STRESS_PRIMARY);
}

static void testSecondaryStressPrefix() {
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("%AA", mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK(mod == PhonemeModifier::MOD_STRESS_SECONDARY);
}

static void testPrefixAndSuffixCombinedKeepsLastOneFound() {
  // Primary stress (prefix) plus long (suffix) on the same token -- again
  // only one survives given single-valued PhonemeModifier, but the base
  // symbol must still come out fully clean.
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("\"AA:", mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK(mod == PhonemeModifier::MOD_STRESS_PRIMARY ||
        mod == PhonemeModifier::MOD_LONG);
}

static void testRhotacizedBacktickNotConfusedWithRetroflexSymbol() {
  // "AF`" (rhotacized /a/) should parse as base "AF" + MOD_RHOTACIZED, not
  // as some unrelated retroflex base symbol -- the backtick suffix only
  // ever applies after a vowel, and parsePhonemeModifiers doesn't need to
  // know the difference since it just matches the trailing tag text.
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("AF`", mod);
  CHECK_EQ(base, std::string("AF"));
  CHECK(mod == PhonemeModifier::MOD_RHOTACIZED);
}

static void testSyllabicConsonant() {
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("L=", mod);
  CHECK_EQ(base, std::string("L"));
  CHECK(mod == PhonemeModifier::MOD_SYLLABIC);
}

static void testDoesNotOverStripBareShortSymbol() {
  // A bare one-letter symbol that happens to equal a tag's own text minus
  // its leading char must not be stripped down to nothing -- the "s.size()
  // > len" guard in the parser exists exactly to keep at least one base
  // character. "Z" has no modifier tags in it at all, so it must survive
  // untouched.
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("Z", mod);
  CHECK_EQ(base, std::string("Z"));
  CHECK(mod == PhonemeModifier::MOD_NONE);
}

static void testSetPhonemeModifierNoneLeavesDefaults() {
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_NONE);
  CHECK(params.modifier == PhonemeModifier::MOD_NONE);
  CHECK_NEAR(params.volume, 1.0f);
  CHECK_NEAR(params.speed, 1.0f);
  CHECK_NEAR(params.voicing, 1.0f);
}

static void testSetPhonemeModifierIsIdempotentNotAccumulating() {
  // Replace, not compose -- a second call overwrites (recomputes from the
  // 1.0/1.0/1.0 baseline), it doesn't compound onto the first call's
  // result.
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_DEVOICED);
  CHECK_NEAR(params.voicing, 0.0f);

  params.setPhonemeModifier(PhonemeModifier::MOD_BREATHY);
  CHECK(params.modifier == PhonemeModifier::MOD_BREATHY);
  CHECK_NEAR(params.voicing, 0.6f);  // not 0.0 * 0.6 -- devoiced's effect is gone
}

static void testStressPrimaryBoostsVolumeAndSpeed() {
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_STRESS_PRIMARY);
  CHECK_NEAR(params.volume, 1.15f);
  CHECK_NEAR(params.speed, 1.0f / 1.2f);  // slower speed = longer duration
  CHECK_NEAR(params.voicing, 1.0f);       // stress alone doesn't touch voicing
}

static void testStressSecondaryBoostsLessThanPrimary() {
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_STRESS_SECONDARY);
  CHECK_NEAR(params.volume, 1.07f);
  CHECK_NEAR(params.speed, 1.0f / 1.1f);
  CHECK(params.volume < 1.15f);  // secondary is a smaller boost than primary
}

static void testLongVowelSlowsSpeedOnly() {
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_LONG);
  CHECK_NEAR(params.speed, 1.0f / 1.8f);
  CHECK_NEAR(params.volume, 1.0f);
  CHECK_NEAR(params.voicing, 1.0f);
}

static void testHalfLongSlowsLessThanLong() {
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_HALF_LONG);
  CHECK_NEAR(params.speed, 1.0f / 1.3f);
  CHECK(params.speed > 1.0f / 1.8f);  // half-long stretches less than full long
}

static void testVoicedForcesFullVoicing() {
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_DEVOICED);
  CHECK_NEAR(params.voicing, 0.0f);
  params.setPhonemeModifier(PhonemeModifier::MOD_VOICED);
  CHECK_NEAR(params.voicing, 1.0f);
}

static void testCreakyDoesNotAffectVoicing() {
  // MOD_CREAKY is deliberately NOT mapped onto voicing -- its real effect
  // (glottal-pulse irregularity) isn't a voicing *amount* change, so it's
  // left for a vocoder to read the modifier directly instead.
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(PhonemeModifier::MOD_CREAKY);
  CHECK(params.modifier == PhonemeModifier::MOD_CREAKY);
  CHECK_NEAR(params.voicing, 1.0f);
  CHECK_NEAR(params.volume, 1.0f);
  CHECK_NEAR(params.speed, 1.0f);
}

static void testParserFeedsDirectlyIntoParams() {
  // The intended end-to-end usage: parse a tagged phoneme string, then
  // hand the resulting modifier straight to a PhonemeSynthesisParams.
  PhonemeModifier mod = PhonemeModifier::MOD_NONE;
  std::string base = parsePhonemeModifiers("\"AA", mod);  // stressed
  PhonemeSynthesisParams params;
  params.setPhonemeModifier(mod);
  CHECK_EQ(base, std::string("AA"));
  CHECK_NEAR(params.volume, 1.15f);
  CHECK_NEAR(params.speed, 1.0f / 1.2f);
}

static void testExactMatchFoundDirectly() {
  // A plain, untagged phoneme that's actually in the table is found on the
  // first (exact-match) pass -- the modifier-fallback code inside
  // getSoundEntry() never even needs to run.
  SoundEntry* aa = ArpabetWAVDictionary.getSoundEntry("AA");
  CHECK(aa != nullptr);
}

static void testFallbackUsesBaseWhenTaggedRecordingMissing() {
  // "AA:" (long /ɑː/) has no dedicated recording in ARPABET_PHONEMES, but
  // getSoundEntry() should fall back to its base "AA", which does.
  SoundEntry* fallback = ArpabetWAVDictionary.getSoundEntry("AA:");
  SoundEntry* base = ArpabetWAVDictionary.getSoundEntry("AA");
  CHECK(fallback != nullptr);
  CHECK(fallback == base);
}

static void testFallbackFailsWhenBaseAlsoMissing() {
  // "AC_h" (aspirated near-open central vowel) -- neither the tagged form
  // nor its base "AC" is in this table (AC isn't used by any of the
  // bundled DE/FR/ES dictionaries/rules, unlike e.g. UF, which does have
  // a base recording now), so both lookups must miss.
  SoundEntry* result = ArpabetWAVDictionary.getSoundEntry("AC_h");
  CHECK(result == nullptr);
}

static void testFallbackDoesNotRetryWhenNoTagsPresent() {
  // A completely unknown, untagged phoneme -- parsePhonemeModifiers leaves
  // it unchanged, so there's no second key to try; this must still just
  // return nullptr (not loop or misbehave).
  SoundEntry* result = ArpabetWAVDictionary.getSoundEntry("ZZZ");
  CHECK(result == nullptr);
}

static void testDiphoneKeyNeverMistakenForModifierTag() {
  // "AA G" (a real diphone key, space-delimited -- see SoundEntry.h) must
  // resolve as an exact match and never get mangled by modifier-tag
  // stripping, even though "_G" (velarized) is a real suffix tag -- the
  // space delimiter is exactly what keeps these from colliding. Uses
  // DIPHONES directly since ArpabetWAVDictionary has no diphone entries.
  AudioDictionary diphones(DIPHONES, NUM_DIPHONES, 8000, PhonemeType::ARPAbet, 1, 16);
  SoundEntry* aaG = diphones.getSoundEntry("AA G");
  CHECK(aaG != nullptr);
  if (aaG) CHECK_EQ(std::string(aaG->name), std::string("AA G"));
}

static void testSegmentSizeIsCompact() {
  // The whole point of the single-value redesign: a Segment costs 2 bytes
  // (1 phonemeId + 1 modifier), not the 5 bytes a uint32_t bitmask
  // (rounded/padded) would have cost.
  CHECK_EQ(sizeof(Segment), static_cast<size_t>(2));
}

// ==================== FormantVocoder modifier wiring ====================
// These exercise preparePhonemeSynthesis()/applyModifierFormantShift() in
// FormantVocoder.h through the public sayPhoneme() string API -- the
// actual synthesis-level effect of a modifier tag, not just the parser or
// PhonemeSynthesisParams's derived numbers. No durationMs override is set
// in the duration tests below, since durationMs (when set) overrides the
// natural per-phoneme duration a modifier would otherwise stretch.

static void testModifierLongVowelExtendsDuration() {
  FormantVocoder synthPlain(16000);
  FormantVocoder synthLong(16000);
  CapturePrint outPlain, outLong;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "AA", outPlain);
  synthLong.sayPhoneme(PhonemeType::ARPAbet, "AA:", outLong);
  CHECK(!outPlain.samples.empty());
  CHECK(outLong.samples.size() > outPlain.samples.size());
}

static void testModifierHalfLongStretchesLessThanLong() {
  FormantVocoder synthHalf(16000);
  FormantVocoder synthLong(16000);
  CapturePrint outHalf, outLong;
  synthHalf.sayPhoneme(PhonemeType::ARPAbet, "AA:\\", outHalf);
  synthLong.sayPhoneme(PhonemeType::ARPAbet, "AA:", outLong);
  CHECK(outHalf.samples.size() < outLong.samples.size());
}

static void testModifierAspiratedExtendsDuration() {
  // MOD_ASPIRATED adds a fixed ~45ms VOT gap to the phoneme's own natural
  // duration (see preparePhonemeSynthesis()'s doc), not a speed multiplier.
  FormantVocoder synthPlain(16000);
  FormantVocoder synthAspirated(16000);
  CapturePrint outPlain, outAspirated;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "P", outPlain);
  synthAspirated.sayPhoneme(PhonemeType::ARPAbet, "P_h", outAspirated);
  CHECK(outAspirated.samples.size() > outPlain.samples.size());
}

static void testModifierSyllabicExtendsDuration() {
  FormantVocoder synthPlain(16000);
  FormantVocoder synthSyllabic(16000);
  CapturePrint outPlain, outSyllabic;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "L", outPlain);
  synthSyllabic.sayPhoneme(PhonemeType::ARPAbet, "L=", outSyllabic);
  CHECK(outSyllabic.samples.size() > outPlain.samples.size());
}

static void testModifierDevoicedIncreasesZeroCrossings() {
  // Mirrors testVoicingParam()'s own logic in test_formant_vocoder.cpp:
  // devoicing a normally-voiced phoneme replaces its periodic harmonic
  // source with broadband noise, which crosses zero far more often.
  FormantVocoder synthVoiced(16000);
  FormantVocoder synthDevoiced(16000);
  CapturePrint outVoiced, outDevoiced;
  PhonemeSynthesisParams p;
  p.durationMs = 150;
  synthVoiced.sayPhoneme(PhonemeType::ARPAbet, "Z", outVoiced, p);
  synthDevoiced.sayPhoneme(PhonemeType::ARPAbet, "Z_0", outDevoiced, p);
  size_t crossVoiced = countZeroCrossings(outVoiced.samples);
  size_t crossDevoiced = countZeroCrossings(outDevoiced.samples);
  CHECK(crossDevoiced > crossVoiced * 2);
}

static void testModifierVoicedDecreasesZeroCrossings() {
  // The opposite override: forcing a normally-unvoiced phoneme ("K") to
  // source from the voiced glottal pulse instead of noise should sharply
  // *reduce* its zero-crossing rate relative to its natural noise-sourced
  // form.
  FormantVocoder synthUnvoiced(16000);
  FormantVocoder synthForcedVoiced(16000);
  CapturePrint outUnvoiced, outForcedVoiced;
  PhonemeSynthesisParams p;
  p.durationMs = 150;
  synthUnvoiced.sayPhoneme(PhonemeType::ARPAbet, "K", outUnvoiced, p);
  synthForcedVoiced.sayPhoneme(PhonemeType::ARPAbet, "K_v", outForcedVoiced, p);
  size_t crossUnvoiced = countZeroCrossings(outUnvoiced.samples);
  size_t crossForcedVoiced = countZeroCrossings(outForcedVoiced.samples);
  CHECK(crossForcedVoiced * 2 < crossUnvoiced);
}

static void testModifierCreakyLowersPitch() {
  // MOD_CREAKY's one honest approximation (see PhonemeModifierBits' own
  // @note): lower F0. A lower fundamental means fewer zero crossings over
  // the same fixed duration.
  FormantVocoder synthPlain(16000);
  FormantVocoder synthCreaky(16000);
  CapturePrint outPlain, outCreaky;
  PhonemeSynthesisParams p;
  p.durationMs = 150;
  p.pitchHz = 150.0f;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "AA", outPlain, p);
  synthCreaky.sayPhoneme(PhonemeType::ARPAbet, "AA_k", outCreaky, p);
  size_t crossPlain = countZeroCrossings(outPlain.samples);
  size_t crossCreaky = countZeroCrossings(outCreaky.samples);
  CHECK(crossCreaky < crossPlain);
}

static void testModifierPalatalizedChangesWaveform() {
  // applyModifierFormantShift() shifts F2 up ~400 Hz for MOD_PALATALIZED --
  // formant content isn't directly inspectable through the public API, so
  // this checks the resulting waveform actually differs from the
  // unmodified phoneme's (same duration, since T is unvoiced and carries
  // no duration-affecting modifier here).
  FormantVocoder synthPlain(16000);
  FormantVocoder synthPalatalized(16000);
  CapturePrint outPlain, outPalatalized;
  PhonemeSynthesisParams p;
  p.durationMs = 100;
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "T", outPlain, p);
  synthPalatalized.sayPhoneme(PhonemeType::ARPAbet, "T_j", outPalatalized, p);
  CHECK_EQ(outPlain.samples.size(), outPalatalized.samples.size());
  bool anyDifference = false;
  for (size_t i = 0; i < outPlain.samples.size(); ++i) {
    if (outPlain.samples[i] != outPalatalized.samples[i]) {
      anyDifference = true;
      break;
    }
  }
  CHECK(anyDifference);
}

static void testModifierPitchContourRisingIncreasesFrequencyOverTime() {
  // f0StartRatio/MidRatio/EndRatio (PhonemeSynthesisParams) interpolate
  // across the phoneme's own duration in FormantVocoder::synthesizeSamples().
  // A rising contour should mean MORE zero crossings in the second half of
  // the output than the first -- higher instantaneous F0 there.
  FormantVocoder synth(16000);
  CapturePrint out;
  PhonemeSynthesisParams p;
  p.durationMs = 200;
  p.pitchHz = 150.0f;
  p.f0StartRatio = 0.7f;
  p.f0MidRatio = 1.0f;
  p.f0EndRatio = 1.4f;
  synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out, p);
  CHECK(!out.samples.empty());

  size_t half = out.samples.size() / 2;
  std::vector<int16_t> firstHalf(out.samples.begin(), out.samples.begin() + half);
  std::vector<int16_t> secondHalf(out.samples.begin() + half, out.samples.end());
  size_t crossFirst = countZeroCrossings(firstHalf);
  size_t crossSecond = countZeroCrossings(secondHalf);
  CHECK(crossSecond > crossFirst);
}

static void testToneRisingIncreasesFrequencyOverTime() {
  // TONE_RISING (formerly ToneAccent.h's ToneShape::RISING, now a
  // PhonemeModifier) should produce the same directional effect as
  // manually setting a rising f0StartRatio/EndRatio -- more zero
  // crossings in the second half than the first.
  FormantVocoder synth(16000);
  CapturePrint out;
  PhonemeSynthesisParams p;
  p.durationMs = 200;
  p.pitchHz = 150.0f;
  p.setPhonemeModifier(PhonemeModifier::TONE_RISING);
  synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out, p);
  CHECK(!out.samples.empty());

  size_t half = out.samples.size() / 2;
  std::vector<int16_t> firstHalf(out.samples.begin(), out.samples.begin() + half);
  std::vector<int16_t> secondHalf(out.samples.begin() + half, out.samples.end());
  CHECK(countZeroCrossings(secondHalf) > countZeroCrossings(firstHalf));
}

static void testToneFallingDecreasesFrequencyOverTime() {
  FormantVocoder synth(16000);
  CapturePrint out;
  PhonemeSynthesisParams p;
  p.durationMs = 200;
  p.pitchHz = 150.0f;
  p.setPhonemeModifier(PhonemeModifier::TONE_FALLING);
  synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out, p);
  CHECK(!out.samples.empty());

  size_t half = out.samples.size() / 2;
  std::vector<int16_t> firstHalf(out.samples.begin(), out.samples.begin() + half);
  std::vector<int16_t> secondHalf(out.samples.begin() + half, out.samples.end());
  CHECK(countZeroCrossings(firstHalf) > countZeroCrossings(secondHalf));
}

static void testToneLevelHighHasHigherFrequencyThanLevelLow() {
  FormantVocoder synthHigh(16000);
  FormantVocoder synthLow(16000);
  CapturePrint outHigh, outLow;
  PhonemeSynthesisParams pHigh;
  pHigh.durationMs = 150;
  pHigh.pitchHz = 150.0f;
  pHigh.setPhonemeModifier(PhonemeModifier::TONE_LEVEL_HIGH);
  PhonemeSynthesisParams pLow = pHigh;
  pLow.setPhonemeModifier(PhonemeModifier::TONE_LEVEL_LOW);
  synthHigh.sayPhoneme(PhonemeType::ARPAbet, "AA", outHigh, pHigh);
  synthLow.sayPhoneme(PhonemeType::ARPAbet, "AA", outLow, pLow);
  CHECK(countZeroCrossings(outHigh.samples) > countZeroCrossings(outLow.samples));
}

static void testToneCheckedShortensDuration() {
  // TONE_CHECKED shortens the syllable (speedMul *= 1.3, see
  // deriveModifierEffect()) -- verify with no explicit durationMs so the
  // natural per-phoneme duration is what gets scaled.
  FormantVocoder synthPlain(16000);
  FormantVocoder synthChecked(16000);
  CapturePrint outPlain, outChecked;
  PhonemeSynthesisParams pChecked;
  pChecked.setPhonemeModifier(PhonemeModifier::TONE_CHECKED);
  synthPlain.sayPhoneme(PhonemeType::ARPAbet, "AA", outPlain);
  synthChecked.sayPhoneme(PhonemeType::ARPAbet, "AA", outChecked, pChecked);
  CHECK(outChecked.samples.size() < outPlain.samples.size());
}

static void testModifierDefaultContourStillProducesValidVoicedOutput() {
  // Leaving f0StartRatio/MidRatio/EndRatio at their PhonemeSynthesisParams
  // defaults (all 1.0 -- a flat, no-op contour) must not break ordinary
  // synthesis: same duration as always, real voiced output, comparable
  // zero-crossing density to the fixed pitchHz requested. This can't
  // compare exact zero-crossing counts against a second instance (per-
  // instance jitter/shimmer make exact counts vary between instances by
  // design -- see testInstancesAreIndependent in test_formant_vocoder.cpp)
  // -- it only checks the contour math introduces no gross regression.
  FormantVocoder synth(16000);
  CapturePrint out;
  PhonemeSynthesisParams p;
  p.durationMs = 150;
  p.pitchHz = 150.0f;
  synth.sayPhoneme(PhonemeType::ARPAbet, "AA", out, p);
  CHECK_EQ(out.samples.size(), static_cast<size_t>(0.15f * 16000));
  CHECK(countZeroCrossings(out.samples) > 0);
}

int main() {
  testNoModifierPassesThrough();
  testSingleSuffixTag();
  testLongVowelSuffix();
  testHalfLongDoesNotMatchLongPrefix();
  testChainedSuffixesKeepsLastOneFound();
  testPrimaryStressPrefix();
  testSecondaryStressPrefix();
  testPrefixAndSuffixCombinedKeepsLastOneFound();
  testRhotacizedBacktickNotConfusedWithRetroflexSymbol();
  testSyllabicConsonant();
  testDoesNotOverStripBareShortSymbol();
  testSetPhonemeModifierNoneLeavesDefaults();
  testSetPhonemeModifierIsIdempotentNotAccumulating();
  testStressPrimaryBoostsVolumeAndSpeed();
  testStressSecondaryBoostsLessThanPrimary();
  testLongVowelSlowsSpeedOnly();
  testHalfLongSlowsLessThanLong();
  testVoicedForcesFullVoicing();
  testCreakyDoesNotAffectVoicing();
  testParserFeedsDirectlyIntoParams();
  testExactMatchFoundDirectly();
  testFallbackUsesBaseWhenTaggedRecordingMissing();
  testFallbackFailsWhenBaseAlsoMissing();
  testFallbackDoesNotRetryWhenNoTagsPresent();
  testDiphoneKeyNeverMistakenForModifierTag();
  testSegmentSizeIsCompact();
  testModifierLongVowelExtendsDuration();
  testModifierHalfLongStretchesLessThanLong();
  testModifierAspiratedExtendsDuration();
  testModifierSyllabicExtendsDuration();
  testModifierDevoicedIncreasesZeroCrossings();
  testModifierVoicedDecreasesZeroCrossings();
  testModifierCreakyLowersPitch();
  testModifierPalatalizedChangesWaveform();
  testModifierPitchContourRisingIncreasesFrequencyOverTime();
  testToneRisingIncreasesFrequencyOverTime();
  testToneFallingDecreasesFrequencyOverTime();
  testToneLevelHighHasHigherFrequencyThanLevelLow();
  testToneCheckedShortensDuration();
  testModifierDefaultContourStillProducesValidVoicedOutput();
  return testSummary();
}
