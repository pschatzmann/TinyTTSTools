/**
 * @file TinyTTSTools.h
 * @brief Main Text-to-Speech engine class for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

// Include all modular components
#include "TinyTTSTools/Basic/TTSTypes.h"
#include "TinyTTSTools/Basic/TTSLogger.h"
#include "TinyTTSTools/Basic/StringUtils.h"
#include "TinyTTSTools/Basic/TTSAudioOutput.h"
#include "TinyTTSTools/Basic/Tokenizer.h"
#include "TinyTTSTools/PhonemeDictionary/CompactPhonemeDictionary.h"
#include "TinyTTSTools/SoundDictionary/AudioEncodedDictionary.h"
#include "TinyTTSTools/G2P/G2PDictionaryAndRulesModel.h"
#include "TinyTTSTools/G2P/G2PDictionaryModel.h"
#include "TinyTTSTools/G2P/G2PHybridModel.h"
#include "TinyTTSTools/G2P/G2PModelBase.h"
#include "TinyTTSTools/G2P/G2PRuleBasedModel.h"
#include "TinyTTSTools/Vocoder/ConcatenatedAudioVocoder.h"
#include "TinyTTSTools/Vocoder/DiphoneVocoder.h"
#include "TinyTTSTools/Vocoder/FormantVocoder.h"
#include "TinyTTSTools/Vocoder/PhonemeVocoder.h"
#include "TinyTTSTools/Vocoder/VocoderBase.h"

// Desktop test-build convenience: example sketches are written against a
// real I2SStream (I2S hardware output), so on a desktop/CI build (no I2S
// hardware) we transparently alias I2SStream to AudioTools' desktop
// MiniAudioStream backend instead -- the sketch source needs no change.
// Only active when IS_MIN_DESKTOP is defined (set by this library's own
// desktop CMake example targets); real Arduino/embedded builds are
// unaffected, and I2SStream itself is never defined by AudioTools on
// desktop (it requires USE_I2S/IS_I2S_IMPLEMENTED), so this alias never
// conflicts with a real I2SStream class.
#if defined(IS_MIN_DESKTOP) && !defined(ARDUINO)
#include "AudioTools/AudioLibs/MiniAudioStream.h"
using I2SStream = MiniAudioStream;
#endif

/**
 * @brief Main Text-to-Speech engine class
 */
class TinyTTSTools {
 public:
  /**
   * @brief Constructor
   * @param g2pModel Reference to grapheme-to-phoneme model for text conversion
   * @param synth Reference to phoneme-to-audio synthesizer
   * @param out Reference to Print object for audio output (e.g., Serial, File,
   * or custom stream)
   * @param config TTS configuration structure with optional settings
   * @details Initializes the TTS engine with the specified configuration.
   *          The output object receives the synthesized audio data as raw PCM
   * samples.
   */
  TinyTTSTools(G2PModelBase& g2pModel, VocoderBase& synth, Print& out,
           const TTSConfig& config = TTSConfig())
      : config_(config) {
    g2p_ = &g2pModel;
    synth_ = &synth;
    p_out_ = &out;
  }

  /**
   * @brief Destructor
   * @details Properly cleans up all allocated resources
   */
  ~TinyTTSTools() = default;

  /**
   * @brief Initialize the TTS engine
   * @param config Optional configuration to update settings
   * @return true if initialization successful, false otherwise
   * @details Must be called before using any TTS functionality.
   *          Updates the configuration if provided.
   */
  bool begin(const TTSConfig& config = TTSConfig()) {
    config_ = config;
    initialized_ = true;
    return true;
  }

  /**
   * @brief Initialize TTS system with sample rate
   * @param sample_rate Audio sample rate in Hz (8000-48000)
   * @return true if initialization successful, false otherwise
   * @details Simplified initialization that sets only the sample rate.
   *          Other configuration parameters will use default values.
   *          Must be called before using any TTS functionality.
   */
  bool begin(int sample_rate) {
    config_.sample_rate = sample_rate;
    initialized_ = true;
    return true;
  }

  /**
   * @brief Set speech completion callback
   * @param callback Function to call when speech is complete
   * @details Called after all audio has been sent to the audio callback.
   */
  void setSpeechCompleteCallback(SpeechCompleteCallback callback) {
    completeCallback_ = callback;
  }

  /**
   * @brief Set speech error callback
   * @param callback Function to call when an error occurs
   * @details Called with an error message string when synthesis fails.
   */
  void setSpeechErrorCallback(SpeechErrorCallback callback) {
    errorCallback_ = callback;
  }

  /**
   * @brief Convert text to phoneme sequence
   * @param text Input text string to convert
   * @return Vector of phoneme symbols
   * @details Performs text preprocessing and grapheme-to-phoneme conversion:
   *          1. Text normalization (lowercase, punctuation handling)
   *          2. Word tokenization
   *          3. Grapheme-to-phoneme conversion using selected model
   *          4. Silence insertion for punctuation and word boundaries
   */
  std::vector<std::string> toPhonemes(const std::string& text) {
    // Step 1: Text preprocessing
    std::vector<std::string> words = Tokenizer::tokenize(text);
    if (words.empty()) {
      if (errorCallback_) {
        errorCallback_("Empty text input");
      }
      return {};
    }

    // Step 2: Grapheme-to-phoneme conversion
    std::vector<std::string> phonemes;
    for (const std::string& word : words) {
      if (word == ".") {
        phonemes.push_back("SIL");  // Add silence for punctuation
      } else {
        std::string wordPhonemes = g2p_->wordToPhonemes(word);
        std::vector<std::string> wordPhonemeList =
            StringUtils::split(wordPhonemes, ' ');
        phonemes.insert(phonemes.end(), wordPhonemeList.begin(),
                        wordPhonemeList.end());
        phonemes.push_back("SP");  // Add short pause between words
      }
    }

    return phonemes;
  }

  /**
   * @brief Generate speech from text
   * @param text Input text string to synthesize
   * @return true if synthesis successful, false on error
   * @details Main TTS function that performs the complete text-to-speech
   * pipeline:
   *          1. Text-to-phoneme conversion using toPhonemes()
   *          2. Phoneme-to-audio synthesis
   *          3. Audio output written to the configured Print object as PCM data
   */
  bool say(const char* text) {
    std::string strText(text);
    return say(strText);
  }

  /**
   * @brief Generate speech from text
   * @param text Input text string to synthesize
   * @return true if synthesis successful, false on error
   * @details Main TTS function that performs the complete text-to-speech
   * pipeline:
   *          1. Text-to-phoneme conversion using toPhonemes()
   *          2. Phoneme-to-audio synthesis
   *          3. Audio output written to the configured Print object as PCM data
   */
  bool say(const std::string& text) {
    if (!initialized_) {
      if (errorCallback_) {
        errorCallback_("TTS not initialized");
      }
      return false;
    }

    // Step 1: Convert text to phonemes
    std::vector<std::string> phonemes = toPhonemes(text);
    if (phonemes.empty()) {
      return false;
    }

    return sayPhonemes(g2p_->getDefaultPhonemeType(), phonemes);
  }

  /**
   * @brief Synthesize audio from a sequence of phonemes
   * @param phonemeType The phoneme representation type of input phonemes
   * @param phonemes Vector of phoneme symbols to synthesize
   * @return true if synthesis successful, false on error
   * @details Allows direct synthesis from phoneme sequences without text
   * preprocessing. Audio data is written to the configured Print output object
   * as PCM samples.
   */
  bool sayPhonemes(PhonemeType phonemeType,
                   const std::vector<std::string>& phonemes) {
    if (!initialized_) {
      if (errorCallback_) {
        errorCallback_("TTS not initialized");
      }
      return false;
    }

    if (phonemes.empty()) {
      if (errorCallback_) {
        errorCallback_("Empty phoneme sequence");
      }
      return false;
    }

    // Join into ONE space-separated sequence and make a single call, rather
    // than calling sayPhoneme() once per individual phoneme token. Every
    // vocoder that needs multi-phoneme context (DiphoneVocoder to form any
    // diphone pair at all; PhonemeVocoder for cross-unit lookahead/
    // coarticulation) only gets that context from the phonemes it sees
    // *within one call* -- calling one phoneme at a time silently starved
    // them of it: DiphoneVocoder in particular can't form a diphone at all
    // from a single phoneme, so it fell back to treating every phoneme as
    // its own isolated one-phoneme "word", audibly fragmenting all speech.
    std::string joined;
    for (const std::string& phoneme : phonemes) {
      if (phoneme.empty()) continue;
      if (!joined.empty()) joined += ' ';
      joined += phoneme;
    }
    bool success = sayPhoneme(phonemeType, joined);

    // Signal completion
    if (completeCallback_) {
      completeCallback_();
    }

    return success;
  }

#ifdef ARDUINO
  /**
   * @brief Convenience method for Arduino String
   * @param text Arduino String object to synthesize
   * @return true if synthesis successful, false on error
   * @details Only available when compiling for Arduino platform
   */
  bool say(const String& text) { return say(std::string(text.c_str())); }

#endif

  /**
   * @brief Get current configuration
   * @return Reference to current TTSConfig structure
   * @details Returns the current configuration settings for inspection
   */
  const TTSConfig& getConfig() const { return config_; }

  /**
   * @brief Update configuration
   * @param config New configuration settings
   * @details Updates the TTS configuration and recreates synthesizer if needed.
   *          Sample rate changes will recreate the formant synthesizer.
   */
  void setConfig(const TTSConfig& config) { config_ = config; }

  /**
   * @brief Synthesize a single phoneme for testing or custom sequences
   * @param phonemeType The phoneme representation type of input phoneme
   * @param phoneme Phoneme symbol to synthesize
   * @return true if synthesis successful, false if synthesizer not available
   * @details Useful for testing individual phonemes or building custom
   * sequences. Audio data is written directly to the configured Print output
   * object.
   */
  bool sayPhoneme(PhonemeType phonemeType, const std::string& phoneme) {
    if (!synth_) {
      return false;
    }

    return synth_->sayPhoneme(phonemeType, phoneme, *p_out_);
  }

  /**
   * @brief Synthesize a single phoneme with optional volume/duration/pitch overrides
   * @param phonemeType The phoneme representation type of input phoneme
   * @param phoneme Phoneme symbol to synthesize
   * @param params Optional per-call synthesis overrides (see PhonemeSynthesisParams)
   * @return true if synthesis successful, false if synthesizer not available
   */
  bool sayPhoneme(PhonemeType phonemeType, const std::string& phoneme,
                  const PhonemeSynthesisParams& params) {
    if (!synth_) {
      return false;
    }

    return synth_->sayPhoneme(phonemeType, phoneme, *p_out_, params);
  }

  /**
   * @brief Set or change the audio output destination
   * @param out Reference to Print object for audio output (e.g., Serial, File,
   * custom stream)
   * @details Allows changing the output destination after construction.
   *          The Print object will receive raw PCM audio data during synthesis.
   */
  void setOutput(Print& out) { p_out_ = &out; }

 protected:
  TTSConfig config_;  ///< Current TTS configuration
  SpeechCompleteCallback completeCallback_ =
      nullptr;                                   ///< Speech completion callback
  SpeechErrorCallback errorCallback_ = nullptr;  ///< Error callback function
  G2PModelBase* g2p_ = nullptr;   ///< Grapheme-to-phoneme converter
  VocoderBase* synth_ = nullptr;  ///< Audio synthesizer/synth
  bool initialized_ = false;      ///< Initialization status flag
  Print* p_out_ = nullptr;  ///< Print output object for receiving synthesized
                            ///< PCM audio data

  /**
   * @brief Check if TTS engine is initialized
   * @return true if initialized, false otherwise
   * @details Used internally to ensure TTS functions are only called after
   * initialization.
   */
  bool isInitialized() const { return initialized_; }
};
