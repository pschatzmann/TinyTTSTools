/**
 * @file PhonemeHuffmanCodes.h
 * @brief Canonical Huffman codes for packed phoneme bytes
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-07
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @brief One packed phoneme byte's canonical Huffman code.
 * @details `packedByte` is the same `(phone_id << 2) | stress` value
 * CompressedPhonemeDictionary would otherwise store literally (see
 * CompactPhonemeDictionary.h); `code`/`length` are its canonical Huffman
 * codeword (MSB-first, `length` bits significant). Derived from real
 * phoneme frequencies across the full CMU dictionary + exception
 * dictionary (~1.45M phoneme instances) -- regenerate via
 * setup/dictionary/build_huffman.py if that corpus changes.
 *
 * Sorted by (length, packedByte) ascending -- canonical Huffman requires
 * this order: codes of the same length are consecutive integers, and code
 * length only increases down the table, which is what makes fast
 * table-driven decoding possible without walking a tree (see
 * CompressedPhonemeDictionary's decoder).
 */
struct PhonemeHuffmanCode {
  uint8_t packedByte;
  uint16_t code;
  uint8_t length;
};

static constexpr PhonemeHuffmanCode PHONEME_HUFFMAN_CODES[] = {
    {88, 0b0000, 4},  // K
    {96, 0b0001, 4},  // T
    {112, 0b0010, 4},  // S
    {144, 0b0011, 4},  // L
    {148, 0b0100, 4},  // R
    {164, 0b0101, 4},  // N
    {16, 0b01100, 5},  // AH
    {20, 0b01101, 5},  // AH0
    {52, 0b01110, 5},  // IH
    {56, 0b01111, 5},  // IY
    {76, 0b10000, 5},  // B
    {80, 0b10001, 5},  // D
    {92, 0b10010, 5},  // P
    {128, 0b10011, 5},  // Z
    {160, 0b10100, 5},  // M
    {8, 0b101010, 6},  // AA
    {12, 0b101011, 6},  // AE
    {36, 0b101100, 6},  // EH
    {37, 0b101101, 6},  // EH1
    {40, 0b101110, 6},  // ER
    {44, 0b101111, 6},  // ER0
    {60, 0b110000, 6},  // OW
    {84, 0b110001, 6},  // G
    {104, 0b110010, 6},  // F
    {124, 0b110011, 6},  // V
    {168, 0b110100, 6},  // NG
    {9, 0b1101010, 7},  // AA1
    {13, 0b1101011, 7},  // AE1
    {24, 0b1101100, 7},  // AO
    {32, 0b1101101, 7},  // AY
    {48, 0b1101110, 7},  // EY
    {49, 0b1101111, 7},  // EY1
    {53, 0b1110000, 7},  // IH1
    {57, 0b1110001, 7},  // IY1
    {72, 0b1110010, 7},  // UW
    {108, 0b1110011, 7},  // HH
    {116, 0b1110100, 7},  // SH
    {136, 0b1110101, 7},  // CH
    {140, 0b1110110, 7},  // JH
    {152, 0b1110111, 7},  // W
    {156, 0b1111000, 7},  // Y
    {17, 0b11110010, 8},  // AH1
    {25, 0b11110011, 8},  // AO1
    {33, 0b11110100, 8},  // AY1
    {41, 0b11110101, 8},  // ER1
    {61, 0b11110110, 8},  // OW1
    {73, 0b11110111, 8},  // UW1
    {120, 0b11111000, 8},  // TH
    {10, 0b111110010, 9},  // AA2
    {14, 0b111110011, 9},  // AE2
    {28, 0b111110100, 9},  // AW
    {29, 0b111110101, 9},  // AW1
    {34, 0b111110110, 9},  // AY2
    {38, 0b111110111, 9},  // EH2
    {50, 0b111111000, 9},  // EY2
    {54, 0b111111001, 9},  // IH2
    {62, 0b111111010, 9},  // OW2
    {68, 0b111111011, 9},  // UH
    {26, 0b1111111000, 10},  // AO2
    {58, 0b1111111001, 10},  // IY2
    {69, 0b1111111010, 10},  // UH1
    {100, 0b1111111011, 10},  // DH
    {18, 0b11111111000, 11},  // AH2
    {30, 0b11111111001, 11},  // AW2
    {42, 0b11111111010, 11},  // ER2
    {64, 0b11111111011, 11},  // OY
    {65, 0b11111111100, 11},  // OY1
    {74, 0b11111111101, 11},  // UW2
    {132, 0b11111111110, 11},  // ZH
    {66, 0b111111111110, 12},  // OY2
    {70, 0b111111111111, 12},  // UH2
};

static constexpr size_t PHONEME_HUFFMAN_CODE_COUNT = 71;
