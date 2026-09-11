#pragma once
// Desktop-only CLI wrapper -- NOT part of the Arduino/ESP-IDF library
// surface (nothing under src/ includes this), only built when
// -DBUILD_DESKTOP_MAIN=ON (see desktop/CMakeLists.txt and the top-level
// CMakeLists.txt). Mirrors the sibling TinyTTS project's own
// desktop/DesktopMain.h (see ../../TinyTTS/desktop/DesktopMain.h) --
// same CLI shape, adapted to TinyTTSTools' G2P-model + Vocoder + Print
// architecture instead of TinyTTS's single monolithic model.
//
// Unlike TinyTTS's version, playback needs no manual ring-buffer chunking
// here: ConcatenatedAudioVocoder already writes to `out` in small batches
// during synthesis (see setBatchSize()), not one huge write() at the end,
// so a real MiniAudioStream can be handed to TinyTTSTools's constructor
// directly and used exactly like every .ino example already does.
//
// ODR note: MiniAudioStream.h does `#define MINIAUDIO_IMPLEMENTATION`
// immediately before `#include "miniaudio.h"` -- the single-header
// library's actual implementation gets compiled wherever that header is
// included. This file must therefore be included from EXACTLY ONE
// translation unit in the whole desktop target (desktop/main.cpp) -- do
// not #include this from a second .cpp, or the build fails with duplicate
// miniaudio symbol definitions at link time.
#include "AudioTools.h"
#include "AudioTools/AudioLibs/MiniAudioStream.h"
#include "TinyTTSTools.h"
#include "TinyTTSTools/Basic/TTSExampleUtils.h"
#include "TinyTTSTools/SoundDictionary/ArpabetWAVDictionary.h"
#include "TinyTTSTools/SoundDictionary/DiphoneWAVDictionary.h"
#include "TinyTTSTools/Vocoder/PSOLAVocoder.h"
#include "TinyTTSTools/Data/dictionary/CompactCmuDictionaryEN_data.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryDE.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryFR.h"
#include "TinyTTSTools/PhonemeDictionary/PhonemeDictionaryES.h"
#include "TinyTTSTools/G2P/G2PDictionaryModel.h"
#include "TinyTTSTools/G2P/G2PHybridModel.h"
#include "TinyTTSTools/G2P/G2PNeuralModel.h"
#include "TinyTTSTools/Data/neural/G2PNeuralWeightsEN_data.h"

#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace tinyttstools {

/**
 * @brief Forwards every write() to a real Print sink while counting how
 * many int16_t samples passed through -- lets run() know the true total
 * playback duration to weigh against real elapsed wall-clock time (see
 * run()'s own doc for why that matters).
 */
class CountingTee : public Print {
 public:
  explicit CountingTee(Print& sink) : sink_(sink) {}
  size_t write(const uint8_t* data, size_t size) override {
    total_samples_ += size / sizeof(int16_t);
    return sink_.write(data, size);
  }
  size_t totalSamples() const { return total_samples_; }

 private:
  Print& sink_;
  size_t total_samples_ = 0;
};

/**
 * @brief CLI entry point for the desktop build: owns every TinyTTSTools
 * piece (G2P model, vocoder, output) for the process's lifetime, so the
 * actual free `main()` (desktop/main.cpp) stays a one-line argv-parsing
 * shim -- `main()` itself can't be a class method.
 *
 * Supports Unix-style piping: `echo "hi" | tinyttstools --stdout > out.wav`
 * reads text from stdin (when no positional TEXT/--file is given) and
 * writes WAV bytes to stdout instead of playing -- see run()'s --help
 * text for the full option list.
 * @author Phil Schatzmann
 * @copyright Apache-2.0
 */
class DesktopMain {
 public:
  int run(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) return opt.help_requested ? 0 : 1;

    std::string text;
    if (!resolveText(opt, text)) return 1;

    if (!buildG2P(opt)) return 1;
    if (!buildVocoder(opt)) return 1;
    if (opt.full_dict) {
      if (opt.language == "en") {
        g2pDictionaryModel_.useCompactDictionary(COMPACT_CMUDICT_EN);
      } else {
        std::fprintf(stderr,
                      "--full-dict is English-only (no equivalent bundled for --language %s) "
                      "-- ignoring\n",
                      opt.language.c_str());
      }
    }

    TTSConfig config{(uint32_t)sample_rate_, 16, 1};
    bool should_play = !opt.no_play && opt.output_file.empty() && !opt.to_stdout;

    if (should_play) {
      // Only constructed (and therefore only ever destructed) when
      // actually used: MiniAudioStream's destructor unconditionally
      // calls end()/ma_device_uninit(), which segfaults if begin() was
      // never called -- found via gdb after the file-writing path (which
      // never touches audio playback at all) crashed on exit.
      i2s_out_.reset(new audio_tools::MiniAudioStream());
      TTSExample::beginI2S(*i2s_out_, config);
      // say() returning does NOT mean playback finished: MiniAudioStream's
      // write() applies real backpressure (blocks/retries while its ring
      // buffer is full -- see MiniAudioStream.h), but its ring buffer is
      // generously sized, so say() can return well before all of it has
      // actually reached the speaker. Returning immediately destroys
      // i2s_out_ while that tail is still draining, cutting off whatever
      // hadn't played yet -- typically the last word.
      //
      // Neither a zero-length wait nor a flat "sleep the full audio
      // duration" margin is correct here (measured empirically): say()
      // paces *partially* toward real-time, so the remaining undrained
      // audio when it returns is somewhere between "all of it" and "none
      // of it" depending on buffer size and synthesis speed, not a fixed
      // amount. Track real elapsed wall-clock time against the actual
      // total sample count instead, and only sleep for whatever's still
      // outstanding (plus a small fixed margin for MiniAudio's own final
      // internal buffering latency) -- correct whether say() paces fully,
      // partially, or not at all toward real-time.
      CountingTee tee(*i2s_out_);
      TinyTTSTools tts(g2p_, *vocoder_, tee, config);
      if (!tts.begin(config)) {
        std::fprintf(stderr, "TinyTTSTools.begin() failed\n");
        return 1;
      }
      auto t0 = std::chrono::steady_clock::now();
      if (!speak(tts, text, opt)) {
        std::fprintf(stderr, "speech synthesis failed\n");
        return 1;
      }
      double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      double total_duration = (double)tee.totalSamples() / sample_rate_;
      double remaining = total_duration - elapsed;
      if (remaining < 0.0) remaining = 0.0;
      std::this_thread::sleep_for(std::chrono::duration<double>(remaining + 0.3));
      return 0;
    }

    // TTSAudioOutputCallback's AudioDataCallback is a plain function
    // pointer (typedef void (*)(const int16_t*, size_t)), not a
    // std::function -- it can't bind a capturing lambda. Route through a
    // static instance pointer instead (fine for a single-shot CLI: only
    // one DesktopMain ever runs per process).
    active_ = this;
    TTSAudioOutputCallback cap_out(&DesktopMain::onAudioStatic);
    TinyTTSTools tts(g2p_, *vocoder_, cap_out, config);
    if (!tts.begin(config)) {
      std::fprintf(stderr, "TinyTTSTools.begin() failed\n");
      return 1;
    }
    if (!speak(tts, text, opt)) {
      std::fprintf(stderr, "speech synthesis failed\n");
      return 1;
    }

    if (!opt.output_file.empty()) {
      std::ofstream f(opt.output_file, std::ios::binary);
      if (!f) {
        std::fprintf(stderr, "cannot open %s for writing\n", opt.output_file.c_str());
        return 1;
      }
      writeWav(f, samples_, config.sample_rate);
    }
    if (opt.to_stdout) {
      writeWav(std::cout, samples_, config.sample_rate);
    }
    return 0;
  }

 private:
  struct Options;

  /**
   * @brief Speak text, threading --pitch/--speed through when set.
   * @details TinyTTSTools::say() has no params-accepting overload, so a
   * non-default pitch/speed bypasses it and replicates say()'s own
   * text->phonemes->joined-string path (toPhonemes() + space-join,
   * matching sayPhonemes()'s own joining) down to the params-accepting
   * sayPhoneme() overload instead.
   */
  bool speak(TinyTTSTools& tts, const std::string& text, const Options& opt) {
    if (opt.pitch_hz <= 0.0f && opt.speed == 1.0f) {
      return tts.say(text);
    }
    std::vector<std::string> phonemes = tts.toPhonemes(text);
    if (phonemes.empty()) return false;
    std::string joined;
    for (const std::string& p : phonemes) {
      if (p.empty()) continue;
      if (!joined.empty()) joined += ' ';
      joined += p;
    }
    PhonemeSynthesisParams params;
    params.pitchHz = opt.pitch_hz;
    params.speed = opt.speed;
    return tts.sayPhoneme(g2p_.getDefaultPhonemeType(), joined, params);
  }

  struct Options {
    std::string text;
    bool have_text = false;
    std::string input_file;
    std::string output_file;
    bool to_stdout = false;
    bool no_play = false;
    std::string vocoder = "phoneme";  // formant | phoneme | diphone | psola
    bool vocoder_explicit = false;    // true once the user passes --vocoder
    std::string language = "en";      // en | de | fr | es
    float pitch_hz = 0.0f;            // 0 = vocoder's own default pitch
    float speed = 1.0f;               // 1.0 = normal rate
    bool full_dict = false;
    bool use_neural = false;
    bool help_requested = false;
  };

  static void printUsage(const char* prog) {
    std::fprintf(stderr,
                  "Usage: %s [options] [TEXT]\n"
                  "\n"
                  "Input:\n"
                  "  TEXT                  Text to speak. If omitted (and --file isn't given),\n"
                  "                        read from stdin -- e.g. echo \"hi\" | %s\n"
                  "  -f, --file FILE       Read text from FILE instead.\n"
                  "\n"
                  "Output:\n"
                  "  -o, --output FILE     Write synthesized audio to FILE as a WAV file,\n"
                  "                        instead of playing it.\n"
                  "  --stdout              Write WAV bytes to stdout instead of playing --\n"
                  "                        for piping, e.g. %s --stdout | aplay, or > out.wav\n"
                  "  --no-play             Skip playback (implied by -o/--stdout).\n"
                  "\n"
                  "Voice:\n"
                  "  --language LANG       en | de | fr | es (default: en). Selects the small\n"
                  "                        built-in dictionary + rule-based G2P fallback for that\n"
                  "                        language (see PhonemeDictionaryDE/FR/ES.h,\n"
                  "                        G2PRuleBasedModelDE/FR/ES.h). de/fr/es have no bundled\n"
                  "                        audio recordings, so --vocoder defaults to 'formant'\n"
                  "                        (procedural) for them unless overridden.\n"
                  "  --vocoder NAME        formant | phoneme | diphone | psola (default: phoneme,\n"
                  "                        or formant when --language isn't en)\n"
                  "                        formant: procedural, no audio data, most robotic.\n"
                  "                        phoneme: pre-recorded samples (ArpabetWAVDictionary),\n"
                  "                        truncated (never stretched) to fit each phoneme's duration.\n"
                  "                        diphone: pre-recorded samples, most natural\n"
                  "                        (DiphoneWAVDictionary).\n"
                  "                        psola: same samples as 'phoneme', but re-synthesized\n"
                  "                        via TD-PSOLA to genuinely stretch/compress duration and\n"
                  "                        shift pitch (see --pitch) instead of only ever truncating.\n"
                  "  --pitch HZ            Override the synthesis pitch. Only audible with\n"
                  "                        --vocoder formant (procedural) or psola (TD-PSOLA\n"
                  "                        re-synthesis) -- phoneme/diphone play back pre-recorded\n"
                  "                        audio verbatim and can't re-pitch it.\n"
                  "  --speed FACTOR        Speaking-rate multiplier: 1.0 = normal (default),\n"
                  "                        2.0 = twice as fast, 0.5 = half speed. Slowing down\n"
                  "                        (< 1.0) only genuinely stretches audio with --vocoder\n"
                  "                        formant or psola -- phoneme/diphone can only ever\n"
                  "                        truncate pre-recorded audio, never lengthen it, so a\n"
                  "                        slower request there just plays the same clip unchanged\n"
                  "                        once it's already shorter than the slowed-down target.\n"
                  "  --full-dict           Use the full ~123k-word CMU dictionary instead of the\n"
                  "                        small built-in one (see docs/TUTORIAL.md). English\n"
                  "                        (--language en) only -- ignored otherwise.\n"
                  "  --neural              Add the neural GRU G2P fallback (~970KB weights) for\n"
                  "                        genuinely novel words (proper nouns, made-up words)\n"
                  "                        that the dictionary doesn't cover, tried before the\n"
                  "                        rule-based fallback. English (--language en) only for\n"
                  "                        now -- trained weights for de/fr/es exist but aren't\n"
                  "                        yet wired into the engine's output table (see\n"
                  "                        docs/ADDING_A_LANGUAGE.md), so --neural is ignored\n"
                  "                        (with a warning) for other languages.\n"
                  "\n"
                  "  -h, --help            Show this help text.\n",
                  prog, prog, prog);
  }

  static bool parseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      auto next = [&](const char* flag) -> const char* {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "%s requires a value\n", flag);
          return nullptr;
        }
        return argv[++i];
      };
      if (a == "-h" || a == "--help") {
        printUsage(argv[0]);
        opt.help_requested = true;
        return false;
      } else if (a == "-f" || a == "--file") {
        const char* v = next(a.c_str());
        if (!v) return false;
        opt.input_file = v;
      } else if (a == "-o" || a == "--output") {
        const char* v = next(a.c_str());
        if (!v) return false;
        opt.output_file = v;
      } else if (a == "--stdout") {
        opt.to_stdout = true;
      } else if (a == "--no-play") {
        opt.no_play = true;
      } else if (a == "--vocoder") {
        const char* v = next(a.c_str());
        if (!v) return false;
        opt.vocoder = v;
        opt.vocoder_explicit = true;
        if (opt.vocoder != "formant" && opt.vocoder != "phoneme" && opt.vocoder != "diphone" &&
            opt.vocoder != "psola") {
          std::fprintf(stderr, "--vocoder must be formant, phoneme, diphone, or psola, got: %s\n", v);
          return false;
        }
      } else if (a == "--language" || a == "--lang") {
        const char* v = next(a.c_str());
        if (!v) return false;
        opt.language = v;
        if (opt.language != "en" && opt.language != "de" && opt.language != "fr" &&
            opt.language != "es") {
          std::fprintf(stderr, "--language must be en, de, fr, or es, got: %s\n", v);
          return false;
        }
      } else if (a == "--pitch") {
        const char* v = next(a.c_str());
        if (!v) return false;
        opt.pitch_hz = std::stof(v);
      } else if (a == "--speed") {
        const char* v = next(a.c_str());
        if (!v) return false;
        opt.speed = std::stof(v);
        if (opt.speed <= 0.0f) {
          std::fprintf(stderr, "--speed must be > 0, got: %s\n", v);
          return false;
        }
      } else if (a == "--full-dict") {
        opt.full_dict = true;
      } else if (a == "--neural") {
        opt.use_neural = true;
      } else if (!a.empty() && a[0] == '-' && a != "-") {
        std::fprintf(stderr, "unknown option: %s\n", a.c_str());
        printUsage(argv[0]);
        return false;
      } else {
        opt.text = a;
        opt.have_text = true;
      }
    }
    return true;
  }

  static bool resolveText(const Options& opt, std::string& out) {
    if (!opt.input_file.empty()) {
      std::ifstream f(opt.input_file);
      if (!f) {
        std::fprintf(stderr, "cannot open %s\n", opt.input_file.c_str());
        return false;
      }
      std::ostringstream ss;
      ss << f.rdbuf();
      out = ss.str();
      return true;
    }
    if (opt.have_text) {
      out = opt.text;
      return true;
    }
    if (isatty(fileno(stdin))) {
      std::fprintf(stderr, "No text given (and stdin is a terminal, not a pipe) -- pass TEXT, --file, or pipe input.\n");
      return false;
    }
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    out = ss.str();
    return true;
  }

  /**
   * @brief Wires g2p_ (a G2PHybridModel) to the small built-in dictionary
   * plus rule-based fallback for `opt.language`, mirroring
   * G2PDictionaryAndRulesModel's own dictionary-then-rules composition but
   * picking the language-specific dictionary/rules pair instead of always
   * English's.
   */
  bool buildG2P(const Options& opt) {
    const PhonemeDictionaryBase* dict = &COMPACT_PHONEME_DICTIONARY_EN;
    G2PModelBase* rules = &g2pRulesEN_;
    if (opt.language == "de") {
      dict = &COMPACT_PHONEME_DICTIONARY_DE;
      rules = &g2pRulesDE_;
    } else if (opt.language == "fr") {
      dict = &COMPACT_PHONEME_DICTIONARY_FR;
      rules = &g2pRulesFR_;
    } else if (opt.language == "es") {
      dict = &COMPACT_PHONEME_DICTIONARY_ES;
      rules = &g2pRulesES_;
    } else if (opt.language != "en") {
      std::fprintf(stderr, "--language must be en, de, fr, or es, got: %s\n",
                    opt.language.c_str());
      return false;
    }
    g2pDictionaryModel_.useCompactDictionary(*dict);
    g2p_.addModel(g2pDictionaryModel_);
    if (opt.use_neural) {
      if (opt.language == "en") {
        if (g2pNeuralModel_.begin(G2P_NEURAL_MODEL_WEIGHTS_EN, G2P_NEURAL_MODEL_WEIGHTS_EN_LEN)) {
          g2p_.addModel(g2pNeuralModel_);
        } else {
          std::fprintf(stderr, "--neural: failed to load neural G2P weights -- falling back to rules only\n");
        }
      } else {
        std::fprintf(stderr,
                      "--neural is English-only for now (de/fr/es weights exist but aren't yet "
                      "wired into the engine's output table -- see docs/ADDING_A_LANGUAGE.md) "
                      "-- ignoring for --language %s\n",
                      opt.language.c_str());
      }
    }
    g2p_.addModel(*rules);
    return true;
  }

  bool buildVocoder(const Options& opt) {
    std::string vocoder = opt.vocoder;
    if (opt.language != "en") {
      // ArpabetWAVDictionary now also has real recordings for the ~23
      // international phonemes PhonemeDictionaryDE/FR/ES.h actually use
      // (see setup/audio/phonemes-from-espeak/
      // generate_international_phonemes_mbrola.sh), but DiphoneWAVDictionary
      // is still English-only (no international diphone pairs), and
      // ArpabetWAVDictionary itself doesn't cover every one of the ~78
      // international ids -- only the ones the bundled DE/FR/ES
      // dictionaries/rules reference. phoneme/psola can therefore work for
      // German/French/Spanish text now, with occasional per-phoneme
      // fallback-to-base gaps (logged via TTS_LOGW); diphone still can't.
      // FormantVocoder remains the only vocoder with full international
      // coverage, so it's still the default unless overridden.
      if (!opt.vocoder_explicit) {
        vocoder = "formant";
      } else if (vocoder == "diphone") {
        std::fprintf(stderr,
                      "warning: --vocoder diphone has no %s diphone recordings -- expect "
                      "missing or wrong sounds; --vocoder formant or phoneme/psola (partial "
                      "coverage) work better for --language %s\n",
                      opt.language.c_str(), opt.language.c_str());
      }
    }
    if (vocoder == "formant") {
      sample_rate_ = 16000;
      vocoder_.reset(new FormantVocoder(sample_rate_));
    } else if (vocoder == "diphone") {
      // DiphoneWAVDictionary.h only defines the raw DIPHONES/NUM_DIPHONES
      // SoundEntry array (unlike ArpabetWAVDictionary.h, which also
      // builds a ready-made AudioDictionary global) -- wrap it here, same
      // as examples/AudioDiphones does.
      diphone_dict_.reset(new AudioDictionary(DIPHONES, NUM_DIPHONES, 8000, PhonemeType::ARPAbet, 1, 16));
      sample_rate_ = diphone_dict_->sampleRate();
      vocoder_.reset(new DiphoneVocoder(*diphone_dict_));
    } else if (vocoder == "psola") {
      sample_rate_ = ArpabetWAVDictionary.sampleRate();
      vocoder_.reset(new PSOLAVocoder(ArpabetWAVDictionary));
    } else {
      sample_rate_ = ArpabetWAVDictionary.sampleRate();
      vocoder_.reset(new PhonemeVocoder(ArpabetWAVDictionary));
    }
    return true;
  }

  static void onAudioStatic(const int16_t* pcm, size_t n) {
    if (active_) active_->samples_.insert(active_->samples_.end(), pcm, pcm + n);
  }

  // Same RIFF/WAVE header layout as TinyTTS's own desktop/DesktopMain.h,
  // targeting an arbitrary ostream (a file or std::cout) so -o/--output
  // and --stdout can share one implementation.
  static void writeWav(std::ostream& f, const std::vector<int16_t>& pcm, uint32_t sample_rate) {
    uint32_t data_bytes = (uint32_t)(pcm.size() * sizeof(int16_t));
    uint32_t byte_rate = sample_rate * 1 /*channels*/ * 2 /*bytes/sample*/;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    uint32_t riff_size = 36 + data_bytes;

    f.write("RIFF", 4);
    f.write((const char*)&riff_size, 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    uint32_t fmt_size = 16;
    f.write((const char*)&fmt_size, 4);
    uint16_t audio_format = 1;  // PCM
    uint16_t num_channels = 1;
    f.write((const char*)&audio_format, 2);
    f.write((const char*)&num_channels, 2);
    f.write((const char*)&sample_rate, 4);
    f.write((const char*)&byte_rate, 4);
    f.write((const char*)&block_align, 2);
    f.write((const char*)&bits_per_sample, 2);
    f.write("data", 4);
    f.write((const char*)&data_bytes, 4);
    f.write((const char*)pcm.data(), data_bytes);
    f.flush();
  }

  G2PDictionaryModel g2pDictionaryModel_;
  G2PNeuralModel g2pNeuralModel_;  ///< neural GRU fallback, opt-in via --neural (EN only)
  G2PRuleBasedModelEN g2pRulesEN_;
  G2PRuleBasedModelDE g2pRulesDE_;
  G2PRuleBasedModelFR g2pRulesFR_;
  G2PRuleBasedModelES g2pRulesES_;
  G2PHybridModel g2p_;                             ///< dictionary + language-specific rules,
                                                    ///< wired up by buildG2P()
  std::unique_ptr<AudioDictionary> diphone_dict_;  // must outlive vocoder_
  std::unique_ptr<VocoderBase> vocoder_;
  int sample_rate_ = 8000;
  std::unique_ptr<audio_tools::MiniAudioStream> i2s_out_;
  std::vector<int16_t> samples_;

  static inline DesktopMain* active_ = nullptr;
};

}  // namespace tinyttstools
