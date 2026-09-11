// Regression tests for the diphone data pipeline and DiphoneVocoder.h fixes
// (2025-09):
//   1. DiphoneWAVDictionary/DiphoneVocoder: processSequenceWithLookahead()
//      walked phonemes with a step of 2 (disjoint pairs), silently
//      skipping every other real transition between phonemes. Fixed to
//      slide by 1, generating N-1 diphones for N phonemes.
//   2. generate_relevant_diphones.sh generated full-phone1 + full-phone2
//      diphone audio (240ms) instead of roughly half+half (~120ms), which
//      would have made every interior phoneme's steady state play twice
//      once the traversal fix (1) landed. Regenerated using each
//      phoneme's own natural duration (Phonemes.h), halved.
//   3. generate_relevant_diphones.sh's cluster expansion used
//      `read -r c1 c2 <<< "$cluster"`, which silently mis-parsed any
//      3-phoneme cluster (e.g. "S K R"): read absorbs every leftover
//      token into the last variable, so MBROLA got called with a
//      two-word "phoneme" and failed outright (4 diphones: S_K, K_R /
//      S_P, P_L / S_P, P_R / S_T, T_R never got generated). Fixed to
//      expand every adjacent pair within a cluster of any length.
//   4. TinyTTSTools::sayPhonemes() called sayPhoneme() once PER INDIVIDUAL
//      PHONEME TOKEN, never passing a multi-phoneme sequence to the
//      vocoder at all. DiphoneVocoder can't form a diphone from a single
//      phoneme, so every real utterance was fragmented into isolated
//      one-phoneme "words", each wrapped in its own SIL_X/X_SIL pair --
//      never a real coarticulated transition anywhere. Fixed by joining
//      the whole utterance into one sequence and calling sayPhoneme() once
//      (TinyTTSTools.h); DiphoneVocoder::processSequenceWithLookahead now
//      splits that sequence into silence-free segments (typically one
//      word each, since TinyTTSTools::toPhonemes() inserts "SP" between
//      words) and synthesizes each as its own SIL-bracketed diphone chain.
#include <cstdio>
#include <string>
#include <vector>
#include "TestUtils.h"
#include "TinyTTSTools/Vocoder/DiphoneVocoder.h"
#include "TinyTTSTools/SoundDictionary/DiphoneWAVDictionary.h"

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

// Wraps DiphoneWAVDictionary but also records every diphone name actually
// looked up via getSoundEntry(), so the test can inspect exactly which
// transitions the vocoder asked for.
class RecordingDictionary : public AudioDictionary {
 public:
  RecordingDictionary()
      : AudioDictionary(DIPHONES, NUM_DIPHONES, 8000, PhonemeType::ARPAbet, 1, 16) {}
  std::vector<std::string> requested;
  SoundEntry* getSoundEntry(const char* phoneme) override {
    requested.push_back(phoneme);
    return AudioDictionary::getSoundEntry(phoneme);
  }
};

static void testDataCompleteness() {
  // 877 original curated diphones + 2 (K_S, P_S, closing a corpus gap found
  // via a sample phrase) + 721 from generating the full consonant-consonant
  // (24x24) and vowel-vowel (15x15) cross product (2025-09): an audit
  // against the full 123k-word CMU dictionary found the curated cluster
  // list left 4.7% of all diphone instances (480 distinct names, 31% of
  // words) missing -- generating the full cross product closes it to 0%.
  CHECK_EQ(NUM_DIPHONES, static_cast<size_t>(1600));
  // The 4 clusters that used to fail outright (bug 3).
  AudioDictionary dict(DIPHONES, NUM_DIPHONES, 8000, PhonemeType::ARPAbet, 1, 16);
  CHECK(dict.getSoundEntry("S K") != nullptr);
  CHECK(dict.getSoundEntry("K R") != nullptr);
  CHECK(dict.getSoundEntry("S P") != nullptr);
  CHECK(dict.getSoundEntry("P L") != nullptr);
  CHECK(dict.getSoundEntry("P R") != nullptr);
  CHECK(dict.getSoundEntry("S T") != nullptr);
  CHECK(dict.getSoundEntry("T R") != nullptr);

  // Diphones are now roughly half+half (~120ms), not full+full (~240ms) --
  // AA_B previously had ~4400 16-bit samples at 22050Hz original quality;
  // it should now be well under half that. Check via the ADPCM entry's
  // byte size instead (compact and format-independent): a full+full
  // diphone at 8kHz/4-bit was ~960+ data bytes; half+half should be
  // roughly half of that.
  SoundEntry* aaB = dict.getSoundEntry("AA B");
  CHECK(aaB != nullptr);
  if (aaB) CHECK(aaB->size < 700);  // generous margin above the ~500-byte expectation

  // Spot-check a few of the 480 diphones a full-CMU-dictionary audit found
  // missing before the consonant-consonant/vowel-vowel completeness pass
  // (2025-09): K_S/P_S (from "fox"/"jumps"), plus the highest-frequency
  // consonant-consonant and vowel-vowel gaps found by the audit.
  CHECK(dict.getSoundEntry("K S") != nullptr);
  CHECK(dict.getSoundEntry("P S") != nullptr);
  CHECK(dict.getSoundEntry("M B") != nullptr);
  CHECK(dict.getSoundEntry("R K") != nullptr);
  CHECK(dict.getSoundEntry("ER IH") != nullptr);
  CHECK(dict.getSoundEntry("IY OW") != nullptr);
}

static void testTraversalCoversEveryTransition() {
  RecordingDictionary dict;
  DiphoneVocoder v(dict);
  CapturePrint out;

  // "HH EH L OW" (~"hello"): 4 phonemes, 3 real transitions. The old
  // step-2 traversal only ever requested HH_EH and L_OW, never EH_L.
  dict.requested.clear();
  v.sayPhoneme(PhonemeType::ARPAbet, "HH EH L OW", out);

  bool sawHH_EH = false, sawEH_L = false, sawL_OW = false;
  for (auto& r : dict.requested) {
    if (r == "HH EH") sawHH_EH = true;
    if (r == "EH L") sawEH_L = true;
    if (r == "L OW") sawL_OW = true;
  }
  CHECK(sawHH_EH);
  CHECK(sawEH_L);  // the transition the bug used to skip entirely
  CHECK(sawL_OW);
}

static void testWordBracketedWithSilence() {
  // Regression test (2025-09): a diphone "X Y" only ever supplies the
  // SECOND half of X and the FIRST half of Y (see
  // setup/audio/diphones/generate_relevant_diphones.sh). The traversal used
  // to slide over adjacent phoneme pairs within a word only, so the first
  // phoneme's own onset (its first half, from silence) and the last
  // phoneme's own release (its second half, into silence) were never
  // rendered by any diphone at all -- every word started mid-attack and
  // ended mid-release. Fixed by bracketing the sequence with SIL on both
  // ends so the leading SIL_<first> and trailing <last>_SIL diphones (both
  // present in the corpus) get requested too.
  RecordingDictionary dict;
  DiphoneVocoder v(dict);
  CapturePrint out;

  dict.requested.clear();
  v.sayPhoneme(PhonemeType::ARPAbet, "HH EH L OW", out);

  bool sawLeadingSIL = false, sawTrailingSIL = false;
  for (auto& r : dict.requested) {
    if (r == "SIL HH") sawLeadingSIL = true;
    if (r == "OW SIL") sawTrailingSIL = true;
  }
  CHECK(sawLeadingSIL);
  CHECK(sawTrailingSIL);

  // A single-phoneme "word" must get both its onset and release, not just
  // one (the old special case only appended a trailing SIL).
  dict.requested.clear();
  v.sayPhoneme(PhonemeType::ARPAbet, "AH", out);
  bool sawSIL_AH = false, sawAH_SIL = false;
  for (auto& r : dict.requested) {
    if (r == "SIL AH") sawSIL_AH = true;
    if (r == "AH SIL") sawAH_SIL = true;
  }
  CHECK(sawSIL_AH);
  CHECK(sawAH_SIL);
}

static void testStressDigitsStripped() {
  // Regression test (2025-09): the full CMU dictionary (COMPACT_CMUDICT_EN)
  // emits stress-marked phonemes like "IH1", but diphone names are keyed by
  // bare phoneme symbols ("IH", not "IH1"). Without stripping the digit
  // first, every stressed vowel would silently fail every diphone lookup.
  RecordingDictionary dict;
  DiphoneVocoder v(dict);
  CapturePrint out;

  dict.requested.clear();
  v.sayPhoneme(PhonemeType::ARPAbet, "K W IH1 K", out);  // "quick", as the real CMU dict renders it

  bool sawW_IH = false, sawIH_K = false, sawStrayDigitName = false;
  for (auto& r : dict.requested) {
    if (r == "W IH") sawW_IH = true;
    if (r == "IH K") sawIH_K = true;
    if (r.find('1') != std::string::npos || r.find('2') != std::string::npos) sawStrayDigitName = true;
  }
  CHECK(sawW_IH);
  CHECK(sawIH_K);
  CHECK(!sawStrayDigitName);
}

static void testMultiWordSequenceSegmentsAtSilence() {
  // Regression test (2025-09): a whole-utterance sequence with "SP" tokens
  // between words (matching TinyTTSTools::toPhonemes()'s output) must be
  // split into per-word segments -- each word gets its own real interior
  // diphones (coarticulated transitions), not just isolated SIL_X/X_SIL
  // pairs, and no diphone should ever be looked up spanning across an "SP".
  RecordingDictionary dict;
  DiphoneVocoder v(dict);
  CapturePrint out;

  dict.requested.clear();
  // "hello world": HH EH L OW | SP | W ER L D
  v.sayPhoneme(PhonemeType::ARPAbet, "HH EH L OW SP W ER L D", out);

  bool sawInteriorTransition = false;  // a real coarticulated transition, not a SIL edge
  bool sawSpanningSP = false;
  bool sawBothWordsBracketed = false;
  bool sawSIL_HH = false, sawOW_SIL = false, sawSIL_W = false, sawD_SIL = false;
  for (auto& r : dict.requested) {
    if (r == "EH L") sawInteriorTransition = true;  // interior of "hello"
    if (r == "ER L") sawInteriorTransition = true;  // interior of "world"
    if (r.find("SP") != std::string::npos) sawSpanningSP = true;
    if (r == "SIL HH") sawSIL_HH = true;
    if (r == "OW SIL") sawOW_SIL = true;
    if (r == "SIL W") sawSIL_W = true;
    if (r == "D SIL") sawD_SIL = true;
  }
  sawBothWordsBracketed = sawSIL_HH && sawOW_SIL && sawSIL_W && sawD_SIL;
  CHECK(sawInteriorTransition);
  CHECK(!sawSpanningSP);
  CHECK(sawBothWordsBracketed);
}

static void testReducedVowelsFallBackToFullVowelDiphones() {
  // Regression test (2025-09): AH0 and ER0 are NOT stress-marked variants
  // of AH/ER -- stripStressMarker() correctly leaves them alone, since
  // they're genuinely distinct reduced-vowel phonemes in Phonemes.h's
  // phoneme_map. But the diphone corpus was only ever generated for the 15
  // main vowels (see generate_relevant_diphones.sh) -- it has no AH0/ER0
  // diphone data at all, so "whispers" ("W IH S P ER0 Z") silently dropped
  // the P->ER0 and ER0->Z transitions entirely (sayPhoneme() returned
  // false and those diphones were never requested/rendered). Fixed by
  // falling back to the closest available full-vowel diphone (AH0->AH,
  // ER0->ER) instead of dropping audio.
  RecordingDictionary dict;
  DiphoneVocoder v(dict);
  CapturePrint out;

  dict.requested.clear();
  bool ok = v.sayPhoneme(PhonemeType::ARPAbet, "W IH S P ER0 Z", out);
  CHECK(ok);

  bool sawP_ER = false, sawER_Z = false, sawReducedVowelName = false;
  for (auto& r : dict.requested) {
    if (r == "P ER") sawP_ER = true;
    if (r == "ER Z") sawER_Z = true;
    if (r.find("ER0") != std::string::npos || r.find("AH0") != std::string::npos) {
      sawReducedVowelName = true;
    }
  }
  CHECK(sawP_ER);
  CHECK(sawER_Z);
  CHECK(!sawReducedVowelName);
}

int main() {
  testDataCompleteness();
  testTraversalCoversEveryTransition();
  testWordBracketedWithSilence();
  testStressDigitsStripped();
  testMultiWordSequenceSegmentsAtSilence();
  testReducedVowelsFallBackToFullVowelDiphones();
  return testSummary();
}
