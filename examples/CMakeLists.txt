
# Download miniaudio.h to examples-libraries if it doesn't exist. Must
# match add_tinytts_example()'s own include dir (top-level CMakeLists.txt:
# ${CMAKE_SOURCE_DIR}/examples-libraries) -- using CMAKE_SOURCE_DIR here
# too, rather than a relative ../../ off CMAKE_CURRENT_SOURCE_DIR, avoids
# the two silently drifting apart again.
set(MINIAUDIO_PATH "${CMAKE_SOURCE_DIR}/examples-libraries/miniaudio.h")
if(NOT EXISTS ${MINIAUDIO_PATH})
    file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
        ${MINIAUDIO_PATH})
endif()

# Examples that don't require AudioTools (always build)
add_subdirectory(G2PCustomDictionary)

# G2PNeural example: dependency-free (no TensorFlow Lite) GRU-based neural
# G2P fallback -- see G2PNeuralModel.h.
add_subdirectory(G2PNeural)

# Examples that require AudioTools (build only when ADD_AUDIO_TOOLS is ON)
if(ADD_AUDIO_TOOLS)
    message(STATUS "Building AudioTools-dependent examples")
    add_subdirectory(AudioFormant)
    add_subdirectory(AudioPhoneme)
    add_subdirectory(AudioDiphones)
    add_subdirectory(AudioPSOLA)
    add_subdirectory(Tests/test-sound)
    add_subdirectory(Tests/test-wav)
else()
    message(STATUS "Skipping AudioTools-dependent examples (ADD_AUDIO_TOOLS=OFF)")
endif()
