# Plain-C++ regression tests (no Arduino .ino wrapping needed -- unlike
# examples/, these don't exercise any AudioTools-specific I/O). Each
# test_*.cpp is its own standalone executable; add it to ctest so `ctest`
# (or `make test`) runs all of them and reports pass/fail per test.

function(add_tinytts_test NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_compile_features(${NAME} PRIVATE cxx_std_17)
    target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${NAME} PRIVATE tinyttstools)
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

add_tinytts_test(test_formant_vocoder)
add_tinytts_test(test_concatenated_audio_vocoder)
add_tinytts_test(test_g2p_rulebased)
add_tinytts_test(test_dictionaries)
add_tinytts_test(test_diphones)
add_tinytts_test(test_audio_format_decoder)
add_tinytts_test(test_phoneme_vocoder)
add_tinytts_test(test_g2p_neural)
