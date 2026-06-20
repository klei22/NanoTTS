#!/bin/sh
# SPDX-License-Identifier: MIT
# Configure, build, and test NanoTTS with a selectable language footprint.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MAKE_CMD=${MAKE:-make}
RUN_TESTS=1
CLEAN_FIRST=0
JOBS=${JOBS:-}
LANGUAGES=${NANOTTS_LANGUAGES:-all}
OUTPUTS=${NANOTTS_OUTPUTS:-all}

usage() {
    cat <<'USAGE'
Usage: ./setup.sh [options]

Build NanoTTS with the repository Makefile and, by default, validate the same
language selection with CMake/CTest.

Options:
  --languages SET  Build all, ipa, one language, or a comma-separated subset.
                   Codes: id Indonesian, sw Kiswahili, es Spanish, ms Malay,
                   mi Māori, haw Hawaiian. Default: all.
  --outputs SET    Build all, none, pwm, pcm, or a comma-separated subset.
                   Default: all. WAV remains available in the host CLI.
  --clean          Remove the selected profile's previous build trees first.
  --no-test        Build only; do not run CMake/CTest.
  -j N, --jobs N   Use N parallel build jobs.
  -h, --help       Show this help text.

Examples:
  ./setup.sh
  ./setup.sh --languages ms
  ./setup.sh --languages mi,haw --clean
  ./setup.sh --languages id,sw,es
  ./setup.sh --languages ipa --outputs pwm
  ./setup.sh --languages mi,haw --outputs pwm,pcm

MAKE selects an alternative make executable. JOBS sets the default job count.
Other Makefile tuning variables such as CC, CFLAGS, USE_LIBM, MAX_EVENTS,
CONTEXT_BYTES, and AUDIO_BLOCK remain available. NANOTTS_OUTPUTS sets the
non-command-line default for --outputs; use --outputs to select adapters.
USAGE
}

die() {
    printf 'setup.sh: %s\n' "$*" >&2
    exit 1
}

detect_jobs() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    elif command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1\n'
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null || printf '1\n'
    else
        printf '1\n'
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --languages|--language)
            [ "$#" -ge 2 ] || die "$1 requires a language set"
            LANGUAGES=$2
            shift
            ;;
        --languages=*|--language=*) LANGUAGES=${1#*=} ;;
        --outputs)
            [ "$#" -ge 2 ] || die "$1 requires an output set"
            OUTPUTS=$2
            shift
            ;;
        --outputs=*) OUTPUTS=${1#*=} ;;
        --clean) CLEAN_FIRST=1 ;;
        --no-test) RUN_TESTS=0 ;;
        -j|--jobs)
            [ "$#" -ge 2 ] || die "$1 requires a positive integer"
            JOBS=$2
            shift
            ;;
        --jobs=*) JOBS=${1#*=} ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (run ./setup.sh --help)" ;;
    esac
    shift
done

LANG_ID=0
LANG_SW=0
LANG_ES=0
LANG_MS=0
LANG_MI=0
LANG_HAW=0
TEXT_FRONTEND=1

case "$LANGUAGES" in
    all)
        LANG_ID=1; LANG_SW=1; LANG_ES=1
        LANG_MS=1; LANG_MI=1; LANG_HAW=1
        ;;
    ipa|none|off)
        TEXT_FRONTEND=0
        ;;
    '') die "language set must not be empty" ;;
    *)
        old_ifs=$IFS
        IFS=,
        # Intentional splitting on the comma-separated profile.
        set -- $LANGUAGES
        IFS=$old_ifs
        for language in "$@"; do
            case "$language" in
                id|indonesian)
                    [ "$LANG_ID" -eq 0 ] || die "duplicate language: id"
                    LANG_ID=1 ;;
                sw|swahili|kiswahili)
                    [ "$LANG_SW" -eq 0 ] || die "duplicate language: sw"
                    LANG_SW=1 ;;
                es|spanish|espanol|castellano)
                    [ "$LANG_ES" -eq 0 ] || die "duplicate language: es"
                    LANG_ES=1 ;;
                ms|malay|melayu)
                    [ "$LANG_MS" -eq 0 ] || die "duplicate language: ms"
                    LANG_MS=1 ;;
                mi|maori|māori)
                    [ "$LANG_MI" -eq 0 ] || die "duplicate language: mi"
                    LANG_MI=1 ;;
                haw|hawaiian|hawaii)
                    [ "$LANG_HAW" -eq 0 ] || die "duplicate language: haw"
                    LANG_HAW=1 ;;
                ipa|none|off) die "ipa cannot be combined with text languages" ;;
                *) die "invalid language '$language'; use id, sw, es, ms, mi, haw, all, or ipa" ;;
            esac
        done
        ;;
esac


BUILD_PWM=0
BUILD_PCM=0
case "$OUTPUTS" in
    all) BUILD_PWM=1; BUILD_PCM=1 ;;
    none|off|'') ;;
    *)
        old_ifs=$IFS
        IFS=,
        set -- $OUTPUTS
        IFS=$old_ifs
        for output in "$@"; do
            case "$output" in
                pwm)
                    [ "$BUILD_PWM" -eq 0 ] || die "duplicate output: pwm"
                    BUILD_PWM=1 ;;
                pcm|raw)
                    [ "$BUILD_PCM" -eq 0 ] || die "duplicate output: pcm"
                    BUILD_PCM=1 ;;
                none|off) die "none cannot be combined with output adapters" ;;
                *) die "invalid output '$output'; use pwm, pcm, all, or none" ;;
            esac
        done
        ;;
esac

if [ "$TEXT_FRONTEND" -eq 1 ] &&
   [ $((LANG_ID + LANG_SW + LANG_ES + LANG_MS + LANG_MI + LANG_HAW)) -eq 0 ]; then
    die "select at least one text language or use ipa"
fi

if [ "$TEXT_FRONTEND" -eq 0 ]; then
    PROFILE=ipa
elif [ "$LANG_ID$LANG_SW$LANG_ES$LANG_MS$LANG_MI$LANG_HAW" = 111111 ]; then
    PROFILE=all
else
    PROFILE=
    for pair in "id:$LANG_ID" "sw:$LANG_SW" "es:$LANG_ES" \
                "ms:$LANG_MS" "mi:$LANG_MI" "haw:$LANG_HAW"; do
        code=${pair%%:*}
        enabled=${pair#*:}
        if [ "$enabled" -eq 1 ]; then
            [ -z "$PROFILE" ] || PROFILE="$PROFILE-"
            PROFILE="$PROFILE$code"
        fi
    done
fi

[ -n "$JOBS" ] || JOBS=$(detect_jobs)
case "$JOBS" in ''|*[!0-9]*|0) die "job count must be a positive integer" ;; esac
case "$BUILD_PWM" in 0|1) ;; *) die "BUILD_PWM must be 0 or 1" ;; esac
case "$BUILD_PCM" in 0|1) ;; *) die "BUILD_PCM must be 0 or 1" ;; esac

command -v "$MAKE_CMD" >/dev/null 2>&1 || die "make executable not found: $MAKE_CMD"
if [ "$RUN_TESTS" -eq 1 ]; then
    command -v cmake >/dev/null 2>&1 || die "cmake is required for tests; use --no-test to skip"
    command -v ctest >/dev/null 2>&1 || die "ctest is required for tests; use --no-test to skip"
fi

OUTPUT_PROFILE=none
if [ "$BUILD_PWM$BUILD_PCM" = 11 ]; then OUTPUT_PROFILE=all
elif [ "$BUILD_PWM" -eq 1 ]; then OUTPUT_PROFILE=pwm
elif [ "$BUILD_PCM" -eq 1 ]; then OUTPUT_PROFILE=pcm
fi
MAKE_BUILD="build-make-$PROFILE-$OUTPUT_PROFILE"
TEST_BUILD="build-test-$PROFILE-$OUTPUT_PROFILE"
printf 'NanoTTS setup: languages=%s outputs=%s jobs=%s\n' \
    "$PROFILE" "$OUTPUT_PROFILE" "$JOBS"

if [ "$CLEAN_FIRST" -eq 1 ]; then
    rm -rf "$ROOT_DIR/$MAKE_BUILD" "$ROOT_DIR/$TEST_BUILD"
fi

"$MAKE_CMD" -C "$ROOT_DIR" -j"$JOBS" all \
    BUILD="$MAKE_BUILD" TEXT_FRONTEND="$TEXT_FRONTEND" \
    LANG_ID="$LANG_ID" LANG_SW="$LANG_SW" LANG_ES="$LANG_ES" \
    LANG_MS="$LANG_MS" LANG_MI="$LANG_MI" LANG_HAW="$LANG_HAW" \
    BUILD_PWM="$BUILD_PWM" BUILD_PCM="$BUILD_PCM"

if [ "$RUN_TESTS" -eq 1 ]; then
    onoff() { [ "$1" -eq 1 ] && printf ON || printf OFF; }
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/$TEST_BUILD" \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DNANOTTS_ENABLE_TEXT_FRONTEND=$(onoff "$TEXT_FRONTEND") \
        -DNANOTTS_ENABLE_LANG_ID=$(onoff "$LANG_ID") \
        -DNANOTTS_ENABLE_LANG_SW=$(onoff "$LANG_SW") \
        -DNANOTTS_ENABLE_LANG_ES=$(onoff "$LANG_ES") \
        -DNANOTTS_ENABLE_LANG_MS=$(onoff "$LANG_MS") \
        -DNANOTTS_ENABLE_LANG_MI=$(onoff "$LANG_MI") \
        -DNANOTTS_ENABLE_LANG_HAW=$(onoff "$LANG_HAW") \
        -DNANOTTS_BUILD_PWM=$(onoff "$BUILD_PWM") \
        -DNANOTTS_BUILD_PCM=$(onoff "$BUILD_PCM")
    cmake --build "$ROOT_DIR/$TEST_BUILD" --parallel "$JOBS"
    ctest --test-dir "$ROOT_DIR/$TEST_BUILD" --output-on-failure
fi

CLI="$ROOT_DIR/$MAKE_BUILD/nanotts"
printf '\nBuild complete. CLI: %s\n' "$CLI"
[ "$LANG_ID" -eq 0 ] || printf 'Indonesian: %s --lang id --text "selamat pagi" -o id.wav\n' "$CLI"
[ "$LANG_SW" -eq 0 ] || printf 'Kiswahili:  %s --lang sw --text "habari yako" -o sw.wav\n' "$CLI"
[ "$LANG_ES" -eq 0 ] || printf 'Spanish:    %s --lang es --text "hola, buenos días" -o es.wav\n' "$CLI"
[ "$LANG_MS" -eq 0 ] || printf 'Malay:      %s --lang ms --text "selamat pagi" -o ms.wav\n' "$CLI"
[ "$LANG_MI" -eq 0 ] || printf 'Māori:      %s --lang mi --text "kia ora, Aotearoa" -o mi.wav\n' "$CLI"
[ "$LANG_HAW" -eq 0 ] || printf 'Hawaiian:   %s --lang haw --text "aloha Hawaiʻi" -o haw.wav\n' "$CLI"
[ "$PROFILE" != ipa ] || printf 'IPA:        %s --ipa "h_o_l_a" -o speech.wav\n' "$CLI"
[ "$BUILD_PCM" -eq 0 ] || printf 'Raw PCM:    %s --ipa "h_o_l_a" --format pcm -o speech.pcm\n' "$CLI"
[ "$BUILD_PWM" -eq 0 ] || printf 'PWM duty:   %s --ipa "h_o_l_a" --format pwm --pwm-top 1023 -o speech.pwm\n' "$CLI"
