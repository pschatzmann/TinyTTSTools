#!/usr/bin/env python3
"""
Train the neural G2P model on train.tsv/val.tsv (see prepare_data.py).

Usage:
    python3 prepare_data.py          # once, or after cmudict_dump.txt changes
    python3 train_g2p_model.py       # trains, saves best checkpoint to g2p_model.pt
"""
import argparse
import random
import sys
import time
from pathlib import Path

import torch
import torch.nn as nn

sys.path.insert(0, str(Path(__file__).parent))
from vocab import grapheme_index, BOS, PAD
from model import G2PModel

HERE = Path(__file__).parent


def load_split(path: Path):
    entries = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            word, idx_str = line.rstrip("\n").split("\t")
            target = [int(x) for x in idx_str.split()]
            enc = [grapheme_index(c) for c in word] + [2]  # + encoder's </s>
            entries.append((word, enc, target))
    return entries


def make_batch(entries, device):
    """entries: list of (word, enc_indices, target_indices). Pads to the
    batch's max length; returns tensors shaped [seq_len, batch]."""
    enc_lens = torch.tensor([len(e[1]) for e in entries])
    tgt_lens = torch.tensor([len(e[2]) for e in entries])
    max_enc, max_tgt = enc_lens.max().item(), tgt_lens.max().item()

    enc_input = torch.full((max_enc, len(entries)), PAD, dtype=torch.long)
    dec_input = torch.full((max_tgt, len(entries)), PAD, dtype=torch.long)
    target = torch.full((max_tgt, len(entries)), PAD, dtype=torch.long)

    for i, (_, enc, tgt) in enumerate(entries):
        enc_input[: len(enc), i] = torch.tensor(enc, dtype=torch.long)
        dec_input[0, i] = BOS
        if len(tgt) > 1:
            dec_input[1: len(tgt), i] = torch.tensor(tgt[:-1], dtype=torch.long)
        target[: len(tgt), i] = torch.tensor(tgt, dtype=torch.long)

    return (enc_input.to(device), enc_lens.to(device),
            dec_input.to(device), target.to(device))


@torch.no_grad()
def evaluate(model, val_entries, device, batch_size, max_len=24):
    model.eval()
    correct, total = 0, 0
    for start in range(0, len(val_entries), batch_size):
        batch = val_entries[start:start + batch_size]
        enc_input, enc_lens, _, target = make_batch(batch, device)
        preds = model.greedy_decode(enc_input, enc_lens, max_len=max_len)
        preds = preds.transpose(0, 1).tolist()  # [batch, max_len]
        tgt_lens = (target != PAD).sum(dim=0).tolist()
        target_t = target.transpose(0, 1).tolist()  # [batch, seq_len]
        for i in range(len(batch)):
            tgt_seq = target_t[i][: tgt_lens[i] - 1]  # exclude trailing EOS
            pred_seq = preds[i]
            if 3 in pred_seq:  # EOS
                pred_seq = pred_seq[: pred_seq.index(3)]
            # else: ran out of max_len steps without emitting EOS -- leave
            # pred_seq as the full untruncated run, which just won't match
            total += 1
            if pred_seq == tgt_seq:
                correct += 1
    model.train()
    return correct / total if total else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", type=Path, default=HERE / "train.tsv")
    ap.add_argument("--val", type=Path, default=HERE / "val.tsv")
    ap.add_argument("--hidden-dim", type=int, default=256)
    ap.add_argument("--batch-size", type=int, default=256)
    ap.add_argument("--epochs", type=int, default=15)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--output", type=Path, default=HERE / "g2p_model.pt")
    args = ap.parse_args()

    if not args.train.exists() or not args.val.exists():
        print(f"Error: {args.train} / {args.val} not found -- run prepare_data.py first")
        sys.exit(1)

    torch.manual_seed(args.seed)
    random.seed(args.seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Device: {device}")

    train_entries = load_split(args.train)
    val_entries = load_split(args.val)
    print(f"Train: {len(train_entries)}, Val: {len(val_entries)}")

    model = G2PModel(hidden_dim=args.hidden_dim).to(device)
    n_params = sum(p.numel() for p in model.parameters())
    print(f"Model params: {n_params:,}")

    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)
    criterion = nn.CrossEntropyLoss(ignore_index=PAD)

    best_acc = -1.0
    for epoch in range(1, args.epochs + 1):
        random.shuffle(train_entries)
        model.train()
        epoch_start = time.time()
        total_loss, n_batches = 0.0, 0

        for start in range(0, len(train_entries), args.batch_size):
            batch = train_entries[start:start + args.batch_size]
            enc_input, enc_lens, dec_input, target = make_batch(batch, device)

            optimizer.zero_grad()
            logits = model(enc_input, enc_lens, dec_input)  # [seq_len, batch, NUM_PHONEMES]
            loss = criterion(logits.reshape(-1, logits.size(-1)), target.reshape(-1))
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 5.0)
            optimizer.step()

            total_loss += loss.item()
            n_batches += 1

        val_acc = evaluate(model, val_entries, device, args.batch_size)
        elapsed = time.time() - epoch_start
        print(f"Epoch {epoch:2d}/{args.epochs}  "
              f"loss={total_loss / n_batches:.4f}  "
              f"val_exact_match={val_acc * 100:.1f}%  "
              f"({elapsed:.0f}s)")

        if val_acc > best_acc:
            best_acc = val_acc
            torch.save({
                "state_dict": model.state_dict(),
                "hidden_dim": args.hidden_dim,
                "val_exact_match": val_acc,
            }, args.output)
            print(f"  -> saved best checkpoint to {args.output} ({val_acc * 100:.1f}%)")

    print(f"\nBest validation exact-match: {best_acc * 100:.1f}%")


if __name__ == "__main__":
    main()
