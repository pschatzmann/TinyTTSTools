"""Packs a (word, [tokens]) entry list into the CompressedPhonemeDictionary
binary format: a blocked word-offset index + Huffman-coded phoneme
bitstream. See src/TinyTTSTools/Dictionary/CompressedPhonemeDictionary.h for
the format this must match exactly.
"""
from pack_common import pack_phoneme_token

BLOCK_SIZE = 8  # must match CompressedPhonemeDictionary::BLOCK_SIZE


class BitWriter:
    def __init__(self):
        self.bytes = bytearray()
        self.cur = 0
        self.nbits = 0

    def write(self, value, length):
        for i in range(length - 1, -1, -1):
            bit = (value >> i) & 1
            self.cur = (self.cur << 1) | bit
            self.nbits += 1
            if self.nbits == 8:
                self.bytes.append(self.cur)
                self.cur = 0
                self.nbits = 0

    def bit_pos(self):
        return len(self.bytes) * 8 + self.nbits

    def finish(self):
        if self.nbits > 0:
            self.cur <<= (8 - self.nbits)
            self.bytes.append(self.cur)
            self.cur = 0
            self.nbits = 0
        return bytes(self.bytes)


def pack_compressed(entries, huffman_codes):
    """entries: sorted, deduplicated list of (word, [tokens]) -- see
    pack_common.load_entries(). huffman_codes: {packed_byte: (code, length)}
    -- see build_huffman.load_huffman_codes_from_corpus(). Returns a dict of
    the six arrays CompressedPhonemeDictionary needs."""
    word_block_offsets = []
    word_lengths = bytearray()
    words_blob = bytearray()
    phoneme_block_bit_offsets = []
    phoneme_counts = bytearray()
    bw = BitWriter()

    for i, (word, tokens) in enumerate(entries):
        if i % BLOCK_SIZE == 0:
            word_block_offsets.append(len(words_blob))
            phoneme_block_bit_offsets.append(bw.bit_pos())
        wbytes = word.encode("ascii")
        assert len(wbytes) < 256, f"word too long: {word!r}"
        words_blob += wbytes
        word_lengths.append(len(wbytes))

        assert len(tokens) < 256, f"too many phonemes: {word!r}"
        phoneme_counts.append(len(tokens))
        for tok in tokens:
            packed = pack_phoneme_token(tok)
            if packed is None:
                raise ValueError(f"Unknown phoneme token {tok!r} in word {word!r}")
            code, length = huffman_codes[packed]
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


def _escape_c_string(data: bytes) -> str:
    out = []
    for b in data:
        c = chr(b)
        if c == '"' or c == '\\':
            out.append('\\' + c)
        elif 32 <= b < 127:
            out.append(c)
        else:
            out.append(f'\\{b:03o}')
    return "".join(out)


def emit_cpp_header(path, var_prefix, packed, guard_comment,
                    include_path="CompressedPhonemeDictionary.h"):
    """Emits the six backing arrays plus a ready `static const
    CompressedPhonemeDictionary <var_prefix>(...)` global. `include_path` is
    the #include path to CompressedPhonemeDictionary.h *relative to `path`*
    -- e.g. plain "CompressedPhonemeDictionary.h" for a file placed directly
    in src/TinyTTSTools/Dictionary/, or "../../Dictionary/..." for one
    placed under src/TinyTTSTools/Data/dictionary/."""
    with open(path, "w") as f:
        f.write(f"// {guard_comment}\n")
        f.write("// Auto-generated -- do not edit by hand.\n")
        f.write("#pragma once\n#include <cstdint>\n#include <cstddef>\n")
        f.write(f'#include "{include_path}"\n\n')

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

        f.write(f"static const CompressedPhonemeDictionary {var_prefix}(\n")
        f.write(f"    {var_prefix}_WORD_BLOCK_OFFSETS, {var_prefix}_WORD_LENGTHS, {var_prefix}_WORDS_BLOB,\n")
        f.write(f"    {var_prefix}_PHONEME_BLOCK_BIT_OFFSETS, {var_prefix}_PHONEME_COUNTS, "
                f"{var_prefix}_PHONEME_BITS, {packed['count']});\n")

    total = (len(wbo) * 4 + len(wl) + len(wb) + len(pbo) * 4 + len(pc) + len(pbits))
    print(f"Wrote {path}: {total} bytes ({total/1024:.1f} KB)")
