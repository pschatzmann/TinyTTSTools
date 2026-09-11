// Regression tests for G2PRuleBasedModelFR.h -- covers the four nasal-vowel
// spelling families, "ch"/"gn" digraphs, accented letters, the "œu"
// ligature-plus-vowel special case, and the silent-final-consonant default.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/G2P/G2PRuleBasedModelFR.h"

int main() {
  G2PRuleBasedModelFR g2p;

  CHECK_EQ(g2p.wordToPhonemes("bonjour"), std::string("B ON ZH UW RU"));
  CHECK_EQ(g2p.wordToPhonemes("enfant"), std::string("AN F AN"));
  CHECK_EQ(g2p.wordToPhonemes("chat"), std::string("SH AF"));
  CHECK_EQ(g2p.wordToPhonemes("salut"), std::string("S AF L UF"));
  CHECK_EQ(g2p.wordToPhonemes("école"), std::string("EP K AO L"));
  CHECK_EQ(g2p.wordToPhonemes("tête"), std::string("T EH T"));

  // "œu" ligature-plus-vowel must resolve to one OF, not OF + a separate u.
  CHECK_EQ(g2p.wordToPhonemes("cœur"), std::string("K OF RU"));

  // Silent final consonants (the common default -- see the class's own
  // documented exceptions).
  CHECK_EQ(g2p.wordToPhonemes("garçon"), std::string("G AF RU S ON"));

  return testSummary();
}
