"""Parse an OLaPh word<TAB>/ipa/ dictionary (olaph_de.txt/olaph_fr.txt/
olaph_es.txt, see olaph_ipa_map.py) into (word, [Phone-name tokens]) entries,
packed the same way CompactPhonemeDictionaryBuilder.h's Seg() packs a
segment: uint16_t = (id << 2) | stress, with a non-stress modifier OR'd in
at bit 9. Skips (with a reason) anything this simple, line-based parser
can't confidently handle: multi-word phrases, words with digits/unusual
punctuation, and any IPA symbol not in the mapping table.

Usage (stats only, no output files -- see main(); run from
setup/dictionary-common/):
    python3 olaph_parse.py ../dictionary-de/olaph_de.txt --lang de --stats-only
"""

import argparse
import collections
import sys

from olaph_ipa_map import (
    BASE_SYMBOL_TO_PHONE, AFFRICATES, DIPHTHONGS, NASAL_VOWELS,
    COMBINING_LONG, STRESS_PRIMARY, STRESS_SECONDARY,
    COMBINING_NONSYLLABIC, COMBINING_NONSYLLABIC_ALT,
    COMBINING_SYLLABIC_BELOW, COMBINING_SYLLABIC_ABOVE,
    COMBINING_TIE, COMBINING_VOICELESS, COMBINING_NASAL, IGNORED_COMBINING,
    COMBINING_ASPIRATED, COMBINING_PALATALIZED, COMBINING_UNRELEASED,
    COMBINING_HALF_LONG, ALT_GLOTTAL_STOP,
)

# Must match Phone enum ids in src/TinyTTSTools/Basic/Phonemes.h exactly.
PHONE_NAME_TO_ID = {
    "SIL": 0, "SP": 1, "AA": 2, "AE": 3, "AH": 4, "AH0": 5, "AO": 6, "AW": 7,
    "AY": 8, "EH": 9, "ER": 10, "ER0": 11, "EY": 12, "IH": 13, "IY": 14,
    "OW": 15, "OY": 16, "UH": 17, "UW": 18, "B": 19, "D": 20, "G": 21,
    "K": 22, "P": 23, "T": 24, "DH": 25, "F": 26, "HH": 27, "S": 28,
    "SH": 29, "TH": 30, "V": 31, "Z": 32, "ZH": 33, "CH": 34, "JH": 35,
    "L": 36, "R": 37, "W": 38, "Y": 39, "M": 40, "N": 41, "NG": 42,
    "UF": 43, "UF0": 44, "OF": 45, "OE": 46, "AN": 47, "EN": 48, "ON": 49,
    "UN": 50, "C": 51, "X": 52, "TS": 53, "PF": 54, "RT": 55, "RR": 56,
    "NY": 57, "LY": 58, "PHI": 59, "BETA": 60, "BR": 61, "MV": 62, "VV": 63,
    "LH": 64, "LZ": 65, "LF": 66, "TR": 67, "DR": 68, "NR": 69, "SR": 70,
    "ZR": 71, "RA": 72, "LR": 73, "RF": 74, "CJ": 75, "JJ": 76, "JZ": 77,
    "GH": 78, "WV": 79, "LL": 80, "QQ": 81, "GU": 82, "NU": 83, "CU": 84,
    "RU": 85, "RT2": 86, "HP": 87, "AP": 88, "GS": 89, "HV": 90, "WH": 91,
    "HU": 92, "IB": 108, "UB": 109, "UM": 110, "EP": 111, "OP": 112,
    "EB": 113, "OB": 114, "OM": 115, "EC": 116, "AC": 117, "AF": 118,
    "OER": 119, "OB2": 120,
}

MOD_NONE = 0
MOD_STRESS_PRIMARY = 1
MOD_STRESS_SECONDARY = 2
MOD_LONG = 3
MOD_HALF_LONG = 4
MOD_NASALIZED = 5
MOD_PALATALIZED = 6
MOD_ASPIRATED = 10
MOD_UNRELEASED = 11
MOD_DEVOICED = 12
MOD_SYLLABIC = 16


def pack_symbol(phone_name, modifier=MOD_NONE):
    pid = PHONE_NAME_TO_ID[phone_name]
    stress = modifier if modifier in (MOD_STRESS_PRIMARY, MOD_STRESS_SECONDARY) else 0
    mod_field = 0 if stress else modifier
    return (pid << 2) | stress | (mod_field << 9)


_ID_TO_NAME = {v: k for k, v in PHONE_NAME_TO_ID.items()}

# Mirrors PhonemeModifiers.h's kModifierTags exactly (tag, ModifierPosition)
# -- only the values pack_symbol() above can ever actually produce need
# entries (this parser never emits MOD_LABIALIZED/VELARIZED/
# PHARYNGEALIZED/VOICED/BREATHY/CREAKY/RHOTACIZED/TONE_*), but the full
# table is kept here so this stays a drop-in match if that ever changes.
_MODIFIER_TAG = {
    MOD_LONG: (":", "suffix"),
    MOD_HALF_LONG: (":\\", "suffix"),
    MOD_NASALIZED: ("~", "suffix"),
    MOD_PALATALIZED: ("_j", "suffix"),
    MOD_ASPIRATED: ("_h", "suffix"),
    MOD_UNRELEASED: ("_}", "suffix"),
    MOD_DEVOICED: ("_0", "suffix"),
    MOD_SYLLABIC: ("=", "suffix"),
    MOD_STRESS_PRIMARY: ("\"", "prefix"),
    MOD_STRESS_SECONDARY: ("%", "prefix"),
}


def packed_symbol_to_text(sym):
    """The exact display string CompressedPhonemeDictionaryWide's
    decodePhonemesAt() (C++) would produce for one packed symbol -- digit
    suffix for stress, else an X-SAMPA modifier tag (see PhonemeModifiers.h
    applyModifierTag()), else the bare Phone name. Used to build the
    neural G2P model's output vocabulary so its predictions read
    identically to a dictionary lookup's tokens."""
    pid = (sym >> 2) & 0x7F
    stress = sym & 0x3
    modifier = (sym >> 9) & 0x1F
    name = _ID_TO_NAME.get(pid, f"id{pid}")
    if stress:
        return name + str(stress)
    if modifier and modifier in _MODIFIER_TAG:
        tag, position = _MODIFIER_TAG[modifier]
        return (tag + name) if position == "prefix" else (name + tag)
    return name


class ParseError(Exception):
    def __init__(self, reason):
        self.reason = reason


# A handful of OLaPh entries use a PRECOMPOSED accented vowel (one
# codepoint, e.g. U+00F5 'õ') where most entries spell the same sound
# decomposed (base vowel + combining diacritic, e.g. 'o' + combining
# tilde U+0303). Substituted explicitly (NOT via a blanket NFD
# normalization -- that also decomposes 'ç' (U+00E7) into 'c' + combining
# cedilla U+0327, a combination this table doesn't separately recognize,
# silently breaking every word containing the very common German ich-Laut).
PRECOMPOSED_SUBSTITUTIONS = {
    "õ": "o" + COMBINING_NASAL,
    "ã": "a" + COMBINING_NASAL,
    "ạ": "a",  # dot-below (creaky/low-tone marking in some source
               # transcriptions) has no modifier equivalent here -- the
               # base vowel quality is kept, the sub-phonemic detail dropped
}


def tokenize_ipa(ipa):
    """Split an IPA string into a flat list of base-symbol/combining-mark
    codepoints (each element is one Python character). Folds a few
    alternate spellings this corpus uses for "the same thing" onto their
    single canonical form so one table entry covers both -- see
    PRECOMPOSED_SUBSTITUTIONS's own doc for why this is NOT a blanket NFD
    normalization.
    """
    for precomposed, decomposed in PRECOMPOSED_SUBSTITUTIONS.items():
        ipa = ipa.replace(precomposed, decomposed)
    ipa = ipa.replace(ALT_GLOTTAL_STOP, "ʔ")
    ipa = ipa.replace(COMBINING_NONSYLLABIC_ALT, COMBINING_NONSYLLABIC)
    return list(ipa)


def parse_ipa(ipa):
    """Returns a list of (Phone name, modifier) segments, or raises
    ParseError with a short machine-readable reason."""
    chars = tokenize_ipa(ipa)
    segments = []
    pending_stress = MOD_NONE
    i = 0
    n = len(chars)

    while i < n:
        c = chars[i]

        if c == STRESS_PRIMARY:
            pending_stress = MOD_STRESS_PRIMARY
            i += 1
            continue
        if c == STRESS_SECONDARY:
            pending_stress = MOD_STRESS_SECONDARY
            i += 1
            continue
        if c in IGNORED_COMBINING:
            i += 1
            continue
        if c == " ":
            raise ParseError("multi_word")

        # Composite lookahead: affricates (3 codepoints), diphthongs (3),
        # nasal vowels (2) -- longest match first.
        matched = False
        for length, table in ((3, AFFRICATES), (3, DIPHTHONGS), (2, NASAL_VOWELS)):
            if i + length <= n:
                key = tuple(chars[i:i + length])
                if key in table:
                    phone = table[key]
                    mod = pending_stress
                    pending_stress = MOD_NONE
                    j = i + length
                    # A following length mark or syllabic mark still applies.
                    if j < n and chars[j] == COMBINING_LONG:
                        mod = MOD_LONG if mod == MOD_NONE else mod
                        j += 1
                    segments.append((phone, mod))
                    i = j
                    matched = True
                    break
        if matched:
            continue

        if c not in BASE_SYMBOL_TO_PHONE:
            raise ParseError(f"unmapped_symbol:{c!r}")

        phone = BASE_SYMBOL_TO_PHONE[c]
        mod = pending_stress
        pending_stress = MOD_NONE
        i += 1

        # Look ahead for diacritics modifying THIS symbol.
        while i < n:
            nc = chars[i]
            if nc == COMBINING_LONG:
                mod = MOD_LONG if mod == MOD_NONE else mod
                i += 1
            elif nc in (COMBINING_SYLLABIC_BELOW, COMBINING_SYLLABIC_ABOVE):
                mod = MOD_SYLLABIC if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_VOICELESS:
                mod = MOD_DEVOICED if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_NASAL:
                mod = MOD_NASALIZED if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_ASPIRATED:
                mod = MOD_ASPIRATED if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_PALATALIZED:
                mod = MOD_PALATALIZED if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_UNRELEASED:
                mod = MOD_UNRELEASED if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_HALF_LONG:
                mod = MOD_HALF_LONG if mod == MOD_NONE else mod
                i += 1
            elif nc == COMBINING_NONSYLLABIC:
                # Offglide not captured by a DIPHTHONGS entry (e.g. an
                # unmapped combination) -- drop the marker, keep the base
                # vowel as a plain segment (approximation).
                i += 1
            elif nc in IGNORED_COMBINING:
                i += 1
            else:
                break

        segments.append((phone, mod))

    return segments


# A rare few entries (278 French, 4 German) carry a 3rd tab-separated
# field instead of the usual word<TAB>/ipa/. French uses it as a
# START/MIDDLE/END position tag when a word's pronunciation depends on
# its position in the sentence (liaison) -- e.g. "que" is /k/ word-
# finally but /kə/ elsewhere; without picking ONE of these as the single
# citation-form pronunciation, iter_parsed_entries's per-word dedup would
# arbitrarily keep whichever line happens to come first in the file and
# most callers would just skip the line as "bad_line_format" entirely,
# silently dropping common words like "que"/"ce". German's 3rd-field
# lines are instead a POS disambiguation tag (NOUN/ADV) on an
# already case-distinct homograph pair (e.g. "Weg"/"weg") -- harmless to
# ignore since case alone already makes them separate dictionary entries.
_POSITION_RANK = {"MIDDLE": 0, "START": 1, "END": 2}


def _pick_preferred_variants(path):
    """word -> ipa for every word that has 2+ same-word 3-field lines,
    picking MIDDLE over START over END (else whichever tag ranks first)."""
    preferred = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 3:
                continue
            word, ipa, tag = parts
            rank = _POSITION_RANK.get(tag, 3)
            if word not in preferred or rank < preferred[word][1]:
                preferred[word] = (ipa, rank)
    return {w: ipa for w, (ipa, _rank) in preferred.items()}


def load_entries(path):
    preferred_variant = _pick_preferred_variants(path)
    with open(path, encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) == 3:
                word, ipa, _tag = parts
                if preferred_variant.get(word) != ipa:
                    continue  # a non-chosen position/POS variant, skip
            elif len(parts) != 2:
                yield lineno, None, None, "bad_line_format"
                continue
            else:
                word, ipa = parts
            ipa = ipa.strip()
            # Some entries (mostly French) give multiple pronunciation
            # variants as "/pron1/, /pron2/, ..." -- take the first,
            # rather than failing on the literal "/" and "," characters
            # left over from naively stripping only the outer slashes.
            ipa = ipa.split(", ")[0]
            ipa = ipa.strip("/")
            yield lineno, word, ipa, None


def iter_parsed_entries(path):
    """Generator of (word, [packed_symbol_uint16, ...]) for every entry in
    `path` this parser can handle -- skips (silently; use main()'s
    --stats-only for a full breakdown) multi-word phrases, words with
    digits, and anything with an unmapped IPA symbol. Deduplicates by
    word, keeping the FIRST occurrence (matches pack_common.py's
    convention for the English pipeline) -- OLaPh's own occasional
    duplicate/near-duplicate lines (e.g. a homograph re-listed) would
    otherwise violate CompressedPhonemeDictionaryWide's sorted-unique-key
    binary search requirement.
    """
    seen = set()
    for lineno, word, ipa, err in load_entries(path):
        if err or not word:
            continue
        if " " in word or any(ch.isdigit() for ch in word):
            continue
        if word in seen:
            continue
        try:
            segments = parse_ipa(ipa)
        except ParseError:
            continue
        if not segments:
            continue
        seen.add(word)
        yield word, [pack_symbol(p, m) for p, m in segments]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("--lang", required=True)
    ap.add_argument("--stats-only", action="store_true")
    ap.add_argument("--out", default=None, help="write word\\tpacked,packed,... dump here")
    ap.add_argument("--max-examples", type=int, default=15)
    args = ap.parse_args()

    total = 0
    ok = 0
    skip_reasons = collections.Counter()
    fail_examples = collections.defaultdict(list)
    total_phonemes = 0
    total_word_bytes = 0
    out_f = open(args.out, "w", encoding="utf-8") if args.out else None

    for lineno, word, ipa, err in load_entries(args.input):
        total += 1
        if err:
            skip_reasons[err] += 1
            continue
        if " " in word or any(ch.isdigit() for ch in word):
            skip_reasons["word_multiword_or_digits"] += 1
            continue
        try:
            segments = parse_ipa(ipa)
        except ParseError as e:
            reason = e.reason.split(":")[0]
            skip_reasons[reason] += 1
            if len(fail_examples[reason]) < args.max_examples:
                fail_examples[reason].append((word, ipa))
            continue
        if not segments:
            skip_reasons["empty_after_parse"] += 1
            continue
        ok += 1
        total_phonemes += len(segments)
        total_word_bytes += len(word.encode("utf-8"))
        if out_f:
            packed = ",".join(str(pack_symbol(p, m)) for p, m in segments)
            out_f.write(f"{word}\t{packed}\n")

    if out_f:
        out_f.close()

    print(f"=== {args.lang} ({args.input}) ===")
    print(f"total lines: {total}")
    print(f"parsed ok:   {ok} ({100.0 * ok / total:.1f}%)")
    print(f"avg phonemes/word: {total_phonemes / ok:.2f}" if ok else "n/a")
    print("skip reasons:")
    for reason, count in skip_reasons.most_common():
        print(f"  {reason:30} {count:8} ({100.0 * count / total:.2f}%)")
        for w, ipa in fail_examples.get(reason, [])[:5]:
            print(f"      e.g. {w!r} /{ipa}/")
    if ok:
        # Rough size projection for a CompactPhonemeDictionary-style
        # uncompressed encoding: 8 bytes/word index overhead + word text +
        # 2 bytes/phoneme.
        projected = ok * 8 + total_word_bytes + total_phonemes * 2
        print(f"projected uncompressed size (CompactPhonemeDictionary-style): "
              f"{projected/1024/1024:.1f} MB")


if __name__ == "__main__":
    main()
