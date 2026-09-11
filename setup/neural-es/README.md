# Neural G2P Training Pipeline (Spanish)

Scaffolded from neural-en/'s pipeline (same architecture, same training/
validation code, copied verbatim -- model.py, train_g2p_model.py,
validate_export.py; export_g2p_model.py copied with its default output
path/symbol names made Spanish-specific, see ../../docs/ADDING_A_LANGUAGE.md)
but with its own Spanish vocabulary and data source.

**Trained**: `g2p_model.pt` (256 hidden units, 15 epochs) reaches **98.7%
validation exact-match** -- the highest of the four languages, consistent
with Spanish's famously regular, near-1:1 letter-to-sound orthography.
Exported and validated: `export_g2p_model.py` + `validate_export.py`
confirm the INT8-quantized export matches the checkpoint exactly (98.7%
both) with 99.9% per-word prediction agreement between quantized and
unquantized inference. Weights are available as
`g2p_model.bin`/`G2PNeuralWeightsES_data.h` (flash) and
`data/neural/g2p_model_es.bin` (SD/PSRAM, see `data/README.md`).

**Not yet usable from C++**: `G2PNeuralModel.h`'s `arpabetForIndex()`/
output table is English-specific -- loading `g2p_model_es.bin` through it
today would decode to nonsense. See the last pipeline step below for what
wiring this up would require.

## Requirements

```bash
pip install torch numpy
```

## Data source

Training pairs come from the OLaPh Spanish corpus
(`../dictionary-es/olaph_es.txt`), parsed via
`../dictionary-common/olaph_parse.py` -- the SAME corpus and parser the
compressed dictionary (`CompactOlaphES_data.h`) is built from, so
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
#    g2p_model.pt as training progresses). DONE -- 98.7% val exact-match.
python3 train_g2p_model.py

# 3. Export + validate the quantized model against the checkpoint. DONE --
#    98.7%/98.7%, 99.9% agreement (see above).
python3 export_g2p_model.py
python3 validate_export.py

# 4. STILL TODO: update G2PNeuralModel.h (or a new Spanish-specific
#    equivalent) with a PHONEME_TABLE matching vocab.py's index-for-index
#    -- see ../neural-en/README.md's "Another language" section for the
#    general steps, and docs/ADDING_A_LANGUAGE.md Step 7.
```

## Files

Same roles as neural-en/'s own (see ../neural-en/README.md for the full
architecture description) -- `model.py`, `train_g2p_model.py`,
`validate_export.py` are byte-for-byte copies (fully language-agnostic,
driven entirely by `vocab.py`'s NUM_GRAPHEMES/NUM_PHONEMES);
`export_g2p_model.py` differs only in its default output path/symbol
names; `vocab.py` and `prepare_data.py` are Spanish-specific.
