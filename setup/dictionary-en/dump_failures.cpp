// Dumps every CMU-dictionary word (word, expected-phonemes) where
// G2PRuleBasedModel's letter-to-sound rules produce the WRONG pronunciation
// (stress-stripped, since the rules never emit stress). This is the source
// list for PhonemeExceptionDictionaryEN_data.h -- checked before falling
// back to rules (see G2PDictionaryAndRulesModel / G2PRuleBasedModel.h).
//
// Build (from the TinyTTSTools repo root):
//   g++ -std=c++17 -O2 -I src setup/dictionary/dump_failures.cpp -o /tmp/dump_failures
//   /tmp/dump_failures > setup/dictionary/failures.txt
#include <cstdio>
#include <string>
#include "TinyTTSTools/G2P/G2PRuleBasedModel.h"
#include "TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h"

static std::string stripStress(const std::string& s) {
  std::string out, tok;
  auto flush = [&]() {
    if (!tok.empty()) {
      if (tok.back() == '0' || tok.back() == '1' || tok.back() == '2') tok.pop_back();
      if (!out.empty()) out += ' ';
      out += tok;
      tok.clear();
    }
  };
  for (char c : s) {
    if (c == ' ') flush();
    else tok += c;
  }
  flush();
  return out;
}

int main() {
  const auto& dict = COMPACT_CMUDICT_EN;
  G2PRuleBasedModel g2p;
  size_t n = dict.size();
  size_t checked = 0, failed = 0;
  for (size_t i = 0; i < n; ++i) {
    std::string word = dict.wordAt(i);
    bool alpha = true;
    for (char c : word)
      if (!(c >= 'a' && c <= 'z') && c != '\'') { alpha = false; break; }
    if (!alpha || word.empty()) continue;
    checked++;
    std::string expected = stripStress(dict.phonemesAt(i));
    std::string actual = g2p.wordToPhonemes(word);
    if (expected != actual) {
      printf("%s\t%s\n", word.c_str(), expected.c_str());
      failed++;
    }
  }
  fprintf(stderr, "checked=%zu failed=%zu (%.2f%%)\n", checked, failed, 100.0 * failed / checked);
}
