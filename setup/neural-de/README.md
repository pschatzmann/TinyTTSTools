# Neural G2P Training Pipeline (German)

Scaffolded from neural-en/'s pipeline (same architecture, same training/
validation code, copied verbatim -- model.py, train_g2p_model.py,
validate_export.py; export_g2p_model.py copied with its default output
path/symbol names made German-specific, see ../../docs/ADDING_A_LANGUAGE.md)
but with its own German vocabulary and data source.

**Trained**: `g2p_model.pt` (256 hidden units, 15 epochs, trained on the
FULL 878k-word OLaPh German corpus -- 850,203 train / 26,294 val pairs,
not the top-100k-filtered dictionary tier) reaches **71.2% validation
exact-match** -- the lowest of the four languages, consistent with
German's much larger vocabulary and morphological complexity (heavy
compounding, umlaut, less regular orthography-to-phoneme mapping than
French/Spanish). Exported and validated: `export_g2p_model.py` +
`validate_export.py` give 71.0% (unquantized checkpoint) / 70.9%
(INT8-quantized export) with 97.0% per-word prediction agreement between
quantized and unquantized inference -- lower agreement than French/
Spanish's ~99.9% but still consistent with a larger, harder phoneme
vocabulary (159 phoneme classes). Weights are available as
`g2p_model.bin`/`G2PNeuralWeightsDE_data.h` (flash) and
`data/neural/g2p_model_de.bin` (SD/PSRAM, see `data/README.md`).

**Not yet usable from C++**: `G2PNeuralModel.h`'s `arpabetForIndex()`/
output table is English-specific -- loading `g2p_model_de.bin` through it
today would decode to nonsense. See the last pipeline step below for what
wiring this up would require.

## Requirements

```bash
pip install torch numpy
```

## Data source

Training pairs come from the OLaPh German corpus
(`../dictionary-de/olaph_de.txt`), parsed via
`../dictionary-common/olaph_parse.py` -- the SAME parser the compressed
dictionary (`CompactOlaphDE_data.h`) is built from. Note this model was
trained BEFORE the shipped `CompactOlaphDE_data.h`/`olaph_de.bin` were
filtered down to German's top 100,000 most frequent words for PSRAM (see
`data/README.md`) -- the neural model's training data still covers the
full, unfiltered corpus, so its out-of-dictionary coverage is broader
than the current small/full dictionary tiers.

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
#    g2p_model.pt as training progresses). DONE -- 71.2% val exact-match
#    (~7.5 hours on a 4-core desktop CPU -- German's full, unfiltered
#    878k-word corpus makes this by far the slowest of the four languages
#    to train; run it without other CPU-heavy work alongside it, see
#    ../../docs/ADDING_A_LANGUAGE.md's Step 4 lesson).
python3 train_g2p_model.py

# 3. Export + validate the quantized model against the checkpoint. DONE --
#    71.0%/70.9%, 97.0% agreement (see above).
python3 export_g2p_model.py
python3 validate_export.py

# 4. STILL TODO: update G2PNeuralModel.h (or a new German-specific
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
names; `vocab.py` and `prepare_data.py` are German-specific.
