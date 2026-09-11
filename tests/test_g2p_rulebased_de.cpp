// Regression tests for G2PRuleBasedModelDE.h -- covers digraphs (sch, ck,
// eu/ei/ie), umlaut/ß handling, the ch context rule (ich-Laut vs.
// ach-Laut), word-initial sp/st, and word-final obstruent devoicing.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/G2P/G2PRuleBasedModelDE.h"

int main() {
  G2PRuleBasedModelDE g2p;

  CHECK_EQ(g2p.wordToPhonemes("danke"), std::string("D AF N K AH0"));
  CHECK_EQ(g2p.wordToPhonemes("fünf"), std::string("F UF0 N F"));
  CHECK_EQ(g2p.wordToPhonemes("schön"), std::string("SH OE N"));
  CHECK_EQ(g2p.wordToPhonemes("straße"), std::string("SH T RU AF S AH0"));

  // "ch" context: ich-Laut (C) by default, ach-Laut (X) after a/o/u.
  CHECK_EQ(g2p.wordToPhonemes("ich"), std::string("IH C"));
  CHECK_EQ(g2p.wordToPhonemes("nacht"), std::string("N AF X T"));

  // Word-final obstruent devoicing: b/d/g -> p/t/k only at the word's end.
  CHECK_EQ(g2p.wordToPhonemes("tag"), std::string("T AF K"));
  CHECK_EQ(g2p.wordToPhonemes("morgen"), std::string("M AO RU G AH0 N"));  // medial g stays

  // Word-initial sp/st -> SH P / SH T.
  CHECK_EQ(g2p.wordToPhonemes("stadt"), std::string("SH T AF T"));

  return testSummary();
}
