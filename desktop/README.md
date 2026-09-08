# TinyTTSTools Desktop CLI

A small command-line tool for synthesizing speech with TinyTTSTools on
desktop -- either played live through your speakers or written to a WAV
file -- without building/running a full `.ino` example. Mirrors the
sibling [TinyTTS](https://github.com/pschatzmann/TinyTTS) project's own
`desktop/` CLI, adapted to TinyTTSTools' G2P-model + Vocoder + Print
architecture.

Not part of the Arduino/ESP-IDF library surface at all -- nothing under
`src/` includes anything here, and it's only built when explicitly
requested (see below).

## Why this exists

Building and running an `.ino` example directly on desktop mostly works,
but has two rough edges this CLI avoids:
- The example's text is hard-coded in its `.ino` file -- there's no way to
  try your own text without editing and recompiling.
- The process never exits on its own (`loop()` repeats forever, matching
  real-hardware behavior), and every audio example uses `I2SStream`, so
  there's no built-in way to capture the output to a file instead of
  playing it live.

## Build

```bash
cd .. # repository root
mkdir -p build && cd build
cmake .. -DBUILD_DESKTOP_MAIN=ON
cmake --build . --target tinyttstools_desktop -j4
```

Produces `build/desktop/tinyttstools`.

## Usage

```
Usage: ./tinyttstools [options] [TEXT]

Input:
  TEXT                  Text to speak. If omitted (and --file isn't given),
                        read from stdin -- e.g. echo "hi" | ./tinyttstools
  -f, --file FILE       Read text from FILE instead.

Output:
  -o, --output FILE     Write synthesized audio to FILE as a WAV file,
                        instead of playing it.
  --stdout              Write WAV bytes to stdout instead of playing --
                        for piping, e.g. ./tinyttstools --stdout | aplay, or > out.wav
  --no-play             Skip playback (implied by -o/--stdout).

Voice:
  --vocoder NAME        formant | phoneme | diphone (default: phoneme)
                        formant: procedural, no audio data, most robotic.
                        phoneme: pre-recorded phoneme samples (ArpabetWAVDictionary).
                        diphone: pre-recorded diphone samples, most natural
                        (DiphoneWAVDictionary).
  --full-dict           Use the full ~123k-word CMU dictionary instead of the
                        small built-in one (see docs/TUTORIAL.md).

  -h, --help            Show this help text.
```

## Examples

```bash
./tinyttstools "The quick brown fox"                       # play live
./tinyttstools --vocoder diphone "The quick brown fox"     # most natural voice
./tinyttstools -o fox.wav "The quick brown fox"            # capture to a file
echo "Hello there" | ./tinyttstools --stdout > hello.wav   # pipe-friendly
./tinyttstools --full-dict "supercalifragilisticexpialidocious"
```

## Notes

- Needs `ADD_AUDIO_TOOLS=ON` (the default) for `MiniAudioStream` playback.
- Fetches (or reuses a local sibling checkout of)
  [miniaudio](https://github.com/mackron/miniaudio) the first time it's
  configured -- see `CMakeLists.txt` for why (a single header, not vendored
  by arduino-audio-tools itself).
- `G2PDictionaryAndRulesModel` is always used for text-to-phoneme
  conversion (add `--full-dict` for the full CMU dictionary tier); the
  neural G2P fallback isn't wired into this CLI.
