"""IPA -> (Phone id, PhonemeModifier) mapping for the OLaPh DE/FR/ES
pronunciation dictionaries (see olaph_parse.py).

Base symbols map to a `Phone` enum name (src/TinyTTSTools/Basic/Phonemes.h);
diacritics map to a PhonemeModifier tag or are handled as composite
(multi-codepoint) sequences detected before single-symbol mapping runs
(diphthongs, affricates, nasalized vowels with their own dedicated Phone
id). Symbol inventory and frequency were extracted directly from the three
corpus files (not assumed) -- see the analysis this table is based on.
"""

# Combining diacritics, applied to the PRECEDING base symbol unless noted.
COMBINING_LONG = "ː"       # ː  length -> MOD_LONG
STRESS_PRIMARY = "ˈ"       # ˈ  primary stress -> MOD_STRESS_PRIMARY on the NEXT symbol
STRESS_SECONDARY = "ˌ"     # ˌ  secondary stress -> MOD_STRESS_SECONDARY on the NEXT symbol
COMBINING_NONSYLLABIC = "̯"  # ̯  non-syllabic (diphthong offglide)
COMBINING_SYLLABIC_BELOW = "̩"  # ̩  syllabic -> MOD_SYLLABIC
COMBINING_SYLLABIC_ABOVE = "̍"  # ̍  syllabic (alt) -> MOD_SYLLABIC
COMBINING_TIE = "͡"        # ͡  ties two symbols into one affricate
COMBINING_VOICELESS = "̥"  # ̥  devoiced -> MOD_DEVOICED
COMBINING_NASAL = "̃"      # ̃  nasalized -> MOD_NASALIZED (or a dedicated
                                 #    nasal-vowel Phone id for French, see below)
COMBINING_RAISED = "̝"     # ̝  raised -- no modifier equivalent, ignored
                                 #    (rare, sub-phonemic detail)
COMBINING_BRIDGE = "̪"     # ̪  dental -- no modifier equivalent, ignored (1 occurrence)
COMBINING_LOWERED = "̞"    # ̞  lowered -- no modifier equivalent, ignored
COMBINING_ASPIRATED = "ʰ"  # ʰ  aspiration (modifier letter, not combining) -> MOD_ASPIRATED
COMBINING_PALATALIZED = "ʲ"  # ʲ  palatalization (modifier letter) -> MOD_PALATALIZED
COMBINING_UNRELEASED = "̚"  # ̚  unreleased -> MOD_UNRELEASED
COMBINING_HALF_LONG = "ˑ"  # ˑ  half-long -> MOD_HALF_LONG
SYLLABLE_BOUNDARY = "."         # .  syllable boundary marker -- ignored (no
                                 #    Phone/modifier equivalent, just a
                                 #    segmentation hint)
ALT_GLOTTAL_STOP = "?"          # a few entries use a bare '?' instead of 'ʔ'
                                 # for the glottal stop -- same mapping

IGNORED_COMBINING = {COMBINING_RAISED, COMBINING_BRIDGE, COMBINING_LOWERED,
                      SYLLABLE_BOUNDARY, "‿", "="}
# An alternate Unicode representation of the same "non-syllabic offglide"
# concept as COMBINING_NONSYLLABIC (U+032F) -- some entries use this one
# instead; treated identically wherever COMBINING_NONSYLLABIC is checked.
COMBINING_NONSYLLABIC_ALT = "̑"

# Base IPA symbol -> Phone enum name. Covers every base symbol observed in
# the three corpora (down to single-digit occurrence counts) except ones
# folded into a composite rule below (affricates, diphthongs, nasal
# vowels) or explicitly unmapped (logged, word skipped).
BASE_SYMBOL_TO_PHONE = {
    # Plain consonants shared with ARPAbet
    "t": "T", "n": "N", "s": "S", "l": "L", "f": "F", "k": "K", "m": "M",
    "d": "D", "b": "B", "p": "P", "v": "V", "z": "Z", "h": "HH", "j": "Y",
    "w": "W", "g": "G", "ɡ": "G",  # ɡ (script g, same phone as g)
    "ʃ": "SH",   # ʃ
    "ʒ": "ZH",   # ʒ
    "ŋ": "NG",   # ŋ
    "θ": "TH",   # θ
    "ð": "DH",   # ð
    "r": "RR",        # trill (French/Spanish/German loanword r)
    "ɹ": "R",    # ɹ (English-style approximant r, rare/loanword)

    # International consonants (Phone 43-120)
    "ʁ": "RU",   # ʁ  German/French uvular r
    "ɣ": "GH",   # ɣ  voiced velar fricative (Spanish g-lenition)
    "ç": "C",    # ç  German ich-Laut
    "x": "X",         # ach-Laut / Spanish jota
    "ʔ": "GS",   # ʔ  glottal stop
    "ɾ": "RT",   # ɾ  alveolar tap
    "β": "BETA", # β  Spanish b-lenition
    "ʎ": "LY",   # ʎ  Spanish traditional ll
    "ɲ": "NY",   # ɲ  Spanish ñ / French gn
    "ʝ": "JZ",   # ʝ  Spanish voiced palatal fricative (yeísmo realization)
    "ɥ": "HU",   # ɥ  French huit-glide

    # Vowels shared with ARPAbet (by IPA symbol identity, see Phonemes.h)
    "ɑ": "AA",   # ɑ
    "æ": "AE",   # æ
    "ʌ": "AH",   # ʌ
    "ə": "AH0",  # ə  schwa
    "ɪ": "IH",   # ɪ
    "i": "IY",
    "ʊ": "UH",   # ʊ
    "ɒ": "OB2",  # ɒ

    # International vowels (Phone 43-120)
    "a": "AF",        # open front unrounded (German/French/Spanish "a")
    "e": "EP",        # close-mid front, pure monophthong
    "o": "OP",         # close-mid back, pure monophthong
    "ɔ": "AO",   # ɔ  (ARPAbet open-mid back rounded, reused)
    "ɛ": "EH",   # ɛ  (ARPAbet open-mid front, reused)
    "u": "UW",
    "y": "UF",        # long/tense ü
    "ʏ": "UF0",  # ʏ  short/lax ü
    "ø": "OF",   # ø  long/tense ö
    "œ": "OE",   # œ  short/lax ö
    "ɐ": "AC",   # ɐ  near-open central (German vocalized-r offglide)
    "ɜ": "EC",   # ɜ  open-mid central unrounded, non-rhotic
    "ɝ": "ER",   # ɝ  rhotacized open-mid central (ARPAbet ER, exact IPA match)
    "ɨ": "IB",   # ɨ  close central unrounded
    "ɤ": "OM",   # ɤ  close-mid back unrounded

    # Rarer consonants (loanword/foreign-origin transcriptions -- each a
    # handful of occurrences in the DE/FR/ES corpora, but exact matches)
    "ʀ": "RT2",  # ʀ  uvular trill (distinct from ʁ, the uvular fricative/
                 #    approximant already mapped to RU)
    "χ": "CU",   # χ  voiceless uvular fricative
    "ʕ": "AP",   # ʕ  voiced pharyngeal fricative/approximant
    "ħ": "HP",   # ħ  voiceless pharyngeal fricative
    "ɕ": "SH",   # ɕ  alveolo-palatal fricative -- no dedicated id, nearest
                 #    approximation is ARPAbet SH
    "ɬ": "LH",   # ɬ  voiceless lateral fricative
    "ɺ": "LF",   # ɺ  lateral flap
    "ʈ": "TR",   # ʈ  voiceless retroflex stop
    "ɫ": "L",    # ɫ  velarized ("dark") l -- no dedicated base id for the
                 #    velarized variant, approximated as plain L
    "c": "CJ",   # c  voiceless palatal stop (IPA value, not ARPAbet "K")
}

# Composite (multi-codepoint) sequences, tried BEFORE single-symbol mapping,
# longest first. Each key is a tuple of codepoints; value is a Phone name.
# Affricates (tie-bar joins two symbols into one segment).
AFFRICATES = {
    ("t", COMBINING_TIE, "s"): "TS",
    ("t", COMBINING_TIE, "ʃ"): "CH",     # tʃ
    ("d", COMBINING_TIE, "ʒ"): "JH",     # dʒ
    ("p", COMBINING_TIE, "f"): "PF",
}

# German diphthongs: base vowel + non-syllabic-marked offglide vowel.
DIPHTHONGS = {
    ("a", "ɪ", COMBINING_NONSYLLABIC): "AY",   # aɪ̯
    ("a", "ʊ", COMBINING_NONSYLLABIC): "AW",   # aʊ̯
    ("ɔ", "ʏ", COMBINING_NONSYLLABIC): "OY",  # ɔʏ̯
}

# French nasal vowels: base vowel + combining tilde -> dedicated Phone id
# (these already exist as first-class symbols in Phonemes.h, e.g.
# {"AN", "ɑ̃", ...} -- match those exact IPA representations).
NASAL_VOWELS = {
    ("ɑ", COMBINING_NASAL): "AN",   # ɑ̃
    ("ɛ", COMBINING_NASAL): "EN",   # ɛ̃
    ("ɔ", COMBINING_NASAL): "ON",   # ɔ̃
    ("œ", COMBINING_NASAL): "UN",   # œ̃
    # Occasionally other vowels are nasalized (e.g. German loanwords) --
    # handled generically via MOD_NASALIZED, not a dedicated id, when no
    # entry above matches.
}
