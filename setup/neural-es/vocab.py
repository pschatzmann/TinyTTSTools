"""
Shared vocabulary definitions for the neural G2P model (ES),
mirroring neural-en/vocab.py's structure exactly -- see that file's own doc
for the general design. Unlike English, this is NOT tied to any existing
shipped C++ weights/kArpabetTable (there is none for ES yet) -- this
vocabulary is a fresh design, derived from the actual OLaPh ES corpus
(see setup/dictionary-common/olaph_parse.py, packed_symbol_to_text()):
grapheme set = every letter appearing at least 50 times in the corpus's
filtered word set; phoneme set = every phoneme TEXT token (matching
CompressedPhonemeDictionaryWide's own decode format -- digit-suffixed stress,
X-SAMPA-tagged modifiers) appearing at least 50 times. Whichever C++ model
this eventually trains for MUST get its own arpabetForIndex()-equivalent
table matching PHONEME_TABLE below index-for-index -- see neural-en/README.md's
"Another language" section for the general steps.
"""

PAD, UNK, BOS, EOS = 0, 1, 2, 3

GRAPHEMES = ["<pad>", "<unk>", "</s>"] + list("abcdefghijklmnopqrstuvwxyz") + ['á', 'í', 'é', 'ó', 'ñ', 'ú', 'ü']
assert len(GRAPHEMES) == 36

_GRAPHEME_INDEX = {c: i for i, c in enumerate(GRAPHEMES) if i >= 3}


def grapheme_index(c: str) -> int:
    return _GRAPHEME_INDEX.get(c, UNK)


# Every phoneme TEXT token (see olaph_parse.packed_symbol_to_text()) that
# appears at least 50 times in the parsed OLaPh ES corpus --
# 61 symbols. Regenerate by rescanning the corpus if it changes
# significantly (see setup/dictionary-common/olaph_parse.py).
PHONEME_TABLE = [
    None, None, None, None,
    "AF", "AF1", "B", "B1", "BETA", "BETA1", "CH", "D",
    "D1", "DH", "DH1", "EP", "EP1", "F", "F1", "G",
    "G1", "GH", "GH1", "IY", "IY1", "JZ", "JZ1", "K",
    "K1", "L", "L1", "LY", "LY1", "M", "M1", "N",
    "N1", "NG", "NG1", "NY", "NY1", "OP", "OP1", "P",
    "P1", "RR", "RR1", "RT", "RT1", "S", "S1", "SH",
    "T", "T1", "TH", "TH1", "UW", "UW1", "W", "X",
    "X1", "Y", "Y1", "Z", "Z1",
]
assert len(PHONEME_TABLE) == 65


def build_index():
    return {sym: i for i, sym in enumerate(PHONEME_TABLE) if sym is not None}


PHONEME_TO_INDEX = build_index()

NUM_GRAPHEMES = len(GRAPHEMES)
NUM_PHONEMES = len(PHONEME_TABLE)  # also num_dec_symbols: decoder input/output share this vocab
