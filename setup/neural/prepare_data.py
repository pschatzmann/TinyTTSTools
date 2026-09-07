#!/usr/bin/env python3
"""
Build train/val splits for the neural G2P model from
setup/dictionary/cmudict_dump.txt (word<TAB>PHONEMES, stress digits kept --
the same dump setup/dictionary/'s pipeline uses, regenerate with
`setup/dictionary/dump_cmudict.cpp` if it's stale).

Filters to words spelled with only a-z (matching G2PNeuralModel.h's
graphemeIndex(), which treats anything else as <unk> -- punctuation/digits
in a word would only teach the model to associate meaningless <unk>
padding with real phonemes) and phoneme sequences using only symbols
vocab.py's PHONEME_TABLE covers (all standard CMU ARPAbet phones do; this
is just a safety net).

Output: train.tsv / val.tsv, each line "word<TAB>space-separated target
indices" (already resolved via vocab.py, so train_g2p_model.py doesn't
need to know about ARPAbet strings at all).
"""
import argparse
import random
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from vocab import PHONEME_TO_INDEX, EOS

HERE = Path(__file__).parent
DEFAULT_INPUT = HERE.parent / "dictionary" / "cmudict_dump.txt"

WORD_RE = re.compile(r"^[a-z]+$")


def phoneme_to_index(phone: str):
    """CMU phone with an optional trailing stress digit -> our target index."""
    if phone and phone[-1].isdigit():
        base, digit = phone[:-1], phone[-1]
    else:
        base, digit = phone, None

    if base in ("AH", "ER") and digit == "0":
        # Reduced-vowel phonemes have their own dedicated symbol (AH0/ER0),
        # not a stress-digit suffix on the base vowel.
        sym = base + "0"
    elif digit in ("1", "2"):
        sym = base + digit
    else:
        sym = base  # consonants (no digit), or digit "0" on any other vowel
    return PHONEME_TO_INDEX.get(sym)


def load_entries(input_path: Path):
    entries = []
    skipped_word, skipped_phone = 0, 0
    with open(input_path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or "\t" not in line:
                continue
            word, phones = line.split("\t", 1)
            word = word.lower()
            if not WORD_RE.match(word):
                skipped_word += 1
                continue
            indices = [phoneme_to_index(p) for p in phones.split()]
            if any(i is None for i in indices):
                skipped_phone += 1
                continue
            entries.append((word, indices + [EOS]))
    print(f"Loaded {len(entries)} usable entries "
          f"(skipped {skipped_word} non-alphabetic words, "
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
        print(f"Error: {args.input} not found. Regenerate it first:")
        print("  cd ../dictionary && g++ -std=c++17 -O2 -I ../../src dump_cmudict.cpp -o /tmp/dump_cmudict")
        print("  /tmp/dump_cmudict > cmudict_dump.txt")
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
