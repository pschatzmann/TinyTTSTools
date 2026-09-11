#!/usr/bin/env bash
# Generate audio for the international/IPA-extension phonemes (Phone ids
# 43-120) actually referenced by PhonemeDictionaryDE/FR/ES.h and
# G2PRuleBasedModelDE/FR/ES.h -- the ARPAbet-only pipeline
# (generate_arpabet_phonemes_mbrola.sh) has no coverage for these, since
# ArpabetWAVDictionary was originally English-only.
#
# Same MBROLA-based approach as generate_arpabet_phonemes_mbrola.sh, for
# the same reason (an isolated phoneme rendered via espeak's `[[xx]]`
# notation at a slow rate doesn't reliably hit its requested duration --
# MBROLA takes an explicit per-phone duration and renders almost exactly
# that). Requires the mbrola-de6, mbrola-fr4, and mbrola-es1 voice
# databases (apt-get install mbrola-de6 mbrola-fr4 mbrola-es1) in addition
# to mbrola-en1.
#
# Each international phone is sourced from whichever voice's own phoneme
# inventory documentation (README.txt in the corresponding
# /usr/share/doc/mbrola-*/ package) actually defines it -- there is no
# single voice covering all of German+French+Spanish, so this picks one
# best-fit source language per phone:
#   - de6 (German):  UF, UF0, OF, OE, C, X, TS, PF, RU
#   - fr4 (French):  AN, EN, ON, UN, HU
#   - es1 (Spanish): RT, RR, NY, LY, EP, OP, AF, and (approximated, see
#                     below) BETA, GH
#
# BETA (/β/, intervocalic b/v lenition) and GH (/ɣ/, intervocalic g
# lenition) are NOT in es1's phoneme inventory (its own README documents
# this as a known limitation -- "the /b/ /B/, /d/ /D/, /g/ /G/ pairs" were
# left out of the database) -- approximated here with es1's plain "b"/"g"
# (the corresponding full stop), not a real fricative. Every other id
# below is a genuine, voice-native phone, not an approximation.
#
# A phone shared across languages with only one audio slot (RU, used by
# both the German and French dictionaries -- both call it /ʁ/, a uvular
# r) is rendered once, from German's de6 -- the French dictionary reuses
# that same recording. This is a real (if minor) cross-language accent
# approximation, documented rather than hidden.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ORIGINAL_DIR="$SCRIPT_DIR/original"
CODEC_DIR="$SCRIPT_DIR/pcm8bit"
mkdir -p "$ORIGINAL_DIR" "$CODEC_DIR"

SAMPLE_RATE=8000

# name:voiceDbPath:sampaSymbol:durationMs -- durationMs matches
# Phonemes.h's phoneme_map exactly (the same shared table
# DiphoneVocoder/PSOLAVocoder's duration math reads from).
PHONES=(
    "UF|de6|y:|150"
    "UF0|de6|Y|110"
    "OF|de6|2:|150"
    "OE|de6|9|140"
    "C|de6|C|100"
    "X|de6|x|110"
    "TS|de6|ts|90"
    "PF|de6|pf|90"
    "RU|de6|R|100"
    "AN|fr4|a~|170"
    "EN|fr4|e~|170"
    "ON|fr4|o~|170"
    "UN|fr4|9~|170"
    "HU|fr4|H|80"
    "RT|es1|r|40"
    "RR|es1|rr|150"
    "NY|es1|J|120"
    "LY|es1|L|110"
    "EP|es1|e|130"
    "OP|es1|o|130"
    "AF|es1|a|140"
    "BETA|es1|b|100"
    "GH|es1|g|100"
    # Modifier-tagged phoneme: German long /aː/ (Seg(Phone::AF,
    # PhonemeModifier::MOD_LONG) in PhonemeDictionaryDE.h, "AF:" in text --
    # see PhonemeModifiers.h). Without a dedicated recording,
    # AudioDictionary::getSoundEntry("AF:") falls back to plain "AF" (see
    # its own TTS_LOGW). "AF-" uses "-" in place of the tag's ":" --
    # illegal/unsafe in a filename -- and is mapped back to the real "AF:"
    # lookup key by generate_wav_dictionary.py's DISPLAY_NAME_OVERRIDES.
    # 220ms (vs. plain AF's 140ms) approximates German's long-vs-short
    # vowel length ratio; de6's own SAMPA marks length with a literal
    # trailing colon ("a:"), unrelated to this filename-safety workaround.
    "AF-|de6|a:|220"
)

declare -A VOICE_DB=(
    [de6]="/usr/share/mbrola/de6/de6"
    [fr4]="/usr/share/mbrola/fr4/fr4"
    [es1]="/usr/share/mbrola/es1/es1"
)

check_dependencies() {
    command -v mbrola >/dev/null 2>&1 || { echo "Error: mbrola not found"; exit 1; }
    command -v sox >/dev/null 2>&1 || { echo "Error: sox not found"; exit 1; }
    for voice in "${!VOICE_DB[@]}"; do
        [ -f "${VOICE_DB[$voice]}" ] || {
            echo "Error: MBROLA voice database not found at ${VOICE_DB[$voice]}"
            echo "  install with: sudo apt-get install mbrola-$voice"
            exit 1
        }
    done
}

generate_phoneme() {
    local entry=$1
    local ph voice sampa dur
    IFS='|' read -r ph voice sampa dur <<< "$entry"
    local db="${VOICE_DB[$voice]}"
    local original="$ORIGINAL_DIR/$ph.wav"
    local codec="$CODEC_DIR/$ph.wav"
    local pho="$ORIGINAL_DIR/temp_$ph.pho"

    if [ -f "$original" ] && [ -f "$codec" ]; then
        echo "Skipping existing: $ph.wav"
        return
    fi

    printf "%s %s\n" "$sampa" "$dur" > "$pho"

    if mbrola "$db" "$pho" "$original" 2>/dev/null; then
        rm -f "$pho"
        local real_ms
        real_ms=$(soxi -D "$original" 2>/dev/null | awk '{printf "%.0f", $1*1000}')
        local diff=$(( real_ms > dur ? real_ms - dur : dur - real_ms ))
        echo "Generated $ph.wav (voice $voice, SAMPA '$sampa'): requested ${dur}ms, rendered ${real_ms}ms (diff ${diff}ms)"

        sox "$original" -r "$SAMPLE_RATE" -c 1 -b 8 -e unsigned-integer "$codec" gain -n -3 2>/dev/null
    else
        echo "Error: MBROLA synthesis failed for $ph (voice $voice, SAMPA '$sampa')"
        rm -f "$pho"
    fi
}

check_dependencies
echo "Generating international phonemes via MBROLA (de6/fr4/es1)..."
for entry in "${PHONES[@]}"; do
    generate_phoneme "$entry"
done

echo
echo "Done. $(ls "$CODEC_DIR"/*.wav 2>/dev/null | wc -l) total phoneme files in $CODEC_DIR"
echo "Next: python3 generate_wav_dictionary.py --input pcm8bit --force"
