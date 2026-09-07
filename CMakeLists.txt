cmake_minimum_required(VERSION 3.16)

# set the project name
project(tinyttstools)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

# Build a desktop test executable directly from an Arduino .ino sketch, with
# no per-example .cpp wrapper needed: the .ino is compiled as C++ (CMake
# otherwise ignores unknown extensions), IS_MIN_DESKTOP is defined (this is
# what TinyTTSTools.h checks to alias I2SStream to AudioTools' desktop
# MiniAudioStream backend), and it's linked against tinyttstools.
function(add_tinytts_example NAME INO_FILE)
    # LANGUAGE CXX alone only tells CMake which compiler/flags to use; GCC/Clang
    # still refuse to compile a ".ino" extension without an explicit "-x c++"
    # (unrecognized extensions are otherwise assumed to be link inputs).
    set_source_files_properties(${INO_FILE} PROPERTIES
        LANGUAGE CXX
        COMPILE_OPTIONS "-x;c++")
    add_executable(${NAME} ${INO_FILE})
    # C++17 is required for CompactPhonemeDictionaryBuilder.h's constexpr
    # dictionary packing (lambdas and mutation/loops inside constexpr
    # functions -- C++14's "relaxed constexpr" isn't enough for the former).
    target_compile_features(${NAME} PRIVATE cxx_std_17)
    target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/examples-libraries)
    target_compile_definitions(${NAME} PRIVATE IS_MIN_DESKTOP)
    target_link_libraries(${NAME} PRIVATE tinyttstools)
endfunction()

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build regression tests (tests/)" ON)
option(ADD_AUDIO_TOOLS "Add Arduino Audio Tools library" ON)

add_library(tinyttstools INTERFACE)

# make include directory available to calling projects 
target_include_directories(tinyttstools INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add Arduino Audio Tools if requested
if(ADD_AUDIO_TOOLS)
    set(ADD_ARDUINO_EMULATOR OFF CACHE BOOL "Add Arduino Emulator Library") 
    set(ADD_PORTAUDIO OFF CACHE BOOL "Add Portaudio Library") 

    include(FetchContent)

    if(NOT TARGET arduino-audio-tools)
        # Prefer a local sibling checkout if one exists (faster, and avoids
        # a network dependency entirely -- SOURCE_DIR skips the
        # clone/fetch step altogether, unlike pointing GIT_REPOSITORY at a
        # local path, which still performs a full git clone from it).
        # Override the sibling path with -DARDUINO_AUDIO_TOOLS_LOCAL_DIR=...
        # if your checkout lives somewhere else. Falls back to fetching
        # from GitHub when no local checkout is found, so this works for
        # any developer/CI without editing this file.
        set(ARDUINO_AUDIO_TOOLS_LOCAL_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/../arduino-audio-tools"
            CACHE PATH "Local arduino-audio-tools checkout to use instead of fetching from GitHub")

        if(EXISTS "${ARDUINO_AUDIO_TOOLS_LOCAL_DIR}/CMakeLists.txt")
            message(STATUS "Using local arduino-audio-tools checkout at ${ARDUINO_AUDIO_TOOLS_LOCAL_DIR}")
            FetchContent_Declare(
                arduino_audio_tools
                SOURCE_DIR "${ARDUINO_AUDIO_TOOLS_LOCAL_DIR}"
            )
        else()
            message(STATUS "No local arduino-audio-tools checkout found -- fetching from GitHub")
            FetchContent_Declare(
                arduino_audio_tools
                GIT_REPOSITORY https://github.com/pschatzmann/arduino-audio-tools.git
                GIT_TAG main
            )
        endif()
        FetchContent_MakeAvailable(arduino_audio_tools)
    endif()
    
    # Link tinyttstools to dependency
    if(TARGET arduino-audio-tools)
        target_link_libraries(tinyttstools INTERFACE arduino-audio-tools)
    endif()
endif()

# build example if requested
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# build regression tests if requested (plain C++, no AudioTools dependency)
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
