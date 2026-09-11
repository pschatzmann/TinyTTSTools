"""
Shared vocabulary definitions for the neural G2P model (FR),
mirroring neural-en/vocab.py's structure exactly -- see that file's own doc
for the general design. Unlike English, this is NOT tied to any existing
shipped C++ weights/kArpabetTable (there is none for FR yet) -- this
vocabulary is a fresh design, derived from the actual OLaPh FR corpus
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

GRAPHEMES = ["<pad>", "<unk>", "</s>"] + list("abcdefghijklmnopqrstuvwxyz") + ['é', 'â', 'è', 'ç', 'î', 'ê', 'û', 'ô', 'ï', "'", 'œ', 'à']
assert len(GRAPHEMES) == 41

_GRAPHEME_INDEX = {c: i for i, c in enumerate(GRAPHEMES) if i >= 3}


def grapheme_index(c: str) -> int:
    return _GRAPHEME_INDEX.get(c, UNK)


# Every phoneme TEXT token (see olaph_parse.packed_symbol_to_text()) that
# appears at least 50 times in the parsed OLaPh FR corpus --
# 38 symbols. Regenerate by rescanning the corpus if it changes
# significantly (see setup/dictionary-common/olaph_parse.py).
PHONEME_TABLE = [
    None, None, None, None,
    "AA", "AF", "AH0", "AN", "AO", "B", "D", "EH",
    "EN", "EP", "F", "G", "HU", "IY", "K", "L",
    "M", "N", "NG", "NY", "OE", "OF", "ON", "OP",
    "P", "RU", "RU:", "S", "SH", "T", "UF", "UN",
    "UW", "V", "W", "Y", "Z", "ZH",
]
assert len(PHONEME_TABLE) == 42


def build_index():
    return {sym: i for i, sym in enumerate(PHONEME_TABLE) if sym is not None}


PHONEME_TO_INDEX = build_index()

NUM_GRAPHEMES = len(GRAPHEMES)
NUM_PHONEMES = len(PHONEME_TABLE)  # also num_dec_symbols: decoder input/output share this vocab
