// Regression tests for Tokenizer.h / StringUtils.h's UTF-8 handling.
//
// Bug history: preprocessText() classified each byte with <cctype>'s
// isalpha()/isspace(), which only recognizes plain ASCII -- every byte of
// a multi-byte UTF-8 character (e.g. the two bytes of "é") failed that
// check and was silently dropped, corrupting any accented word ("café" ->
// "caf"). Since PhonemeDictionaryFR/DE/ES.h key their entries by the
// accented UTF-8 spelling, this made every accented dictionary word miss
// its lookup. Passing a signed char outside [0, 127] to <cctype> functions
// is also undefined behavior per the standard, independent of the
// dropped-byte bug.
#include <cstdio>
#include <string>
#include "TestUtils.h"
#include "TinyTTSTools/Basic/StringUtils.h"
#include "TinyTTSTools/Basic/Tokenizer.h"

int main() {
  // Accented words must survive tokenization byte-for-byte.
  CHECK_EQ(Tokenizer::tokenize("café").size(), 1u);
  CHECK_EQ(Tokenizer::tokenize("café")[0], std::string("café"));
  CHECK_EQ(Tokenizer::tokenize("acheté")[0], std::string("acheté"));
  CHECK_EQ(Tokenizer::tokenize("naïve")[0], std::string("naïve"));

  // ASCII uppercase still lowercases; a non-ASCII byte is preserved as-is
  // (known limitation: it is not itself case-folded).
  CHECK_EQ(Tokenizer::tokenize("Zürich")[0], std::string("zürich"));

  // Plain ASCII behavior is unaffected.
  CHECK_EQ(Tokenizer::tokenize("Hello World")[0], std::string("hello"));
  CHECK_EQ(Tokenizer::tokenize("Hello World")[1], std::string("world"));
  CHECK_EQ(Tokenizer::tokenize("Wait. Really?").size(), 4u);  // wait . really .

  // StringUtils::toLowerCase() directly: ASCII bytes lowercased, non-ASCII
  // UTF-8 bytes passed through unchanged rather than dropped/corrupted.
  // Known limitation: "É" (0xC3 0x89) is a genuinely different code point
  // from "é" (0xC3 0xA9), not an ASCII-style case shift, so it is NOT
  // itself case-folded here -- only the ASCII "CAF" prefix lowercases.
  CHECK_EQ(StringUtils::toLowerCase("CAFÉ"), std::string("caf\xC3\x89"));
  CHECK_EQ(StringUtils::toLowerCase("HELLO"), std::string("hello"));

  return testSummary();
}
