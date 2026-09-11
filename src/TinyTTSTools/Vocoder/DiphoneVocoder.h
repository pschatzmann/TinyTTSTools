/**
 * @file DiphoneVocoder.h
 * @brief Diphone-based speech synthesizer for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-12
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 * 
 * @note This module uses AudioDictionary for accessing diphone audio data.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../SoundDictionary/AudioDictionary.h"
#include "ConcatenatedAudioVocoder.h"
#include "../Basic/StringUtils.h"

/**
 * @brief Diphone-based speech synthesizer with advanced concatenation
 * @details Generates speech audio using pre-recorded diphone samples with
 * sophisticated concatenation techniques for natural-sounding output.
 * Diphones are sound units that capture the transition between
 * two phonemes, providing more natural-sounding speech than
 * individual phonemes.
 * 
 * This class inherits all advanced concatenation capabilities from 
 * ConcatenatedAudioVocoder including phase alignment, cross-fade, fade-out,
 * and coarticulation effects. See ConcatenatedAudioVocoder documentation
 * for detailed feature descriptions and usage examples.
 * 
 * ## Diphone Processing
 *
 * A whole-utterance sequence normally contains silence-class tokens (SIL/
 * SP) between words -- TinyTTSTools::toPhonemes() inserts "SP" after every
 * word. DiphoneVocoder first splits the sequence into silence-free segments
 * at those tokens (typically one word each), renders each silence token as
 * real silence of its own duration, and processes each segment by sliding
 * over every adjacent pair of phonemes within it (not disjoint pairs -- N
 * phonemes need N-1 diphones to capture every real transition between
 * them):
 * - Brackets the segment with SIL on both ends (unless already present) so
 *   the first phoneme's own onset and the last phoneme's own release get
 *   their own diphones -- a diphone only ever supplies a phoneme's second
 *   half when it's the first half of the pair, and its first half when it's
 *   the second half of the pair, so without this the segment's very first
 *   and very last phonemes would never be heard in full
 * - Converts each adjacent phoneme pair to a diphone identifier (e.g., "AA EH")
 * - Applies advanced concatenation between adjacent diphones
 * - Uses roughly half of each constituent phoneme's natural duration,
 *   matching how the diphone audio itself is generated (half-of-phone1 +
 *   half-of-phone2, so consecutive diphones reconstruct each shared
 *   phoneme's duration once when played back-to-back)
 * 
 * @note This class uses an AudioDictionary to access diphone audio data.
 * @note Memory footprint: ~893KB flash for the default `DiphoneWAVDictionary`
 * audio data alone -- the largest of the available vocoders, in exchange
 * for the most natural output. Budget noticeably more than that for the
 * whole feature: this class also pulls in ConcatenatedAudioVocoder's
 * cross-fade/coarticulation code and an ADPCM decoder, which adds real code
 * size on top of the data (an `AudioDiphones` desktop build measured
 * roughly +1.0MB total vs. an equivalent FormantVocoder-based sketch; a
 * real microcontroller build can be higher still -- an ESP32 build of
 * `AudioDiphones` has overflowed the default partition at ~1.5MB). See
 * https://github.com/pschatzmann/TinyTTSTools/blob/main/docs/MEMORY.md
 * for a comparison table across all vocoders and G2P models.
 */
class DiphoneVocoder : public ConcatenatedAudioVocoder {
 public:
  /**
   * @brief Constructor
   * @param dictionary Audio dictionary containing diphone audio data
   * @param batchSize Maximum number of samples to write in a single batch (default: 256)
   */
  DiphoneVocoder(AudioDictionary& dictionary, size_t batchSize = 256)
      : ConcatenatedAudioVocoder(batchSize), dictionary_(dictionary) {
  }

  /**
   * @brief Get vocoder type name
   * @return "DiphoneVocoder"
   */
  std::string getType() const override { return "DiphoneVocoder"; }

  /**
   * @brief Check if vocoder is ready for synthesis
   * @return true if dictionary is available, false otherwise
   */
  bool isReady() const override {
    return true; // Dictionary manages its own readiness
  }

  /**
   * @brief Get the sample rate from the dictionary
   * @return Sample rate in Hz
   */
  int sampleRate() const override {
    return dictionary_.sampleRate();
  }

 protected:
  AudioDictionary& dictionary_;  ///< Reference to the audio dictionary

  /**
   * @brief Implementation-specific single phoneme synthesis (not applicable for diphones)
   * @param phoneme Single phoneme symbol in the default phoneme type format
   * @param out Output stream for audio data
   * @return false (diphones require pairs of phonemes)
   */
  bool synthesizePhoneme(const std::string& phoneme, ::Print& out,
                         const PhonemeSynthesisParams& params) override {
    // Diphones require pairs of phonemes, so single phoneme synthesis is not applicable
    // Fall back to sequence processing which will handle pairing
    return processSequenceWithLookahead(phoneme, out, params);
  }

  /**
   * @brief Override sequence processing for diphone-specific logic
   * @param sequence Phoneme sequence to convert to diphones
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @return true if synthesis successful, false otherwise
   */
  bool processSequenceWithLookahead(const std::string& sequence, ::Print& out,
                                    const PhonemeSynthesisParams& params) override {
    if (!isReady()) {
      return false;
    }

    // Parse phoneme sequence
    std::vector<std::string> phonemes = StringUtils::split(sequence, ' ');
    if (phonemes.empty()) {
      return false;
    }

    // Strip stress digits (e.g. "IH1" -> "IH") before building diphone
    // names or looking up durations: diphone names are keyed by bare
    // phoneme symbols, but a G2P source using the full CMU dictionary
    // (COMPACT_CMUDICT_EN) emits stress-marked phonemes -- without this,
    // any word with a stressed vowel would fail every diphone lookup.
    for (std::string& phoneme : phonemes) {
      int stress;
      phoneme = stripStressMarker(phoneme, stress);
    }

    // AH0 and ER0 are NOT stress-marked variants of AH/ER (stripStressMarker
    // correctly leaves them alone -- they're genuinely distinct reduced-
    // vowel phonemes in Phonemes.h's phoneme_map). But the diphone corpus
    // (generate_relevant_diphones.sh) was only ever generated for the 15
    // main vowels -- it has no AH0/ER0 diphone data at all (e.g. no
    // "P ER0"), so any word using them (e.g. "whispers" -> "... P ER0 Z")
    // silently failed every diphone lookup touching that vowel, dropping
    // that part of the word. Fall back to the closest available full-vowel
    // unit rather than dropping audio.
    for (std::string& phoneme : phonemes) {
      if (phoneme == "AH0") phoneme = "AH";
      else if (phoneme == "ER0") phoneme = "ER";
    }

    // A whole utterance arrives as ONE sequence with silence-class tokens
    // (SIL/SP, inserted by TinyTTSTools::toPhonemes() between words) mixed
    // in. Those aren't phonemes with their own diphone entries -- attempting
    // e.g. "N SP" or "SP F" as a diphone name always fails -- so split the
    // sequence into silence-free segments (typically one word each),
    // synthesize each as its own SIL-bracketed diphone chain, and render
    // each silence-class token as real silence of its own duration in
    // between. A single phoneme with no silence anywhere in the sequence
    // (isReady()-checked callers may pass just one word) is handled the
    // same way, as a one-segment sequence.
    PhonemeType phonemeType = dictionary_.phonemeType();
    bool success = true;
    std::vector<std::string> segment;
    auto flushSegment = [&]() {
      if (segment.empty()) return;
      if (!synthesizeSegment(segment, out, params, phonemeType)) success = false;
      segment.clear();
    };
    for (const std::string& token : phonemes) {
      if (phonemes_.isSilence(phonemeType, token.c_str())) {
        flushSegment();
        uint16_t duration = resolveDuration(phonemes_.getPhonemeDuration(phonemeType, token), params);
        outputSilence(sampleRate(), duration, out);
      } else {
        segment.push_back(token);
      }
    }
    flushSegment();

    return success;
  }

  /**
   * @brief Synthesize one silence-free phoneme segment (typically one word)
   * @param phonemes Segment's phonemes, already stress-stripped, no silence tokens
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @param phonemeType The dictionary's phoneme type
   * @return true if every diphone in the segment was found, false otherwise
   */
  bool synthesizeSegment(std::vector<std::string> phonemes, ::Print& out,
                        const PhonemeSynthesisParams& params, PhonemeType phonemeType) {
    // A diphone audio unit (source file "X_Y.wav", looked up as "X Y" --
    // see SoundEntry.h on why the lookup key uses a space) only ever
    // supplies the SECOND half of X and the FIRST half of Y (see
    // generate_relevant_diphones.sh). Sliding over adjacent pairs within a
    // segment therefore never renders the very first phoneme's own onset
    // (its first half, from silence) or the very last phoneme's own
    // release (its second half, into silence) -- the segment would start
    // mid-attack and end mid-release. Bracket it with SIL on both ends so
    // the leading "SIL <first>" and trailing "<last> SIL" diphones (both
    // present in the corpus -- see Data/wav/diphones/) get generated too.
    if (phonemes.front() != "SIL") phonemes.insert(phonemes.begin(), "SIL");
    if (phonemes.back() != "SIL") phonemes.push_back("SIL");

    bool success = true;

    // Slide over every ADJACENT pair (i, i+1), not disjoint pairs (0,1),
    // (2,3), ... -- N phonemes have N-1 real transitions between them, and
    // each one needs its own diphone. Stepping by 2 instead of 1 silently
    // skipped every other transition (e.g. "HH EH L OW" only ever
    // synthesized HH_EH and L_OW, never the EH->L transition in the
    // middle), replacing real coarticulated diphone audio with a generic
    // cross-fade between unrelated diphone units for half of any utterance.
    // Diphone audio is generated as roughly half-of-phone1 + half-of-phone2
    // (see setup/audio/diphones/generate_relevant_diphones.sh), so playing
    // consecutive diphones back-to-back reconstructs each shared phoneme's
    // duration once, not twice.
    for (size_t i = 0; i + 1 < phonemes.size(); i++) {
      std::string diphoneName = phonemes[i] + " " + phonemes[i + 1];

      // Calculate duration for the diphone: each side contributes roughly
      // half its natural phoneme duration, matching how the audio itself
      // was generated -- except SIL, a special case (see
      // diphoneSideDurationMs()'s doc).
      uint16_t duration1 = diphoneSideDurationMs(phonemeType, phonemes[i]);
      uint16_t duration2 = diphoneSideDurationMs(phonemeType, phonemes[i + 1]);
      uint16_t diphoneDuration = resolveDuration(duration1 + duration2, params);

      // Get current diphone entry
      const SoundEntry* currentEntry = getAudioEntry(diphoneName);
      if (!currentEntry) {
        success = false;
        continue;
      }

      // Get next diphone entry for enhanced concatenation
      const SoundEntry* nextEntry = nullptr;
      if (i + 2 < phonemes.size()) {
        std::string nextDiphoneName = phonemes[i + 1] + " " + phonemes[i + 2];
        nextEntry = getNextAudioEntry(diphoneName, nextDiphoneName);
      }

      // Process with enhanced concatenation
      processAudioUnit(currentEntry, nextEntry, diphoneDuration, out, params);
    }

    return success;
  }

  // The fixed silence portion generate_relevant_diphones.sh actually bakes
  // into every SIL_<phoneme>/<phoneme>_SIL recording's boundary side (its
  // own SILENCE_BOUNDARY_MS constant) -- unrelated to SIL's own
  // Phonemes.h table duration (400ms), which is how long a real inter-word
  // *pause* should last, not how much silence a diphone recording's edge
  // contains. Confirmed empirically: real SIL_HH/HH_SIL/SIL_T/T_SIL
  // recordings measure ~87.5-128ms total, matching this constant's
  // contribution + the other side's real half-duration; using SIL's own
  // 400ms/2=200ms here instead overstated every boundary diphone's target
  // by 100-160ms. That happened to have no audible effect only because
  // processUnitCombination() treats the duration as a truncation ceiling,
  // never a stretch/pad target -- an inflated-but-unenforced number is
  // still wrong to compute, and a future change to that consumption logic
  // could turn this into real dead-air at every word boundary.
  static constexpr uint16_t kBoundarySilenceMs = 50;

  /// One side's duration contribution to a diphone: half of a real
  /// phoneme's natural duration, or kBoundarySilenceMs for the SIL side of
  /// a word-boundary diphone (see kBoundarySilenceMs's own doc for why
  /// SIL needs different handling here than every other phoneme).
  uint16_t diphoneSideDurationMs(PhonemeType phonemeType, const std::string& phoneme) const {
    if (phoneme == "SIL") return kBoundarySilenceMs;
    return phonemes_.getPhonemeDuration(phonemeType, phoneme) / 2;
  }

  /**
   * @brief Get audio entry by diphone identifier (implements ConcatenatedAudioVocoder interface)
   * @param identifier Diphone identifier (e.g., "AA EH")
   * @return Pointer to SoundEntry or nullptr if not found
   */
  const SoundEntry* getAudioEntry(const std::string& identifier) override {
    return dictionary_.getSoundEntry(identifier.c_str());
  }

  /**
   * @brief Get next audio entry in sequence (implements ConcatenatedAudioVocoder interface)
   * @param currentId Current diphone identifier
   * @param nextId Next diphone identifier  
   * @return Pointer to next SoundEntry or nullptr if not found
   */
  const SoundEntry* getNextAudioEntry(const std::string& currentId, const std::string& nextId) override {
    // For diphones, next entry is simply the next diphone
    return dictionary_.getSoundEntry(nextId.c_str());
  }
};
