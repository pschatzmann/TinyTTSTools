/**
 * @file PhonemeHuffmanCodesWideES.h
 * @brief Canonical Huffman codes for ES's widened packed
 * phoneme symbols, derived from the OLaPh ES corpus.
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include "CompressedPhonemeDictionaryWide.h"

/**
 * @brief 62 distinct packed symbols, 519727 total
 * instances across the parsed OLaPh ES corpus (see
 * setup/dictionary-es/olaph_es.txt and
 * setup/dictionary-common/olaph_parse.py, olaph_build_huffman.py --
 * regenerate via
 * `python3 olaph_build_huffman.py --lang es` (run from
 * setup/dictionary-common/) if the corpus or its parsing changes). Sorted
 * by (length, packedSymbol)
 * ascending -- canonical Huffman requires this order (see
 * CompressedPhonemeDictionaryWide's decoder).
 */
static constexpr PhonemeHuffmanCodeWide PHONEME_HUFFMAN_CODES_WIDE_ES[] = {
    {444, 0b000, 3},  // EP
    {448, 0b001, 3},  // OP
    {472, 0b010, 3},  // AF
    {56, 0b0110, 4},  // IY
    {96, 0b0111, 4},  // T
    {112, 0b1000, 4},  // S
    {164, 0b1001, 4},  // N
    {220, 0b1010, 4},  // RT
    {88, 0b10110, 5},  // K
    {92, 0b10111, 5},  // P
    {100, 0b11000, 5},  // DH
    {144, 0b11001, 5},  // L
    {160, 0b11010, 5},  // M
    {72, 0b110110, 6},  // UW
    {80, 0b110111, 6},  // D
    {120, 0b111000, 6},  // TH
    {156, 0b111001, 6},  // Y
    {240, 0b111010, 6},  // BETA
    {76, 0b1110110, 7},  // B
    {104, 0b1110111, 7},  // F
    {208, 0b1111000, 7},  // X
    {221, 0b1111001, 7},  // RT1
    {224, 0b1111010, 7},  // RR
    {312, 0b1111011, 7},  // GH
    {84, 0b11111000, 8},  // G
    {152, 0b11111001, 8},  // W
    {97, 0b111110100, 9},  // T1
    {116, 0b111110101, 9},  // SH
    {121, 0b111110110, 9},  // TH1
    {168, 0b111110111, 9},  // NG
    {228, 0b111111000, 9},  // NY
    {232, 0b111111001, 9},  // LY
    {89, 0b1111110100, 10},  // K1
    {113, 0b1111110101, 10},  // S1
    {128, 0b1111110110, 10},  // Z
    {145, 0b1111110111, 10},  // L1
    {161, 0b1111111000, 10},  // M1
    {165, 0b1111111001, 10},  // N1
    {308, 0b1111111010, 10},  // JZ
    {81, 0b11111110110, 11},  // D1
    {93, 0b11111110111, 11},  // P1
    {101, 0b11111111000, 11},  // DH1
    {209, 0b11111111001, 11},  // X1
    {241, 0b11111111010, 11},  // BETA1
    {313, 0b11111111011, 11},  // GH1
    {57, 0b111111111000, 12},  // IY1
    {105, 0b111111111001, 12},  // F1
    {225, 0b111111111010, 12},  // RR1
    {77, 0b1111111110110, 13},  // B1
    {229, 0b1111111110111, 13},  // NY1
    {233, 0b1111111111000, 13},  // LY1
    {309, 0b1111111111001, 13},  // JZ1
    {445, 0b1111111111010, 13},  // EP1
    {449, 0b1111111111011, 13},  // OP1
    {473, 0b1111111111100, 13},  // AF1
    {73, 0b11111111111010, 14},  // UW1
    {85, 0b11111111111011, 14},  // G1
    {136, 0b11111111111100, 14},  // CH
    {157, 0b11111111111101, 14},  // Y1
    {169, 0b11111111111110, 14},  // NG1
    {129, 0b111111111111110, 15},  // Z1
    {153, 0b111111111111111, 15},  // W1
};

static constexpr size_t PHONEME_HUFFMAN_CODE_WIDE_ES_COUNT = 62;
