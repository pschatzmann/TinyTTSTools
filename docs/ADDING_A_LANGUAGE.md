# Adding a Language

TinyTTSTools ships English plus three international languages (German,
French, Spanish), each built the same way. This is the checklist that
produced them -- follow it to add a fifth. It's organized as a sequence of
independent layers; each is useful on its own, and you can stop after any
one of them (a language with just steps 1-3 already works end-to-end, just
with a small vocabulary and `FormantVocoder`-only audio).

## What German/French/Spanish already have (and don't)

| Layer | DE | FR | ES | File(s) |
|---|---|---|---|---|
| Phone ids + modifiers | ✅ shared | ✅ shared | ✅ shared | `Basic/Phonemes.h`, `Basic/PhonemeModifiers.h` |
| Small built-in dictionary (top-1000-freq, corpus-generated) | ✅ 986 words | ✅ 981 words | ✅ 970 words | `PhonemeDictionary/PhonemeDictionaryDE/FR/ES.h` |
| Rule-based G2P fallback | ✅ | ✅ | ✅ | `G2P/G2PRuleBasedModelDE/FR/ES.h` |
| International phoneme audio | ✅ partial (~23 phones) | ✅ partial | ✅ partial | `Data/wav/arpabet/*.h` (via `setup/audio/phonemes-from-espeak/generate_international_phonemes_mbrola.sh`) |
| Full-vocabulary compressed dictionary | ✅ 93k words (top-100k freq-filtered) | ✅ 60k words (top-100k freq-filtered) | ✅ 62k words (top-100k freq-filtered) | `Data/dictionary/CompactOlaphDE/FR/ES_data.h` |
| ...also SD/PSRAM-loadable | ✅ | ✅ | ✅ | `data/dictionary/olaph_{de,fr,es}.bin` + `CompressedPhonemeDictionaryWideSD` |
| Neural G2P vocabulary + data prep | ✅ scaffolded | ✅ scaffolded | ✅ scaffolded | `setup/neural-de/fr/es/vocab.py`, `prepare_data.py` |
| Neural G2P **trained weights** | ✅ trained (71.2% val exact-match) | ✅ trained (94.8% val exact-match) | ✅ trained (98.7% val exact-match) | `setup/neural-de/fr/es/g2p_model.pt` |
| ...wired into `G2PNeuralModel.h` | ✅ `G2PNeuralLanguage::DE` | ✅ `G2PNeuralLanguage::FR` | ✅ `G2PNeuralLanguage::ES` | -- |
| Desktop CLI wiring | ✅ `--language de/fr/es`, `--neural` | ✅ | ✅ | `desktop/DesktopMain.h` |
| Diphone audio (`DiphoneVocoder`) | ❌ none | ❌ none | ❌ none | -- |

None of the three has a hand-verified pronunciation corpus the way English
has CMUdict cross-checked over decades -- both dictionary tiers (small and
full-vocabulary) come from the same third-party corpus (OLaPh) whose
transcription conventions (e.g. Castilian vs. Latin American Spanish
/θ/-/s/ distinction) may not match what you'd choose by hand, and the
rule-based models are unverified approximations (no CMUdict-equivalent to
measure accuracy against). Every file below documents its own accuracy caveats in its class doc
-- read them before trusting a specific pronunciation.

## Step 1: Phone ids and modifiers -- probably nothing to do

Check whether `Phone`'s international/IPA extension (ids 43-120, see
[PHONEMES.md](PHONEMES.md)) already covers every sound your language
needs -- it was designed to cover the IPA pulmonic/non-pulmonic consonant
chart and vowel quadrilateral broadly, not just German/French/Spanish, so
most languages need nothing new here. If it's genuinely missing something
(a click, an ejective variant, a vowel quality with no `Phone` id at all),
add it to the enum in `Basic/Phonemes.h` (`Phone` enum + `phoneme_map`
entry: ARPAbet/IPA/X-SAMPA strings, duration, class) -- keep the existing
"IPA-first" design (id groups organized by natural class, not by which
language uses them) rather than adding language-specific ids.
`PhonemeModifier` (`Basic/PhonemeModifiers.h`) is fully generic (any
`Phone` id can carry any modifier) and essentially never needs a change
for a new language.

## Step 2: Small built-in dictionary

Create `src/TinyTTSTools/PhonemeDictionary/PhonemeDictionary{XX}.h`: a
`static constexpr PhonemeWordSource PHONEME_DICTIONARY_{XX}[]` table built
with `PH_WORD("word", ...)`, entries sorted by raw UTF-8 byte value (NOT
human alphabetical order for a language with accented letters), ending in
`TTS_COMPACT_DICTIONARY(COMPACT_PHONEME_DICTIONARY_{XX}, PHONEME_DICTIONARY_{XX});`.

Three authoring styles freely mix within one `PH_WORD()` call (see
[PHONEMES.md](PHONEMES.md) for the full picture):

```cpp
PH_WORD("cat", Phone::K, Phone::AE, Phone::T),                                  // bare Phone list
PH_WORD("stressed", Seg(Phone::AA, PhonemeModifier::MOD_STRESS_PRIMARY), Phone::T), // Seg() for a modifier
```

**Prefer generating this file, not hand-transcribing it** -- if Step 6
(the full OLaPh corpus) is already built for your language,
`setup/dictionary-common/generate_small_dictionary.py --lang {xx}
--freq-file {xx}_full.txt --top-n 1000` produces this file automatically
from real corpus transcriptions for your language's N most frequent
words (per a hermitdave/FrequencyWords `{xx}_full.txt`, see Step 6.6),
correctly decomposed into `Phone`/`Seg()`/`PhonemeModifier` source form
and pre-sorted. This is how `PhonemeDictionaryDE/FR/ES.h` are built today
(~970-990 words each, up from an original ~58-word hand-transcribed
starter set) -- do Step 6 first if you want this shortcut, or hand-author
a few dozen common words (numbers, greetings, pronouns) the old way if
you don't have a corpus yet and just want something to test against. When
hand-transcribing, document the transcription conventions you chose
(vowel-length handling, rhotic realization, seseo vs. distinción, ...) in
the file's own doc comment.

## Step 3: Rule-based G2P fallback

Create `src/TinyTTSTools/G2P/G2PRuleBasedModel{XX}.h`, deriving from
`G2PRuleBasedModelBase` (`G2P/G2PRuleBasedModelBase.h`) -- it supplies the
`Rule` struct, `matchesAt()`/`appendTokens()`, and the main word-scan loop
(`wordToPhonemes()` -> lowercase -> `applyRules()`), all identical across
every language. Your subclass only needs to override whichever of these it
actually uses (see `G2PRuleBasedModelDE.h` for a language using all four,
`G2PRuleBasedModelES.h` for one using only two):
- `tryWordStartRule()` -- patterns anchored to the first letter (e.g.
  German "sp"->/ʃp/). Skip this override if your language has none.
- `tryWordEndRule()` -- suffix patterns anchored to the last letter. Skip
  if none.
- `tryDigraphRule()` -- multi-character patterns checked anywhere in the
  word (digraphs, trigraphs, accented letters as 2-byte UTF-8 sequences).
  Almost every language needs this one.
- `applySingleLetter()` -- **required**, no default: the final per-letter
  fallback once no multi-character rule matched.
- `duplicateSkipChars()` -- only override if your digraph rules already
  consume a doubled letter another way (Spanish excludes 'l'/'r', already
  consumed by its own "ll"/"rr" digraph rules) -- otherwise the base
  default (`"bcdfglmnprstz"`) is fine.

None of the actual RULES are shared between languages -- writing a new
language's rule tables is still a from-scratch exercise in that
language's own orthography, not a diff against another language's. The
base class only removes the boilerplate that used to be copy-pasted
around them.

Handle UTF-8 explicitly: any accented letter is a multi-byte sequence, and
`char c = word[i]` (the English model's single-byte-per-letter model) will
silently misparse one unless you add an explicit rule matching that exact
byte sequence (see `G2PRuleBasedModelDE.h`'s `"\xc3\xa4"` (ä) style
entries) BEFORE the single-letter fallback ever sees it.

This has no accuracy measurement possible without a reference corpus --
build it as a best-effort approximation of the language's real spelling
rules, and validate by ear (or against Step 5's corpus, informally) rather
than trusting a claimed accuracy percentage.

## Step 4: Wire it up

Add both new headers to `src/TinyTTSTools.h`, and (optionally) extend
`desktop/DesktopMain.h`'s `--language` flag: `buildG2P()` needs a
`G2PRuleBasedModel{XX}` member and a branch pointing
`g2pDictionaryModel_` at `COMPACT_PHONEME_DICTIONARY_{XX}`; `buildVocoder()`
needs the language added to the "no audio recordings" check (defaults to
`--vocoder formant` unless Step 5 below is done). Add a
`test_g2p_rulebased_{xx}.cpp` (a handful of `CHECK_EQ` assertions against
words you've hand-verified) and register it in `tests/CMakeLists.txt`.

At this point the language works end-to-end (`FormantVocoder` +
dictionary + rules), just with a small vocabulary and no recorded audio.
**Stop here if that's enough.**

## Step 5: International phoneme audio (optional -- unlocks PhonemeVocoder/PSOLAVocoder)

`PhonemeVocoder`/`PSOLAVocoder` need a real recording per phoneme;
`ArpabetWAVDictionary` only has the original 41 English ARPAbet phonemes
plus the ~23 international ones German/French/Spanish actually reference.
If your language needs phonemes with no recording yet:

1. Find an MBROLA voice for your language (`apt-cache search mbrola`) --
   check its `/usr/share/doc/mbrola-{voice}/README.txt` for the exact
   SAMPA symbols it supports (voice-specific conventions vary from strict
   X-SAMPA -- e.g. Spanish `es1` uses plain `r`/`rr` for tap/trill, not
   X-SAMPA's `4`/`r`).
2. Add entries to `setup/audio/phonemes-from-espeak/
   generate_international_phonemes_mbrola.sh`'s `PHONES` array:
   `"PhoneOrTagName|voice|sampa_symbol|duration_ms"` (duration from
   `Phonemes.h`'s `phoneme_map`). A modifier-tagged phoneme (e.g. a
   language's long-vowel variant) needs a filesystem-safe name (`-`
   standing in for `:`) plus a `DISPLAY_NAME_OVERRIDES` entry in
   `generate_wav_dictionary.py` mapping it back to the real tagged lookup
   key -- see the existing `AF-` → `"AF:"` entry for the pattern (colon
   is illegal in FAT32 filenames, which `AudioDictionarySD` depends on).
3. Run the script, then regenerate:
   `python3 generate_wav_dictionary.py --input pcm8bit --force`.
4. Update `DesktopMain.h`'s vocoder-compatibility check if this closes the
   gap for `phoneme`/`psola`.

`DiphoneVocoder` has no equivalent path here -- it would need real
phoneme-PAIR recordings (1,600+ diphones for English), a much larger
undertaking not attempted for any of DE/FR/ES.

## Step 6: Full-vocabulary compressed dictionary (optional -- for real coverage)

If a pronunciation corpus exists for your language in IPA (OLaPh covers
many languages beyond DE/FR/ES -- check
[huggingface.co/datasets/cstr/g2p-dicts](https://huggingface.co/datasets/cstr/g2p-dicts)
first):

1. Download it to `setup/dictionary-{xx}/olaph_{xx}.txt` (tab-separated
   `word<TAB>/IPA transcription/`).
2. Extend `setup/dictionary-common/olaph_ipa_map.py`'s
   `BASE_SYMBOL_TO_PHONE`/`AFFRICATES`/`DIPHTHONGS`/`NASAL_VOWELS` tables
   for any IPA symbol your language uses that isn't already mapped --
   **derive this from the actual corpus**, don't guess: run a quick
   frequency scan of the corpus's IPA column first (see the Python
   one-liner pattern in `olaph_parse.py`'s own history/commit, or just
   `collections.Counter()` over every character) so you're covering real,
   frequency-ranked symbols, not a textbook IPA chart that may not match
   what the corpus actually contains. Watch for two traps hit while
   building the DE/FR/ES tables: (a) don't blanket Unicode-NFD-normalize
   the whole string -- it decomposes wanted precomposed characters (`ç` →
   `c` + combining cedilla) that then fail to parse; substitute only the
   specific precomposed characters you've confirmed need it. (b) check
   for a "multiple pronunciation variants per line" format
   (`/pron1/, /pron2/`) before assuming one IPA field is one
   transcription.
3. Test with `python3 olaph_parse.py ../dictionary-{xx}/olaph_{xx}.txt
   --lang {xx} --stats-only` -- iterate on the mapping table until
   coverage is high (German/French/Spanish landed at 92-100%); the
   per-reason skip breakdown with examples makes it obvious what's still
   missing.
4. Build the compressed dictionary:
   ```bash
   cd setup/dictionary-common
   python3 olaph_build_huffman.py --lang {xx}
   python3 olaph_pack_compressed.py --lang {xx}
   ```
   Watch the reported max Huffman code length against
   `CompressedPhonemeDictionaryWide::DecodeTables::kMaxLen` (24) -- a
   richer phoneme+modifier alphabet than German's (236 distinct symbols,
   max 23 bits) could exceed it, in which case widen `kMaxLen` (and
   confirm `PhonemeHuffmanCodeWide::code`, already `uint32_t`, still has
   headroom) before regenerating. This step ALSO writes
   `data/dictionary/olaph_{xx}.bin`, the SD-card/PSRAM-loadable form (see
   `CompressedPhonemeDictionaryWideSD` in `data/README.md`) -- both forms
   come from the same run, nothing extra to do for that.
5. Verify round-trip correctness against real words before trusting the
   output -- compile a throwaway program that `#include`s the generated
   `CompactOlaph{XX}_data.h` and looks up known words, comparing decoded
   output to the corpus.
6. **Optional but recommended for PSRAM targets**: the raw OLaPh corpus
   (600k-1.1M entries) packs to tens of MB, too large for ESP32 PSRAM
   (typically 2-8MB total, shared with everything else the sketch needs --
   see [MEMORY.md](MEMORY.md#loading-runtime-data-into-psram-esp32)).
   Filter the corpus down to a manageable vocabulary using a word-frequency
   list before packing, e.g.
   [hermitdave/FrequencyWords](https://github.com/hermitdave/FrequencyWords)'s
   `content/2016/{xx}/{xx}_full.txt` (already sorted by frequency, one
   `word count` pair per line):
   ```python
   with open("{xx}_full.txt", encoding="utf-8") as f:
       allowed = {line.split()[0].lower() for line in f.read().splitlines()[:100000]}
   with open("olaph_{xx}.txt", encoding="utf-8") as fin, \
        open("olaph_{xx}.txt.filtered", "w", encoding="utf-8") as fout:
       for line in fin:
           if line.split("\t", 1)[0].lower() in allowed:
               fout.write(line)
   ```
   then swap the filtered file in for `setup/dictionary-{xx}/olaph_{xx}.txt`
   (keep the original as `.txt.full` for reference, it's not git-tracked)
   and re-run step 4. German/French/Spanish's shipped dictionaries were all
   filtered this way to a top-100k-word cutoff, bringing them from
   19.1MB/11.1MB/4.3MB (878k/600k/256k words) down to 1.6MB/1.0MB/0.9MB
   (93k/62k/60k words -- lower than 100k since not every frequency-list
   entry has an exact match in OLaPh, e.g. inflected forms or multi-word
   phrases). Coverage against a frequency-ranked vocabulary degrades far
   more gracefully than a straight word-count cutoff would.

This tier targets desktop/SD-card/PSRAM, not flash-resident embedding --
see [MEMORY.md](MEMORY.md).

## Step 7: Neural G2P (optional -- best-effort predictions for out-of-vocabulary words)

1. Create `setup/neural-{xx}/`, copying `model.py`, `train_g2p_model.py`,
   `validate_export.py` from `neural-en/` (or any existing language's
   directory) **verbatim** -- they're fully generic, driven entirely by
   `vocab.py`'s `NUM_GRAPHEMES`/`NUM_PHONEMES`. `export_g2p_model.py` is
   ALMOST verbatim but NOT quite: its default `--output-header` path and
   the two emitted symbol names (`G2P_NEURAL_MODEL_WEIGHTS`/`_LEN`) must
   be made language-specific (`G2PNeuralWeights{XX}_data.h`,
   `G2P_NEURAL_MODEL_WEIGHTS_{XX}`/`_{XX}_LEN`) -- copying it byte-for-byte
   from another language and running it will silently overwrite THAT
   language's shipped weights file instead of writing a new one (this
   actually happened while building the French pipeline; caught only
   because the checkpoint's self-reported accuracy didn't match the
   target file's -- always `git status`/`git diff --stat` the weights
   file immediately after a fresh `export_g2p_model.py` run to catch this
   class of mistake before it's committed).
2. Generate `vocab.py` from the actual parsed corpus (Step 6's parser),
   not hand-picked: graphemes = every letter appearing at least ~50 times
   in the filtered word set; phonemes = every phoneme TEXT token (via
   `olaph_parse.packed_symbol_to_text()`, so the model's output vocabulary
   matches the dictionary's own decode format) appearing at least ~50
   times. Pick the threshold based on your corpus size -- too low wastes
   output classes on essentially-untrainable rare symbols, too high drops
   real vocabulary.
3. Write `prepare_data.py` (adapt an existing language's copy): filter to
   in-vocabulary graphemes/phonemes, write `train.tsv`/`val.tsv`. Run it
   to verify -- it's data prep, not training, and worth confirming before
   committing to a training run (word-retention rate should be >95%+ for
   a reasonable frequency threshold).
4. Train: `python3 train_g2p_model.py`. CPU-only is fine; budget tens of
   minutes to hours depending on corpus size -- French (244k training
   words, 4-core CPU, no GPU) took ~2 hours for the default 15 epochs and
   reached 94.8% validation exact-match; Spanish (582k words, same
   hardware) took ~6.5 hours and reached **98.7%**, the highest of the
   four languages (Spanish's near-1:1 letter-to-sound orthography is the
   easiest case by far); German (850k words -- the FULL unfiltered corpus,
   not the top-100k-filtered dictionary tier, since training predates that
   filtering) took ~7.5 hours and reached only **71.2%**, the lowest of
   the four -- German's heavy compounding and less regular orthography
   make it a genuinely harder case, not a training-setup problem (per-
   epoch progress climbed steadily and smoothly the whole way, just to a
   lower ceiling). Training time doesn't scale purely linearly with corpus
   size, and is highly sensitive to CPU contention from anything else
   running concurrently on the same machine -- one run of this appeared
   stalled for 3+ hours with zero epochs completed, confirmed still
   genuinely progressing (rising CPU time, no pathological data) rather
   than stuck, and resumed a normal pace once competing background work
   stopped. Don't run other CPU-heavy builds/scripts alongside a training
   run if you can avoid it.
5. Export and validate BEFORE touching any C++:
   `python3 export_g2p_model.py && python3 validate_export.py`. This also
   copies the binary to `data/neural/g2p_model_{xx}.bin` (SD/PSRAM,
   `G2PNeuralModelSD`) automatically -- see `data/README.md`.
6. Add a `G2PNeuralLanguage` enumerator (in `G2PNeuralModel.h`) for the
   new language, plus its own case in `graphemeIndexFor()` (grapheme ->
   vocab index, matching `vocab.py`'s `GRAPHEMES` order -- watch for
   accented letters needing `decodeUtf8()`'s multi-byte handling, not just
   plain a-z) and `symbolForIndex()` (a `PHONEME_TABLE` matching
   `vocab.py` index-for-index). Both tables are fixed metadata of one
   specific trained model, not inferred automatically from the weights
   file. Call `begin(weights, len, G2PNeuralLanguage::XX)` with the
   matching enumerator -- see DE/FR/ES for the pattern already wired up.

## Step 8: Tests

At minimum: a `test_g2p_rulebased_{xx}.cpp` (Step 4) and dictionary
round-trip checks (extend `test_dictionaries.cpp`'s pattern -- lookup a
handful of known words, check the decoded phoneme string, including at
least one word exercising a modifier tag if your dictionary uses any).
Register new test files in `tests/CMakeLists.txt`. Run the full suite
(`ctest` from the build directory) before considering the language done --
every step above should leave it green.
