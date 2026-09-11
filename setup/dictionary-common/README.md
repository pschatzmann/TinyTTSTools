# OLaPh dictionary pipeline (German/French/Spanish)

Shared, language-parameterized pipeline that builds the German/French/
Spanish full-vocabulary dictionaries (`CompactOlaphDE/FR/ES_data.h` +
`data/dictionary/olaph_{de,fr,es}.bin`) from the
[OLaPh](https://huggingface.co/datasets/cstr/g2p-dicts) pronunciation
corpus (real IPA transcriptions, one text file per language in the
sibling `../dictionary-de/`, `../dictionary-fr/`, `../dictionary-es/`
directories). This is the DE/FR/ES equivalent of `../dictionary-en/`'s
CMU-dict pipeline, but is not a copy of it -- it uses a widened
(`uint16_t`) packed symbol (base `Phone` id + stress + a `PhonemeModifier`)
via `CompressedPhonemeDictionaryWide`, a separate class from English's
`CompressedPhonemeDictionary`; the English pipeline/data is completely
untouched by anything here.

See [`../../docs/ADDING_A_LANGUAGE.md`](../../docs/ADDING_A_LANGUAGE.md)
("Step 6") for the full walkthrough, including the two real bugs hit
while building the DE/FR/ES tables (a blanket-Unicode-NFD trap, a
multi-pronunciation-variant corpus format) worth knowing before extending
`olaph_ipa_map.py` for a new language.

## Pipeline order

```bash
cd setup/dictionary-common

# 1. Check IPA-symbol/coverage before committing to a full run.
python3 olaph_parse.py ../dictionary-de/olaph_de.txt --lang de --stats-only

# 2. Compute this language's canonical Huffman codes (from real corpus
#    frequencies) and emit PhonemeHuffmanCodesWideDE.h.
python3 olaph_build_huffman.py --lang de

# 3. Pack the full corpus: emits CompactOlaphDE_data.h (flash-resident)
#    AND data/dictionary/olaph_de.bin (SD-card/PSRAM-loadable, same
#    payload, see CompressedPhonemeDictionaryWideSD.h).
python3 olaph_pack_compressed.py --lang de
```

Substitute `fr`/`es` for the other two languages. Steps 2 and 3 both
reparse the corpus and recompute frequencies independently (not sharing
state across separate script invocations) -- simpler and safer than
caching, at the cost of a few minutes of redundant parsing per run; corpus
sizes here make this a non-issue in practice. The shipped
`olaph_{de,fr,es}.txt` corpora are pre-filtered to each language's top
100,000 most frequent words (60k-124k lines after filtering -- see
[ADDING_A_LANGUAGE.md](../../docs/ADDING_A_LANGUAGE.md) Step 6.6) so the
packed dictionaries fit ESP32 PSRAM; the original unfiltered corpora
(256k-1.1M lines) are kept alongside as `olaph_{de,fr,es}.txt.full` for
reference.

## Files

- `olaph_ipa_map.py` -- the IPA symbol -> `Phone`/`PhonemeModifier` mapping
  tables (base symbols, composite affricate/diphthong/nasal-vowel
  sequences, diacritics). Data-driven from the actual corpora's symbol
  frequencies, not a textbook IPA chart -- extend this first if adding a
  new language surfaces unmapped symbols.
- `olaph_parse.py` -- parses one `olaph_{lang}.txt` corpus line into
  packed `uint16_t` symbols (`pack_symbol()`, same bit layout
  `CompactPhonemeDictionaryBuilder.h`'s `Seg()` uses) or the reverse
  (`packed_symbol_to_text()`, used by the neural pipeline's `vocab.py`
  generation too). `--stats-only` reports parse coverage with a
  per-failure-reason breakdown and examples -- the first thing to run
  against a new/changed corpus.
- `olaph_build_huffman.py` -- canonical Huffman code computation
  (reusing `../dictionary-en/build_huffman.py`'s algorithm unchanged) +
  `PhonemeHuffmanCodesWide{LANG}.h` emission. Exposes
  `compute_huffman_codes()`, imported by `olaph_pack_compressed.py` so the
  two never risk computing different codes for the same corpus.
- `olaph_pack_compressed.py` -- packs the parsed corpus into
  `CompressedPhonemeDictionaryWide`'s six backing arrays (mirroring
  `../dictionary-en/pack_compressed.py`'s `BitWriter`/blocked-index logic
  exactly, differing only in symbol width and UTF-8 word encoding) and
  emits both the flash header and the loadable binary.

## Verifying a regeneration

Same principle as `../dictionary-en/`'s: build a small program against the
new header (or the `.bin` via `CompressedPhonemeDictionaryWideSD`, see
`data/README.md`) and spot-check `lookup()` against known words --
`docs/ADDING_A_LANGUAGE.md`'s Step 6 has a concrete example. There's no
automated round-trip test target for the full corpus (its size makes that
impractical to run routinely), so trust coverage numbers from
`olaph_parse.py --stats-only` plus manual spot-checks over an automated
full-corpus check.
