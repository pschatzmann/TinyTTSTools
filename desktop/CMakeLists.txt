# Desktop CLI: ./tinyttstools "some text" -- MiniAudioStream output
# (arduino-audio-tools, already linked into the tinyttstools INTERFACE
# target by the top-level CMakeLists.txt when ADD_AUDIO_TOOLS is on).
#
#   cmake -S . -B build -DBUILD_DESKTOP_MAIN=ON
#   cmake --build build
#   ./build/desktop/tinyttstools "Hello world!"
#
# Mirrors the sibling TinyTTS project's own desktop/CMakeLists.txt --
# see its comments for the reasoning behind each piece below; this file
# only notes what's different for TinyTTSTools.

# audio-tools doesn't vendor miniaudio.h itself (single-header
# github.com/mackron/miniaudio) -- every arduino-audio-tools example that
# uses MiniAudioStream keeps its own copy for the same reason. Prefer a
# sibling checkout's own copy (e.g. arduino-audio-tools/tests-cmake/*/miniaudio.h)
# if one exists, matching this project's existing "prefer local sibling
# checkout" convention (see the top-level CMakeLists.txt's
# ARDUINO_AUDIO_TOOLS_LOCAL_DIR) -- faster, no network dependency.
set(MINIAUDIO_LOCAL_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../arduino-audio-tools/tests-cmake/miniaudio"
    CACHE PATH "Directory containing a local miniaudio.h to use instead of fetching from GitHub")

if(EXISTS "${MINIAUDIO_LOCAL_DIR}/miniaudio.h")
    message(STATUS "Using local miniaudio.h at ${MINIAUDIO_LOCAL_DIR}")
    set(miniaudio_SOURCE_DIR "${MINIAUDIO_LOCAL_DIR}")
else()
    message(STATUS "No local miniaudio.h found -- fetching from GitHub")
    include(FetchContent)
    # Deliberately FetchContent_Populate(), not FetchContent_MakeAvailable():
    # the latter also runs add_subdirectory() on miniaudio's own
    # CMakeLists.txt, which defines an installable libminiaudio.a target we
    # never build or link -- `cmake --install` then fails outright looking
    # for that file.
    FetchContent_Declare(miniaudio
      GIT_REPOSITORY "https://github.com/mackron/miniaudio.git"
      GIT_TAG master)
    FetchContent_GetProperties(miniaudio)
    if(NOT miniaudio_POPULATED)
        FetchContent_Populate(miniaudio)
    endif()
endif()

add_executable(tinyttstools_desktop main.cpp)
set_target_properties(tinyttstools_desktop PROPERTIES OUTPUT_NAME tinyttstools)
target_compile_features(tinyttstools_desktop PRIVATE cxx_std_17)
target_link_libraries(tinyttstools_desktop PRIVATE tinyttstools)
target_include_directories(tinyttstools_desktop PRIVATE ${miniaudio_SOURCE_DIR})
# NO_MAIN: AudioTools/AudioLibs/Emulation/Main.h (pulled in transitively by
# IS_MIN_DESKTOP) otherwise defines its own int main(void) -- conflicts with
# this target's own main.cpp.
target_compile_definitions(tinyttstools_desktop PRIVATE IS_MIN_DESKTOP NO_MAIN)

# Needed on Linux: miniaudio's implementation needs libm/libdl for its
# backend probing; std::thread (audio-tools' desktop Task) links pthread.
find_package(Threads REQUIRED)
target_link_libraries(tinyttstools_desktop PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)

# See TinyTTS's own desktop/CMakeLists.txt for why this is linked
# unconditionally (32-bit ARM without native 8-byte atomics lowers
# std::atomic<uint64_t> ops, used throughout miniaudio, to __atomic_*_8
# libcalls in libatomic; every other platform simply doesn't use it).
target_link_libraries(tinyttstools_desktop PRIVATE atomic)

install(TARGETS tinyttstools_desktop RUNTIME DESTINATION bin)
