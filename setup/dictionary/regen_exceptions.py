"""Regenerates
src/TinyTTSTools/Dictionary/PhonemeExceptionDictionaryEN_data.h from
setup/dictionary/failures.txt (see dump_failures.cpp) using the current
canonical Huffman codes (see build_huffman.py).

Usage (from the TinyTTSTools repo root):
    cd setup/dictionary
    python3 regen_exceptions.py
"""
import os
from pack_common import load_entries
from pack_compressed import pack_compressed, emit_cpp_header
from build_huffman import load_huffman_codes_from_corpus

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "failures.txt")
OUT = os.path.join(HERE, "..", "..", "src", "TinyTTSTools", "Dictionary",
                   "PhonemeExceptionDictionaryEN_data.h")


def main():
    entries = load_entries(SRC)
    print(f"{len(entries)} entries")

    huffman_codes, _freq = load_huffman_codes_from_corpus()
    packed = pack_compressed(entries, huffman_codes)
    emit_cpp_header(
        OUT,
        "PHONEME_EXCEPTION_DICTIONARY_EN",
        packed,
        f"Exception dictionary for G2PRuleBasedModel: every word (out of the "
        f"full CMU dictionary, see CompactCmuDictionaryEN_data.h) where the "
        f"letter-to-sound rules produce the wrong pronunciation "
        f"({len(entries)} words). Checked before falling back to rules -- "
        "see G2PDictionaryAndRulesModel.getDictionaryModel()."
        "useCompactDictionary(). Blocked offset index + Huffman-coded "
        "phonemes -- see CompressedPhonemeDictionary.h and "
        "PhonemeHuffmanCodes.h.",
        include_path="CompressedPhonemeDictionary.h",
    )


if __name__ == "__main__":
    main()
