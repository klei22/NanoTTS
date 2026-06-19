# idtts

`idtts` is a compact, clean-room Indonesian speech synthesizer written in
portable C99. It accepts either a deliberately small IPA profile compatible
with separated Indonesian eSpeak output, or Indonesian text through an
independent built-in grapheme-to-phoneme front end. It produces signed 16-bit
mono PCM through a caller-provided callback.

The runtime is MIT licensed, uses no heap allocation, contains no operating
system or audio-driver code, and does not link to eSpeak. The current voice is
an original rule-based formant voice. Version 0.2.1 includes the 0.2 acoustic corrections and adds a one-command host setup workflow. Version 0.2.0 corrected the baseline's
severe noise/vowel imbalance and is suitable for embedded evaluation, but it
remains deliberately robotic and has not yet received a formal native-listener
intelligibility score.

## What is implemented

- Strict UTF-8 parser for the versioned `espeak-id-ipa/1` profile.
- Aliases used by eSpeak, including `g`/`ɡ`, tied or untied affricates,
  `ç`/`ʃ`, stress marks, length marks, and Indonesian diphthongs.
- Indonesian-only text front end with digraphs, regular letter mappings,
  basic stress, a small independently authored `e` disambiguation table,
  integer expansion through billions, and Indonesian spelling of all-uppercase
  acronyms such as `GPS`.
- Source/filter renderer with a stable four-resonator voiced cascade, a broad
  F2 detail path, separately shaped frication, a nasal path, stop and affricate
  staging, diphthong motion, immediate-neighbor coarticulation, trill/tap
  behavior, and punctuation-aware phrase contours.
- Synchronous PCM callback suitable for WAV files, I2S, DAC, or PWM queues.
- Optional no-`libm` build using compact internal approximations.
- CMake package files, a plain Makefile, CLI, tests, MCU example, and an
  eSpeak compatibility checker.

## Build and run

For a normal host build plus the complete test suite:

```sh
./setup.sh
```

Use `./setup.sh --clean` for a clean rebuild, `--no-test` when CMake is not
installed, and `--jobs N` to choose the parallel job count. The script builds
the CLI at `build-make/idtts`. Run `./setup.sh --help` for all options.

The equivalent CMake commands are:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Synthesize directly from Indonesian text:

```sh
./build/idtts --text "selamat pagi, sistem siap" -o speech.wav
```

Use eSpeak only as an external text-to-IPA producer:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi" |
  ./build/idtts --ipa-file - -o speech.wav
```

Inspect the normalized events without rendering audio:

```sh
./build/idtts --ipa "s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i" --dump-phones
```

The CLI supports 8, 16, and other sample rates from 8000 through 24000 Hz,
rate and pitch changes, volume, automatic per-phrase punctuation contours or
explicit rise/fall/continuation/level overrides, strict or permissive IPA
parsing, and file/stdin input. Run
`./build/idtts --help` for all options.

## Library use

```c
#include "idtts/idtts.h"

static int audio_write(void *user, const int16_t *pcm, size_t count)
{
    /* Queue `count` mono PCM samples to I2S/DAC/PWM here. */
    (void)user;
    return platform_audio_queue(pcm, count) ? 0 : 1;
}

int speak(void)
{
    static idtts_t tts;
    idtts_options_t options;
    idtts_parse_info_t parse;

    if (idtts_init(&tts, 16000) != IDTTS_OK)
        return -1;

    idtts_default_options(&options);
    return idtts_speak_ipa(
        &tts,
        "s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i",
        IDTTS_NPOS,
        &options,
        audio_write,
        NULL,
        &parse) == IDTTS_OK ? 0 : -1;
}
```

The callback is invoked with blocks of native `int16_t` PCM. It may block or
copy into a DMA ring. Returning nonzero aborts synthesis. The library performs
no allocation and retains no pointer to the supplied text or callback buffer.

## MCU-oriented configurations

Default build:

```text
512 phone events
3072-byte public context
128-sample callback block
IPA parser + Indonesian G2P + renderer
```

A compact IPA-only configuration:

```sh
cmake -S . -B build-mcu \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DIDTTS_ENABLE_TEXT_FRONTEND=OFF \
  -DIDTTS_USE_LIBM=OFF \
  -DIDTTS_MAX_EVENTS=128 \
  -DIDTTS_CONTEXT_BYTES=1024 \
  -DIDTTS_AUDIO_BLOCK=64 \
  -DIDTTS_BUILD_CLI=OFF \
  -DIDTTS_BUILD_TESTS=OFF \
  -DIDTTS_BUILD_EXAMPLES=OFF
cmake --build build-mcu
```

The opaque context has a compile-time size check. A configuration that makes
`IDTTS_CONTEXT_BYTES` too small fails to compile instead of corrupting memory.
Context-affecting definitions are exported through the CMake target so an
application built with `idtts::idtts` gets the matching public layout.

Measured in this build environment with GCC `-Os`, the full library's object
sections total about 20.4 KB of code/read-only data before linker and C-library
contributions. The 128-event IPA-only no-`libm` configuration totals about
14.8 KB. These are host object measurements, not promises for any MCU; final
flash use depends on compiler, ABI, dead-code elimination, and the target's
floating-point support.

See [docs/PORTING.md](docs/PORTING.md) for bare-metal integration and memory
tradeoffs, [docs/API.md](docs/API.md) for the API contract,
[docs/INTELLIGIBILITY.md](docs/INTELLIGIBILITY.md) for the 50-case audio
evaluation, and [docs/VALIDATION.md](docs/VALIDATION.md) for the build matrix.

## IPA profile

The supported input is intentionally not arbitrary IPA. Restricting it to the
phones and formatting needed by Indonesian makes the parser small and makes
failures explicit. The complete profile, aliases, punctuation behavior, and
error policy are in [docs/IPA_PROFILE.md](docs/IPA_PROFILE.md).

Recommended producer command:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ TEXT
```

No eSpeak executable, source, voice data, rule table, or dictionary is needed
on the target device.

## Test matrix

The repository tests:

- parser normalization and strict/permissive errors;
- Indonesian G2P digraphs, vowel variants, diphthongs, and numbers;
- deterministic non-silent synthesis;
- API validation and callback abortion;
- normal, IPA-only, no-`libm`, and reduced-memory builds;
- a separate 50-case Indonesian intelligibility corpus and metric tool.

`tools/check_espeak_ipa.py` can generate IPA from a locally installed
`espeak-ng` or `espeak` binary and verify that every corpus line is accepted by
the parser. During this implementation, an additional synthetic scan of all
one-, two-, and three-letter ASCII inputs checked 18,278 outputs from eSpeak
1.48.15 without an unsupported IPA symbol. That is a compatibility smoke test,
not proof that every version, word, dialect, or future output is covered.

## Important limitations

- The supplied acoustic voice has undergone objective level, timing, spectrum,
  isolated-phone, and 50-utterance regression analysis, but not a blinded
  native-listener transcription study.
- The direct text front end is deliberately modest. Plain `e`, diphthong versus
  hiatus, acronyms, foreign names, abbreviations, and code-switching may need
  lexical overrides. The IPA API is the authoritative path when pronunciation
  matters.
- The renderer uses single-precision floating point. The no-`libm` option
  removes transcendental library calls but does not make the renderer
  fixed-point.
- Input is converted to a bounded event array before rendering; it is not a
  streaming text parser.
- The parser supports a selected UTF-8 profile, not full Unicode normalization.

## Design and provenance

The renderer follows general published source/filter and formant-synthesis
principles, while all runtime code and acoustic tables in this repository are
newly authored. It contains no eSpeak, SAM, Flite, or Klatt program source or
voice data. See [docs/DESIGN.md](docs/DESIGN.md) and
[PROVENANCE.md](PROVENANCE.md).

## License

MIT. See [LICENSE](LICENSE).
