// Regression test for G2PRuleBasedModelEN.h's vowel+'r' double-R bug
// (2025-09): the e/i/o/u + 'r' rules emitted the R-content themselves
// (folded into "ER", or explicitly as "AO R" for 'o') but never skipped
// past the 'r' character the way the digraph rules already did -- so the
// same 'r' got reprocessed and re-emitted on the next loop iteration.
// Also covers the silent-final-'e' edge case for single-letter words.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/G2P/G2PRuleBasedModelEN.h"

int main() {
  G2PRuleBasedModelEN g2p;

  // Vowel+'r' patterns -- each of these previously produced a duplicated R.
  CHECK_EQ(g2p.wordToPhonemes("her"), std::string("HH ER"));
  CHECK_EQ(g2p.wordToPhonemes("bird"), std::string("B ER D"));
  CHECK_EQ(g2p.wordToPhonemes("for"), std::string("F AO R"));
  CHECK_EQ(g2p.wordToPhonemes("turn"), std::string("T ER N"));
  CHECK_EQ(g2p.wordToPhonemes("or"), std::string("AO R"));
  CHECK_EQ(g2p.wordToPhonemes("first"), std::string("F ER S T"));

  // 'a'+'r' was already correct (doesn't anticipate the 'r') -- must stay so.
  CHECK_EQ(g2p.wordToPhonemes("car"), std::string("K AA R"));

  // Single-letter "e": the silent-final-e rule used to fire even when 'e'
  // was simultaneously the first AND last letter, producing an empty
  // phoneme string for the word.
  CHECK(!g2p.wordToPhonemes("e").empty());

  // "aa" digraph (e.g. Scandinavian-origin names/words like "aardvark")
  // previously fell through to two separate single-letter 'a' -> AE
  // mappings instead of one long AA.
  CHECK_EQ(g2p.wordToPhonemes("aardvark"), std::string("AA R D V AA R K"));

  return testSummary();
}
