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
| `ADD_AUDIO_TOOLS` | `ON` | Fetch arduino-audio-tools and build the audio-output examples (`AudioFormant`, `AudioPhoneme`, `AudioBiphones`) that need it |

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
cmake --build . --target AudioBiphones -j4
```

Examples that don't need `ADD_AUDIO_TOOLS` (`G2PCustomDictionary`,
`G2PNeural` -- phoneme conversion only, no audio output) build even with
that option off.

Note: example binaries call `TTSExample::waitForSerial()` in `setup()`,
which blocks until a serial connection is available -- on real hardware
that's the point (wait for you to open the serial monitor), but it means
running an example binary directly in a desktop/CI shell will hang. Build
them to verify they compile; run them on an actual board (or adapt
`setup()` for a one-shot desktop demo, as needed) to see their output.

## Troubleshooting

- **`FetchContent` network timeout on first configure**: either fix your
  network access, or configure with `-DADD_AUDIO_TOOLS=OFF` if you only
  need the library and its tests.
- **Stale example/test list after editing a `CMakeLists.txt`**: re-run
  `cmake ..` in `build/` -- CMake doesn't pick up new targets or file
  lists without a reconfigure.
