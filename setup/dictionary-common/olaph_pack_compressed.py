"""Packs a parsed OLaPh corpus into CompressedPhonemeDictionaryWide's binary
format: a blocked word-offset index + Huffman-coded (widened uint16_t
symbol) phoneme bitstream. Mirrors pack_compressed.py's BitWriter/blocked-
index logic exactly (same BLOCK_SIZE, same six-array shape) -- only the
per-phoneme symbol width (uint16_t vs uint8_t) and word encoding (UTF-8,
not ASCII-only -- German/French/Spanish words need it) differ.

Usage (run from setup/dictionary-common/; corpus path is derived from
--lang, see olaph_build_huffman.corpus_path()):
    python3 olaph_pack_compressed.py --lang de
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "dictionary-en"))

from olaph_build_huffman import compute_huffman_codes, corpus_path
from pack_compressed import BitWriter, _escape_c_string, emit_binary_file

OUT_DIR = os.path.join(HERE, "..", "..", "src", "TinyTTSTools", "Data", "dictionary")
DATA_OUT_DIR = os.path.join(HERE, "..", "..", "data", "dictionary")
BLOCK_SIZE = 8  # must match CompressedPhonemeDictionaryWide::BLOCK_SIZE


def pack_compressed_wide(entries, huffman_codes):
    word_block_offsets = []
    word_lengths = bytearray()
    words_blob = bytearray()
    phoneme_block_bit_offsets = []
    phoneme_counts = bytearray()
    bw = BitWriter()

    for i, (word, symbols) in enumerate(entries):
        if i % BLOCK_SIZE == 0:
            word_block_offsets.append(len(words_blob))
            phoneme_block_bit_offsets.append(bw.bit_pos())
        wbytes = word.encode("utf-8")
        assert len(wbytes) < 256, f"word too long: {word!r}"
        words_blob += wbytes
        word_lengths.append(len(wbytes))

        assert len(symbols) < 256, f"too many phonemes: {word!r}"
        phoneme_counts.append(len(symbols))
        for sym in symbols:
            code, length = huffman_codes[sym]
            bw.write(code, length)

    phoneme_bits = bw.finish()
    return {
        "word_block_offsets": word_block_offsets,
        "word_lengths": bytes(word_lengths),
        "words_blob": bytes(words_blob),
        "phoneme_block_bit_offsets": phoneme_block_bit_offsets,
        "phoneme_counts": bytes(phoneme_counts),
        "phoneme_bits": phoneme_bits,
        "count": len(entries),
    }


def emit_cpp_header(path, var_prefix, packed, lang_upper, guard_comment):
    with open(path, "w") as f:
        f.write(f"// {guard_comment}\n")
        f.write("// Auto-generated -- do not edit by hand.\n")
        f.write("#pragma once\n#include <cstdint>\n#include <cstddef>\n")
        f.write('#include "../../PhonemeDictionary/CompressedPhonemeDictionaryWide.h"\n')
        f.write(f'#include "../../PhonemeDictionary/PhonemeHuffmanCodesWide{lang_upper}.h"\n\n')

        wbo = packed["word_block_offsets"]
        f.write(f"static const uint32_t {var_prefix}_WORD_BLOCK_OFFSETS[] = {{\n")
        for i in range(0, len(wbo), 16):
            f.write("  " + ",".join(str(x) for x in wbo[i:i + 16]) + ",\n")
        f.write("};\n\n")

        wl = packed["word_lengths"]
        f.write(f"static const uint8_t {var_prefix}_WORD_LENGTHS[] = {{\n")
        for i in range(0, len(wl), 20):
            f.write("  " + ",".join(str(b) for b in wl[i:i + 20]) + ",\n")
        f.write("};\n\n")

        wb = packed["words_blob"]
        f.write(f"static const char {var_prefix}_WORDS_BLOB[] =\n")
        chunk = 4000
        for i in range(0, len(wb), chunk):
            f.write(f'    "{_escape_c_string(wb[i:i + chunk])}"\n')
        if not wb:
            f.write('    ""\n')
        f.write(";\n\n")

        pbo = packed["phoneme_block_bit_offsets"]
        f.write(f"static const uint32_t {var_prefix}_PHONEME_BLOCK_BIT_OFFSETS[] = {{\n")
        for i in range(0, len(pbo), 16):
            f.write("  " + ",".join(str(x) for x in pbo[i:i + 16]) + ",\n")
        f.write("};\n\n")

        pc = packed["phoneme_counts"]
        f.write(f"static const uint8_t {var_prefix}_PHONEME_COUNTS[] = {{\n")
        for i in range(0, len(pc), 20):
            f.write("  " + ",".join(str(b) for b in pc[i:i + 20]) + ",\n")
        f.write("};\n\n")

        pbits = packed["phoneme_bits"]
        f.write(f"static const uint8_t {var_prefix}_PHONEME_BITS[] = {{\n")
        for i in range(0, len(pbits), 20):
            f.write("  " + ",".join(str(b) for b in pbits[i:i + 20]) + ",\n")
        f.write("};\n\n")

        f.write(f"static const CompressedPhonemeDictionaryWide {var_prefix}(\n")
        f.write(f"    {var_prefix}_WORD_BLOCK_OFFSETS, {var_prefix}_WORD_LENGTHS, {var_prefix}_WORDS_BLOB,\n")
        f.write(f"    {var_prefix}_PHONEME_BLOCK_BIT_OFFSETS, {var_prefix}_PHONEME_COUNTS, "
                f"{var_prefix}_PHONEME_BITS, {packed['count']},\n")
        f.write(f"    PHONEME_HUFFMAN_CODES_WIDE_{lang_upper}, PHONEME_HUFFMAN_CODE_WIDE_{lang_upper}_COUNT);\n")

    total = (len(wbo) * 4 + len(wl) + len(wb) + len(pbo) * 4 + len(pc) + len(pbits))
    print(f"Wrote {path}: {total} bytes ({total/1024/1024:.1f} MB)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", required=True)
    args = ap.parse_args()

    lang = args.lang
    lang_upper = lang.upper()
    print(f"Parsing + computing Huffman codes for {lang}...")
    entries, huffman_codes, _freq = compute_huffman_codes(corpus_path(lang))
    entries.sort(key=lambda e: e[0])
    # Words must be unique for binary search -- iter_parsed_entries already
    # dedupes by first occurrence, so this is just a safety assertion.
    words = [w for w, _ in entries]
    assert len(words) == len(set(words)), "duplicate words after dedup -- should not happen"

    packed = pack_compressed_wide(entries, huffman_codes)

    os.makedirs(OUT_DIR, exist_ok=True)
    out_path = os.path.join(OUT_DIR, f"CompactOlaph{lang_upper}_data.h")
    emit_cpp_header(
        out_path, f"COMPACT_OLAPH_{lang_upper}", packed, lang_upper,
        f"OLaPh {lang_upper} pronunciation dictionary ({packed['count']} words), "
        f"Huffman-compressed (widened uint16_t symbol) -- see "
        f"setup/dictionary-common/olaph_pack_compressed.py.")

    # Also emit the runtime-loadable binary form (for
    # CompressedPhonemeDictionaryWideSD -- SD card/LittleFS, optionally
    # PSRAM on ESP32) -- same six arrays, same binary layout
    # pack_compressed.emit_binary_file() already documents (the format is
    # symbol-width-agnostic, see CompressedPhonemeDictionaryWideSD.h's own
    # doc), just written to data/ instead of compiled into flash.
    os.makedirs(DATA_OUT_DIR, exist_ok=True)
    bin_path = os.path.join(DATA_OUT_DIR, f"olaph_{lang}.bin")
    emit_binary_file(bin_path, packed)


if __name__ == "__main__":
    main()
