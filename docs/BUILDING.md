# Building on Desktop

TinyTTSTools is a header-only Arduino library, but the whole thing --
library, examples, and regression tests -- also builds as plain desktop
C++ via CMake. This is the fastest way to iterate: no board, no upload
step, and the full test suite runs in well under a second.

## Prerequisites

- CMake 3.16+
- A C++17 compiler (g++ or clang++)
- Internet access the first time you configure (CMake's `FetchContent`
  pulls in [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)
  for the audio-output examples; skip this with `-DADD_AUDIO_TOOLS=OFF`
  if you only want the library/tests and have no network access)

No Arduino IDE, no board packages, no TensorFlow Lite -- none of that is
needed for a desktop build.

## Configure and build

From the repository root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j4
```

Useful CMake options (pass as `-D<OPTION>=<VALUE>` to the `cmake ..` step):

| Option | Default | Effect |
|---|---|---|
| `BUILD_EXAMPLES` | `ON` | Build everything under `examples/` |
| `BUILD_TESTS` | `ON` | Build the regression tests under `tests/` |
| `ADD_AUDIO_TOOLS` | `ON` | Fetch arduino-audio-tools and build the audio-output examples (`AudioFormant`, `AudioPhoneme`, `AudioDiphones`) that need it |
| `BUILD_DESKTOP_MAIN` | `OFF` | Build the desktop CLI (`./tinyttstools`, see [desktop/README.md](../desktop/README.md)) -- the actual way to hear synthesized speech on desktop; see the note below about why the `.ino` examples can't do this directly |

For example, a fast library+tests-only configure with no network
dependency:

```bash
cmake .. -DADD_AUDIO_TOOLS=OFF
cmake --build . -j4
```

If you change `CMakeLists.txt` (in the repo root, `examples/`, or
`tests/`) after the first configure, re-run `cmake ..` before building
again so it picks up new targets.

## Running the regression tests

```bash
cd build
ctest --output-on-failure
```

This runs every test under `tests/` (plain C++, no AudioTools dependency)
and reports pass/fail per test, e.g.:

```
Test project .../TinyTTSTools/build
    Start 1: test_formant_vocoder
1/8 Test #1: test_formant_vocoder ..............   Passed    0.02 sec
    Start 2: test_concatenated_audio_vocoder
2/8 Test #2: test_concatenated_audio_vocoder ...   Passed    0.01 sec
...
100% tests passed, 0 tests failed out of 8
```

To run a single test directly (useful when iterating on one area), either
filter by name:

```bash
ctest -R test_diphones --output-on-failure
```

or build and run its executable directly (each test is also a standalone
binary under `build/tests/`):

```bash
cmake --build . --target test_diphones -j4
./tests/test_diphones
```

Adding a new test file under `tests/` requires one line in
`tests/CMakeLists.txt` (`add_tinytts_test(test_your_new_file)`) and a
re-run of `cmake ..` to pick it up.

## Building a single example

Each example under `examples/` is a normal CMake target (built from its
`.ino` file, compiled as C++). Build just one instead of everything:

```bash
cmake --build . --target AudioDiphones -j4
```

Examples that don't need `ADD_AUDIO_TOOLS` (`G2PCustomDictionary`,
`G2PNeural` -- phoneme conversion only, no audio output) build even with
that option off.

Note: example binaries call `TTSExample::waitForSerial()` in `setup()` --
on real hardware that's the point (wait for you to open the serial
monitor), and on the desktop emulation it turns out `Serial` reports
ready immediately, so `setup()`/`loop()` actually run rather than hanging
as you might expect. That means running an example binary directly *does*
synthesize and play audio through the desktop's real audio device --  but
`loop()` repeats forever (matching the real-hardware examples' own
`while(true)`-style behavior), so the process itself never exits on its
own; run it under `timeout` for a one-shot listen (e.g.
`timeout 15 ./examples/AudioPhoneme/AudioPhoneme`), or use the desktop CLI
below for a proper one-shot, scriptable way to render or play arbitrary
text.

## Desktop CLI (`./tinyttstools`)

For actually listening to (or capturing) synthesized speech on desktop --
rather than building an `.ino` example and guessing at its hard-coded
phrase -- build the desktop CLI:

```bash
cmake .. -DBUILD_DESKTOP_MAIN=ON
cmake --build . --target tinyttstools_desktop -j4
./desktop/tinyttstools "Hello world"                     # plays through your speakers
./desktop/tinyttstools -o out.wav "Hello world"           # writes a WAV file instead
echo "Hello world" | ./desktop/tinyttstools --stdout | aplay  # pipe-friendly
```

See [desktop/README.md](../desktop/README.md) for the full option list
(`--vocoder formant|phoneme|diphone`, `--full-dict`, `--file`, ...). This
target needs `ADD_AUDIO_TOOLS=ON` (the default) for `MiniAudioStream`
playback, and fetches (or reuses a local sibling checkout of)
[miniaudio](https://github.com/mackron/miniaudio) the first time it's
configured.

## Troubleshooting

- **`FetchContent` network timeout on first configure**: either fix your
  network access, or configure with `-DADD_AUDIO_TOOLS=OFF` if you only
  need the library and its tests.
- **Stale example/test list after editing a `CMakeLists.txt`**: re-run
  `cmake ..` in `build/` -- CMake doesn't pick up new targets or file
  lists without a reconfigure.
