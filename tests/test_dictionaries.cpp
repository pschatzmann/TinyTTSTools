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
#include "TinyTTSTools/PhonemeDictionary/CompactPhonemeDictionaryBuilder.h"
#include "TinyTTSTools/PhonemeDictionary/FallbackPhonemeDictionary.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryDE.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryEN.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryES.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryFR.h"
#include "TinyTTSTools/PhonemeDictionary/RuntimePhonemeDictionary.h"
#include "TinyTTSTools/G2P/G2PDictionaryModel.h"
#include "TinyTTSTools/G2P/G2PDictionaryAndRulesModel.h"
#include "TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeExceptionDictionaryEN_data.h"

static void testPhonemesTable() {
  Phonemes p;
  // NG is id 42, the last of the original 43 ARPAbet entries (0-42) --
  // previously unreachable because getPhonemeById()'s bound check was
  // `id >= 42`.
  const PhonemeInfo* ng = p.getPhonemeById(42);
  CHECK(ng != nullptr);
  if (ng) CHECK_EQ(std::string(ng->arpabet), std::string("NG"));

  // Id 43 is now valid too -- it's UF, the first entry of Phone's merged
  // international/IPA extension (ids 43-120), not out of range anymore.
  const PhonemeInfo* uf = p.getPhonemeById(43);
  CHECK(uf != nullptr);
  if (uf) CHECK_EQ(std::string(uf->arpabet), std::string("UF"));

  // The table has 121 entries total (0-120) -- 121 is the real boundary now.
  const PhonemeInfo* ob2 = p.getPhonemeById(120);
  CHECK(ob2 != nullptr);
  if (ob2) CHECK_EQ(std::string(ob2->arpabet), std::string("OB2"));
  CHECK(p.getPhonemeById(121) == nullptr);  // out of range

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

static void testWidenedPackedSymbolSupportsInternationalIdsAndModifiers() {
  // Seg()/CompactPhonemeDictionary's packed symbol widened from uint8_t
  // ((id << 2) | stress, capped at id<=63) to uint16_t ((id << 2) |
  // stress, with a modifier OR'd in at bit 9 when stress is 0) -- this is
  // needed for German/French/Spanish, both for the international/IPA
  // Phone ids (43-120, previously rejected at compile time by
  // cxPackPhone()) and for non-stress modifiers (previously silently
  // dropped by Seg(), since the old byte format had nowhere to put them).
  static constexpr PhonemeWordSource table[] = {
      PH_WORD("bare", Phone::K, Phone::AE, Phone::T),
      PH_WORD("intl", Seg(Phone::UF, PhonemeModifier::MOD_LONG), Seg(Phone::AN)),
      PH_WORD("mixed", Phone::AE, Seg(Phone::T, PhonemeModifier::MOD_PALATALIZED)),
      PH_WORD("palatal", Seg(Phone::T, PhonemeModifier::MOD_PALATALIZED), Seg(Phone::EN)),
      PH_WORD("stressed", Seg(Phone::AA, PhonemeModifier::MOD_STRESS_PRIMARY), Seg(Phone::T)),
  };
  TTS_COMPACT_DICTIONARY(testIntlDict, table);

  std::string out;
  // Plain-list style: bare Phone values, no Seg() wrapper needed.
  CHECK(testIntlDict.lookup("bare", out));
  CHECK_EQ(out, std::string("K AE T"));
  // Mixing bare Phone and Seg() in the same PH_WORD() call.
  CHECK(testIntlDict.lookup("mixed", out));
  CHECK_EQ(out, std::string("AE T_j"));
  // German "über"-style: long UF (international id 43) + French nasal AN.
  CHECK(testIntlDict.lookup("intl", out));
  CHECK_EQ(out, std::string("UF: AN"));
  // Palatalized T (a non-stress modifier -- the old Seg() would drop this).
  CHECK(testIntlDict.lookup("palatal", out));
  CHECK_EQ(out, std::string("T_j EN"));
  // Stress still folds into its dedicated 2-bit field, unchanged -- decoded
  // back out as a digit suffix (the format CompactPhonemeDictionary always
  // used), not the X-SAMPA prefix tag form.
  CHECK(testIntlDict.lookup("stressed", out));
  CHECK_EQ(out, std::string("AA1 T"));
}

static void testRuntimeDictionaryAndFallback() {
  // Extended phoneme-string authoring style (option c): stored/returned
  // verbatim, no packing -- the string is already the same
  // modifier-tag/stress-digit format PSOLAVocoder/FormantVocoder parse.
  static constexpr PhonemeStringSource extra[] = {
      {"tja", "T_j AA"},
      {"ueber", "UF: B ER0"},
  };
  RuntimePhonemeDictionary extraDict(extra);
  CHECK_EQ(extraDict.size(), static_cast<size_t>(2));

  std::string out;
  CHECK(extraDict.lookup("ueber", out));
  CHECK_EQ(out, std::string("UF: B ER0"));
  CHECK(extraDict.lookup("tja", out));
  CHECK_EQ(out, std::string("T_j AA"));
  CHECK(!extraDict.lookup("hello", out));  // not in this small dictionary

  // FallbackPhonemeDictionary composes the compile-time PH_WORD()
  // dictionary with this runtime one -- both authoring styles resolve
  // through a single PhonemeDictionaryBase.
  FallbackPhonemeDictionary combined(COMPACT_PHONEME_DICTIONARY_EN, extraDict);
  CHECK(combined.lookup("hello", out));  // from the compile-time dictionary
  CHECK(combined.lookup("ueber", out));  // from the runtime dictionary
  CHECK_EQ(out, std::string("UF: B ER0"));
  CHECK(!combined.lookup("notarealword123", out));  // in neither
}

static void testInternationalStarterDictionaries() {
  std::string out;

  // DE/FR/ES are now generated from the real OLaPh corpus for each
  // language's top-1000 most frequent words (see
  // setup/dictionary-common/generate_small_dictionary.py), not
  // hand-transcribed -- so entries carry real stress/length markers OLaPh
  // includes that the old hand-authored set didn't bother with.
  CHECK_EQ(COMPACT_PHONEME_DICTIONARY_DE.size(), static_cast<size_t>(986));
  CHECK(COMPACT_PHONEME_DICTIONARY_DE.lookup("danke", out));
  CHECK_EQ(out, std::string("D1 AF NG K AH0"));
  CHECK(COMPACT_PHONEME_DICTIONARY_DE.lookup("fünf", out));  // international id (UF0)
  CHECK_EQ(out, std::string("F UF0 N F"));
  CHECK(COMPACT_PHONEME_DICTIONARY_DE.lookup("tag", out));  // MOD_LONG (long a)
  CHECK_EQ(out, std::string("T AF: K"));
  CHECK(!COMPACT_PHONEME_DICTIONARY_DE.lookup("notarealword123", out));

  CHECK_EQ(COMPACT_PHONEME_DICTIONARY_FR.size(), static_cast<size_t>(981));
  CHECK(COMPACT_PHONEME_DICTIONARY_FR.lookup("bonjour", out));
  CHECK_EQ(out, std::string("B ON ZH UW RU"));
  CHECK(COMPACT_PHONEME_DICTIONARY_FR.lookup("un", out));  // nasal vowel
  CHECK_EQ(out, std::string("UN"));

  CHECK_EQ(COMPACT_PHONEME_DICTIONARY_ES.size(), static_cast<size_t>(970));
  CHECK(COMPACT_PHONEME_DICTIONARY_ES.lookup("perro", out));  // trilled r
  CHECK_EQ(out, std::string("P EP RR OP"));
  CHECK(COMPACT_PHONEME_DICTIONARY_ES.lookup("país", out));  // irregular stress
  CHECK_EQ(out, std::string("P AF IY1 S"));
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
  testWidenedPackedSymbolSupportsInternationalIdsAndModifiers();
  testRuntimeDictionaryAndFallback();
  testInternationalStarterDictionaries();
  testCmuAndExceptionDictionaries();
  testG2PDictionaryModelComposition();
  return testSummary();
}
