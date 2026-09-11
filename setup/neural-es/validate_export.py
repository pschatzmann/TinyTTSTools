#!/usr/bin/env python3
"""
Re-implements G2PNeuralModel.h's gruStep()/predict() in NumPy against an
exported g2p_model.bin, and compares its predictions to the unquantized
PyTorch checkpoint on the validation set -- catching any quantization or
export-format bug BEFORE touching the C++ side at all (which this NumPy
implementation deliberately mirrors byte-for-byte, so a match here is
strong evidence the real C++ inference will produce the same output too).

Reports three numbers:
  1. NumPy-quantized vs ground-truth val.tsv exact-match accuracy
     (the number that matters -- this is what the exported model will
     actually score once loaded into G2PNeuralModel.h)
  2. PyTorch-unquantized vs ground-truth (the checkpoint's own accuracy,
     for comparison -- quantization should cost very little)
  3. NumPy-quantized vs PyTorch-unquantized agreement rate (how often
     quantization changed the prediction at all, matching the sibling
     TinyTTS project's own "0.96% of predictions differ" sanity check)
"""
import argparse
import struct
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).parent))
from vocab import NUM_GRAPHEMES, NUM_PHONEMES, grapheme_index, BOS, EOS
from model import G2PModel

HERE = Path(__file__).parent


class NumpyG2P:
    """Mirrors G2PNeuralModel.h's parseWeights()/predict() exactly."""

    def __init__(self, buf: bytes):
        pos = 0
        self.hidden_dim, self.num_graphemes, self.num_dec_symbols, self.num_phonemes = \
            struct.unpack_from("<4i", buf, pos)
        pos += 16
        h = self.hidden_dim

        def read_floats(count):
            nonlocal pos
            arr = np.frombuffer(buf, dtype="<f4", count=count, offset=pos)
            pos += count * 4
            return arr

        def read_quantized():
            nonlocal pos
            rows = 3 * h
            data = np.frombuffer(buf, dtype=np.int8, count=rows * h, offset=pos).reshape(rows, h)
            pos += rows * h
            scale = read_floats(rows)
            return data, scale

        self.enc_emb = read_floats(self.num_graphemes * h).reshape(self.num_graphemes, h)
        self.dec_emb = read_floats(self.num_dec_symbols * h).reshape(self.num_dec_symbols, h)
        self.enc_w_ih, self.enc_w_ih_scale = read_quantized()
        self.enc_w_hh, self.enc_w_hh_scale = read_quantized()
        self.dec_w_ih, self.dec_w_ih_scale = read_quantized()
        self.dec_w_hh, self.dec_w_hh_scale = read_quantized()
        self.enc_b_ih = read_floats(3 * h)
        self.enc_b_hh = read_floats(3 * h)
        self.dec_b_ih = read_floats(3 * h)
        self.dec_b_hh = read_floats(3 * h)
        self.fc_w = read_floats(self.num_phonemes * h).reshape(self.num_phonemes, h)
        self.fc_b = read_floats(self.num_phonemes)
        assert pos + 2 * self.num_phonemes == len(buf), "buffer length mismatch"

    def _gru_step(self, x, h, w_ih, w_ih_scale, w_hh, w_hh_scale, b_ih, b_hh):
        hidden = self.hidden_dim
        # dequantized_row[i] = w[i] * scale[i] -- scale the int8 dot product
        # once at the end, exactly like gruStep()'s comment explains.
        rzn_ih = b_ih + (w_ih.astype(np.float32) @ x) * w_ih_scale
        rzn_hh = b_hh + (w_hh.astype(np.float32) @ h) * w_hh_scale
        r = 1.0 / (1.0 + np.exp(-(rzn_ih[:hidden] + rzn_hh[:hidden])))
        z = 1.0 / (1.0 + np.exp(-(rzn_ih[hidden:2 * hidden] + rzn_hh[hidden:2 * hidden])))
        n = np.tanh(rzn_ih[2 * hidden:] + r * rzn_hh[2 * hidden:])
        return (1 - z) * n + z * h

    def predict(self, word: str, max_steps: int = 20):
        h = np.zeros(self.hidden_dim, dtype=np.float32)
        for c in word:
            x = self.enc_emb[grapheme_index(c)]
            h = self._gru_step(x, h, self.enc_w_ih, self.enc_w_ih_scale,
                               self.enc_w_hh, self.enc_w_hh_scale, self.enc_b_ih, self.enc_b_hh)
        h = self._gru_step(self.enc_emb[2], h, self.enc_w_ih, self.enc_w_ih_scale,
                           self.enc_w_hh, self.enc_w_hh_scale, self.enc_b_ih, self.enc_b_hh)

        out = []
        dec_x = self.dec_emb[2]  # <s>
        for _ in range(max_steps):
            h = self._gru_step(dec_x, h, self.dec_w_ih, self.dec_w_ih_scale,
                               self.dec_w_hh, self.dec_w_hh_scale, self.dec_b_ih, self.dec_b_hh)
            logits = self.fc_w @ h + self.fc_b
            idx = int(np.argmax(logits))
            if idx == 3:  # </s>
                break
            out.append(idx)
            dec_x = self.dec_emb[idx]
        return out


def load_val_entries(path: Path):
    entries = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            word, idx_str = line.rstrip("\n").split("\t")
            target = [int(x) for x in idx_str.split()][:-1]  # drop trailing EOS
            entries.append((word, target))
    return entries


@torch.no_grad()
def pytorch_predict(model: G2PModel, word: str, max_len: int = 20):
    enc = [grapheme_index(c) for c in word] + [2]
    enc_input = torch.tensor(enc, dtype=torch.long).unsqueeze(1)  # [seq_len, 1]
    enc_lens = torch.tensor([len(enc)])
    preds = model.greedy_decode(enc_input, enc_lens, max_len=max_len).squeeze(1).tolist()
    if 3 in preds:
        preds = preds[: preds.index(3)]
    return preds


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", type=Path, default=HERE / "g2p_model.bin")
    ap.add_argument("--checkpoint", type=Path, default=HERE / "g2p_model.pt")
    ap.add_argument("--val", type=Path, default=HERE / "val.tsv")
    ap.add_argument("--limit", type=int, default=None, help="only check the first N val entries (faster)")
    args = ap.parse_args()

    for p in (args.bin, args.checkpoint, args.val):
        if not p.exists():
            print(f"Error: {p} not found")
            sys.exit(1)

    numpy_model = NumpyG2P(args.bin.read_bytes())
    print(f"Loaded {args.bin}: hidden_dim={numpy_model.hidden_dim}, "
          f"num_phonemes={numpy_model.num_phonemes}")

    ckpt = torch.load(args.checkpoint, map_location="cpu", weights_only=True)
    torch_model = G2PModel(hidden_dim=ckpt["hidden_dim"])
    torch_model.load_state_dict(ckpt["state_dict"])
    torch_model.eval()

    val_entries = load_val_entries(args.val)
    if args.limit:
        val_entries = val_entries[: args.limit]
    print(f"Checking against {len(val_entries)} validation words...")

    numpy_correct, torch_correct, agree = 0, 0, 0
    for word, target in val_entries:
        numpy_pred = numpy_model.predict(word)
        torch_pred = pytorch_predict(torch_model, word)
        if numpy_pred == target:
            numpy_correct += 1
        if torch_pred == target:
            torch_correct += 1
        if numpy_pred == torch_pred:
            agree += 1

    n = len(val_entries)
    print(f"\nNumPy-quantized (exported model) exact-match: {numpy_correct / n * 100:.1f}%")
    print(f"PyTorch-unquantized (checkpoint) exact-match:  {torch_correct / n * 100:.1f}%")
    print(f"Quantized vs unquantized agreement:            {agree / n * 100:.1f}%")


if __name__ == "__main__":
    main()
