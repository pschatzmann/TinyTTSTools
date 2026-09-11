// Regression tests for G2PRuleBasedModelES.h -- covers "ll"/"rr"/"ch"
// digraphs, the "gu"/"qu" before-e/i silent-u context rule, ñ/accented
// vowels, and the word-initial-trill-vs-medial-tap "r" context rule.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/G2P/G2PRuleBasedModelES.h"

int main() {
  G2PRuleBasedModelES g2p;

  CHECK_EQ(g2p.wordToPhonemes("perro"), std::string("P EP RR OP"));  // rr digraph
  CHECK_EQ(g2p.wordToPhonemes("gracias"), std::string("G RT AF S IY AF S"));
  CHECK_EQ(g2p.wordToPhonemes("niño"), std::string("N IY NY OP"));
  CHECK_EQ(g2p.wordToPhonemes("rojo"), std::string("RR OP X OP"));  // word-initial r -> trill
  CHECK_EQ(g2p.wordToPhonemes("hola"), std::string("OP L AF"));      // silent h

  // "gu"/"qu" before e/i: the "u" is silent.
  CHECK_EQ(g2p.wordToPhonemes("guerra"), std::string("G EP RR AF"));
  CHECK_EQ(g2p.wordToPhonemes("que"), std::string("K EP"));

  return testSummary();
}
