"""Computes canonical Huffman codes for packed phoneme bytes from real
corpus frequencies, and emits the readable
src/TinyTTSTools/Dictionary/PhonemeHuffmanCodes.h table.

Usage (from the TinyTTSTools repo root, after regenerating
setup/dictionary/cmudict_dump.txt and failures.txt -- see dump_cmudict.cpp /
dump_failures.cpp):
    cd setup/dictionary
    python3 build_huffman.py
"""
import heapq
import os

from pack_common import load_entries, count_phoneme_frequencies, packed_byte_name

HERE = os.path.dirname(os.path.abspath(__file__))
CMUDICT_DUMP = os.path.join(HERE, "cmudict_dump.txt")
FAILURES_DUMP = os.path.join(HERE, "failures.txt")
OUT = os.path.join(HERE, "..", "..", "src", "TinyTTSTools", "Dictionary", "PhonemeHuffmanCodes.h")


def build_code_lengths(freq):
    """Standard Huffman via a heap; returns {symbol: length}."""
    heap = [[w, [[sym, 0]]] for sym, w in freq.items()]
    heapq.heapify(heap)
    if len(heap) == 1:
        only = heap[0][1][0][0]
        return {only: 1}
    while len(heap) > 1:
        lo = heapq.heappop(heap)
        hi = heapq.heappop(heap)
        for pair in lo[1]:
            pair[1] += 1
        for pair in hi[1]:
            pair[1] += 1
        heapq.heappush(heap, [lo[0] + hi[0], lo[1] + hi[1]])
    return {sym: length for sym, length in heap[0][1]}


def canonicalize(lengths):
    """Assign canonical codes: sort by (length, symbol), codes increase by 1,
    left-shifted whenever length increases. Returns [(symbol, code, length)]."""
    order = sorted(lengths.items(), key=lambda kv: (kv[1], kv[0]))
    codes = []
    code = 0
    prev_len = order[0][1]
    for sym, length in order:
        code <<= (length - prev_len)
        codes.append((sym, code, length))
        code += 1
        prev_len = length
    return codes


def load_huffman_codes_from_corpus():
    """Recomputes canonical codes from the current text dumps -- the single
    source of truth other scripts (pack_compressed.py) should import this
    from, so the codes used to *encode* always match PhonemeHuffmanCodes.h."""
    cmu = load_entries(CMUDICT_DUMP)
    exc = load_entries(FAILURES_DUMP)
    freq = count_phoneme_frequencies([cmu, exc])
    lengths = build_code_lengths(freq)
    codes = canonicalize(lengths)
    return {sym: (code, length) for sym, code, length in codes}, freq


def main():
    codes_map, freq = load_huffman_codes_from_corpus()
    codes = sorted(((sym, c, l) for sym, (c, l) in codes_map.items()), key=lambda t: (t[2], t[0]))

    total_bytes_before = sum(freq.values())
    total_bits = sum(freq[sym] * length for sym, _c, length in codes)
    print(f"symbols: {len(codes)}")
    print(f"max code length: {max(l for _, _, l in codes)} bits")
    print(f"before: {total_bytes_before} bytes")
    print(f"after:  {total_bits} bits = {(total_bits + 7)//8} bytes")
    print(f"reduction: {100*(1 - (total_bits/8)/total_bytes_before):.1f}%")

    with open(OUT, "w") as f:
        f.write("""/**
 * @file PhonemeHuffmanCodes.h
 * @brief Canonical Huffman codes for packed phoneme bytes
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-07
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @brief One packed phoneme byte's canonical Huffman code.
 * @details `packedByte` is the same `(phone_id << 2) | stress` value
 * CompressedPhonemeDictionary would otherwise store literally (see
 * CompactPhonemeDictionary.h); `code`/`length` are its canonical Huffman
 * codeword (MSB-first, `length` bits significant). Derived from real
 * phoneme frequencies across the full CMU dictionary + exception
 * dictionary (~1.45M phoneme instances) -- regenerate via
 * setup/dictionary/build_huffman.py if that corpus changes.
 *
 * Sorted by (length, packedByte) ascending -- canonical Huffman requires
 * this order: codes of the same length are consecutive integers, and code
 * length only increases down the table, which is what makes fast
 * table-driven decoding possible without walking a tree (see
 * CompressedPhonemeDictionary's decoder).
 */
struct PhonemeHuffmanCode {
  uint8_t packedByte;
  uint16_t code;
  uint8_t length;
};

static constexpr PhonemeHuffmanCode PHONEME_HUFFMAN_CODES[] = {
""")
        for sym, code, length in codes:
            bits = format(code, f'0{length}b')
            f.write(f'    {{{sym}, 0b{bits}, {length}}},  // {packed_byte_name(sym)}\n')
        f.write("};\n\n")
        f.write(f"static constexpr size_t PHONEME_HUFFMAN_CODE_COUNT = {len(codes)};\n")
    print("Wrote", OUT)


if __name__ == "__main__":
    main()
