#!/bin/sh
# SPDX-License-Identifier: MIT
# Build and test idtts with its plain Makefile.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MAKE_CMD=${MAKE:-make}
RUN_TESTS=1
CLEAN_FIRST=0
JOBS=${JOBS:-}

usage() {
    cat <<'USAGE'
Usage: ./setup.sh [options]

Build idtts with the repository Makefile and, by default, run the CMake/CTest
suite afterward.

Options:
  --clean          Remove previous build trees before building.
  --no-test        Build only; do not run the CMake/CTest suite.
  -j N, --jobs N   Use N parallel make jobs.
  -h, --help       Show this help text.

Environment variables accepted by the Makefile are preserved, including CC,
CFLAGS, TEXT_FRONTEND, USE_LIBM, MAX_EVENTS, CONTEXT_BYTES, and AUDIO_BLOCK.
MAKE selects an alternative make executable; JOBS sets the default job count.
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

printf 'idtts setup: root=%s jobs=%s\n' "$ROOT_DIR" "$JOBS"

if [ "$CLEAN_FIRST" -eq 1 ]; then
    "$MAKE_CMD" -C "$ROOT_DIR" clean
fi

"$MAKE_CMD" -C "$ROOT_DIR" -j"$JOBS" all

if [ "$RUN_TESTS" -eq 1 ]; then
    "$MAKE_CMD" -C "$ROOT_DIR" test
fi

printf '\nBuild complete. CLI: %s/build-make/idtts\n' "$ROOT_DIR"
printf 'Example: %s/build-make/idtts --text "selamat pagi" -o speech.wav\n' "$ROOT_DIR"
