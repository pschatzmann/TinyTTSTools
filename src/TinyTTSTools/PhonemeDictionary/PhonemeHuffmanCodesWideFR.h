/**
 * @file PhonemeHuffmanCodesWideFR.h
 * @brief Canonical Huffman codes for FR's widened packed
 * phoneme symbols, derived from the OLaPh FR corpus.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include "CompressedPhonemeDictionaryWide.h"

/**
 * @brief 40 distinct packed symbols, 365510 total
 * instances across the parsed OLaPh FR corpus (see
 * setup/dictionary-fr/olaph_fr.txt and
 * setup/dictionary-common/olaph_parse.py, olaph_build_huffman.py --
 * regenerate via
 * `python3 olaph_build_huffman.py --lang fr` (run from
 * setup/dictionary-common/) if the corpus or its parsing changes). Sorted
 * by (length, packedSymbol)
 * ascending -- canonical Huffman requires this order (see
 * CompressedPhonemeDictionaryWide's decoder).
 */
static constexpr PhonemeHuffmanCodeWide PHONEME_HUFFMAN_CODES_WIDE_FR[] = {
    {340, 0b000, 3},  // RU
    {36, 0b0010, 4},  // EH
    {56, 0b0011, 4},  // IY
    {96, 0b0100, 4},  // T
    {112, 0b0101, 4},  // S
    {444, 0b0110, 4},  // EP
    {472, 0b0111, 4},  // AF
    {24, 0b10000, 5},  // AO
    {80, 0b10001, 5},  // D
    {88, 0b10010, 5},  // K
    {92, 0b10011, 5},  // P
    {144, 0b10100, 5},  // L
    {156, 0b10101, 5},  // Y
    {160, 0b10110, 5},  // M
    {164, 0b10111, 5},  // N
    {188, 0b11000, 5},  // AN
    {196, 0b11001, 5},  // ON
    {20, 0b110100, 6},  // AH0
    {72, 0b110101, 6},  // UW
    {76, 0b110110, 6},  // B
    {84, 0b110111, 6},  // G
    {104, 0b111000, 6},  // F
    {124, 0b111001, 6},  // V
    {128, 0b111010, 6},  // Z
    {132, 0b111011, 6},  // ZH
    {172, 0b111100, 6},  // UF
    {116, 0b1111010, 7},  // SH
    {152, 0b1111011, 7},  // W
    {192, 0b1111100, 7},  // EN
    {448, 0b1111101, 7},  // OP
    {180, 0b11111100, 8},  // OF
    {184, 0b11111101, 8},  // OE
    {368, 0b11111110, 8},  // HU
    {228, 0b111111110, 9},  // NY
    {168, 0b1111111110, 10},  // NG
    {200, 0b11111111110, 11},  // UN
    {8, 0b111111111110, 12},  // AA
    {1876, 0b1111111111110, 13},  // RU+mod3
    {52, 0b11111111111110, 14},  // IH
    {300, 0b11111111111111, 14},  // CJ
};

static constexpr size_t PHONEME_HUFFMAN_CODE_WIDE_FR_COUNT = 40;
