/**
 * @file ConcatenatedAudioVocoder.h
 * @brief Base class for concatenative speech synthesis with advanced audio processing
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-08-23
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 * 
 * @note This module provides advanced concatenation features for audio-based vocoders
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "VocoderBase.h"
#include "../Basic/Phonemes.h"
#include "../Basic/StringUtils.h"
#include "../Dictionary/SoundEntry.h"

/**
 * @brief Abstract base class for audio concatenation with click-free transitions
 * @details Concatenates pre-recorded audio units (phonemes, diphones, etc.)
 * into continuous speech. Two things always happen, with no configuration
 * needed: unit boundaries are cut at optimal zero-crossings (phase
 * alignment) and a unit's very end is faded out whenever it isn't
 * cross-faded into the next one -- both are cheap and never hurt quality,
 * so there is no reason to disable them.
 *
 * The one real quality/cost tradeoff is cross-fade (`setCrossFade()`,
 * default on): overlapping a unit's tail with the next unit's head via
 * linear interpolation, at a small extra CPU and memory cost (~128 bytes
 * for the tail buffer). Disable it on very tight memory/CPU budgets;
 * `setCrossFadeDuration()` tunes its length.
 *
 * @note Derived classes must implement getAudioEntry() and getNextAudioEntry()
 *       to provide access to their specific audio data sources.
 */
class ConcatenatedAudioVocoder : public VocoderBase {
 public:
  /**
   * @brief Constructor
   * @param batchSize Maximum number of samples to write in a single batch (default: 256)
   */
  ConcatenatedAudioVocoder(size_t batchSize = 256)
      : batchSize_(batchSize) {
    sampleBuffer_.reserve(batchSize_ * sizeof(int16_t));
  }

  /**
   * @brief Set the fade-out duration
   * @param durationMs Fade-out duration in milliseconds
   * @details Fade-out is always applied to a unit's ending whenever it isn't
   * cross-faded into a following unit (cross-fade disabled, or this is the
   * last unit in a sequence); this only tunes its length.
   */
  void setFadeOutDuration(float durationMs) {
    fadeOutSamples_ = static_cast<size_t>((durationMs * sampleRate()) / 1000.0f);
  }

  /**
   * @brief Set the batch size for writing audio samples
   * @param batchSize Maximum number of samples to write in a single batch
   */
  void setBatchSize(size_t batchSize) {
    batchSize_ = batchSize;
    sampleBuffer_.reserve(batchSize_ * sizeof(int16_t));
  }

  /**
   * @brief Enable or disable cross-fade between audio units (default: enabled)
   * @param enable true to enable cross-fade, false to disable
   * @details Cross-fade provides the highest quality transitions but requires
   * a small amount of extra memory (~128 bytes) and CPU. Disable only for
   * performance-critical or very memory-constrained applications.
   * @see setCrossFadeDuration() to configure transition length
   */
  void setCrossFade(bool enable) {
    enableCrossFade_ = enable;
  }

  /**
   * @brief Set cross-fade duration
   * @param durationMs Cross-fade duration in milliseconds
   */
  void setCrossFadeDuration(float durationMs) {
    crossFadeSamples_ = static_cast<size_t>((durationMs * sampleRate()) / 1000.0f);
  }

  /**
   * @brief Reset concatenation state (clears stored cross-fade tail and phase-alignment amplitude)
   * @details Call this when starting a new sentence/phrase, switching
   * speakers or voices, or after a long pause where continuity with the
   * previous unit isn't wanted. Automatically managed during normal
   * synthesis; manual reset is only needed for these special cases.
   */
  void reset() {
    lastNegativeAmplitude_ = 0;
    lastUnitTail_.clear();
  }

  bool isCrossFade() const { return enableCrossFade_; }

 protected:
  Phonemes phonemes_;  ///< Phonemes instance for duration lookup
  bool enableCrossFade_ = true;  ///< Enable cross-fade between audio units
  size_t fadeOutSamples_ = 64;  ///< Number of samples for fade-out (default ~8ms at 8kHz)
  size_t crossFadeSamples_ = 32;  ///< Number of samples for cross-fade (default ~4ms at 8kHz)
  size_t batchSize_;  ///< Maximum number of samples to write in a single batch
  mutable std::vector<uint8_t> sampleBuffer_;  ///< Buffer for batch writing samples
  mutable std::vector<int16_t> overlapBuffer_;  ///< Buffer for unit overlap processing
  mutable std::vector<int16_t> lastUnitTail_;  ///< Store end of previous unit for cross-fade
  mutable int16_t lastNegativeAmplitude_ = 0;  ///< Store last negative amplitude (as positive) for phase alignment

  /**
   * @brief Abstract interface to get audio entry by identifier
   * @param identifier Audio unit identifier (e.g., phoneme, diphone)
   * @return Pointer to SoundEntry or nullptr if not found
   * @note Must be implemented by derived classes
   */
  virtual const SoundEntry* getAudioEntry(const std::string& identifier) = 0;

  /**
   * @brief Abstract interface to get next audio entry in sequence
   * @param currentId Current audio unit identifier
   * @param nextId Next audio unit identifier  
   * @return Pointer to next SoundEntry or nullptr if not found
   * @note Must be implemented by derived classes
   */
  virtual const SoundEntry* getNextAudioEntry(const std::string& currentId, const std::string& nextId) = 0;

  /**
   * @brief Get sample rate for audio processing (must be implemented by derived classes)
   * @return Sample rate in Hz
   */
  virtual int sampleRate() const = 0;

  /**
   * @brief Add a sample to the buffer and flush if batch size is reached
   * @param sample 16-bit audio sample to add
   * @param out Output stream for audio data
   * @param volume Optional volume scale (1.0 = unchanged); from PhonemeSynthesisParams
   */
  void addSampleToBatch(int16_t sample, ::Print& out, float volume = 1.0f) const {
    if (volume != 1.0f) {
      int32_t v = (int32_t)(sample * volume);
      if (v > 32767) v = 32767;
      else if (v < -32768) v = -32768;
      sample = (int16_t)v;
    }
    const uint8_t* sampleBytes = reinterpret_cast<const uint8_t*>(&sample);
    sampleBuffer_.insert(sampleBuffer_.end(), sampleBytes, sampleBytes + sizeof(int16_t));

    // Check if we have enough bytes for the specified number of samples
    if (sampleBuffer_.size() >= batchSize_ * sizeof(int16_t)) {
      flushBatch(out);
    }
  }

  /**
   * @brief Ratio mapping a requested pitch to a source-sample read index step
   * @details A naive resample-based pitch shift suitable for constrained
   * platforms: reading source samples at `i * ratio` instead of `i` raises
   * or lowers perceived pitch without changing the requested output
   * duration. This is not a true PSOLA/phase-vocoder shift (timbre drifts
   * more as the ratio moves away from 1.0), but it is cheap and needs no
   * extra buffers. kNominalRecordingPitchHz is an approximation of the
   * pre-recorded units' own average pitch; a real per-recording pitch
   * isn't tracked, so this is only ever a rough shift.
   */
  static constexpr float kNominalRecordingPitchHz = 110.0f;
  float computePitchRatio(float pitchHz) const {
    return (pitchHz > 0.0f) ? (pitchHz / kNominalRecordingPitchHz) : 1.0f;
  }

  /**
   * @brief Read a (possibly pitch-shifted) sample from an audio unit
   * @param entry SoundEntry to read from
   * @param i Output sample index
   * @param pitchRatio From computePitchRatio(); 1.0 = no shift
   */
  int16_t sampleAtPitch(const SoundEntry* entry, size_t i, float pitchRatio) const {
    if (pitchRatio == 1.0f) return (*entry)[i];
    size_t maxIdx = entry->samples() > 0 ? entry->samples() - 1 : 0;
    size_t srcIndex = static_cast<size_t>(i * pitchRatio);
    if (srcIndex > maxIdx) srcIndex = maxIdx;
    return (*entry)[srcIndex];
  }

  /**
   * @brief Flush any remaining samples in the buffer
   * @param out Output stream for audio data
   */
  void flushBatch(::Print& out) const {
    if (!sampleBuffer_.empty()) {
      out.write(sampleBuffer_.data(), sampleBuffer_.size());
      sampleBuffer_.clear();
    }
  }

  /**
   * @brief Process audio unit data with enhanced concatenation
   * @param entry Pointer to SoundEntry
   * @param nextEntry Pointer to next SoundEntry (can be null for last unit)
   * @param durationMs Requested duration in milliseconds
   * @param out Output stream for audio data
   * @param params Optional volume/pitch overrides (durationMs is passed separately
   *        since callers may resolve it per-unit, e.g. half+half for diphones)
   */
  void processAudioUnit(const SoundEntry* entry, const SoundEntry* nextEntry,
                       uint16_t durationMs, ::Print& out,
                       const PhonemeSynthesisParams& params = PhonemeSynthesisParams{}) {
    if (!entry) return;

    // Use the enhanced combination logic for better audio quality
    processUnitCombination(entry, nextEntry, durationMs, out, params);
  }

  /**
   * @brief Enhanced audio unit combination with cross-fade and coarticulation
   * @param currentEntry Current SoundEntry
   * @param nextEntry Next SoundEntry (can be null for last unit)
   * @param currentDuration Duration for current unit
   * @param out Output stream for audio data
   * @param params Optional volume/pitch overrides
   */
  void processUnitCombination(const SoundEntry* currentEntry,
                             const SoundEntry* nextEntry,
                             uint16_t currentDuration,
                             ::Print& out,
                             const PhonemeSynthesisParams& params) {
    if (!currentEntry) return;

    size_t totalSamples = currentEntry->samples();
    if (currentDuration > 0) {
      size_t maxSamples = static_cast<size_t>((currentDuration * sampleRate()) / 1000);
      if (maxSamples < totalSamples) totalSamples = maxSamples;
    }

    if (totalSamples == 0) return;

    float pitchRatio = computePitchRatio(params.pitchHz);

    // Track start offset if we apply cross-fade with previous unit
    size_t startOffset = 0;

    // Apply cross-fade with previous unit if we have stored tail
    if (!lastUnitTail_.empty() && enableCrossFade_) {
      // processCrossFadeWithPrevious() clears lastUnitTail_ before returning,
      // so it must report how many samples it actually consumed -- reading
      // lastUnitTail_.size() afterward (as done previously) always reads 0.
      startOffset = processCrossFadeWithPrevious(currentEntry, totalSamples, out, params, pitchRatio);
    }

    // Determine processing parameters based on combination settings
    size_t fadeOutStart = totalSamples;
    size_t crossFadeStart = totalSamples;

    if (nextEntry && enableCrossFade_) {
      // Reserve samples for cross-fade
      crossFadeStart = totalSamples > crossFadeSamples_ ?
                      totalSamples - crossFadeSamples_ : 0;
      fadeOutStart = crossFadeStart; // No separate fade-out when cross-fading
    } else if (totalSamples > fadeOutSamples_) {
      // Not cross-fading into a next unit (disabled, or this is the last
      // unit) -- always fade the ending out instead of cutting it abruptly.
      fadeOutStart = totalSamples - fadeOutSamples_;
    }

    // Apply phase alignment for better cuts (end of current unit)
    size_t finalSampleCount = applyPhaseAlignment(currentEntry, totalSamples, false);

    // Process the main body of the unit (before fade/cross-fade)
    size_t mainBodyEnd = std::min({fadeOutStart, crossFadeStart, finalSampleCount});
    for (size_t i = startOffset; i < mainBodyEnd; i++) {
      addSampleToBatch(sampleAtPitch(currentEntry, i, pitchRatio), out, params.volume);
    }

    // Handle ending: cross-fade or simple fade-out
    if (nextEntry && enableCrossFade_ && crossFadeStart < finalSampleCount) {
      storeUnitTailForCrossFade(currentEntry, crossFadeStart, finalSampleCount, nextEntry, pitchRatio);
    } else if (fadeOutStart < finalSampleCount) {
      processSimpleFadeOut(currentEntry, fadeOutStart, finalSampleCount, out, params.volume, pitchRatio);
      lastUnitTail_.clear(); // Clear tail if not cross-fading
    } else {
      // No fading, write remaining samples
      for (size_t i = mainBodyEnd; i < finalSampleCount; i++) {
        addSampleToBatch(sampleAtPitch(currentEntry, i, pitchRatio), out, params.volume);
      }
      lastUnitTail_.clear(); // Clear tail if not cross-fading
    }
  }

  /**
   * @brief Enhanced phase alignment with amplitude-aware zero crossing detection
   * @param entry SoundEntry to analyze
   * @param totalSamples Original total samples
   * @param isStart true if aligning start of unit, false if aligning end
   * @return Adjusted sample count for optimal phase alignment
   */
  size_t applyPhaseAlignment(const SoundEntry* entry, size_t totalSamples, bool isStart = false) {
    // Degenerate case: "no adjustment" means start-at-0 for a start offset,
    // but keep-the-full-length (totalSamples) for an end cutoff -- returning
    // totalSamples for BOTH would make a start offset equal to the unit's
    // own length, skipping its entire body.
    if (totalSamples <= 1) return isStart ? 0 : totalSamples;

    if (isStart) {
      // For unit start: find zero crossing where amplitude exceeds stored threshold
      return findOptimalStartPoint(entry, totalSamples);
    } else {
      // For unit end: find zero crossing and store negative amplitude
      return findOptimalEndPoint(entry, totalSamples);
    }
  }

  /**
   * @brief Find optimal start point for audio unit based on previous amplitude
   * @param entry SoundEntry to analyze
   * @param totalSamples Total samples available
   * @return Optimal start index for phase alignment
   */
  size_t findOptimalStartPoint(const SoundEntry* entry, size_t totalSamples) {
    if (lastNegativeAmplitude_ == 0 || totalSamples <= 1) {
      return 0; // No previous amplitude info, start at beginning
    }
    
    // Search first 25% of samples for appropriate zero crossing
    size_t searchRange = std::min(totalSamples / 4, static_cast<size_t>(64)); // Limit search to 64 samples max
    
    for (size_t i = 1; i < searchRange && i < totalSamples; i++) {
      int16_t prevSample = (*entry)[i-1];
      int16_t currSample = (*entry)[i];
      
      // Look for zero upward crossing where amplitude exceeds threshold
      if (prevSample <= 0 && currSample > 0) {
        // Check if the positive amplitude exceeds our stored threshold
        if (currSample >= lastNegativeAmplitude_) {
          return i; // Start at this zero crossing
        }
      }
    }
    
    // If no suitable crossing found, use a simpler approach
    for (size_t i = 1; i < searchRange && i < totalSamples; i++) {
      if ((*entry)[i-1] <= 0 && (*entry)[i] > 0) {
        return i; // Any zero upward crossing
      }
    }
    
    return 0; // Default to start
  }

  /**
   * @brief Find optimal end point for audio unit and store amplitude info
   * @param entry SoundEntry to analyze
   * @param totalSamples Total samples available
   * @return Optimal end index for phase alignment
   */
  size_t findOptimalEndPoint(const SoundEntry* entry, size_t totalSamples) {
    // Search last 25% of samples for zero crossing
    size_t searchRange = totalSamples / 4;
    size_t searchStart = searchRange > 0 && searchRange < totalSamples ? 
                        totalSamples - searchRange : 1;
    
    // Track the largest negative amplitude in the end section
    int16_t maxNegativeAmplitude = 0;
    size_t optimalCutPoint = totalSamples;
    
    for (size_t i = totalSamples - 1; i >= searchStart; --i) {
      int16_t sample = (*entry)[i];
      
      // Track negative amplitudes (store as positive values)
      if (sample < 0) {
        int16_t negativeAbs = static_cast<int16_t>(-sample);
        if (negativeAbs > maxNegativeAmplitude) {
          maxNegativeAmplitude = negativeAbs;
        }
      }
      
      // Look for zero upward crossing
      if (i > 0 && (*entry)[i-1] <= 0 && (*entry)[i] > 0) {
        optimalCutPoint = i + 1; // Cut just after zero crossing
        break;
      }
      
      if (i == searchStart) break; // Prevent underflow
    }
    
    // Store the maximum negative amplitude for next unit alignment
    if (maxNegativeAmplitude > 0) {
      lastNegativeAmplitude_ = maxNegativeAmplitude;
    }
    
    return optimalCutPoint;
  }

  /**
   * @brief Process cross-fade with previously stored audio unit tail
   * @param currentEntry Current SoundEntry
   * @param totalSamples Total samples in current unit
   * @param out Output stream
   * @param params Optional volume override
   * @param pitchRatio From computePitchRatio(); 1.0 = no shift
   * @return Total number of samples-into-the-unit consumed (phase-aligned
   *         start offset + samples spent on the cross-fade itself) --
   *         callers must resume reading currentEntry from this index, not
   *         from the phase-aligned start offset alone.
   */
  size_t processCrossFadeWithPrevious(const SoundEntry* currentEntry, size_t totalSamples, ::Print& out,
                                      const PhonemeSynthesisParams& params, float pitchRatio) {
    // Find optimal start point for current unit using stored amplitude
    size_t startOffset = applyPhaseAlignment(currentEntry, totalSamples, true);

    // Adjust cross-fade parameters based on start offset
    size_t availableSamples = totalSamples - startOffset;
    size_t fadeSamples = std::min({crossFadeSamples_, lastUnitTail_.size(), availableSamples});

    for (size_t i = 0; i < fadeSamples; i++) {
      float fadeRatio = static_cast<float>(i) / static_cast<float>(fadeSamples);

      // Previous unit sample (fading out) -- already pitch-shifted (if any)
      // when it was captured by storeUnitTailForCrossFade()
      int16_t prevSample = lastUnitTail_[i];
      float prevWeight = 1.0f - fadeRatio;

      // Current unit sample (fading in) - start from optimal offset
      int16_t currentSample = sampleAtPitch(currentEntry, startOffset + i, pitchRatio);
      float currentWeight = fadeRatio;

      // Combine samples with cross-fade
      int16_t combinedSample = static_cast<int16_t>(
        (prevSample * prevWeight) + (currentSample * currentWeight)
      );

      addSampleToBatch(combinedSample, out, params.volume);
    }

    lastUnitTail_.clear();
    return startOffset + fadeSamples;
  }

  /**
   * @brief Store audio unit tail for cross-fade with next unit
   * @param currentEntry Current SoundEntry
   * @param fadeStart Start index for cross-fade in current unit
   * @param fadeEnd End index for current unit
   * @param nextEntry Next unit entry (for optimization)
   * @param pitchRatio From computePitchRatio(); 1.0 = no shift
   */
  void storeUnitTailForCrossFade(const SoundEntry* currentEntry, size_t fadeStart,
                                size_t fadeEnd, const SoundEntry* nextEntry, float pitchRatio) {
    size_t fadeSamples = fadeEnd - fadeStart;

    // Store the tail for cross-fade with next unit
    lastUnitTail_.clear();
    lastUnitTail_.reserve(fadeSamples);

    for (size_t i = 0; i < fadeSamples; i++) {
      int16_t sample = sampleAtPitch(currentEntry, fadeStart + i, pitchRatio);
      lastUnitTail_.push_back(sample);
    }
  }

  /**
   * @brief Process simple fade-out for audio unit ending
   * @param entry SoundEntry to process
   * @param fadeStart Start index for fade-out
   * @param fadeEnd End index
   * @param out Output stream
   * @param volume Optional volume override
   * @param pitchRatio From computePitchRatio(); 1.0 = no shift
   */
  void processSimpleFadeOut(const SoundEntry* entry, size_t fadeStart, size_t fadeEnd, ::Print& out,
                            float volume, float pitchRatio) {
    size_t fadeSamples = fadeEnd - fadeStart;

    for (size_t i = 0; i < fadeSamples; i++) {
      int16_t sample = sampleAtPitch(entry, fadeStart + i, pitchRatio);
      float fadeMultiplier = 1.0f - (static_cast<float>(i) / static_cast<float>(fadeSamples));
      sample = static_cast<int16_t>(sample * fadeMultiplier);
      addSampleToBatch(sample, out, volume);
    }
  }

  /**
   * @brief Override to add batch flushing after sequence processing
   * @param phoneme Phoneme symbol in the default phoneme type format (may be sequence)
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides
   * @return true if synthesis successful, false otherwise
   */
  bool synthesizePhoneme(const std::string& phoneme, ::Print& out,
                         const PhonemeSynthesisParams& params) override {
    // Enhanced sequence processing with lookahead for better concatenation
    bool success = processSequenceWithLookahead(phoneme, out, params);

    // Flush any remaining samples in the buffer
    flushBatch(out);

    return success;
  }

  /**
   * @brief Enhanced sequence processing with next-unit lookahead
   * @param sequence Audio unit string (may contain multiple space-separated units)
   * @param out Output stream for audio data
   * @param params Optional volume/duration/pitch overrides, applied to every unit in the sequence
   * @return true if synthesis successful, false otherwise
   */
  virtual bool processSequenceWithLookahead(const std::string& sequence, ::Print& out,
                                            const PhonemeSynthesisParams& params) {
    if (!isReady()) {
      return false;
    }

    // Parse unit sequence
    std::vector<std::string> units = StringUtils::split(sequence, ' ');
    if (units.empty()) {
      return false;
    }

    // Strip stress digits (e.g. "IH1" -> "IH") before any lookup: audio
    // dictionaries are keyed by bare phoneme/diphone names with no stress
    // marker, but a G2P source using the full CMU dictionary
    // (COMPACT_CMUDICT_EN) emits stress-marked phonemes -- without this,
    // every stressed vowel would silently fail every dictionary lookup.
    for (std::string& unit : units) {
      int stress;
      unit = stripStressMarker(unit, stress);
    }

    bool success = true;

    // Process each unit with context from the next unit
    for (size_t i = 0; i < units.size(); i++) {
      const std::string& currentUnit = units[i];

      // Check for silence
      if (phonemes_.isSilence(getDefaultPhonemeType(), currentUnit.c_str())) {
        uint16_t duration = resolveDuration(
            phonemes_.getPhonemeDuration(getDefaultPhonemeType(), currentUnit), params);
        outputSilence(sampleRate(), duration, out);
        continue;
      }

      // Get current unit entry
      const SoundEntry* currentEntry = getAudioEntry(currentUnit);
      if (!currentEntry) {
        success = false;
        continue;
      }

      // Get next unit entry for enhanced concatenation
      const SoundEntry* nextEntry = nullptr;
      if (i + 1 < units.size()) {
        const std::string& nextUnit = units[i + 1];
        if (!phonemes_.isSilence(getDefaultPhonemeType(), nextUnit.c_str())) {
          nextEntry = getNextAudioEntry(currentUnit, nextUnit);
        }
      }

      // Get duration and process with enhanced combination
      uint16_t duration = resolveDuration(
          phonemes_.getPhonemeDuration(getDefaultPhonemeType(), currentUnit), params);
      processAudioUnit(currentEntry, nextEntry, duration, out, params);
    }

    return success;
  }
};
