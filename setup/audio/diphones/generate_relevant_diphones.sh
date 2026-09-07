#!/usr/bin/env bash

# Generate only linguistically relevant diphones for English TTS
# Based on common English phonotactic patterns
# Output: 8-bit 8kHz WAV files optimized for embedded systems

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ORIGINAL_DIR="$SCRIPT_DIR/original"

# Common English phonemes
vowels=("AA" "AE" "AH" "AO" "AW" "AY" "EH" "ER" "EY" "IH" "IY" "OW" "OY" "UH" "UW")
consonants=("B" "CH" "D" "DH" "F" "G" "HH" "JH" "K" "L" "M" "N" "NG" "P" "R" "S" "SH" "T" "TH" "V" "W" "Y" "Z" "ZH")
silence=("SIL")

VOICE="en1"  # MBROLA voice (en1, us1, us2, us3 available)
MBROLA_DATABASE="/usr/share/mbrola/en1/en1"  # Path to MBROLA database
SAMPLE_RATE=8000
BIT_DEPTH=8
AMPLITUDE=100

# Per-phoneme natural durations in milliseconds, matching
# src/TinyTTSTools/Basic/Phonemes.h's phoneme_map exactly. A diphone
# recording should span roughly the second half of phone1's steady state
# through the first half of phone2's -- NOT a full phone1 followed by a
# full phone2 -- so that playing consecutive diphones back-to-back
# reconstructs each shared phoneme's duration once, not twice (see
# half_duration() below, and DiphoneVocoder.h's traversal, which relies on
# this). Falls back to 120ms (SIL_DURATION_FALLBACK) for any phoneme not
# listed here.
declare -A PHONEME_DURATIONS_MS=(
    [AA]=150 [AE]=130 [AH]=120 [AO]=160 [AW]=180 [AY]=180
    [EH]=130 [ER]=140 [EY]=170 [IH]=110 [IY]=140 [OW]=170 [OY]=180
    [UH]=120 [UW]=150
    [B]=70 [CH]=120 [D]=60 [DH]=100 [F]=120 [G]=70 [HH]=90 [JH]=110
    [K]=80 [L]=100 [M]=100 [N]=90 [NG]=110 [P]=80 [R]=90 [S]=130
    [SH]=120 [T]=70 [TH]=110 [V]=100 [W]=80 [Y]=70 [Z]=110 [ZH]=110
)
DURATION_FALLBACK_MS=120
MIN_HALF_DURATION_MS=20  # floor so very short stops don't round to ~0ms
SILENCE_BOUNDARY_MS=50   # short silence used at word-boundary diphones
PURE_SILENCE_MS=100      # SIL_SIL diphone length

# Returns roughly half of $1's natural duration (see PHONEME_DURATIONS_MS
# above), floored at MIN_HALF_DURATION_MS.
half_duration() {
    local full=${PHONEME_DURATIONS_MS[$1]:-$DURATION_FALLBACK_MS}
    local half=$((full / 2))
    if [ "$half" -lt "$MIN_HALF_DURATION_MS" ]; then
        half=$MIN_HALF_DURATION_MS
    fi
    echo "$half"
}

# Codec options for minimal file size
# Options: pcm, ulaw, alaw, adpcm
CODEC="adpcm"  # ADPCM gives ~75% size reduction with good speech quality

# Advanced options
ULTRA_MINIMAL=false  # Set to true for 4kHz µ-law (extreme size reduction)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --codec)
            CODEC="$2"
            shift 2
            ;;
        --ultra-minimal)
            ULTRA_MINIMAL=true
            shift
            ;;
        --convert-existing)
            CONVERT_EXISTING=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Generate linguistically relevant diphones for English TTS using MBROLA"
            echo ""
            echo "Options:"
            echo "  --codec CODEC          Compression codec (adpcm, ulaw, alaw, pcm) [default: adpcm]"
            echo "  --ultra-minimal        Enable ultra-minimal mode (4kHz ADPCM)"
            echo "  --convert-existing     Convert existing original files to selected codec"
            echo "  --help, -h             Show this help message"
            echo ""
            echo "Output directories:"
            echo "  ./original/            High-quality original files (22kHz 16-bit)"
            echo "  ./adpcm/               ADPCM compressed files (8kHz)"
            echo "  ./u-law/               µ-law compressed files (8kHz)"
            echo "  ./a-law/               A-law compressed files (8kHz)"
            echo "  ./pcm-8bit/            8-bit PCM files (8kHz)"
            echo ""
            echo "Examples:"
            echo "  $0                     Generate with default ADPCM compression"
            echo "  $0 --codec ulaw        Generate with µ-law compression"
            echo "  $0 --ultra-minimal     Generate with ultra-minimal settings"
            echo "  $0 --convert-existing  Convert existing originals to current codec"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

if [ "$ULTRA_MINIMAL" = true ]; then
    SAMPLE_RATE=4000
    CODEC="adpcm"
    echo "ULTRA MINIMAL MODE: 4kHz ADPCM encoding"
fi

# Set codec directory based on selected codec
case "$CODEC" in
    "ulaw")
        CODEC_DIR="$SCRIPT_DIR/u-law"
        ;;
    "alaw")
        CODEC_DIR="$SCRIPT_DIR/a-law"
        ;;
    "adpcm")
        CODEC_DIR="$SCRIPT_DIR/adpcm"
        ;;
    "pcm")
        CODEC_DIR="$SCRIPT_DIR/pcm-8bit"
        ;;
    *)
        CODEC_DIR="$SCRIPT_DIR/adpcm"  # Default fallback
        ;;
esac

# Create the directories if they don't exist
mkdir -p "$ORIGINAL_DIR"
mkdir -p "$CODEC_DIR"

echo "Script directory: $SCRIPT_DIR"
echo "Original files will be generated in: $ORIGINAL_DIR"
echo "Compressed files will be generated in: $CODEC_DIR"

# Check for required tools
check_dependencies() {
    local missing=0
    
    if ! command -v mbrola &> /dev/null; then
        echo "Error: mbrola is not installed. Please install it first."
        echo "On Ubuntu/Debian: sudo apt-get install mbrola mbrola-en1"
        echo "On macOS: brew install mbrola"
        missing=1
    fi
    
    if ! command -v sox &> /dev/null; then
        echo "Error: sox is not installed. Please install it first."
        echo "On Ubuntu/Debian: sudo apt-get install sox"
        echo "On macOS: brew install sox"
        missing=1
    fi
    
    # Check if MBROLA voice database exists
    if [ ! -f "$MBROLA_DATABASE" ]; then
        echo "Error: MBROLA voice database not found at $MBROLA_DATABASE"
        echo "Please install the MBROLA en1 voice database:"
        echo "On Ubuntu/Debian: sudo apt-get install mbrola-en1"
        missing=1
    fi
    
    if [ $missing -eq 1 ]; then
        exit 1
    fi
}

# Cleanup function
cleanup() {
    echo "Cleaning up temporary files..."
    rm -f "$ORIGINAL_DIR"/temp_*.wav
    rm -f "$ORIGINAL_DIR"/temp_*.pho
    rm -f "$CODEC_DIR"/temp_*.wav
}

# Set up signal handlers for cleanup
trap cleanup EXIT INT TERM

# Convert existing high-quality files to optimized format
convert_existing_files() {
    local converted=0
    echo "Converting existing WAV files from original to ${CODEC} ${SAMPLE_RATE}Hz..."
    echo "Source directory: $ORIGINAL_DIR"
    echo "Target directory: $CODEC_DIR"
    
    cd "$ORIGINAL_DIR"
    
    for file in *.wav; do
        if [ -f "$file" ]; then
            local target_file="$CODEC_DIR/$file"
            local temp_file="$CODEC_DIR/temp_convert_$file"
            echo "Converting: $file to ${CODEC} format"
            
            # Convert using sox with selected codec
            local sox_cmd="sox \"$ORIGINAL_DIR/$file\" -r $SAMPLE_RATE -c 1"
            
            case "$CODEC" in
                "ulaw")
                    sox_cmd="$sox_cmd -e mu-law \"$temp_file\""
                    ;;
                "alaw")
                    sox_cmd="$sox_cmd -e a-law \"$temp_file\""
                    ;;
                "adpcm")
                    sox_cmd="$sox_cmd -e ima-adpcm \"$temp_file\""
                    ;;
                "pcm")
                    sox_cmd="$sox_cmd -b $BIT_DEPTH -e signed-integer \"$temp_file\" dither"
                    ;;
                *)
                    echo "Unknown codec: $CODEC, using PCM"
                    sox_cmd="$sox_cmd -b $BIT_DEPTH -e signed-integer \"$temp_file\" dither"
                    ;;
            esac
            
            # Add effects after the output file
            sox_cmd="$sox_cmd gain -n -3"
            
            if eval "$sox_cmd" 2>/dev/null; then
                mv "$temp_file" "$target_file"
                converted=$((converted + 1))
            else
                echo "Warning: Failed to convert $file"
                rm -f "$temp_file"
            fi
        fi
    done
    
    echo "Converted $converted files to ${CODEC} ${SAMPLE_RATE}Hz"
}

echo "Generating linguistically relevant diphones for English..."
echo "Output format: ${CODEC} ${SAMPLE_RATE}Hz mono WAV files"
echo "Checking dependencies..."

check_dependencies

echo "Dependencies OK. Starting generation..."

# Check if we should convert existing files
if [ "$CONVERT_EXISTING" = true ]; then
    echo "Converting existing files mode..."
    convert_existing_files
    exit 0
fi

# Check if there are existing files to backup
cd "$ORIGINAL_DIR"
existing_original=$(ls -1 *.wav 2>/dev/null | wc -l)
cd "$CODEC_DIR"
existing_codec=$(ls -1 *.wav 2>/dev/null | wc -l)

if [ "$existing_original" -gt 0 ] || [ "$existing_codec" -gt 0 ]; then
    echo "Found $existing_original original files and $existing_codec codec files."
    echo "Creating backups..."
    
    if [ "$existing_original" -gt 0 ]; then
        mkdir -p "$ORIGINAL_DIR/backup"
        cp "$ORIGINAL_DIR"/*.wav "$ORIGINAL_DIR/backup/" 2>/dev/null || true
    fi
    
    if [ "$existing_codec" -gt 0 ]; then
        mkdir -p "$CODEC_DIR/backup"
        cp "$CODEC_DIR"/*.wav "$CODEC_DIR/backup/" 2>/dev/null || true
    fi
    
    # Ask if user wants to keep existing files or regenerate
    echo "Options:"
    echo "  1) Keep existing files and only generate missing ones"
    echo "  2) Convert existing original files to ${CODEC} format"
    echo "  3) Delete existing files and regenerate all"
    echo "  4) Exit without changes"
    
    read -p "Choose option (1-4): " choice
    case $choice in
        1)
            echo "Keeping existing files, will only generate missing ones..."
            ;;
        2)
            echo "Converting existing original files to ${CODEC} format..."
            convert_existing_files
            ;;
        3)
            echo "Deleting existing files and regenerating all..."
            rm -f "$ORIGINAL_DIR"/*.wav 2>/dev/null || true
            rm -f "$CODEC_DIR"/*.wav 2>/dev/null || true
            ;;
        4)
            echo "Exiting without changes."
            exit 0
            ;;
        *)
            echo "Invalid choice. Exiting."
            exit 1
            ;;
    esac
fi

count=0

# Function to convert ARPABET to MBROLA phonemes (English voice en1)
convert_phoneme() {
    case $1 in
        # Vowels - using MBROLA en1 phoneme set (SAMPA notation)
        "AA") echo "A:" ;;      # father [ɑː] -> barn
        "AE") echo "{" ;;       # cat [æ] -> pat
        "AH") echo "V" ;;       # but [ʌ] -> putt
        "AO") echo "O:" ;;      # law [ɔː] -> born
        "AW") echo "aU" ;;      # how [aʊ] -> now
        "AY") echo "aI" ;;      # eye [aɪ] -> buy
        "EH") echo "e" ;;       # red [ɛ] -> pet
        "ER") echo "3:" ;;      # bird [ɜː] -> burn
        "EY") echo "eI" ;;      # say [eɪ] -> bay
        "IH") echo "I" ;;       # bit [ɪ] -> pit
        "IY") echo "i:" ;;      # beat [iː] -> bean
        "OW") echo "@U" ;;      # go [oʊ] -> no
        "OY") echo "OI" ;;      # boy [ɔɪ] -> boy
        "UH") echo "U" ;;       # put [ʊ] -> good
        "UW") echo "u:" ;;      # boot [uː] -> boon
        
        # Consonants - using MBROLA en1 phoneme set (SAMPA notation)
        "B") echo "b" ;;        # bat [b] -> but
        "CH") echo "tS" ;;      # chin [tʃ] -> chain
        "D") echo "d" ;;        # dog [d] -> den
        "DH") echo "D" ;;       # this [ð] -> then
        "F") echo "f" ;;        # fish [f] -> full
        "G") echo "g" ;;        # go [g] -> game
        "HH") echo "h" ;;       # hat [h] -> hat
        "JH") echo "dZ" ;;      # joy [dʒ] -> Jane
        "K") echo "k" ;;        # cat [k] -> can
        "L") echo "l" ;;        # let [l] -> like
        "M") echo "m" ;;        # man [m] -> man
        "N") echo "n" ;;        # no [n] -> not
        "NG") echo "N" ;;       # sing [ŋ] -> long
        "P") echo "p" ;;        # pat [p] -> put
        "R") echo "r" ;;        # red [r] -> run
        "S") echo "s" ;;        # sit [s] -> some
        "SH") echo "S" ;;       # she [ʃ] -> ship
        "T") echo "t" ;;        # tap [t] -> ten
        "TH") echo "T" ;;       # thin [θ] -> thin
        "V") echo "v" ;;        # van [v] -> very
        "W") echo "w" ;;        # wet [w] -> went
        "Y") echo "j" ;;        # yes [j] -> yes
        "Z") echo "z" ;;        # zoo [z] -> zeal
        "ZH") echo "Z" ;;       # measure [ʒ] -> measure
        "SIL") echo "_" ;;      # silence
        *) echo "$1" ;;         # fallback
    esac
}

# Generate diphone with optimized audio parameters using MBROLA
generate_diphone() {
    local ph1=$1
    local ph2=$2
    local original_filename="${ORIGINAL_DIR}/${ph1}_${ph2}.wav"
    local codec_filename="${CODEC_DIR}/${ph1}_${ph2}.wav"
    local temp_file="${ORIGINAL_DIR}/temp_${ph1}_${ph2}.wav"
    local temp_pho="${ORIGINAL_DIR}/temp_${ph1}_${ph2}.pho"
    
    # Skip if both files already exist
    if [ -f "$original_filename" ] && [ -f "$codec_filename" ]; then
        echo "Skipping existing: ${ph1}_${ph2}.wav (both original and compressed exist)"
        return
    fi
    
    local ph1_mb=$(convert_phoneme "$ph1")
    local ph2_mb=$(convert_phoneme "$ph2")
    
    count=$((count + 1))
    echo "[$count] Generating: $ph1 + $ph2 -> original and ${CODEC} versions"
    
    # Create MBROLA phoneme file (.pho). Real (non-silence) sides use HALF
    # their natural duration each -- see half_duration() above -- so the
    # diphone spans roughly [second half of ph1] + [first half of ph2],
    # not a full ph1 followed by a full ph2. DiphoneVocoder.h plays
    # consecutive diphones back-to-back, so a full+full diphone would make
    # every interior phoneme's steady state sound twice as long as it
    # should.
    if [ "$ph1_mb" = "_" ] && [ "$ph2_mb" = "_" ]; then
        # Pure silence - generate short pause
        echo "_ ${PURE_SILENCE_MS}" > "$temp_pho"
    elif [ "$ph1_mb" = "_" ]; then
        # Silence to phoneme
        local d2=$(half_duration "$ph2")
        echo "_ ${SILENCE_BOUNDARY_MS}" > "$temp_pho"
        echo "${ph2_mb} ${d2}" >> "$temp_pho"
    elif [ "$ph2_mb" = "_" ]; then
        # Phoneme to silence
        local d1=$(half_duration "$ph1")
        echo "${ph1_mb} ${d1}" > "$temp_pho"
        echo "_ ${SILENCE_BOUNDARY_MS}" >> "$temp_pho"
    else
        # Phoneme to phoneme - the core diphone case
        local d1=$(half_duration "$ph1")
        local d2=$(half_duration "$ph2")
        echo "${ph1_mb} ${d1}" > "$temp_pho"
        echo "${ph2_mb} ${d2}" >> "$temp_pho"
    fi
    
    # Generate with MBROLA
    if mbrola "$MBROLA_DATABASE" "$temp_pho" "$temp_file" 2>/dev/null; then
        # Clean up phoneme file
        rm -f "$temp_pho"
    else
        echo "Error: MBROLA synthesis failed for $ph1 + $ph2"
        rm -f "$temp_pho"
        return
    fi
    
    # Copy to original directory (high quality)
    if [ -f "$temp_file" ]; then
        # First, create the original file (22kHz, 16-bit)
        sox "$temp_file" -r 22050 -b 16 -c 1 "$original_filename" gain -n -3
        
        # Then create the compressed version
        local sox_cmd="sox \"$temp_file\" -r $SAMPLE_RATE -c 1"
        
        case "$CODEC" in
            "ulaw")
                sox_cmd="$sox_cmd -e mu-law \"$codec_filename\""
                ;;
            "alaw")
                sox_cmd="$sox_cmd -e a-law \"$codec_filename\""
                ;;
            "adpcm")
                sox_cmd="$sox_cmd -e ima-adpcm \"$codec_filename\""
                ;;
            "pcm")
                sox_cmd="$sox_cmd -b $BIT_DEPTH -e signed-integer \"$codec_filename\" dither"
                ;;
            *)
                echo "Unknown codec: $CODEC, using PCM"
                sox_cmd="$sox_cmd -b $BIT_DEPTH -e signed-integer \"$codec_filename\" dither"
                ;;
        esac
        
        # Add effects after the output file
        sox_cmd="$sox_cmd gain -n -3"
        
        if eval "$sox_cmd"; then
            rm -f "$temp_file"
            
            # Verify both output files
            if [ -f "$original_filename" ] && [ -f "$codec_filename" ]; then
                orig_size=$(stat -c%s "$original_filename" 2>/dev/null || stat -f%z "$original_filename" 2>/dev/null || echo "0")
                codec_size=$(stat -c%s "$codec_filename" 2>/dev/null || stat -f%z "$codec_filename" 2>/dev/null || echo "0")
                if [ "$orig_size" -lt 100 ] || [ "$codec_size" -lt 100 ]; then
                    echo "Warning: Generated files for $(basename "$original_filename") are very small (orig: ${orig_size}, codec: ${codec_size} bytes)"
                fi
            else
                echo "Error: Failed to generate files for $(basename "$original_filename")"
            fi
        else
            echo "Error: sox conversion failed for $temp_file"
            rm -f "$temp_file"
        fi
    else
        echo "Error: MBROLA failed to generate $temp_file"
    fi
}

echo "=== 1. Silence transitions (word boundaries) ==="
# Silence to vowels (word initial vowels)
for v in "${vowels[@]}"; do
    generate_diphone "SIL" "$v"
done

# Silence to consonants (word initial consonants)
for c in "${consonants[@]}"; do
    generate_diphone "SIL" "$c"
done

# Vowels to silence (word final vowels)
for v in "${vowels[@]}"; do
    generate_diphone "$v" "SIL"
done

# Consonants to silence (word final consonants)
for c in "${consonants[@]}"; do
    generate_diphone "$c" "SIL"
done

# Silence to silence (pauses)
generate_diphone "SIL" "SIL"

echo "=== 2. Vowel to consonant transitions ==="
# All vowels can precede most consonants
for v in "${vowels[@]}"; do
    for c in "${consonants[@]}"; do
        generate_diphone "$v" "$c"
    done
done

echo "=== 3. Consonant to vowel transitions ==="
# All consonants can precede vowels
for c in "${consonants[@]}"; do
    for v in "${vowels[@]}"; do
        generate_diphone "$c" "$v"
    done
done

echo "=== 4. Common consonant clusters ==="
# Common initial consonant clusters
clusters_initial=(
    "P L" "P R" "B L" "B R" "T R" "D R" "K L" "K R" "K W"
    "G L" "G R" "G W" "F L" "F R" "TH R" "SH R" "S K" "S L"
    "S M" "S N" "S P" "S T" "S W" "S K R" "S P L" "S P R"
    "S T R" "T W" "D W" "HH W" "HH Y"
)

# `read -r c1 c2 <<< "$cluster"` silently mis-parsed any 3-phoneme cluster
# (e.g. "S K R" for "scr-"): read absorbs every leftover token into the last
# variable, so c2 became the literal string "K R" and MBROLA synthesis
# failed outright ("MBROLA synthesis failed for S + K R", one per 3-phoneme
# cluster: S K R, S P L, S P R, S T R). Generate every ADJACENT pair within
# a cluster instead, so clusters of any length work correctly -- a 3-phone
# cluster "S K R" becomes two diphones, S_K and K_R, matching how
# DiphoneVocoder itself walks a phoneme sequence.
for cluster in "${clusters_initial[@]}"; do
    read -r -a tokens <<< "$cluster"
    for ((i = 0; i + 1 < ${#tokens[@]}; i++)); do
        generate_diphone "${tokens[i]}" "${tokens[i + 1]}"
    done
done

# Common final consonant clusters
clusters_final=(
    "N T" "N D" "N S" "N Z" "M P" "M S" "L T" "L D" "L S"
    "L Z" "R T" "R D" "R S" "R Z" "S T" "S K" "F T" "K T"
    "P T" "NG K" "NG S" "NG Z" "TH S" "V Z" "D Z" "T S"
    "K S" "P S"
)

for cluster in "${clusters_final[@]}"; do
    read -r -a tokens <<< "$cluster"
    for ((i = 0; i + 1 < ${#tokens[@]}; i++)); do
        generate_diphone "${tokens[i]}" "${tokens[i + 1]}"
    done
done

echo "=== 5. Vowel sequences (diphthongs already covered) ==="
# Some vowel-vowel transitions for hiatus (rare but occur)
common_vv=(
    "AH AH" "IY AH" "AH IY" "EY AH" "AH EY" "OW AH" "AH OW"
)

for vv in "${common_vv[@]}"; do
    read -r v1 v2 <<< "$vv"
    generate_diphone "$v1" "$v2"
done

echo "=== 6. Liquid and nasal transitions ==="
# R, L, M, N, NG can appear in various positions
liquids_nasals=("R" "L" "M" "N" "NG")

for ln in "${liquids_nasals[@]}"; do
    for ln2 in "${liquids_nasals[@]}"; do
        if [ "$ln" != "$ln2" ]; then
            generate_diphone "$ln" "$ln2"
        fi
    done
done

echo "=== 7. Full consonant-consonant completeness ==="
# Sections 1-6 above only cover a hand-picked list of "common" clusters, not
# the full consonant x consonant cross product (24x24=576) -- an audit
# against the full 123k-word CMU dictionary (2025-09) found 4.7% of all
# diphone instances needed (480 distinct names, affecting 31% of words)
# missing as a result, dominated by consonant-consonant pairs the curated
# list never anticipated (M_B, R_K, M_Z, N_B, T_L, ...). Generate every
# consonant-consonant pair; generate_diphone() skips ones already present.
for c1 in "${consonants[@]}"; do
    for c2 in "${consonants[@]}"; do
        generate_diphone "$c1" "$c2"
    done
done

echo "=== 8. Full vowel-vowel completeness ==="
# Same rationale as section 7: section 5 only covered a few hand-picked
# vowel hiatus pairs, but real words need far more (ER_IH, IY_OW, ER_AH,
# ER_IY, IY_AA, AY_ER, ...). Generate every vowel-vowel pair.
for v1 in "${vowels[@]}"; do
    for v2 in "${vowels[@]}"; do
        generate_diphone "$v1" "$v2"
    done
done

echo "=== Diphone generation complete! ==="
echo "Generated $count relevant diphones for English TTS using MBROLA"
echo "Original files (22kHz 16-bit): $ORIGINAL_DIR"
echo "Compressed files (${CODEC} ${SAMPLE_RATE}Hz): $CODEC_DIR"

# Count final files
cd "$ORIGINAL_DIR"
original_count=$(ls -1 *.wav 2>/dev/null | wc -l)
cd "$CODEC_DIR"
codec_count=$(ls -1 *.wav 2>/dev/null | wc -l)

echo "Total original diphone files: $original_count"
echo "Total compressed diphone files: $codec_count"

# Show total sizes
if command -v du &> /dev/null; then
    original_size=$(du -sh "$ORIGINAL_DIR" 2>/dev/null | cut -f1)
    codec_size=$(du -sh "$CODEC_DIR" 2>/dev/null | cut -f1)
    echo "Original files size: $original_size"
    echo "Compressed files size: $codec_size"
fi

# Show sample file info
if [ $codec_count -gt 0 ]; then
    echo ""
    echo "Sample compressed file information:"
    first_file=$(ls -1 "$CODEC_DIR"/*.wav | head -1)
    if command -v soxi &> /dev/null; then
        echo "Using soxi to show details for: $(basename "$first_file")"
        soxi "$first_file"
    elif command -v file &> /dev/null; then
        echo "File info for: $(basename "$first_file")"
        file "$first_file"
        ls -lh "$first_file"
    fi
fi

echo ""
echo "Diphone database ready for TTS synthesis!"
echo "Original files: High quality for development and testing"
echo "Compressed files: Optimized for embedded systems and microcontrollers"
