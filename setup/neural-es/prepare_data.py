#!/usr/bin/env python3
"""
Build train/val splits for the neural G2P model from
setup/dictionary-es/olaph_es.txt -- the same OLaPh corpus + parser
(setup/dictionary-common/olaph_parse.py) the compressed dictionary
(CompactOlaphES_data.h) is built from, so this model's training data
and the dictionary it's a fallback for cover the exact same word set with
the exact same phoneme transcriptions.

Output: train.tsv / val.tsv, each line "word<TAB>space-separated target
indices" (already resolved via vocab.py, so train_g2p_model.py doesn't
need to know about phoneme strings at all) -- same format as neural-en/'s.
"""
import argparse
import os
import random
import sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "dictionary-common"))

from vocab import PHONEME_TO_INDEX, EOS, UNK, grapheme_index
from olaph_parse import iter_parsed_entries, packed_symbol_to_text

DEFAULT_INPUT = HERE.parent / "dictionary-es" / "olaph_es.txt"


def load_entries(input_path: Path):
    entries = []
    skipped_word, skipped_phone = 0, 0
    total = 0
    for word, symbols in iter_parsed_entries(str(input_path)):
        total += 1
        if any(grapheme_index(ch) == UNK for ch in word.lower()):
            skipped_word += 1
            continue
        indices = [PHONEME_TO_INDEX.get(packed_symbol_to_text(s)) for s in symbols]
        if any(i is None for i in indices):
            skipped_phone += 1
            continue
        entries.append((word.lower(), indices + [EOS]))
    print(f"Loaded {len(entries)} usable entries out of {total} parsed "
          f"(skipped {skipped_word} with an out-of-vocabulary grapheme, "
          f"{skipped_phone} with an unmapped phoneme)")
    return entries


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    ap.add_argument("--output-dir", type=Path, default=HERE)
    ap.add_argument("--val-fraction", type=float, default=0.03)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    if not args.input.exists():
        print(f"Error: {args.input} not found.")
        sys.exit(1)

    entries = load_entries(args.input)
    rng = random.Random(args.seed)
    rng.shuffle(entries)

    n_val = max(1, int(len(entries) * args.val_fraction))
    val, train = entries[:n_val], entries[n_val:]

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, split in (("train.tsv", train), ("val.tsv", val)):
        with open(args.output_dir / name, "w", encoding="utf-8") as f:
            for word, indices in split:
                f.write(f"{word}\t{' '.join(map(str, indices))}\n")
        print(f"Wrote {len(split)} entries to {args.output_dir / name}")


if __name__ == "__main__":
    main()
