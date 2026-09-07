// Regression tests for the dictionary infrastructure built up in 2025-09:
//   - Phonemes.h's getPhonemeById() off-by-one (id >= 42 excluded the last,
//     valid entry NG/id 42).
//   - Phonemes.h's X-SAMPA for ER/ER0 used an invalid backtick diacritic
//     instead of the correct backslash rhotic marker.
//   - CompactPhonemeDictionary (constexpr-built small default dictionary).
//   - CompressedPhonemeDictionary (blocked offset index + Huffman phonemes)
//     via the shipped CMU/exception dictionaries, exercised through the
//     PhonemeDictionaryBase interface + G2PDictionaryModel/
//     G2PDictionaryAndRulesModel composition.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/Basic/Phonemes.h"
#include "TinyTTSTools/Dictionary/PhonemeDictionaryEN.h"
#include "TinyTTSTools/G2P/G2PDictionaryModel.h"
#include "TinyTTSTools/G2P/G2PDictionaryAndRulesModel.h"
#include "TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h"
#include "TinyTTSTools/Dictionary/PhonemeExceptionDictionaryEN_data.h"

static void testPhonemesTable() {
  Phonemes p;
  // NG is id 42, the last of 43 entries (0-42) -- previously unreachable
  // because getPhonemeById()'s bound check was `id >= 42`.
  const PhonemeInfo* ng = p.getPhonemeById(42);
  CHECK(ng != nullptr);
  if (ng) CHECK_EQ(std::string(ng->arpabet), std::string("NG"));
  CHECK(p.getPhonemeById(43) == nullptr);  // still correctly out of range

  // X-SAMPA rhotic diacritic is a backslash, not a backtick.
  const char* er = p.translatePhoneme(PhonemeType::ARPAbet, "ER", PhonemeType::XSAMPA);
  CHECK(er != nullptr);
  if (er) CHECK_EQ(std::string(er), std::string("3\\"));
  const char* er0 = p.translatePhoneme(PhonemeType::ARPAbet, "ER0", PhonemeType::XSAMPA);
  CHECK(er0 != nullptr);
  if (er0) CHECK_EQ(std::string(er0), std::string("@\\"));
}

static void testSmallBuiltInDictionary() {
  CHECK_EQ(COMPACT_PHONEME_DICTIONARY_EN.size(), static_cast<size_t>(534));
  std::string out;
  CHECK(COMPACT_PHONEME_DICTIONARY_EN.lookup("hello", out));
  CHECK(COMPACT_PHONEME_DICTIONARY_EN.lookup("zero", out));
  CHECK_EQ(out, std::string("Z IY R OW"));
  CHECK(!COMPACT_PHONEME_DICTIONARY_EN.lookup("notarealword123", out));
}

static void testCmuAndExceptionDictionaries() {
  CHECK_EQ(COMPACT_CMUDICT_EN.size(), static_cast<size_t>(123463));
  std::string out;
  CHECK(COMPACT_CMUDICT_EN.lookup("hello", out));
  CHECK_EQ(out, std::string("HH AH0 L OW1"));
  CHECK(COMPACT_CMUDICT_EN.lookup("computer", out));
  CHECK_EQ(out, std::string("K AH0 M P Y UW1 T ER0"));

  CHECK(PHONEME_EXCEPTION_DICTIONARY_EN.size() > 0);
  CHECK(PHONEME_EXCEPTION_DICTIONARY_EN.lookup("'cause", out));
  CHECK_EQ(out, std::string("K AH Z"));
}

static void testG2PDictionaryModelComposition() {
  G2PDictionaryModel g2p;
  CHECK_EQ(g2p.getPhonemeDictionarySize(), static_cast<size_t>(534));

  g2p.useCompactDictionary(COMPACT_CMUDICT_EN);
  CHECK_EQ(g2p.getPhonemeDictionarySize(), static_cast<size_t>(123463));
  CHECK_EQ(g2p.wordToPhonemes("hello"), std::string("HH AH0 L OW1"));

  static PhonemeEntry custom[] = {{"arduino", "AA R D UW IY N OW"}};
  g2p.setPhonemeDictionary(custom, 1, PhonemeType::ARPAbet);
  CHECK_EQ(g2p.wordToPhonemes("arduino"), std::string("AA R D UW IY N OW"));
  CHECK(g2p.wordToPhonemes("hello").empty());  // not in the custom override

  g2p.resetPhonemeDictionary();
  CHECK_EQ(g2p.getPhonemeDictionarySize(), static_cast<size_t>(534));

  // Exception dictionary wired into the dictionary+rules hybrid via the
  // existing G2PHybridModel-based composition (no bespoke plumbing needed).
  G2PDictionaryAndRulesModel hybrid;
  hybrid.getDictionaryModel().useCompactDictionary(PHONEME_EXCEPTION_DICTIONARY_EN);
  CHECK_EQ(hybrid.wordToPhonemes("'cause"), std::string("K AH Z"));  // exception-dict hit
  CHECK(!hybrid.wordToPhonemes("unbelievable").empty());             // rule fallback still works
}

int main() {
  testPhonemesTable();
  testSmallBuiltInDictionary();
  testCmuAndExceptionDictionaries();
  testG2PDictionaryModelComposition();
  return testSummary();
}
