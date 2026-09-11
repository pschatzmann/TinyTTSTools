// Regression test for a critical bug found in SoundEntry/the diphone data
// pipeline (2025-09): SoundEntry had no ADPCM decode path at all -- only
// bits==8 (PCM8) and bits==16 (PCM16) were handled. Every diphone
// (DiphoneWAVDictionary.h, all 877 entries) was constructed without a
// `bits` argument, defaulting to 16, so the raw 4-bit IMA-ADPCM
// *compressed* bytes were reinterpreted directly as linear 16-bit PCM --
// i.e. every diphone was pure noise at runtime, despite compiling fine and
// passing every structural test (lookup, sizes, traversal) that existed
// before this file. Fixed by adding a real IMA-ADPCM decoder
// (AudioFormatDecoder.h) and passing bits=4 (the real bitsPerSample an
// IMA-ADPCM WAV declares) when constructing diphone SoundEntry values.
//
// The expected values below were verified bit-exact (0 mismatches across
// all 877 diphones, ~987k samples) against `sox`'s own IMA-ADPCM decoder
// as ground truth -- see setup/audio/diphones/README.md. They're embedded
// here so this test doesn't need sox installed to catch a regression.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "TestUtils.h"
#include "TinyTTSTools/SoundDictionary/AudioFormatDecoder.h"
#include "TinyTTSTools/SoundDictionary/DiphoneWAVDictionary.h"

static void testPcm8Decoder() {
  const uint8_t data[] = {0, 128, 255, 64};
  Pcm8Decoder d;
  CHECK_EQ(d.sampleCount(data, 4), static_cast<size_t>(4));
  CHECK_EQ(d.sampleAt(data, 4, 0), static_cast<int16_t>(-32768));  // 0 -> min
  CHECK_EQ(d.sampleAt(data, 4, 1), static_cast<int16_t>(0));       // 128 -> mid (silence)
  CHECK_EQ(d.sampleAt(data, 4, 2), static_cast<int16_t>(32512));   // 255 -> near max
}

static void testPcm16Decoder() {
  const uint8_t data[] = {0x34, 0x12, 0xCD, 0xAB};  // LE: 0x1234, 0xABCD
  Pcm16Decoder d;
  CHECK_EQ(d.sampleCount(data, 4), static_cast<size_t>(2));
  CHECK_EQ(d.sampleAt(data, 4, 0), static_cast<int16_t>(0x1234));
  CHECK_EQ(d.sampleAt(data, 4, 1), static_cast<int16_t>(0xABCD));  // -21555
}

static void testAdpcmDecoderAgainstSoxReference() {
  const SoundEntry* e = nullptr;
  for (size_t i = 0; i < NUM_DIPHONES; i++) {
    if (strcmp(DIPHONES[i].name, "AA B") == 0) {
      e = &DIPHONES[i];
      break;
    }
  }
  CHECK(e != nullptr);
  if (!e) return;
  CHECK_EQ(e->bits, static_cast<uint8_t>(4));  // must be tagged ADPCM, not silently default to 16
  CHECK_EQ(e->samples(), static_cast<size_t>(1010));  // 2 full 256-byte blocks, 505 samples each

  CHECK_EQ((*e)[0], static_cast<int16_t>(0));
  CHECK_EQ((*e)[1], static_cast<int16_t>(0));
  CHECK_EQ((*e)[2], static_cast<int16_t>(7));
  CHECK_EQ((*e)[9], static_cast<int16_t>(-1));
  CHECK_EQ((*e)[500], static_cast<int16_t>(-14291));
  CHECK_EQ((*e)[504], static_cast<int16_t>(-5017));  // last sample of block 0
  CHECK_EQ((*e)[505], static_cast<int16_t>(-4528));  // first sample of block 1 (its own predictor)
  CHECK_EQ((*e)[506], static_cast<int16_t>(-6852));
  CHECK_EQ((*e)[507], static_cast<int16_t>(-7788));
  CHECK_EQ((*e)[1009], static_cast<int16_t>(1));  // last sample overall

  // Values should never look like the pre-fix "reinterpreted noise"
  // pattern (immediately swinging to near-full-scale in the first few
  // samples of a diphone that should start near silence).
  CHECK(std::abs(static_cast<int>((*e)[0])) < 1000);
  CHECK(std::abs(static_cast<int>((*e)[1])) < 1000);
}

// Regression test for a second instance of the same "wrong bits value" bug
// class: this project previously shipped a whole separate generated class
// (ArpabetAltWAVDictionary, removed 2025-09) that reused the real
// IMA-ADPCM {P}_SIL diphone data as standalone phoneme sounds, but was
// hand-written with bits=8 (PCM8) instead of bits=4, so it decoded the
// same compressed bytes wrong in a different way. That class was a public,
// AudioDictionary-shaped, Doxygen-visible fixture that looked usable but
// wasn't (each entry is only ~half a phoneme's real duration, by
// diphone-generation design) -- purely a regression-test vehicle wearing
// production clothes. This inlined version keeps the exact same coverage
// (a manually bits=4-tagged SoundEntry over the same underlying bytes,
// checked against DIPHONES["AA SIL"]) without shipping a public class
// whose only real purpose was to be wrong on purpose in a controlled way.
static void testManualBitsFourMatchesDiphoneSource() {
  SoundEntry manual("AA", sound_data_AA_SIL_size, sound_data_AA_SIL, 4);
  CHECK_EQ(manual.bits, static_cast<uint8_t>(4));  // must be ADPCM, not PCM8

  const SoundEntry* src = nullptr;
  for (size_t i = 0; i < NUM_DIPHONES; i++) {
    if (strcmp(DIPHONES[i].name, "AA SIL") == 0) {
      src = &DIPHONES[i];
      break;
    }
  }
  CHECK(src != nullptr);
  if (!src) return;

  CHECK_EQ(manual.samples(), src->samples());
  size_t n = manual.samples();
  bool allMatch = true;
  for (size_t i = 0; i < n; i++) {
    if (manual[i] != (*src)[i]) {
      allMatch = false;
      break;
    }
  }
  CHECK(allMatch);
}

int main() {
  testPcm8Decoder();
  testPcm16Decoder();
  testAdpcmDecoderAgainstSoxReference();
  testManualBitsFourMatchesDiphoneSource();
  return testSummary();
}
