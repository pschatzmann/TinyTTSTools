"""
Shared vocabulary definitions for the neural G2P model (DE),
mirroring neural-en/vocab.py's structure exactly -- see that file's own doc
for the general design. Unlike English, this is NOT tied to any existing
shipped C++ weights/kArpabetTable (there is none for DE yet) -- this
vocabulary is a fresh design, derived from the actual OLaPh DE corpus
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

GRAPHEMES = ["<pad>", "<unk>", "</s>"] + list("abcdefghijklmnopqrstuvwxyz") + ['ä', 'ü', 'ö', 'ß', '-']
assert len(GRAPHEMES) == 34

_GRAPHEME_INDEX = {c: i for i, c in enumerate(GRAPHEMES) if i >= 3}


def grapheme_index(c: str) -> int:
    return _GRAPHEME_INDEX.get(c, UNK)


# Every phoneme TEXT token (see olaph_parse.packed_symbol_to_text()) that
# appears at least 50 times in the parsed OLaPh DE corpus --
# 155 symbols. Regenerate by rescanning the corpus if it changes
# significantly (see setup/dictionary-common/olaph_parse.py).
PHONEME_TABLE = [
    None, None, None, None,
    "AA1", "AA2", "AA:", "AC", "AE", "AF", "AF1", "AF2",
    "AF:", "AH0", "AN", "AN:", "AO", "AO1", "AO2", "AO:",
    "AW", "AW1", "AW2", "AY", "AY1", "AY2", "B", "B1",
    "B2", "C", "C1", "C2", "CH", "CH1", "CH2", "D",
    "D1", "D2", "EC", "EH", "EH1", "EH2", "EH:", "EN",
    "EN:", "EP", "EP1", "EP2", "EP:", "F", "F1", "F2",
    "G", "G1", "G2", "GS", "GS1", "GS2", "HH", "HH1",
    "HH2", "IH", "IH1", "IH2", "IY", "IY1", "IY2", "IY:",
    "JH", "JH1", "JH2", "K", "K1", "K2", "L", "L1",
    "L2", "L=", "M", "M1", "M2", "M=", "N", "N1",
    "N2", "N=", "NG", "NG=", "OE", "OE1", "OE2", "OE:",
    "OF", "OF1", "OF2", "OF:", "ON", "ON:", "OP", "OP1",
    "OP2", "OP:", "OP~", "P", "P1", "P2", "PF", "PF1",
    "PF2", "R", "RR", "RT", "RU", "RU1", "RU2", "S",
    "S1", "S2", "SH", "SH1", "SH2", "T", "T1", "T2",
    "TH", "TS", "TS1", "TS2", "UF", "UF0", "UF01", "UF02",
    "UF1", "UF2", "UF:", "UH", "UH1", "UH2", "UW", "UW1",
    "UW2", "UW:", "V", "V1", "V2", "W", "W1", "X",
    "X1", "Y", "Y1", "Y2", "Z", "Z1", "Z2", "ZH",
    "ZH1", "ZH2", "Z_0",
]
assert len(PHONEME_TABLE) == 159


def build_index():
    return {sym: i for i, sym in enumerate(PHONEME_TABLE) if sym is not None}


PHONEME_TO_INDEX = build_index()

NUM_GRAPHEMES = len(GRAPHEMES)
NUM_PHONEMES = len(PHONEME_TABLE)  # also num_dec_symbols: decoder input/output share this vocab
