#!/usr/bin/env python3
"""
Export a trained checkpoint (train_g2p_model.py's g2p_model.pt) to the
binary format src/TinyTTSTools/G2P/G2PNeuralModel.h loads, and emit it as
a C++ header (NOT wired into G2PNeuralModel.h yet -- see docs/ADDING_A_LANGUAGE.md).

Binary format (must match G2PNeuralModel.h::parseWeights() exactly):
  int32 hidden_dim, num_graphemes, num_dec_symbols, num_phonemes
  float32 enc_emb[num_graphemes * hidden_dim]
  float32 dec_emb[num_dec_symbols * hidden_dim]
  for each of enc_w_ih, enc_w_hh, dec_w_ih, dec_w_hh (shape [3*hidden_dim, hidden_dim]):
    int8    weight[3*hidden_dim * hidden_dim]   -- row-major, per-row scale below
    float32 row_scale[3*hidden_dim]
  float32 enc_b_ih[3*hidden_dim], float32 enc_b_hh[3*hidden_dim]
  float32 dec_b_ih[3*hidden_dim], float32 dec_b_hh[3*hidden_dim]
  float32 fc_w[num_phonemes * hidden_dim], float32 fc_b[num_phonemes]
  uint8 phoneme_symbol_id[num_phonemes], uint8 phoneme_tone[num_phonemes]
    -- unused by G2PNeuralModel.h (it maps output index -> ARPAbet directly
    via its own kArpabetTable, matching vocab.py's PHONEME_TABLE), written
    as zeros here; present only so the buffer length matches what
    parseWeights() expects to skip.

The four GRU weight matrices (~94% of the model's params) are INT8-
quantized (symmetric, per output row: scale = max(abs(row))/127) --
matching the scheme the sibling TinyTTS project's own
export_dictionary_model.py uses for the same architecture. Validated by
validate_export.py, which re-runs the quantized model in NumPy (mirroring
G2PNeuralModel.h's gruStep()/argmaxLogits() arithmetic exactly) and
compares against the unquantized PyTorch model on a validation sample.
"""
import argparse
import struct
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).parent))
from vocab import NUM_GRAPHEMES, NUM_PHONEMES
from model import G2PModel

HERE = Path(__file__).parent


def quantize_per_row(weight: np.ndarray):
    """weight: [rows, cols] float32. Returns (int8 [rows, cols], float32 row_scale[rows])."""
    max_abs = np.abs(weight).max(axis=1)
    max_abs[max_abs == 0] = 1.0  # avoid div-by-zero for an all-zero row
    scale = max_abs / 127.0
    quantized = np.round(weight / scale[:, None]).clip(-127, 127).astype(np.int8)
    return quantized, scale.astype(np.float32)


def build_binary(state_dict, hidden_dim: int) -> bytes:
    def f32(name):
        return state_dict[name].detach().cpu().numpy().astype(np.float32)

    out = bytearray()
    out += struct.pack("<4i", hidden_dim, NUM_GRAPHEMES, NUM_PHONEMES, NUM_PHONEMES)

    out += f32("enc_emb.weight").tobytes()
    out += f32("dec_emb.weight").tobytes()

    for name in ("encoder.weight_ih_l0", "encoder.weight_hh_l0",
                 "decoder.weight_ih_l0", "decoder.weight_hh_l0"):
        q, scale = quantize_per_row(f32(name))
        out += q.tobytes()
        out += scale.tobytes()

    for name in ("encoder.bias_ih_l0", "encoder.bias_hh_l0",
                 "decoder.bias_ih_l0", "decoder.bias_hh_l0"):
        out += f32(name).tobytes()

    out += f32("fc.weight").tobytes()
    out += f32("fc.bias").tobytes()

    out += bytes(NUM_PHONEMES)  # phoneme_symbol_id (unused, see docstring)
    out += bytes(NUM_PHONEMES)  # phoneme_tone (unused, see docstring)
    return bytes(out)


def escape_c_string(data: bytes) -> str:
    out = []
    for b in data:
        c = chr(b)
        if c == '"' or c == "\\":
            out.append("\\" + c)
        elif 32 <= b < 127:
            out.append(c)
        else:
            out.append(f"\\{b:03o}")
    return "".join(out)


def emit_cpp_header(path: Path, data: bytes):
    with open(path, "w") as f:
        f.write("// Weights for a GRU-based Spanish grapheme-to-phoneme\n"
                "// fallback -- trained by setup/neural-es/train_g2p_model.py,\n"
                "// exported by setup/neural-es/export_g2p_model.py. See\n"
                "// setup/neural-es/README.md (or docs/SETUP.md) for how to retrain.\n"
                "// NOT wired into G2PNeuralModel.h yet -- its PHONEME_TABLE is\n"
                "// English-specific; a Spanish-language model needs its own\n"
                "// arpabetForIndex()-equivalent table matching this model's\n"
                "// vocab.py index-for-index -- see docs/ADDING_A_LANGUAGE.md.\n"
                "//\n"
                f"// Real payload is {len(data)} bytes despite this header's larger\n"
                "// text size (C string literal octal escapes, ~4 bytes of source\n"
                "// per payload byte). OPTIONAL: only pulled in if you explicitly\n"
                "// include this header and wire it up via G2PNeuralModel.\n\n")
        f.write("#pragma once\n#include <cstddef>\n#include <cstdint>\n\n")
        f.write("inline const unsigned char G2P_NEURAL_MODEL_WEIGHTS_ES[] =\n")
        chunk = 4000
        for i in range(0, len(data), chunk):
            f.write(f'"{escape_c_string(data[i:i + chunk])}"\n')
        f.write(";\n")
        f.write(f"inline const size_t G2P_NEURAL_MODEL_WEIGHTS_ES_LEN = {len(data)};\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", type=Path, default=HERE / "g2p_model.pt")
    ap.add_argument("--output-bin", type=Path, default=HERE / "g2p_model.bin")
    ap.add_argument("--output-header", type=Path,
                    default=HERE.parent.parent / "src" / "TinyTTSTools" / "Data" / "neural" / "G2PNeuralWeightsES_data.h")
    args = ap.parse_args()

    if not args.checkpoint.exists():
        print(f"Error: {args.checkpoint} not found -- run train_g2p_model.py first")
        sys.exit(1)

    ckpt = torch.load(args.checkpoint, map_location="cpu", weights_only=True)
    hidden_dim = ckpt["hidden_dim"]
    print(f"Loaded checkpoint: hidden_dim={hidden_dim}, "
          f"val_exact_match={ckpt.get('val_exact_match', float('nan')) * 100:.1f}%")

    data = build_binary(ckpt["state_dict"], hidden_dim)
    print(f"Binary payload: {len(data)} bytes ({len(data) / 1024:.1f} KB)")

    args.output_bin.write_bytes(data)
    print(f"Wrote {args.output_bin}")

    args.output_header.parent.mkdir(parents=True, exist_ok=True)
    emit_cpp_header(args.output_header, data)
    print(f"Wrote {args.output_header}")

    # Also copy the raw binary into data/neural/ -- the runtime-loadable
    # counterpart for G2PNeuralModelSD (SD card/LittleFS, optionally PSRAM
    # on ESP32), same bytes as output_bin, just placed where data/README.md
    # documents loadable data living.
    data_dir = HERE.parent.parent / "data" / "neural"
    data_dir.mkdir(parents=True, exist_ok=True)
    data_out = data_dir / "g2p_model_es.bin"
    data_out.write_bytes(data)
    print(f"Wrote {data_out}")


if __name__ == "__main__":
    main()
