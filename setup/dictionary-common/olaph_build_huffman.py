"""Computes canonical Huffman codes for the widened (uint16_t) packed
phoneme symbol from real OLaPh corpus frequencies, and emits a readable
PhonemeHuffmanCodesWide{LANG}.h table (one per language -- see
CompressedPhonemeDictionaryWide.h's own doc for why this can't be a single
shared table the way English's PhonemeHuffmanCodes.h is).

Reuses build_huffman.py's Huffman algorithm (build_code_lengths/
canonicalize) unchanged -- only the symbol source (olaph_parse's packed
uint16_t instead of pack_common's packed uint8_t) and output struct type
differ.

Usage (corpus path is derived from --lang: setup/dictionary-{lang}/
olaph_{lang}.txt -- see corpus_path() below):
    python3 olaph_build_huffman.py --lang de
"""
import argparse
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# build_code_lengths()/canonicalize() live in dictionary-en/ (the only
# other place that needs plain Huffman-coding logic, for the English
# pipeline) -- reused here rather than duplicated.
sys.path.insert(0, os.path.join(HERE, "..", "dictionary-en"))

from build_huffman import build_code_lengths, canonicalize
from olaph_parse import iter_parsed_entries, PHONE_NAME_TO_ID

OUT_DIR = os.path.join(HERE, "..", "..", "src", "TinyTTSTools", "PhonemeDictionary")


def corpus_path(lang):
    return os.path.join(HERE, "..", f"dictionary-{lang}", f"olaph_{lang}.txt")

ID_TO_NAME = {v: k for k, v in PHONE_NAME_TO_ID.items()}


def compute_huffman_codes(input_path):
    """Parses `input_path` (an olaph_{lang}.txt corpus) and returns
    (entries, codes_map, freq) -- entries is the deduplicated (word,
    [packed_symbol,...]) list (already computed as a side effect of
    counting frequencies, so callers needing both, like
    olaph_pack_compressed.py, don't have to parse the corpus twice).
    codes_map is {packed_symbol: (code, length)}."""
    entries = []
    freq = collections.Counter()
    for word, symbols in iter_parsed_entries(input_path):
        entries.append((word, symbols))
        for sym in symbols:
            freq[sym] += 1
    lengths = build_code_lengths(freq)
    codes = canonicalize(lengths)
    codes_map = {sym: (code, length) for sym, code, length in codes}
    return entries, codes_map, freq


def symbol_name(sym):
    pid = (sym >> 2) & 0x7F
    stress = sym & 0x3
    modifier = (sym >> 9) & 0x1F
    name = ID_TO_NAME.get(pid, f"id{pid}")
    if stress:
        name += str(stress)
    elif modifier:
        name += f"+mod{modifier}"
    return name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", required=True)
    args = ap.parse_args()

    _entries, codes_map, freq = compute_huffman_codes(corpus_path(args.lang))
    codes = sorted(((sym, c, l) for sym, (c, l) in codes_map.items()),
                    key=lambda t: (t[2], t[0]))

    max_len = max(l for _, _, l in codes)
    if max_len > 24:  # must match CompressedPhonemeDictionaryWide::DecodeTables::kMaxLen
        raise SystemExit(f"max Huffman code length {max_len} exceeds DecodeTables::kMaxLen (24) "
                          f"-- widen kMaxLen in CompressedPhonemeDictionaryWide.h first")

    total_symbols = sum(freq.values())
    total_bits = sum(freq[sym] * length for sym, _c, length in codes)
    print(f"lang: {args.lang}")
    print(f"distinct symbols: {len(codes)}")
    print(f"max code length: {max_len} bits")
    print(f"total symbol instances: {total_symbols}")
    print(f"packed (uncompressed, 2 bytes/symbol): {total_symbols*2/1024/1024:.1f} MB")
    print(f"huffman-coded: {total_bits/8/1024/1024:.1f} MB "
          f"({100*(1 - (total_bits/8)/(total_symbols*2)):.1f}% smaller)")

    lang_upper = args.lang.upper()
    out_path = os.path.join(OUT_DIR, f"PhonemeHuffmanCodesWide{lang_upper}.h")
    with open(out_path, "w") as f:
        f.write(f"""/**
 * @file PhonemeHuffmanCodesWide{lang_upper}.h
 * @brief Canonical Huffman codes for {args.lang.upper()}'s widened packed
 * phoneme symbols, derived from the OLaPh {args.lang.upper()} corpus.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include "CompressedPhonemeDictionaryWide.h"

/**
 * @brief {len(codes)} distinct packed symbols, {total_symbols} total
 * instances across the parsed OLaPh {args.lang.upper()} corpus (see
 * setup/dictionary-{args.lang}/olaph_{args.lang}.txt and
 * setup/dictionary-common/olaph_parse.py, olaph_build_huffman.py --
 * regenerate via
 * `python3 olaph_build_huffman.py --lang {args.lang}` (run from
 * setup/dictionary-common/) if the corpus or its parsing changes). Sorted
 * by (length, packedSymbol)
 * ascending -- canonical Huffman requires this order (see
 * CompressedPhonemeDictionaryWide's decoder).
 */
static constexpr PhonemeHuffmanCodeWide PHONEME_HUFFMAN_CODES_WIDE_{lang_upper}[] = {{
""")
        for sym, code, length in codes:
            bits = format(code, f'0{length}b')
            f.write(f'    {{{sym}, 0b{bits}, {length}}},  // {symbol_name(sym)}\n')
        f.write("};\n\n")
        f.write(f"static constexpr size_t PHONEME_HUFFMAN_CODE_WIDE_{lang_upper}_COUNT = {len(codes)};\n")
    print("Wrote", out_path)


if __name__ == "__main__":
    main()
