/**
 * @file PhonemeModifiers.h
 * @brief Draft diacritic/modifier layer for Phone segments.
 * @author Phil Schatzmann
 * @date 2026-09-10
 *
 * @details Phonemes.h's international/IPA extension (ids 43-120)
 * deliberately did not enumerate diacritic combinations (palatalized-k,
 * long-a, aspirated-p, ...) as their own
 * symbols, because base-segment x diacritic is multiplicative -- any of
 * ~110 base segments can combine with several independent modifiers, and
 * giving each combination its own enum id would never converge.
 *
 * Instead a modifier is carried ALONGSIDE a base phoneme id, the same way
 * FormantRules.h's PhonemeFlagBits already rides alongside a PhonemeRule's
 * symbol. A future G2P layer would produce a base id + modifier pair per
 * segment (see Segment below) instead of a bare id.
 *
 * @note PhonemeModifier is a single-valued `uint8_t` enum -- ONE
 * modifier per segment, not an OR-able bitmask. This was a deliberate
 * memory/simplicity choice (see project discussion, 2026-09): dictionary
 * entries carrying a modifier are expected to be rare, so a segment costs
 * 1 extra byte (`Segment::modifier`) always, rather than the 4 bytes an
 * OR-able uint32_t bitmask would cost. The real tradeoff this accepts: a
 * segment that would linguistically need two simultaneous modifiers (e.g.
 * a palatalized AND aspirated consonant) can only keep one -- see
 * parsePhonemeModifiers()'s own doc for exactly how that case is handled
 * (last tag found wins, with a logged warning, not a silent drop).
 *
 * String encoding: for a plain-text phoneme string (space-separated
 * tokens, the format PhonemeDictionaryBase already returns), a modifier is
 * an ASCII tag attached to its base symbol using the REAL X-SAMPA
 * diacritic codes (Wells 1995), not an invented convention -- verified
 * against the espeak-ng X-SAMPA reference table, itself derived from
 * Wells' original spec. Every diacritic except stress is a SUFFIX; stress
 * is a PREFIX on the stressed syllable's first segment, per X-SAMPA's own
 * convention -- see the `position` field on ModifierTag below. e.g.:
 *   "P_h AA: N"       -- aspirated P, long AA, plain N
 *   "T_j EN"          -- palatalized T, French nasal EN
 *   "B AH0 \"N AE N AH0"  -- "banana": primary stress on the 2nd syllable
 * This makes phoneme strings interoperable with other X-SAMPA-aware
 * tooling; PhonemeInfo::xsampa stays the per-segment display form, this
 * table is what a full string (segments + diacritics) is built from.
 *
 * Deliberately OUT of scope:
 *  - Combinations that are really base-segment identity, not a modifier on
 *    top of one (e.g. dental vs. alveolar /t/) -- those stay as distinct
 *    Phone symbols if a language ever needs the contrast.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "TTSLogger.h"

/**
 * @brief One diacritic modifier applicable to (in principle) any base
 * phoneme id -- a single value per segment, not an OR-able bitmask (see
 * the file-level @note on why, and the tradeoff that comes with it). Not
 * every value is meaningful on every segment class (e.g. MOD_ASPIRATED
 * only applies to stops, MOD_NASALIZED only to vowels/approximants) -- a
 * parser/vocoder is expected to ignore combinations that don't apply
 * rather than reject them.
 *
 * @note This type exists precisely so a modifier is NOT a recorded asset
 * -- recording every base-segment x modifier combination would be the
 * same combinatorial explosion Phonemes.h's 43-120 id band already
 * rejected for base symbols. How each vocoder is expected to realize a
 * modifier without a dedicated recording:
 *  - FormantVocoder is parametric, so nothing here ever needs a
 *    recording -- every modifier is a formant-frequency shift, a duration
 *    scale, or a source-model tweak applied to the base PhonemeRule's
 *    numbers (see the per-value acoustic-effect notes further down).
 *  - The diphone/concatenated vocoder plays back recorded audio, so it
 *    depends on what the modifier changes:
 *    - MOD_LONG/MOD_HALF_LONG, MOD_STRESS_PRIMARY/SECONDARY,
 *      MOD_UNRELEASED: pure timing -- re-synthesize the existing
 *      recording at a new duration/pitch via PSOLAVocoder.h, already in
 *      this codebase for exactly that purpose. No new recording needed.
 *    - MOD_DEVOICED/MOD_VOICED, MOD_NASALIZED and secondary articulation
 *      (MOD_PALATALIZED/MOD_LABIALIZED/MOD_VELARIZED/
 *      MOD_PHARYNGEALIZED): approximable by filtering/spectral-shifting
 *      the existing recording, with some quality loss relative to a
 *      dedicated recording.
 *    - MOD_BREATHY/MOD_CREAKY, and the non-pulmonic segments in Phone's
 *      43-120 band (clicks/ejectives/implosives): these change the source
 *      mechanism, not just the filter -- the cases where a real
 *      recording (or a source model neither vocoder has today) would
 *      meaningfully outperform any DSP trick on an existing recording.
 *  Net guidance: record dedicated audio only for the specific
 *  modifier x phoneme combinations a target voice actually needs and
 *  where DSP approximation clearly falls short -- not exhaustively.
 */
enum class PhonemeModifier : uint8_t {
  MOD_NONE = 0,

  // -- Stress (vowels) ------------------------------------------------------
  // Phonemes.h's `Phone` enum used to have Phone::AA1/AA2-style
  // stress-variant ids (one pair per vowel, 30 ids total) purely as C++
  // authoring sugar over the compact dictionary's `(baseId << 2) | stress`
  // packing (see CompactPhonemeDictionaryBuilder.h) -- they've been removed
  // (the packed-byte format itself is unchanged; a stressed word in
  // PH_WORD() tables is now authored as Seg(Phone::X, MOD_STRESS_PRIMARY)
  // instead). This value is the general-purpose replacement for any future
  // non-English G2P built on Segment, so it never needs to double every
  // vowel id the way AA1/AA2 did.
  MOD_STRESS_PRIMARY = 1,    // ˈ  main stress of the word
  MOD_STRESS_SECONDARY = 2,  // ˌ  secondary stress

  // -- Length (applies mainly to vowels, also geminate consonants) --------
  MOD_LONG = 3,       // ː  e.g. Japanese/Finnish/Arabic long vowels,
                      //    Italian/Japanese geminate consonants
  MOD_HALF_LONG = 4,  // ˑ  intermediate length (some Scandinavian,
                      //    Estonian's 3-way quantity system)

  // -- Nasalization (vowels/approximants) ----------------------------------
  MOD_NASALIZED = 5,  // ~  e.g. Portuguese/Polish/Hindi nasal vowels beyond
                      //    the 4 French ones already given dedicated
                      //    symbols (AN/EN/ON/UN)

  // -- Secondary articulation (mainly consonants) --------------------------
  MOD_PALATALIZED = 6,      // ʲ  e.g. Russian/Irish "soft" consonants
  MOD_LABIALIZED = 7,       // ʷ  e.g. Arabic/Northwest Caucasian
  MOD_VELARIZED = 8,        // ˠ  e.g. Russian/Irish "hard"/dark consonants
  MOD_PHARYNGEALIZED = 9,   // ˤ  Arabic "emphatic" consonants (ص ض ط ظ)

  // -- Phonation / voice onset time (mainly consonants) --------------------
  MOD_ASPIRATED = 10,    // ʰ  phonemic in Mandarin/Korean/Thai/Hindi
                         //    (3-4 way stop contrast, not just
                         //    voiced/unvoiced)
  MOD_UNRELEASED = 11,   // ̚  e.g. Mandarin/Cantonese/Vietnamese
                         //    syllable-final stops (no audible burst)
  MOD_DEVOICED = 12,     // ̥  partial devoicing of an otherwise-voiced
                         //    segment (Japanese devoiced high vowels,
                         //    German final-obstruent devoicing)
  MOD_VOICED = 13,       // ̬  allophonic voicing on a normally-voiceless
                         //    segment (Spanish/Korean intervocalic
                         //    lenition, English flapping)

  // -- Phonation type (vowels, mainly) -------------------------------------
  MOD_BREATHY = 14,  // ̤  e.g. Gujarati, Hmong, some Burmese registers
  MOD_CREAKY = 15,   // ̰  e.g. Danish stød, Hmong, Vietnamese, Burmese

  // -- Syllabicity / rhoticity ----------------------------------------------
  MOD_SYLLABIC = 16,    // ̩  a consonant filling a syllable nucleus
                        //    (English "bottle" /bɒtl̩/, Czech "vlk")
  MOD_RHOTACIZED = 17,  // ˞  r-coloring added to a vowel (distinct from
                        //    ARPAbet's dedicated ER, for languages where
                        //    any vowel can be rhotacized, e.g. Mandarin
                        //    erhua)

  // -- Lexical tone (contour, on a syllable's vowel/nucleus) -----------------
  // Formerly a separate ToneAccent.h (ToneShape/ToneSpec/Syllable),
  // deleted as unused dead code (zero consumers) -- folded in here
  // instead, matching how tone is conventionally written in practice
  // (e.g. Mandarin Pinyin's tone diacritics sit directly ON the vowel,
  // not as a wrapper around the whole syllable). Named descriptively
  // (not "Mandarin tone 1-4") since the same shapes recur across
  // unrelated tone languages with different pitch ranges/registers.
  // Each value's pitch-contour effect (relative to whatever F0 the
  // phoneme would otherwise use) is in deriveModifierEffect() below,
  // consumed by FormantVocoder via PhonemeSynthesisParams::f0StartRatio/
  // MidRatio/EndRatio -- the same 3-point interpolation mechanism
  // ToneAccent.h's ToneSpec sketched, just reached through a modifier
  // instead of a dedicated struct. PitchAccentPattern (Japanese-style
  // per-mora high/low, a WORD-level structure, not reducible to a single
  // segment's tag) had no such natural single-value form and was dropped
  // along with the rest of that file, not migrated.
  TONE_LEVEL_HIGH = 18,  // ˥  e.g. Mandarin tone 1 (55), Cantonese tone 1
  TONE_LEVEL_MID = 19,   // ˧  e.g. Cantonese tone 3, Vietnamese "ngang"
  TONE_LEVEL_LOW = 20,   // ˩  e.g. Vietnamese "huyền"
  TONE_RISING = 21,      // e.g. Mandarin tone 2 (35), Vietnamese "sắc"
  TONE_FALLING = 22,     // e.g. Mandarin tone 4 (51), Vietnamese "nặng"
  TONE_DIPPING = 23,     // falling then rising, e.g. Mandarin tone 3 (214),
                         //    Vietnamese "hỏi"
  TONE_PEAKING = 24,     // rising then falling within one syllable
  TONE_CHECKED = 25,     // short, glottalized, often stop-final -- e.g.
                         //    Thai/Vietnamese "sắc"/"nặng" in closed
                         //    syllables, Cantonese tones 1/3/6 in
                         //    stop-final syllables
};

/**
 * @brief Whether a modifier's ASCII tag attaches before or after its base
 * symbol. X-SAMPA suffixes almost everything but marks stress as a prefix
 * on the syllable, so this can't be a uniform "always append" rule.
 */
enum class ModifierPosition : uint8_t { SUFFIX, PREFIX };

/**
 * @brief Tag <-> value lookup, one row per modifier, using X-SAMPA's
 * actual diacritic codes -- verified against the espeak-ng X-SAMPA
 * reference table (github.com/espeak-ng/espeak-ng/blob/master/docs/
 * phonemes/xsampa.md), itself derived from Wells' 1995 X-SAMPA spec. This
 * is the "how do I write it in text" answer: attach `tag` to a base Phone
 * symbol per `position`, with no space in between, e.g.:
 *   "P_h N"              -- aspirated P, plain N
 *   "T_j EN"             -- palatalized T, French nasal vowel EN
 *   "AF_k"               -- creaky-voiced /a/
 *   "B AH0 \"N AE N AH0" -- "banana", primary stress prefixed onto the
 *                           2nd syllable's first segment
 * A token normally carries at most one tag -- see parsePhonemeModifiers()
 * for what happens if more than one is present (PhonemeModifier is
 * single-valued, so they can't both be kept).
 */
struct ModifierTag {
  const char* tag;
  PhonemeModifier bit;
  ModifierPosition position;
};

static const ModifierTag kModifierTags[] = {
    {":", PhonemeModifier::MOD_LONG, ModifierPosition::SUFFIX},
    // AA:   -- long /ɑː/
    {":\\", PhonemeModifier::MOD_HALF_LONG, ModifierPosition::SUFFIX},
    // AA:\  -- half-long
    {"~", PhonemeModifier::MOD_NASALIZED, ModifierPosition::SUFFIX},
    // AF~   -- nasalized /a/
    {"_j", PhonemeModifier::MOD_PALATALIZED, ModifierPosition::SUFFIX},
    // T_j   -- palatalized t
    {"_w", PhonemeModifier::MOD_LABIALIZED, ModifierPosition::SUFFIX},
    // K_w   -- labialized k
    {"_G", PhonemeModifier::MOD_VELARIZED, ModifierPosition::SUFFIX},
    // L_G   -- velarized ("dark") l
    {"_?\\", PhonemeModifier::MOD_PHARYNGEALIZED, ModifierPosition::SUFFIX},
    // T_?\  -- pharyngealized/emphatic t (Arabic-style)
    {"_h", PhonemeModifier::MOD_ASPIRATED, ModifierPosition::SUFFIX},
    // P_h   -- aspirated p
    {"_}", PhonemeModifier::MOD_UNRELEASED, ModifierPosition::SUFFIX},
    // K_}   -- unreleased/unexploded k
    {"_0", PhonemeModifier::MOD_DEVOICED, ModifierPosition::SUFFIX},
    // Z_0   -- devoiced z
    {"_v", PhonemeModifier::MOD_VOICED, ModifierPosition::SUFFIX},
    // B_v   -- allophonically voiced b (e.g. Spanish intervocalic "b")
    {"_t", PhonemeModifier::MOD_BREATHY, ModifierPosition::SUFFIX},
    // AF_t  -- breathy /a/
    {"_k", PhonemeModifier::MOD_CREAKY, ModifierPosition::SUFFIX},
    // AF_k  -- creaky /a/
    {"=", PhonemeModifier::MOD_SYLLABIC, ModifierPosition::SUFFIX},
    // L=    -- syllabic l ("bottle")
    {"`", PhonemeModifier::MOD_RHOTACIZED, ModifierPosition::SUFFIX},
    // AF`   -- rhotacized /a/ (X-SAMPA reuses backtick for retroflex
    // consonant letters too, e.g. t`/d`/s` in Phone's 43-120 band -- the
    // two never collide since one only follows a vowel, the other a
    // consonant)
    {"\"", PhonemeModifier::MOD_STRESS_PRIMARY, ModifierPosition::PREFIX},
    // "AA   -- primary stress (X-SAMPA marks the syllable, prefix form)
    {"%", PhonemeModifier::MOD_STRESS_SECONDARY, ModifierPosition::PREFIX},
    // %AA   -- secondary stress
};

static constexpr size_t kModifierTagCount =
    sizeof(kModifierTags) / sizeof(kModifierTags[0]);

/**
 * @brief Strip every modifier tag matching kModifierTags off `token` and
 * return the bare base symbol, reporting the modifier found via
 * `modifier` (reset to MOD_NONE first).
 * @details Prefix tags are stripped from the front, suffix tags from the
 * back, each repeatedly until no more match -- so both a prefix and a
 * suffix tag on the same token ("\"AA:") are fully removed, leaving a
 * clean base symbol ("AA") regardless of how many tags matched.
 *
 * PhonemeModifier is single-valued (see its own @note), so if MORE
 * THAN ONE distinct tag is found on one token (e.g. "T_j_h" -- both
 * palatalized and aspirated), only the LAST one found is kept; the
 * earlier one is logged (TTS_LOGW) and discarded rather than silently
 * dropped. Prefer keeping such combinations to a single tag when
 * authoring, since which one survives depends on scan order, not
 * linguistic importance.
 * @param token One phoneme string, e.g. "T_j", "\"AA", "AF_k".
 * @param modifier Out-param: the modifier found (MOD_NONE if none).
 * @return `token` with every recognized tag removed, e.g. "T", "AA", "AF".
 */
inline std::string parsePhonemeModifiers(const std::string& token,
                                          PhonemeModifier& modifier) {
  modifier = PhonemeModifier::MOD_NONE;
  std::string s = token;
  bool sawAny = false;

  auto record = [&](PhonemeModifier found) {
    if (sawAny && found != modifier) {
      TTS_LOGW(
          "parsePhonemeModifiers: '%s' carries more than one modifier tag "
          "-- keeping the last one found, PhonemeModifier is "
          "single-valued (see its own doc)",
          token.c_str());
    }
    modifier = found;
    sawAny = true;
  };

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < kModifierTagCount; ++i) {
      const ModifierTag& t = kModifierTags[i];
      if (t.position != ModifierPosition::PREFIX) continue;
      size_t len = std::char_traits<char>::length(t.tag);
      if (s.size() > len && s.compare(0, len, t.tag) == 0) {
        record(t.bit);
        s.erase(0, len);
        changed = true;
      }
    }
  }

  changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < kModifierTagCount; ++i) {
      const ModifierTag& t = kModifierTags[i];
      if (t.position != ModifierPosition::SUFFIX) continue;
      size_t len = std::char_traits<char>::length(t.tag);
      if (s.size() > len && s.compare(s.size() - len, len, t.tag) == 0) {
        record(t.bit);
        s.erase(s.size() - len, len);
        changed = true;
      }
    }
  }

  return s;
}

/**
 * @brief Reverse of kModifierTags: the ASCII tag + attach position for a
 * given modifier value, e.g. `modifierTag(MOD_LONG)` -> `{":",
 * ModifierPosition::SUFFIX}`. Used wherever a modifier that started as a
 * packed value (e.g. CompactPhonemeDictionary's packed segment, or a
 * caller-set PhonemeSynthesisParams::modifier) needs to be rendered back
 * into the plain-text phoneme-string form parsePhonemeModifiers() reads.
 * @return nullptr for MOD_NONE (nothing to attach) or any value with no
 * table entry.
 */
inline const ModifierTag* modifierTag(PhonemeModifier modifier) {
  if (modifier == PhonemeModifier::MOD_NONE) return nullptr;
  for (size_t i = 0; i < kModifierTagCount; ++i) {
    if (kModifierTags[i].bit == modifier) return &kModifierTags[i];
  }
  return nullptr;
}

/**
 * @brief Attach `modifier`'s tag onto `base` per its ModifierPosition,
 * e.g. `applyModifierTag("AA", MOD_LONG)` -> `"AA:"`,
 * `applyModifierTag("AA", MOD_STRESS_PRIMARY)` -> `"\"AA"`. Returns `base`
 * unchanged for MOD_NONE or an unmapped value (e.g. a TONE_* value, which
 * has no X-SAMPA tag of its own -- tone is carried structurally via
 * PhonemeSynthesisParams::f0*Ratio, not written into the phoneme string).
 */
inline std::string applyModifierTag(const std::string& base, PhonemeModifier modifier) {
  const ModifierTag* t = modifierTag(modifier);
  if (t == nullptr) return base;
  return t->position == ModifierPosition::PREFIX ? std::string(t->tag) + base
                                                  : base + t->tag;
}

/**
 * @brief Multiplicative volume/speed/voicing/pitch-contour effect derived
 * from a single modifier value, per the mappings documented on
 * PhonemeSynthesisParams::setPhonemeModifier() and, for the TONE_* values,
 * on PhonemeModifier's own tone section. Extracted here (rather than left
 * inline on that one method) so a vocoder that detects a modifier tag on
 * an individual token -- not just a caller-supplied whole-call
 * PhonemeSynthesisParams -- can apply the exact same effect without
 * duplicating the magic numbers. voicingIsAbsolute distinguishes
 * MOD_DEVOICED/MOD_VOICED (hard overrides to 0.0/1.0) from MOD_BREATHY (a
 * partial multiplier onto whatever voicing already is).
 * f0StartRatio/MidRatio/EndRatio mirror
 * PhonemeSynthesisParams::f0StartRatio/MidRatio/EndRatio exactly -- a
 * TONE_* modifier is a shorthand for setting those three directly.
 */
struct ModifierEffect {
  float volumeMul = 1.0f;
  float speedMul = 1.0f;
  float voicingMul = 1.0f;
  bool voicingIsAbsolute = false;
  float voicingAbsolute = 1.0f;
  float f0StartRatio = 1.0f;
  float f0MidRatio = 1.0f;
  float f0EndRatio = 1.0f;
};

inline ModifierEffect deriveModifierEffect(PhonemeModifier modifier) {
  ModifierEffect e;
  switch (modifier) {
    case PhonemeModifier::MOD_STRESS_PRIMARY:
      e.speedMul /= 1.2f;
      e.volumeMul *= 1.15f;
      break;
    case PhonemeModifier::MOD_STRESS_SECONDARY:
      e.speedMul /= 1.1f;
      e.volumeMul *= 1.07f;
      break;
    case PhonemeModifier::MOD_LONG:
      e.speedMul /= 1.8f;
      break;
    case PhonemeModifier::MOD_HALF_LONG:
      e.speedMul /= 1.3f;
      break;
    case PhonemeModifier::MOD_BREATHY:
      e.voicingMul *= 0.6f;
      break;
    case PhonemeModifier::MOD_DEVOICED:
      e.voicingIsAbsolute = true;
      e.voicingAbsolute = 0.0f;
      break;
    case PhonemeModifier::MOD_VOICED:
      e.voicingIsAbsolute = true;
      e.voicingAbsolute = 1.0f;
      break;
    // Tone contours: ratios are relative to whatever F0 the phoneme would
    // otherwise use (a five-level/Chao-tone-letter-style reference scale
    // -- a concrete language should scale/replace these, not assume they
    // transfer as-is; absolute pitch range varies a lot even among tone
    // languages). TONE_CHECKED additionally shortens the syllable, since
    // checked tones are characteristically short/glottalized.
    case PhonemeModifier::TONE_LEVEL_HIGH:
      e.f0StartRatio = e.f0MidRatio = e.f0EndRatio = 1.30f;
      break;
    case PhonemeModifier::TONE_LEVEL_MID:
      e.f0StartRatio = e.f0MidRatio = e.f0EndRatio = 1.00f;
      break;
    case PhonemeModifier::TONE_LEVEL_LOW:
      e.f0StartRatio = e.f0MidRatio = e.f0EndRatio = 0.75f;
      break;
    case PhonemeModifier::TONE_RISING:
      e.f0StartRatio = 0.85f;
      e.f0MidRatio = 1.00f;
      e.f0EndRatio = 1.30f;
      break;
    case PhonemeModifier::TONE_FALLING:
      e.f0StartRatio = 1.30f;
      e.f0MidRatio = 1.00f;
      e.f0EndRatio = 0.70f;
      break;
    case PhonemeModifier::TONE_DIPPING:
      e.f0StartRatio = 1.00f;
      e.f0MidRatio = 0.65f;
      e.f0EndRatio = 1.15f;
      break;
    case PhonemeModifier::TONE_PEAKING:
      e.f0StartRatio = 0.90f;
      e.f0MidRatio = 1.30f;
      e.f0EndRatio = 0.90f;
      break;
    case PhonemeModifier::TONE_CHECKED:
      e.f0StartRatio = 1.10f;
      e.f0MidRatio = 1.05f;
      e.f0EndRatio = 1.00f;
      e.speedMul *= 1.3f;  // >1.0 shortens (see PhonemeSynthesisParams::
                            // speed doc -- speed>1.0 is faster/shorter),
                            // matching a checked tone's characteristic
                            // short, glottalized syllable.
      break;
    default:
      break;  // no effect for this modifier (or NONE)
  }
  return e;
}

/**
 * @brief A single synthesizable segment: a base Phone id (0-42 ARPAbet or
 * 43-120 international/IPA extension -- see Phonemes.h) plus its (single)
 * modifier.
 */
struct Segment {
  uint8_t phonemeId;                                    ///< id into Phone
  PhonemeModifier modifier = PhonemeModifier::MOD_NONE;
};

/**
 * @brief Acoustic effect of each modifier. Implemented in
 * FormantVocoder::preparePhonemeSynthesis()/applyModifierFormantShift()
 * (see FormantVocoder.h) for a token carrying one of these tags directly,
 * and in PhonemeSynthesisParams::setPhonemeModifier() (via
 * deriveModifierEffect() above) for a caller setting one on the whole
 * call. Covered by direct tests in test_phoneme_modifiers.cpp
 * ("FormantVocoder modifier wiring" section) and test_psola_vocoder.cpp.
 * FormantVocoder honors the full acoustic-effect list below (formant
 * shifts, voicing overrides, duration, pitch contour). PSOLAVocoder honors
 * duration and pitch-contour effects the same way (see its own class doc
 * for exactly what it can/can't realize -- formant shifts like
 * MOD_PALATALIZED have no effect there, since PSOLA is purely time-domain).
 *
 * - MOD_LONG / MOD_HALF_LONG: scale duration via speed /= 1.8 / 1.3 (see
 *   deriveModifierEffect()) -- pure timing change, no formant change.
 * - MOD_NASALIZED: same treatment as the AN/EN/ON/UN entries in
 *   FormantRules.h -- set PF_NASAL (ctx.flags.nasal), damp F1 amplitude
 *   ~15%.
 * - MOD_PALATALIZED: shift F2 up 400 Hz toward front-vowel range.
 * - MOD_LABIALIZED: shift F2 down 400 Hz toward back-vowel range.
 * - MOD_VELARIZED: shift F2 down 200 Hz (milder than labialization).
 * - MOD_PHARYNGEALIZED: F1 *= 1.15, F2 *= 0.85 ("darkening"); this is the
 *   Arabic emphatic-consonant effect and also colors an adjacent vowel.
 * - MOD_ASPIRATED: NOT a filter-coefficient change -- a fixed +45ms added
 *   to the phoneme's own natural duration (approximating a VOT gap before
 *   the next phoneme's voicing onset; not a real spectral VOT structure).
 * - MOD_UNRELEASED: burst amplitudes (a1/a2/a3) scaled to 30% -- softens
 *   the release rather than truly suppressing it.
 * - MOD_DEVOICED: ctx.flags.voiced forced false for this instance
 *   regardless of the base rule's PF_VOICED bit (sources noise instead of
 *   the glottal pulse).
 * - MOD_VOICED: the opposite override -- ctx.flags.voiced forced true
 *   (sources the voiced glottal pulse instead of noise) regardless of the
 *   base rule's PF_VOICED bit.
 * - MOD_BREATHY: voicing scalar *= 0.6 -- blends noise into the voiced
 *   source via the existing voicing<1.0 mechanism (same one
 *   PhonemeSynthesisParams::voicing already uses), not a separate path.
 * - MOD_CREAKY: F0 *= 0.75 only -- the one honest part of "lower and
 *   irregularize the glottal pulse rate" a simple multiply can
 *   approximate; no per-pulse irregularity model exists (would need a
 *   real source-model change FormantVocoder doesn't have yet).
 * - MOD_SYLLABIC: a fixed +40ms added to the consonant's own duration
 *   (approximating the syllable-nucleus time it carries) -- does NOT
 *   actually detect or drop an adjacent schwa; that needs sequence-level
 *   lookahead FormantVocoder's per-token loop doesn't have.
 * - MOD_RHOTACIZED: blend the vowel's F3 40%/60% toward ER's own F3
 *   (~1650 Hz), the way rhotic vowels are already handled via ER/ER0.
 */
