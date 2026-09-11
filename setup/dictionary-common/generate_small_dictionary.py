"""Generates a small, readable PH_WORD()-source built-in phoneme dictionary
(src/TinyTTSTools/PhonemeDictionary/PhonemeDictionary{DE,FR,ES}.h) from real
OLaPh transcriptions for a language's N most frequent words, instead of
hand-transcribing them. Replaces the previous ~58-59-word HAND-TRANSCRIBED
starter set with corpus-backed pronunciations for the words that actually
matter most in running text.

Word frequency ranking comes from a hermitdave/FrequencyWords
(https://github.com/hermitdave/FrequencyWords) `{xx}_full.txt` file (one
"word count" pair per line, already sorted by frequency descending) --
download `content/2016/{xx}/{xx}_full.txt` and pass its path via
--freq-file. Matching against OLaPh is case-insensitive (FrequencyWords is
lowercase; OLaPh capitalizes German nouns per normal orthography) --
falls back to the first case variant found in the corpus. Not every
frequency-ranked word has an OLaPh entry (typically <5%, e.g. FrequencyWords
includes OCR noise/foreign words OLaPh doesn't cover) -- those are skipped
and reported, not approximated.

The packed uint16_t symbol format olaph_parse.iter_parsed_entries() already
produces (id << 2 | stress, non-stress modifier OR'd in at bit 9) is
IDENTICAL to CompactPhonemeDictionaryBuilder.h's Seg()/cxPackPhone() packing
-- see olaph_parse.py's own module doc -- so this script only needs to
decode each packed symbol back to a readable `Phone::X` / `Seg(Phone::X,
PhonemeModifier::Y)` source token, not re-derive anything.

Usage (run from setup/dictionary-common/):
    python3 generate_small_dictionary.py --lang de --freq-file /path/to/de_full.txt --top-n 1000
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

from olaph_build_huffman import corpus_path
from olaph_parse import iter_parsed_entries, PHONE_NAME_TO_ID

OUT_DIR = os.path.join(HERE, "..", "..", "src", "TinyTTSTools", "PhonemeDictionary")

_ID_TO_NAME = {v: k for k, v in PHONE_NAME_TO_ID.items()}

# Only the values pack_symbol() in olaph_parse.py can ever actually
# produce need entries here (mirrors olaph_parse.py's own _MODIFIER_TAG
# comment).
_MOD_NAME = {
    3: "MOD_LONG",
    4: "MOD_HALF_LONG",
    5: "MOD_NASALIZED",
    6: "MOD_PALATALIZED",
    10: "MOD_ASPIRATED",
    11: "MOD_UNRELEASED",
    12: "MOD_DEVOICED",
    16: "MOD_SYLLABIC",
}


def symbol_to_cpp(sym):
    pid = (sym >> 2) & 0x7F
    stress = sym & 0x3
    modifier = (sym >> 9) & 0x1F
    name = _ID_TO_NAME[pid]
    if stress == 1:
        return f"Seg(Phone::{name}, PhonemeModifier::MOD_STRESS_PRIMARY)"
    if stress == 2:
        return f"Seg(Phone::{name}, PhonemeModifier::MOD_STRESS_SECONDARY)"
    if modifier:
        return f"Seg(Phone::{name}, PhonemeModifier::{_MOD_NAME[modifier]})"
    return f"Phone::{name}"


def escape_c_string(word):
    return word.replace("\\", "\\\\").replace('"', '\\"')


HEADER_TEMPLATE = '''/**
 * @file PhonemeDictionary{LANG_UPPER}.h
 * @brief Human-readable source for a small built-in {LANG_NAME} phoneme dictionary
 * @author Phil Schatzmann
 * @version 2.0.0
 * @date {DATE}
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include "CompactPhonemeDictionaryBuilder.h"

/**
 * @brief Built-in {LANG_NAME} phoneme dictionary ({COUNT} most frequent words)
 * @details Plain, readable `PH_WORD("word", Phone::X, Phone::Y, ...)` source
 * table -- packed into CompactPhonemeDictionary's compact binary arrays at
 * COMPILE time (see CompactPhonemeDictionaryBuilder.h), same mechanism as
 * PhonemeDictionaryEN.h.
 *
 * @note UNLIKE the original hand-transcribed ~58-word starter set this
 * replaces, every entry here is GENERATED from the real OLaPh pronunciation
 * corpus (see setup/dictionary-common/), for the {COUNT} most frequent
 * {LANG_NAME} words per hermitdave/FrequencyWords
 * (https://github.com/hermitdave/FrequencyWords) -- run
 * `setup/dictionary-common/generate_small_dictionary.py --lang {LANG_LOWER}`
 * to regenerate. Same corpus/transcription conventions as the full
 * `COMPACT_OLAPH_{LANG_UPPER}` dictionary (`CompactOlaph{LANG_UPPER}_data.h`),
 * just re-encoded into `Phone`/`PhonemeModifier` source form instead of the
 * packed binary format -- see that file's own doc for corpus caveats
 * (third-party transcription conventions that may not match what you'd
 * choose by hand).
 * @note Entries must stay sorted (by raw UTF-8 byte value) for binary
 * search; see CompactPhonemeDictionaryBuilder.h. Regenerating via the
 * script above keeps this invariant automatically.
 */
static constexpr PhonemeWordSource PHONEME_DICTIONARY_{LANG_UPPER}[] = {{
{ENTRIES}}};

TTS_COMPACT_DICTIONARY(COMPACT_PHONEME_DICTIONARY_{LANG_UPPER}, PHONEME_DICTIONARY_{LANG_UPPER});
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", required=True)
    ap.add_argument("--freq-file", required=True,
                     help="path to a hermitdave/FrequencyWords {xx}_full.txt")
    ap.add_argument("--top-n", type=int, default=1000)
    args = ap.parse_args()

    lang = args.lang
    lang_upper = lang.upper()

    with open(args.freq_file, encoding="utf-8") as f:
        top_words = [line.split()[0] for line in f.read().splitlines()[:args.top_n] if line.strip()]

    entries = dict(iter_parsed_entries(corpus_path(lang)))
    lower_index = {}
    for w in entries:
        lower_index.setdefault(w.lower(), w)

    chosen = {}
    missing = []
    for fw in top_words:
        if fw in entries:
            chosen[fw] = entries[fw]
        elif fw.lower() in lower_index:
            actual = lower_index[fw.lower()]
            chosen[actual] = entries[actual]
        else:
            missing.append(fw)

    print(f"{lang}: matched {len(chosen)}/{len(top_words)} of top {args.top_n} "
          f"frequency-ranked words ({len(missing)} missing, e.g. {missing[:10]})")

    words_sorted = sorted(chosen.keys(), key=lambda w: w.encode("utf-8"))

    lines = []
    for w in words_sorted:
        args_str = ", ".join(symbol_to_cpp(s) for s in chosen[w])
        lines.append(f'    PH_WORD("{escape_c_string(w)}", {args_str}),\n')

    lang_names = {"de": "German", "fr": "French", "es": "Spanish"}
    out = HEADER_TEMPLATE.format(
        LANG_UPPER=lang_upper, LANG_LOWER=lang, LANG_NAME=lang_names.get(lang, lang_upper),
        COUNT=len(words_sorted), DATE="2026-09-10", ENTRIES="".join(lines),
    )

    out_path = os.path.join(OUT_DIR, f"PhonemeDictionary{lang_upper}.h")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(out)
    print(f"Wrote {out_path}: {len(words_sorted)} words")


if __name__ == "__main__":
    main()
