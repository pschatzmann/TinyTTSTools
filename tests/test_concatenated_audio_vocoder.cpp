// Regression test for two bugs found in ConcatenatedAudioVocoder.h's
// cross-fade path (2025-09):
//   1. applyPhaseAlignment()'s disabled/degenerate early-return always
//      returned totalSamples, which is correct for an END cutoff but wrong
//      for a START offset (should be 0) -- caused a unit following a
//      cross-faded predecessor to have its entire body skipped whenever
//      phase alignment was disabled.
//   2. processUnitCombination() read lastUnitTail_.size() to compute how
//      many samples the cross-fade had consumed, but
//      processCrossFadeWithPrevious() already clears lastUnitTail_ before
//      returning -- so that computation always evaluated to 0, and the
//      main-body loop re-emitted the just-cross-faded samples a second
//      time (a duplicated segment at every cross-faded transition).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "TestUtils.h"
#include "TinyTTSTools/Vocoder/ConcatenatedAudioVocoder.h"

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

// Two 40-sample synthetic units with distinguishable values (1000+i and
// 2000+i) so duplication/loss/mixing is unambiguous in the output.
static SoundEntry makeEntry(const char* name, std::vector<int16_t>& samplesVec,
                            std::vector<uint8_t>& raw, int base, int n = 40) {
  samplesVec.clear();
  for (int i = 0; i < n; i++) samplesVec.push_back(static_cast<int16_t>(base + i));
  raw.resize(samplesVec.size() * 2);
  memcpy(raw.data(), samplesVec.data(), raw.size());
  return SoundEntry(name, static_cast<uint16_t>(raw.size()), raw.data(), 16);
}

class TestVocoder : public ConcatenatedAudioVocoder {
 public:
  SoundEntry a, b;
  TestVocoder(SoundEntry a_, SoundEntry b_) : a(a_), b(b_) {
    setCrossFade(true);
    setCrossFadeDuration(1.0f);  // 1ms @ 8kHz = 8 samples
  }
  std::string getType() const override { return "Test"; }
  int sampleRate() const override { return 8000; }
  const SoundEntry* getAudioEntry(const std::string& id) override {
    if (id == "AA") return &a;
    if (id == "IY") return &b;
    return nullptr;
  }
  const SoundEntry* getNextAudioEntry(const std::string&, const std::string& nextId) override {
    return getAudioEntry(nextId);
  }
};

static void runCase() {
  std::vector<int16_t> unitA, unitB;
  std::vector<uint8_t> unitAraw, unitBraw;
  SoundEntry a = makeEntry("AA", unitA, unitAraw, 1000);
  SoundEntry b = makeEntry("IY", unitB, unitBraw, 2000);
  TestVocoder v(a, b);
  // Phase alignment is now always on (no toggle) -- this synthetic
  // amplitude-ramp data never triggers its zero-crossing search anyway.

  CapturePrint out;
  v.sayPhoneme(PhonemeType::ARPAbet, "AA IY", out);

  // Expected length is exact for this synthetic data: 40 + 40 - 8 (crossfade
  // overlap) = 72 -- concatenation length minus the overlap that gets merged
  // rather than appended.
  size_t expectedLen = unitA.size() + unitB.size() - 8;
  CHECK_EQ(out.samples.size(), expectedLen);
  if (out.samples.size() != expectedLen) return;  // avoid OOB below

  // First 32 samples: unaltered unit A body.
  for (int i = 0; i < 32; i++) CHECK_EQ(out.samples[i], static_cast<int16_t>(1000 + i));

  // Samples 32-39: cross-fade blend, monotonically moving from unit A's
  // tail toward unit B's head (first blended sample must equal unit A's
  // tail exactly -- fadeRatio 0 -- and must never equal a raw, unblended
  // value from either unit except at that one boundary).
  CHECK_EQ(out.samples[32], static_cast<int16_t>(1032));

  // Samples 40-71: unit B's body, resuming from its own index 8 (not
  // index 0 -- that's the bug-2 regression: a fixed build must NOT repeat
  // samples 2000..2007).
  for (int i = 0; i < 32; i++) CHECK_EQ(out.samples[40 + i], static_cast<int16_t>(2008 + i));
}

int main() {
  runCase();
  return testSummary();
}
