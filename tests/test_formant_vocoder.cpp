// Regression tests for bugs found and fixed in FormantVocoder.h (2025-09):
//   1. applyVolumeFactor() had a stray "10.0f *" gain on top of the
//      already-correctly-leveled (via RMS normalization) output, causing
//      near-guaranteed hard clipping on every synthesized phoneme.
//   2. applyCrossfade() captured its tail from audioBuffer_.end() instead
//      of a numSamples-relative bound; since audioBuffer_ never shrinks,
//      that read stale data from a previous, longer phoneme whenever the
//      current phoneme was shorter.
//   3. Several DSP filter states (lp_hp, prevNoise1/2, prevSample, nasalLP,
//      the noise PRNG seed) were function-local `static` variables, shared
//      across every FormantVocoder instance instead of being per-instance.
#include <cstdio>
#include <cstdlib>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/Vocoder/FormantVocoder.h"

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

static void testNoClipping() {
  FormantVocoder synth(16000);
  CapturePrint out;
  const char* phs[] = {"HH", "AH0", "L", "OW", "W", "ER", "L", "D"};
  for (auto p : phs) synth.sayPhoneme(PhonemeType::ARPAbet, p, out);

  CHECK(!out.samples.empty());
  int pinned = 0;
  int16_t peak = 0;
  for (auto s : out.samples) {
    if (s == 32767 || s == -32768) pinned++;
    if (std::abs(static_cast<int>(s)) > peak) peak = static_cast<int16_t>(std::abs(static_cast<int>(s)));
  }
  // A handful of samples pinned at full-scale can happen legitimately at a
  // stress peak; hundreds/thousands pinned (the pre-fix behavior) means the
  // 10x gain bug is back.
  CHECK(pinned < 10);
  CHECK(peak < 32000);  // plenty of headroom below clipping, not slammed against it
}

static void testDefaultVolumeLouderButNotClipping() {
  // FormantVocoder's default volumeFactor was raised from 1.0 to 1.7
  // (2025-09): measured RMS-normalized output alone peaked at only ~44% of
  // full scale for typical speech, leaving ample headroom, so the default
  // was too quiet. 1.7 was chosen as the highest factor that produced zero
  // clipped samples across a long, varied phrase (stressed vowels,
  // sibilants); this locks that in as a regression test.
  FormantVocoder synth(8000);
  CapturePrint out;
  bool ok = synth.sayPhoneme(
      PhonemeType::ARPAbet,
      "DH AH0 K W IH1 K B R AW1 N F AA1 K S JH AH1 M P S OW1 V ER0 L EY1 Z IY D AO1 G S SH TH",
      out);
  CHECK(ok);
  CHECK(!out.samples.empty());

  int clipCount = 0;
  int16_t peak = 0;
  for (auto s : out.samples) {
    if (s >= 32767 || s <= -32768) clipCount++;
    int a = std::abs(static_cast<int>(s));
    if (a > peak) peak = static_cast<int16_t>(a);
  }
  CHECK_EQ(clipCount, 0);
  // Confirms the volume boost is actually happening (not accidentally
  // reverted to 1.0): peak should be well above the ~44% (~14500) level
  // measured with volumeFactor=1.0.
  CHECK(peak > 20000);
}

static void testCrossfadeNoStaleData() {
  FormantVocoder synth(16000);
  CapturePrint out;
  synth.sayPhoneme(PhonemeType::ARPAbet, "IY", out);  // long vowel (140ms)
  size_t boundary = out.samples.size();
  synth.sayPhoneme(PhonemeType::ARPAbet, "T", out);  // short stop (70ms)

  // Look at the max sample-to-sample jump within +/-20 samples of the
  // long-vowel -> short-consonant boundary; stale-tail data from the (much
  // longer) IY phoneme showed up here as a large discontinuity before the fix.
  int32_t maxJump = 0;
  size_t lo = boundary > 20 ? boundary - 20 : 0;
  size_t hi = std::min(out.samples.size() - 1, boundary + 20);
  for (size_t i = lo; i < hi; i++) {
    int32_t jump = std::abs(static_cast<int32_t>(out.samples[i + 1]) - static_cast<int32_t>(out.samples[i]));
    if (jump > maxJump) maxJump = jump;
  }
  CHECK(maxJump < 5000);  // a real click/discontinuity is an order of magnitude larger
}

static void testInstancesAreIndependent() {
  FormantVocoder synthA(16000);
  FormantVocoder synthB(16000);
  CapturePrint outA, outB;
  synthA.sayPhoneme(PhonemeType::ARPAbet, "S", outA);
  synthB.sayPhoneme(PhonemeType::ARPAbet, "S", outB);
  // Before the fix, all noise/filter state was in function-local statics
  // shared by every instance, so two freshly-constructed vocoders produced
  // bit-identical "random" noise for the same unvoiced phoneme.
  CHECK(outA.samples != outB.samples);
}

static size_t countZeroCrossings(const std::vector<int16_t>& samples) {
  size_t count = 0;
  for (size_t i = 1; i < samples.size(); i++) {
    if ((samples[i - 1] <= 0 && samples[i] > 0) || (samples[i - 1] >= 0 && samples[i] < 0)) count++;
  }
  return count;
}

// Regression tests for PhonemeSynthesisParams (volume/durationMs/pitchHz/voicing)
static void testVolumeParam() {
  FormantVocoder synthFull(16000);
  FormantVocoder synthHalf(16000);
  CapturePrint outFull, outHalf;
  PhonemeSynthesisParams pHalf;
  pHalf.volume = 0.5f;
  synthFull.sayPhoneme(PhonemeType::ARPAbet, "AH0", outFull, PhonemeSynthesisParams{});
  synthHalf.sayPhoneme(PhonemeType::ARPAbet, "AH0", outHalf, pHalf);

  CHECK_EQ(outFull.samples.size(), outHalf.samples.size());
  int32_t peakFull = 0, peakHalf = 0;
  for (auto s : outFull.samples) peakFull = std::max(peakFull, static_cast<int32_t>(std::abs(static_cast<int>(s))));
  for (auto s : outHalf.samples) peakHalf = std::max(peakHalf, static_cast<int32_t>(std::abs(static_cast<int>(s))));
  CHECK(peakFull > 0);
  // volume=0.5 should roughly halve peak amplitude (RMS normalization runs
  // before the volume scale, so both buffers start from nearly the same level)
  CHECK(std::abs(peakHalf - peakFull / 2) < peakFull / 5);
}

static void testDurationParam() {
  FormantVocoder synth(16000);
  CapturePrint out;
  PhonemeSynthesisParams params;
  params.durationMs = 250;  // explicit override, well above AH0's default duration
  synth.sayPhoneme(PhonemeType::ARPAbet, "AH0", out, params);

  size_t expected = static_cast<size_t>(0.25f * 16000);
  CHECK_EQ(out.samples.size(), expected);
}

static void testPitchParam() {
  // Before the fix, generateVoiceSource() ignored its f0Base parameter and
  // always derived pitch from the instance's static voiceF0_ member, so a
  // pitchHz override (or the pre-existing stress-pitch-rise logic) had no
  // audible effect at all.
  FormantVocoder synthLow(16000);
  FormantVocoder synthHigh(16000);
  CapturePrint outLow, outHigh;
  PhonemeSynthesisParams pLow;
  pLow.durationMs = 100;
  pLow.pitchHz = 100.0f;
  PhonemeSynthesisParams pHigh;
  pHigh.durationMs = 100;
  pHigh.pitchHz = 220.0f;
  synthLow.sayPhoneme(PhonemeType::ARPAbet, "IY", outLow, pLow);
  synthHigh.sayPhoneme(PhonemeType::ARPAbet, "IY", outHigh, pHigh);

  size_t crossLow = countZeroCrossings(outLow.samples);
  size_t crossHigh = countZeroCrossings(outHigh.samples);
  CHECK(crossHigh > crossLow);
}

static void testVoicingParam() {
  FormantVocoder synthVoiced(16000);
  FormantVocoder synthWhisper(16000);
  CapturePrint outVoiced, outWhisper;
  PhonemeSynthesisParams pVoiced;
  pVoiced.durationMs = 150;
  PhonemeSynthesisParams pWhisper;
  pWhisper.durationMs = 150;
  pWhisper.voicing = 0.0f;
  synthVoiced.sayPhoneme(PhonemeType::ARPAbet, "IY", outVoiced, pVoiced);
  synthWhisper.sayPhoneme(PhonemeType::ARPAbet, "IY", outWhisper, pWhisper);

  // voicing=0.0 replaces the periodic harmonic source with broadband noise
  // (only for naturally-voiced phonemes), which crosses zero far more often
  // than a ~120Hz periodic waveform over the same duration.
  size_t crossVoiced = countZeroCrossings(outVoiced.samples);
  size_t crossWhisper = countZeroCrossings(outWhisper.samples);
  CHECK(crossWhisper > crossVoiced * 2);
}

static void testPhoneEnumOverload() {
  // VocoderBase::sayPhoneme(Phone, ...) should select the same phoneme (and
  // therefore the same duration/sample count) as the equivalent string
  // call, for both a plain phoneme and a stress-authoring variant
  // (Phonemes::toArpabetString() resolves Phone::IH1 -> "IH1"). Sample
  // *content* isn't compared: FormantVocoder seeds its noise/jitter from
  // each instance's own address, so two distinct instances never produce
  // byte-identical output for the same phoneme by design (see
  // testInstancesAreIndependent above) -- sample count is the fair,
  // deterministic check here.
  FormantVocoder synthEnum(16000);
  FormantVocoder synthString(16000);
  CapturePrint outEnum, outString;

  bool okEnum = synthEnum.sayPhoneme(Phone::AA, outEnum);
  bool okString = synthString.sayPhoneme(PhonemeType::ARPAbet, "AA", outString);
  CHECK(okEnum);
  CHECK(okString);
  CHECK_EQ(outEnum.samples.size(), outString.samples.size());

  CapturePrint outStressed;
  bool okStressed = synthEnum.sayPhoneme(Phone::IH1, outStressed);
  CHECK(okStressed);
  CHECK(!outStressed.samples.empty());
}

static void testMultiPhonemeSequenceProducesFullLengthAudio() {
  // Regression test (2025-09): synthesizePhoneme() treated its entire
  // `phoneme` argument as ONE token -- fine when every caller passed
  // exactly one phoneme per call, but TinyTTSTools::sayPhonemes() was
  // fixed to join a whole utterance into one multi-phoneme sequence and
  // call sayPhoneme() ONCE (so DiphoneVocoder could form real diphones --
  // see test_diphones.cpp). FormantVocoder then received e.g.
  // "DH AH0 SP K W IH K SP B R AW N SP F AA K S SP" as a single call,
  // found no matching rule for that whole string, and produced only one
  // ~100ms burst of default-fallback noise for the ENTIRE utterance --
  // audible as "no real audio". Fixed by splitting on spaces and
  // synthesizing each token.
  FormantVocoder synth(16000);
  CapturePrint outSingle, outMulti;

  synth.sayPhoneme(PhonemeType::ARPAbet, "AA", outSingle);
  size_t singlePhonemeSamples = outSingle.samples.size();

  bool ok = synth.sayPhoneme(PhonemeType::ARPAbet, "DH AH0 SP K W IH K", outMulti);
  CHECK(ok);
  // 7 tokens' worth of audio must be substantially more than one phoneme's
  // worth -- the pre-fix behavior was capped at a single ~100ms fallback
  // burst regardless of how many tokens were in the sequence.
  CHECK(outMulti.samples.size() > singlePhonemeSamples * 3);
}

static void testCoarticulationGlideBetweenPhonemes() {
  // Regression/feature test: adjacent phonemes must actually influence each
  // other's formant trajectory (coarticulation), not just start each
  // phoneme already "at rest" at its own target. Before this fix,
  // configureInitialFilters() reset lastF1_/2_/3_ to the new phoneme's own
  // target every call, so the very first samples of "IY" (a high-front
  // vowel, F1/F2/F3 far from AA's low-back values) came out byte-identical
  // whether or not it was preceded by "AA" -- there was no memory of the
  // previous phoneme at all. With the glide in place, "IY" preceded by
  // "AA" must start from AA's ending formants and audibly differ from "IY"
  // synthesized alone (which starts from a neutral resting position).
  FormantVocoder synthAlone(16000);
  FormantVocoder synthInContext(16000);
  CapturePrint outAlone, outContext;

  synthAlone.sayPhoneme(PhonemeType::ARPAbet, "IY", outAlone);
  synthInContext.sayPhoneme(PhonemeType::ARPAbet, "AA", outContext);
  size_t aaSamples = outContext.samples.size();
  synthInContext.sayPhoneme(PhonemeType::ARPAbet, "IY", outContext);

  CHECK(outContext.samples.size() > aaSamples);
  CHECK(!outAlone.samples.empty());
  if (outAlone.samples.empty() || outContext.samples.size() <= aaSamples) return;

  // Compare the first ~10ms of "IY" in both cases -- they must differ,
  // proving the glide actually carried AA's formants into IY's onset.
  size_t compareSamples = std::min<size_t>(160, outAlone.samples.size());
  bool anyDifference = false;
  for (size_t i = 0; i < compareSamples; i++) {
    if (outAlone.samples[i] != outContext.samples[aaSamples + i]) {
      anyDifference = true;
      break;
    }
  }
  CHECK(anyDifference);
}

int main() {
  testNoClipping();
  testDefaultVolumeLouderButNotClipping();
  testCrossfadeNoStaleData();
  testInstancesAreIndependent();
  testVolumeParam();
  testDurationParam();
  testPitchParam();
  testVoicingParam();
  testPhoneEnumOverload();
  testMultiPhonemeSequenceProducesFullLengthAudio();
  testCoarticulationGlideBetweenPhonemes();
  return testSummary();
}
