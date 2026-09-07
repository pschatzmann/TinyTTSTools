"""Regenerates src/TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h
from setup/dictionary/cmudict_dump.txt (see dump_cmudict.cpp) using the
current canonical Huffman codes (see build_huffman.py).

Usage (from the TinyTTSTools repo root):
    cd setup/dictionary
    python3 regen_cmudict.py
"""
import os
from pack_common import load_entries
from pack_compressed import pack_compressed, emit_cpp_header
from build_huffman import load_huffman_codes_from_corpus

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "cmudict_dump.txt")
OUT = os.path.join(HERE, "..", "..", "src", "TinyTTSTools", "Data", "dictionary",
                   "CompactCmuDictionaryEN_data.h")


def main():
    entries = load_entries(SRC)
    print(f"{len(entries)} entries")

    huffman_codes, _freq = load_huffman_codes_from_corpus()
    packed = pack_compressed(entries, huffman_codes)
    emit_cpp_header(
        OUT,
        "COMPACT_CMUDICT_EN",
        packed,
        f"Compact binary form of the full CMU Pronouncing Dictionary "
        f"({len(entries)} words), ported from tronghieuit/tiny-tts's exported "
        "cmudict via TinyTTS. Blocked offset index + Huffman-coded phonemes -- "
        "see CompressedPhonemeDictionary.h and PhonemeHuffmanCodes.h.",
        include_path="../../Dictionary/CompressedPhonemeDictionary.h",
    )


if __name__ == "__main__":
    main()
