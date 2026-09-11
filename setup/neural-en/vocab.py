"""
Shared vocabulary definitions for the neural G2P model, matching
src/TinyTTSTools/G2P/G2PNeuralModel.h EXACTLY -- every index here must
agree with that file's graphemeIndex() and arpabetForIndex() or a trained
model's weights won't decode to the right phonemes when loaded there.

Input (grapheme) vocabulary: 29 symbols, fixed order.
Output (phoneme) vocabulary: 74 symbols. Indices 0-3 are special
(pad/unk/<s>/</s>); 4-73 are ARPAbet phonemes (including the stress-digit
and reduced-vowel (AH0/ER0) variants TinyTTSTools's Phone enum uses).
"""

PAD, UNK, BOS, EOS = 0, 1, 2, 3

GRAPHEMES = ["<pad>", "<unk>", "</s>"] + list("abcdefghijklmnopqrstuvwxyz")
assert len(GRAPHEMES) == 29


def grapheme_index(c: str) -> int:
    if "a" <= c <= "z":
        return 3 + (ord(c) - ord("a"))
    return UNK


# Output phoneme table -- MUST match G2PNeuralModel.h's kArpabetTable
# exactly, index for index (indices 0-3 are the special pad/unk/<s>/</s>
# classes, never a real training target other than EOS at sequence end).
PHONEME_TABLE = [
    None, None, None, None,
    "AA", "AA1", "AA2", "AE", "AE1", "AE2", "AH0", "AH1", "AH2",
    "AO", "AO1", "AO2", "AW", "AW1", "AW2", "AY", "AY1", "AY2",
    "B", "CH", "D", "DH",
    "EH", "EH1", "EH2", "ER0", "ER1", "ER2", "EY", "EY1", "EY2",
    "F", "G", "HH",
    "IH", "IH1", "IH2", "IY", "IY1", "IY2",
    "JH", "K", "L", "M", "N", "NG",
    "OW", "OW1", "OW2", "OY", "OY1", "OY2",
    "P", "R", "S", "SH", "T", "TH",
    "UH", "UH1", "UH2", "UW", "UW", "UW1", "UW2",
    "V", "W", "Y", "Z", "ZH",
]
assert len(PHONEME_TABLE) == 74

# NOTE: "UW" deliberately appears twice (indices 65 and 66) -- this
# mirrors an artifact of the originally-ported model's own training data
# (a rare "uw" - with-no-stress-digit case), kept here so this table stays
# index-for-index IDENTICAL to G2PNeuralModel.h's kArpabetTable, letting a
# freshly-trained model be a drop-in replacement for the shipped weights
# with zero C++ changes. build_index() below always resolves "UW" to the
# LATER index (66); index 65 simply never gets used as a training target,
# which is harmless (the model just never learns to predict it).
def build_index():
    return {sym: i for i, sym in enumerate(PHONEME_TABLE) if sym is not None}


PHONEME_TO_INDEX = build_index()

NUM_GRAPHEMES = len(GRAPHEMES)
NUM_PHONEMES = len(PHONEME_TABLE)  # also num_dec_symbols: decoder input/output share this vocab
