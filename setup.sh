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

usage() {
    cat <<'USAGE'
Usage: ./setup.sh [options]

Build NanoTTS with the repository Makefile and, by default, validate the same
language selection with CMake/CTest.

Options:
  --languages SET  Build all, id, sw, es, a comma-separated subset, or ipa.
                   id = Indonesian; sw = Kiswahili; es = Spanish;
                   ipa = no text modules. Default: all.
  --clean          Remove the selected profile's previous build trees first.
  --no-test        Build only; do not run CMake/CTest.
  -j N, --jobs N   Use N parallel build jobs.
  -h, --help       Show this help text.

Examples:
  ./setup.sh                            # Indonesian + Kiswahili + Spanish
  ./setup.sh --languages es             # Spanish-only text build
  ./setup.sh --languages id,es --clean  # Indonesian + Spanish
  ./setup.sh --languages ipa            # smallest IPA-only build

MAKE selects an alternative make executable. JOBS sets the default job count.
NANOTTS_LANGUAGES sets the default language profile. Other Makefile tuning
variables such as CC, CFLAGS, USE_LIBM, MAX_EVENTS, CONTEXT_BYTES, and
AUDIO_BLOCK remain available in the environment or on a direct make command.
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
        --languages=*|--language=*)
            LANGUAGES=${1#*=}
            ;;
        --clean)
            CLEAN_FIRST=1
            ;;
        --no-test)
            RUN_TESTS=0
            ;;
        -j|--jobs)
            [ "$#" -ge 2 ] || die "$1 requires a positive integer"
            JOBS=$2
            shift
            ;;
        --jobs=*)
            JOBS=${1#*=}
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1 (run ./setup.sh --help)"
            ;;
    esac
    shift
done

LANG_ID=0
LANG_SW=0
LANG_ES=0
TEXT_FRONTEND=1

case "$LANGUAGES" in
    all)
        LANG_ID=1; LANG_SW=1; LANG_ES=1
        ;;
    ipa|none|off)
        TEXT_FRONTEND=0
        ;;
    '')
        die "language set must not be empty"
        ;;
    *)
        old_ifs=$IFS
        IFS=,
        # Intentional word splitting on the comma-separated profile.
        set -- $LANGUAGES
        IFS=$old_ifs
        for language in "$@"; do
            case "$language" in
                id|indonesian)
                    [ "$LANG_ID" -eq 0 ] || die "duplicate language: id"
                    LANG_ID=1
                    ;;
                sw|swahili|kiswahili)
                    [ "$LANG_SW" -eq 0 ] || die "duplicate language: sw"
                    LANG_SW=1
                    ;;
                es|spanish|espanol|castellano)
                    [ "$LANG_ES" -eq 0 ] || die "duplicate language: es"
                    LANG_ES=1
                    ;;
                ipa|none|off)
                    die "ipa cannot be combined with text languages"
                    ;;
                *)
                    die "invalid language '$language'; use id, sw, es, all, or ipa"
                    ;;
            esac
        done
        ;;
esac

if [ "$TEXT_FRONTEND" -eq 1 ] &&
   [ "$LANG_ID" -eq 0 ] && [ "$LANG_SW" -eq 0 ] && [ "$LANG_ES" -eq 0 ]; then
    die "select at least one text language or use ipa"
fi

case "$TEXT_FRONTEND:$LANG_ID$LANG_SW$LANG_ES" in
    0:000) PROFILE=ipa ;;
    1:100) PROFILE=id ;;
    1:010) PROFILE=sw ;;
    1:001) PROFILE=es ;;
    1:110) PROFILE=id-sw ;;
    1:101) PROFILE=id-es ;;
    1:011) PROFILE=sw-es ;;
    1:111) PROFILE=all ;;
    *) die "internal language-profile error" ;;
esac

[ -n "$JOBS" ] || JOBS=$(detect_jobs)
case "$JOBS" in
    ''|*[!0-9]*|0) die "job count must be a positive integer" ;;
esac

command -v "$MAKE_CMD" >/dev/null 2>&1 || die "make executable not found: $MAKE_CMD"
if [ "$RUN_TESTS" -eq 1 ]; then
    command -v cmake >/dev/null 2>&1 || \
        die "cmake is required for tests; install it or use --no-test"
    command -v ctest >/dev/null 2>&1 || \
        die "ctest is required for tests; install it or use --no-test"
fi

MAKE_BUILD="build-make-$PROFILE"
TEST_BUILD="build-test-$PROFILE"
printf 'NanoTTS setup: profile=%s jobs=%s\n' "$PROFILE" "$JOBS"

if [ "$CLEAN_FIRST" -eq 1 ]; then
    rm -rf "$ROOT_DIR/$MAKE_BUILD" "$ROOT_DIR/$TEST_BUILD"
fi

"$MAKE_CMD" -C "$ROOT_DIR" -j"$JOBS" all \
    BUILD="$MAKE_BUILD" \
    TEXT_FRONTEND="$TEXT_FRONTEND" \
    LANG_ID="$LANG_ID" LANG_SW="$LANG_SW" LANG_ES="$LANG_ES"

if [ "$RUN_TESTS" -eq 1 ]; then
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/$TEST_BUILD" \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DNANOTTS_ENABLE_TEXT_FRONTEND=$( [ "$TEXT_FRONTEND" -eq 1 ] && printf ON || printf OFF ) \
        -DNANOTTS_ENABLE_LANG_ID=$( [ "$LANG_ID" -eq 1 ] && printf ON || printf OFF ) \
        -DNANOTTS_ENABLE_LANG_SW=$( [ "$LANG_SW" -eq 1 ] && printf ON || printf OFF ) \
        -DNANOTTS_ENABLE_LANG_ES=$( [ "$LANG_ES" -eq 1 ] && printf ON || printf OFF )
    cmake --build "$ROOT_DIR/$TEST_BUILD" --parallel "$JOBS"
    ctest --test-dir "$ROOT_DIR/$TEST_BUILD" --output-on-failure
fi

CLI="$ROOT_DIR/$MAKE_BUILD/nanotts"
printf '\nBuild complete. CLI: %s\n' "$CLI"
[ "$LANG_ID" -eq 0 ] || \
    printf 'Indonesian: %s --lang id --text "selamat pagi" -o id.wav\n' "$CLI"
[ "$LANG_SW" -eq 0 ] || \
    printf 'Kiswahili:  %s --lang sw --text "habari yako" -o sw.wav\n' "$CLI"
[ "$LANG_ES" -eq 0 ] || \
    printf 'Spanish:    %s --lang es --text "hola, buenos días" -o es.wav\n' "$CLI"
if [ "$PROFILE" = ipa ]; then
    printf 'IPA:        %s --ipa "h_o_l_a" -o speech.wav\n' "$CLI"
fi
