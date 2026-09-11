# Neural G2P Training Pipeline (French)

Scaffolded from neural-en/'s pipeline (same architecture, same training/
export/validation code, copied verbatim -- model.py, train_g2p_model.py,
export_g2p_model.py, validate_export.py) but with its own French
vocabulary and data source.

**Trained**: `g2p_model.pt` (256 hidden units, 15 epochs) reaches **94.8%
validation exact-match** -- markedly higher than English's ~74%, since
French orthography is far more regular. Exported and validated:
`export_g2p_model.py` + `validate_export.py` confirm the INT8-quantized
export matches the checkpoint exactly (94.8% both) with 99.7% per-word
prediction agreement between quantized and unquantized inference. Weights
are available as `g2p_model.bin`/`G2PNeuralWeightsFR_data.h` (flash) and
`data/neural/g2p_model_fr.bin` (SD/PSRAM, see `data/README.md`).

**Not yet usable from C++**: `G2PNeuralModel.h`'s `arpabetForIndex()`/
output table is English-specific -- loading `g2p_model_fr.bin` through it
today would decode to nonsense. See the last pipeline step below for what
wiring this up would require.

## Requirements

```bash
pip install torch numpy
```

## Data source

Training pairs come from the OLaPh French corpus
(`../dictionary-fr/olaph_fr.txt`), parsed via
`../dictionary-common/olaph_parse.py` -- the SAME corpus and parser the
compressed dictionary (`CompactOlaphFR_data.h`) is built from, so
this model's training data and the dictionary it would be a fallback for
cover the same words with the same transcriptions.

## Vocabulary

`vocab.py` was generated from the actual parsed corpus, not hand-picked:
- **Graphemes** (`NUM_GRAPHEMES`): every letter appearing at least 50
  times in the filtered word set (plain a-z plus the language's own
  accented letters/punctuation that clears that bar).
- **Phonemes** (`NUM_PHONEMES`): every phoneme TEXT token (matching
  `CompressedPhonemeDictionaryWide`'s own decode format -- digit-suffixed
  stress, X-SAMPA-tagged modifiers) appearing at least 50 times.

Words/transcriptions using a grapheme or phoneme below that frequency
threshold are dropped by `prepare_data.py` (reported as "skipped"), not
approximated -- a tiny fraction of the corpus (well under 1%), consistent
with the same long-tail-drop approach used when building the dictionary.

## Pipeline

```bash
# 1. Build train/val splits (train.tsv/val.tsv, both gitignored).
python3 prepare_data.py

# 2. Train (saves the best checkpoint by validation exact-match to
#    g2p_model.pt as training progresses). DONE -- 94.8% val exact-match.
python3 train_g2p_model.py

# 3. Export + validate the quantized model against the checkpoint. DONE --
#    94.8%/94.8%, 99.7% agreement (see above).
python3 export_g2p_model.py
python3 validate_export.py

# 4. STILL TODO: update G2PNeuralModel.h (or a new French-specific
#    equivalent) with a PHONEME_TABLE matching vocab.py's index-for-index
#    -- see ../neural-en/README.md's "Another language" section for the
#    general steps, and docs/ADDING_A_LANGUAGE.md Step 7.
```

## Files

Same roles as neural-en/'s own (see ../neural-en/README.md for the full
architecture description) -- `model.py`, `train_g2p_model.py`,
`export_g2p_model.py`, `validate_export.py` are byte-for-byte copies
(fully language-agnostic, driven entirely by `vocab.py`'s
NUM_GRAPHEMES/NUM_PHONEMES); only `vocab.py` and `prepare_data.py` are
French-specific.
