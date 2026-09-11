# Neural G2P Training Pipeline

Trains the GRU-based neural grapheme-to-phoneme model
(`src/TinyTTSTools/G2P/G2PNeuralModel.h`) from scratch and exports it to
the binary format that file loads. The architecture and I/O vocabulary
here are chosen to be index-for-index compatible with the currently-shipped
weights (`Data/neural/G2PNeuralWeightsEN_data.h`, ported from the sibling
TinyTTS project) -- a freshly-trained model is a drop-in replacement, no
C++ changes needed.

## Requirements

```bash
pip install torch numpy
```

CPU-only training works fine (no CUDA required) -- a full run on the
~112k-word English training set takes on the order of tens of minutes on
a modern CPU.

## Pipeline

```bash
# 1. Build train/val splits from the CMU dictionary dump (regenerate
#    ../dictionary-en/cmudict_dump.txt first if it's stale -- see
#    ../dictionary-en/README.md).
python3 prepare_data.py

# 2. Train (saves the best checkpoint by validation exact-match to
#    g2p_model.pt as training progresses).
python3 train_g2p_model.py

# 3. Validate the exported (quantized) model against the checkpoint
#    BEFORE touching any C++ -- catches quantization/export bugs early.
python3 export_g2p_model.py
python3 validate_export.py

# 4. If validate_export.py's numbers look right, the header is already
#    written (export_g2p_model.py's default --output-header points
#    straight at Data/neural/G2PNeuralWeightsEN_data.h; it also copies the
#    same binary to data/neural/g2p_model_en.bin for G2PNeuralModelSD --
#    see data/README.md). Rebuild and run tests/test_g2p_neural.cpp to
#    confirm the real C++ inference agrees.
```

## Files

- `vocab.py` -- input (29-grapheme) and output (74-phoneme) vocabularies,
  index-for-index identical to `G2PNeuralModel.h`'s `graphemeIndex()`/
  `kArpabetTable`. Anything touching indices should import from here, not
  redefine them, to avoid the two sides drifting apart.
- `prepare_data.py` -- filters `../dictionary-en/cmudict_dump.txt` to
  alphabetic words and maps each phoneme (with stress digit) to the target
  vocabulary, writing `train.tsv`/`val.tsv`.
- `model.py` -- the `G2PModel` architecture: single-layer `nn.GRU` encoder
  and decoder (hidden_dim=256 by default), matching `gruStep()`'s gate
  order and the reference's "no bridge layer, decoder hidden state
  initialized directly from the encoder's final hidden state" wiring
  exactly.
- `train_g2p_model.py` -- the training loop (teacher forcing,
  cross-entropy loss, gradient clipping); reports validation exact-match
  accuracy every epoch and keeps the best checkpoint.
- `export_g2p_model.py` -- quantizes the four GRU weight matrices to INT8
  (symmetric, per output row -- same scheme as the sibling TinyTTS
  project's `export_dictionary_model.py`), writes the raw binary
  (`g2p_model.bin`), the C++ header, and a copy of the same binary to
  `data/neural/g2p_model_en.bin` (for `G2PNeuralModelSD`, see
  `data/README.md`).
- `validate_export.py` -- a NumPy reimplementation of
  `G2PNeuralModel.h`'s exact inference arithmetic (dequantize-on-the-fly
  GRU steps, greedy decode), so the export can be checked against the
  training checkpoint without needing to compile/run any C++.

## Training your own model (English or another language)

- **Improve English accuracy**: just rerun the pipeline as-is with more
  epochs, a different `--hidden-dim`, or after fixing/extending
  `cmudict_dump.txt`.
- **Another language**: German/French/Spanish already have this
  scaffolded -- `../neural-de/`, `../neural-fr/`, `../neural-es/`, built
  from the OLaPh corpus (`../dictionary-de/fr/es/`) instead of CMU dict.
  `../neural-fr/` (94.8% validation exact-match), `../neural-es/` (98.7%)
  and `../neural-de/` (71.2%) are all already trained. See
  [`../../docs/ADDING_A_LANGUAGE.md`](../../docs/ADDING_A_LANGUAGE.md)
  (Step 7) for the full walkthrough if adding a fifth language, or
  wiring one of these three into `G2PNeuralModel.h`: in short, `model.py`/`train_g2p_model.py`/
  `export_g2p_model.py`/`validate_export.py` are fully generic (copy them
  verbatim), only `vocab.py` (grapheme/phoneme sets, generated from the
  actual corpus, not hand-picked) and `prepare_data.py` need to be
  language-specific, and:
  5. **Update `G2PNeuralModel.h`'s `arpabetForIndex()`** (or write a
     language-appropriate equivalent) to match the new `PHONEME_TABLE`
     -- that table is fixed metadata of a *specific* trained model, not
     something the architecture infers automatically. This step is what
     `../neural-fr/`'s already-trained model is still waiting on.
