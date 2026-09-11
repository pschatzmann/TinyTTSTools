"""Shared phoneme-token <-> packed-byte mapping for the dictionary pipeline.

The packed byte format is `(phone_id << 2) | stress`, where phone_id is the
`Phone` enum's plain 0-42 range (see src/TinyTTSTools/Basic/Phonemes.h) and
stress is 0 (none/unstressed), 1 (primary) or 2 (secondary) -- this is the
same encoding CompactPhonemeDictionary and CompressedPhonemeDictionary both
use for phonemeData.
"""

# Must match the `Phone` enum in src/TinyTTSTools/Basic/Phonemes.h exactly.
NAME_TO_ID = {
    "SIL": 0, "SP": 1,
    "AA": 2, "AE": 3, "AH": 4, "AH0": 5, "AO": 6, "AW": 7, "AY": 8, "EH": 9,
    "ER": 10, "ER0": 11, "EY": 12, "IH": 13, "IY": 14, "OW": 15, "OY": 16,
    "UH": 17, "UW": 18,
    "B": 19, "D": 20, "G": 21, "K": 22, "P": 23, "T": 24,
    "DH": 25, "F": 26, "HH": 27, "S": 28, "SH": 29, "TH": 30, "V": 31,
    "Z": 32, "ZH": 33,
    "CH": 34, "JH": 35,
    "L": 36, "R": 37, "W": 38, "Y": 39,
    "M": 40, "N": 41, "NG": 42,
}
ID_TO_NAME = {v: k for k, v in NAME_TO_ID.items()}


def pack_phoneme_token(token):
    """Pack one ARPABET token (e.g. 'AH0', 'AH1', 'IY2', 'T') into a single
    byte. Returns None for an unrecognized token."""
    stress = 0
    base = token
    if token and token[-1] in "12" and token not in ("AH0", "ER0"):
        stress = int(token[-1])
        base = token[:-1]
    pid = NAME_TO_ID.get(base)
    if pid is None:
        return None
    return (pid << 2) | stress


def packed_byte_name(b):
    """Human-readable name for a packed byte, e.g. 17 -> 'AH1'."""
    pid, stress = b >> 2, b & 3
    return ID_TO_NAME.get(pid, f"id{pid}") + (str(stress) if stress else "")


def load_entries(path):
    """Load a tab-separated `word\\tPHONEMES` dump (see dump_cmudict.cpp /
    dump_failures.cpp) into a sorted, deduplicated list of
    (word, [tokens])."""
    entries = []
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            word, phonemes = line.split("\t")
            entries.append((word, phonemes.split()))
    entries.sort(key=lambda e: e[0])
    seen = set()
    deduped = []
    for w, p in entries:
        if w in seen:
            continue
        seen.add(w)
        deduped.append((w, p))
    return deduped


def count_phoneme_frequencies(entry_lists):
    """entry_lists: one or more lists of (word, [tokens]), e.g. from multiple
    load_entries() calls -- returns {packed_byte: count} across all of them."""
    freq = {}
    for entries in entry_lists:
        for _word, tokens in entries:
            for tok in tokens:
                b = pack_phoneme_token(tok)
                if b is None:
                    raise ValueError(f"Unknown phoneme token {tok!r}")
                freq[b] = freq.get(b, 0) + 1
    return freq
