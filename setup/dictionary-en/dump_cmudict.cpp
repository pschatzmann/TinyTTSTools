// Dumps the currently-shipped CMU dictionary's (word, phonemes) pairs to a
// tab-separated text file -- the ground-truth source the rest of this
// pipeline (build_huffman.py, pack_compressed.py) regenerates the compact
// binary header from. Run this BEFORE regenerating, so a rebuild is a pure
// re-packing of already-correct data, not a re-derivation of pronunciations.
//
// Build (from the TinyTTSTools repo root):
//   g++ -std=c++17 -O2 -I src setup/dictionary/dump_cmudict.cpp -o /tmp/dump_cmudict
//   /tmp/dump_cmudict > setup/dictionary/cmudict_dump.txt
#include <cstdio>
#include "TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h"

int main() {
  size_t n = COMPACT_CMUDICT_EN.size();
  for (size_t i = 0; i < n; ++i) {
    printf("%s\t%s\n", COMPACT_CMUDICT_EN.wordAt(i).c_str(), COMPACT_CMUDICT_EN.phonemesAt(i).c_str());
  }
  fprintf(stderr, "dumped %zu entries\n", n);
}
