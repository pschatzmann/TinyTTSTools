"""Exports data/dictionary/cmudict.bin: the same full CMU Pronouncing
Dictionary as CompactCmuDictionaryEN_data.h (see regen_cmudict.py), but as
a runtime-loadable binary file for CompressedPhonemeDictionarySD (see
src/TinyTTSTools/PhonemeDictionary/CompressedPhonemeDictionarySD.h) instead of a
compiled-in flash array -- for loading into PSRAM on ESP32, or anywhere
flash is tighter than SD/LittleFS storage.

Usage (from the TinyTTSTools repo root):
    cd setup/dictionary
    python3 export_dynamic_cmudict.py
"""
import os
from pack_common import load_entries
from pack_compressed import pack_compressed, emit_binary_file
from build_huffman import load_huffman_codes_from_corpus

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "cmudict_dump.txt")
OUT = os.path.join(HERE, "..", "..", "data", "dictionary", "cmudict.bin")


def main():
    entries = load_entries(SRC)
    print(f"{len(entries)} entries")

    huffman_codes, _freq = load_huffman_codes_from_corpus()
    packed = pack_compressed(entries, huffman_codes)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    emit_binary_file(OUT, packed)


if __name__ == "__main__":
    main()
