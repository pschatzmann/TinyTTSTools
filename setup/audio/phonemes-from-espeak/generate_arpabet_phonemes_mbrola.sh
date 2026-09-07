#!/usr/bin/env bash
# Generate real, well-durationed isolated ARPAbet phoneme recordings via
# MBROLA, replacing download_arpabet_wavs.py's espeak/gTTS pipeline.
#
# The old pipeline rendered each isolated phoneme via espeak-ng's `[[xx]]`
# phoneme notation at a slow, deliberate speech rate (-s 120). That produced
# abnormally sustained/held recordings -- every vowel routinely hit
# download_arpabet_wavs.py's hard-coded 1-second crop, meaning what actually
# shipped was an arbitrary crop of an artificially long, non-representative
# rendering, not a clean excerpt of the phoneme itself. Played back at the
# phoneme's real (60-200ms) table duration, that mostly captured whatever
# was in the first fraction of a second -- often just an attack/onset
# ramp -- explaining reports that AudioPhoneme was "very difficult to
# understand".
#
# MBROLA takes an explicit per-phone duration in milliseconds and renders
# almost exactly that (already proven for the diphone corpus -- see
# generate_relevant_diphones.sh, where requested and measured real
# durations routinely match within a few percent). This generates each
# phoneme at EXACTLY its Phonemes.h table duration (the same shared source
# of truth DiphoneVocoder's half+half math already uses), then measures the
# actual rendered duration from the real output file as a verification step
# so generation and playback are guaranteed consistent, not just hoped to
# be.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ORIGINAL_DIR="$SCRIPT_DIR/original"
CODEC_DIR="$SCRIPT_DIR/pcm8bit"
mkdir -p "$ORIGINAL_DIR" "$CODEC_DIR"

VOICE="en1"
MBROLA_DATABASE="/usr/share/mbrola/en1/en1"
SAMPLE_RATE=8000

# Natural phoneme durations in milliseconds, matching
# src/TinyTTSTools/Basic/Phonemes.h's phoneme_map exactly (the same shared
# table DiphoneVocoder's playback duration math reads from).
declare -A PHONEME_DURATIONS_MS=(
    [AA]=150 [AE]=130 [AH]=120 [AH0]=80 [AO]=160 [AW]=180 [AY]=180
    [EH]=130 [ER]=140 [ER0]=90 [EY]=170 [IH]=110 [IY]=140 [OW]=170 [OY]=180
    [UH]=120 [UW]=150
    [B]=70 [CH]=120 [D]=60 [DH]=100 [F]=120 [G]=70 [HH]=90 [JH]=110
    [K]=80 [L]=100 [M]=100 [N]=90 [NG]=110 [P]=80 [R]=90 [S]=130
    [SH]=120 [T]=70 [TH]=110 [V]=100 [W]=80 [Y]=70 [Z]=110 [ZH]=110
)

# Map ARPAbet to MBROLA en1's phoneme set (SAMPA notation) -- the exact
# same mapping already proven to work for the diphone corpus (see
# generate_relevant_diphones.sh's convert_phoneme()), extended with AH0
# (schwa) and ER0 (unstressed r-colored vowel), which the diphone script
# never needed. ER0 reuses ER's own symbol ("3:") -- en1 rejects "3",
# "@r", "3r" and "@`" as a standalone segment ("Unknown recovery for
# _-X segment") -- and is distinguished from ER only by its shorter
# requested duration (90ms vs 140ms); an approximation, not a distinct
# r-colored-schwa timbre.
convert_phoneme() {
    case "$1" in
        AA) echo "A:" ;; AE) echo "{" ;; AH) echo "V" ;; AH0) echo "@" ;;
        AO) echo "O:" ;; AW) echo "aU" ;; AY) echo "aI" ;; EH) echo "e" ;;
        ER) echo "3:" ;; ER0) echo "3:" ;; EY) echo "eI" ;; IH) echo "I" ;;
        IY) echo "i:" ;; OW) echo "@U" ;; OY) echo "OI" ;; UH) echo "U" ;;
        UW) echo "u:" ;;
        B) echo "b" ;; CH) echo "tS" ;; D) echo "d" ;; DH) echo "D" ;;
        F) echo "f" ;; G) echo "g" ;; HH) echo "h" ;; JH) echo "dZ" ;;
        K) echo "k" ;; L) echo "l" ;; M) echo "m" ;; N) echo "n" ;;
        NG) echo "N" ;; P) echo "p" ;; R) echo "r" ;; S) echo "s" ;;
        SH) echo "S" ;; T) echo "t" ;; TH) echo "T" ;; V) echo "v" ;;
        W) echo "w" ;; Y) echo "j" ;; Z) echo "z" ;; ZH) echo "Z" ;;
        *) echo "$1" ;;
    esac
}

check_dependencies() {
    command -v mbrola >/dev/null 2>&1 || { echo "Error: mbrola not found"; exit 1; }
    command -v sox >/dev/null 2>&1 || { echo "Error: sox not found"; exit 1; }
    [ -f "$MBROLA_DATABASE" ] || { echo "Error: MBROLA voice database not found at $MBROLA_DATABASE"; exit 1; }
}

generate_phoneme() {
    local ph=$1
    local mb
    mb=$(convert_phoneme "$ph")
    local dur=${PHONEME_DURATIONS_MS[$ph]}
    local original="$ORIGINAL_DIR/$ph.wav"
    local codec="$CODEC_DIR/$ph.wav"
    local pho="$ORIGINAL_DIR/temp_$ph.pho"

    if [ -f "$original" ] && [ -f "$codec" ]; then
        echo "Skipping existing: $ph.wav"
        return
    fi

    printf "%s %s\n" "$mb" "$dur" > "$pho"

    if mbrola "$MBROLA_DATABASE" "$pho" "$original" 2>/dev/null; then
        rm -f "$pho"
        # Verify the rendered duration actually matches what was requested
        # (the whole point of moving to MBROLA instead of trusting
        # espeak's free-running isolated-phoneme rendering).
        local real_ms
        real_ms=$(soxi -D "$original" 2>/dev/null | awk '{printf "%.0f", $1*1000}')
        local diff=$(( real_ms > dur ? real_ms - dur : dur - real_ms ))
        echo "Generated $ph.wav: requested ${dur}ms, rendered ${real_ms}ms (diff ${diff}ms)"

        sox "$original" -r "$SAMPLE_RATE" -c 1 -b 8 -e unsigned-integer "$codec" gain -n -3 2>/dev/null
    else
        echo "Error: MBROLA synthesis failed for $ph"
        rm -f "$pho"
    fi
}

check_dependencies
echo "Generating ARPAbet phonemes via MBROLA (voice: $VOICE)..."
for ph in "${!PHONEME_DURATIONS_MS[@]}"; do
    generate_phoneme "$ph"
done

echo
echo "Done. $(ls "$CODEC_DIR"/*.wav 2>/dev/null | wc -l) phoneme files in $CODEC_DIR"
echo "Next: python3 generate_wav_dictionary.py --input pcm8bit --force"
