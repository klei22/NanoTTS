# NanoTTS

NanoTTS is a compact, clean-room, MIT-licensed text-to-speech library for
microcontrollers and other resource-constrained systems. It is written in
portable C99, performs no heap allocation, and emits signed 16-bit mono PCM
through a caller-provided callback.

NanoTTS 0.4 provides three independent, optional text front ends:

| Code | Language | Text module |
|---|---|---|
| `id` | Indonesian | compact rule-based G2P, numbers, acronym handling |
| `sw` | Kiswahili | regular orthography, penultimate stress, numbers, loan digraphs |
| `es` | Spanish | Latin-American-style seseo and yeísmo, written stress, numbers |

All modules share one small IPA parser, event representation, prosody layer,
and source/filter formant renderer. The caller selects a text language before
parsing. Unused language modules can be excluded from the binary entirely.

## Highlights

- Portable C99 with no operating-system or audio-driver dependency.
- No heap allocation, global mutable state, files, threads, or locale use.
- Explicit language selection at initialization or with `nanotts_set_language`.
- Compile-time single-language, selected multi-language, all-language, and
  IPA-only configurations.
- One language dispatch per text utterance; no language dispatch in the phone,
  frame, or sample loops. A single-language build calls its parser directly.
- Strict UTF-8 parser for the finite `nanotts-ipa/1` profile.
- Original formant/source-filter renderer with stops, affricates, fricatives,
  nasals, diphthongs, coarticulation, and punctuation-aware phrase contours.
- Synchronous PCM callback suitable for WAV output, I2S, DAC, or PWM queues.
- Optional no-`libm` build using compact internal approximations.
- CMake package, plain Makefile, setup helper, CLI, tests, and MCU example.

NanoTTS does not link to eSpeak and does not contain eSpeak rules,
dictionaries, voices, source, or generated audio. eSpeak may be used as an
optional external IPA producer during development or on a host.

## Quick start

Build all text modules and run the tests:

```sh
./setup.sh
```

Build selected modules or the IPA-only renderer:

```sh
./setup.sh --languages id
./setup.sh --languages sw
./setup.sh --languages es
./setup.sh --languages id,es
./setup.sh --languages ipa
```

The default all-language CLI is placed at:

```text
build-make-all/nanotts
```

Generate WAV files:

```sh
./build-make-all/nanotts \
  --lang id --text "selamat pagi, sistem siap" \
  -o indonesia.wav

./build-make-all/nanotts \
  --lang sw --text "habari yako, mfumo uko tayari" \
  -o kiswahili.wav

./build-make-all/nanotts \
  --lang es --text "hola, buenos días; el sistema está listo" \
  -o espanol.wav
```

List the modules compiled into a binary:

```sh
./build-make-all/nanotts --list-languages
```

Inspect normalized events without rendering:

```sh
./build-make-all/nanotts \
  --lang es --text "queso, guitarra y pingüino" --dump-phones
```

## CMake build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Useful module configurations:

```sh
# Indonesian only
cmake -S . -B build-id \
  -DNANOTTS_ENABLE_LANG_ID=ON \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=OFF

# Kiswahili only
cmake -S . -B build-sw \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=ON \
  -DNANOTTS_ENABLE_LANG_ES=OFF

# Spanish only
cmake -S . -B build-es \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=ON

# IPA only: no text normalizer or G2P module
cmake -S . -B build-ipa \
  -DNANOTTS_ENABLE_TEXT_FRONTEND=OFF
```

`NANOTTS_ENABLE_LANG_ID`, `NANOTTS_ENABLE_LANG_SW`, and
`NANOTTS_ENABLE_LANG_ES` affect flash only. They do not add per-sample
renderer cost. CMake rejects a text-enabled configuration with no selected
language.

## Library use

```c
#include <nanotts/nanotts.h>

static int audio_write(void *user, const int16_t *pcm, size_t count)
{
    audio_queue_t *queue = (audio_queue_t *)user;
    return audio_queue_write(queue, pcm, count) ? 0 : 1;
}

int speak_spanish(audio_queue_t *queue)
{
    static nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t parse;

    if (nanotts_init(&tts, 16000u, NANOTTS_LANG_SPANISH) != NANOTTS_OK) {
        return -1;
    }

    nanotts_default_options(&options);
    return nanotts_speak_text(
        &tts,
        "hola, el sistema está listo",
        NANOTTS_NPOS,
        &options,
        audio_write,
        queue,
        &parse) == NANOTTS_OK ? 0 : -1;
}
```

The selected language is stored in the caller-owned context and may be changed
while it is idle:

```c
nanotts_set_language(&tts, NANOTTS_LANG_INDONESIAN);
nanotts_speak_text(&tts, "terima kasih", NANOTTS_NPOS,
                   &options, audio_write, queue, &parse);
```

`nanotts_reset` clears renderer and event state while preserving the selected
language and sample rate. Separate contexts can be used concurrently; one
context is not reentrant.

## IPA input

The IPA path is language-neutral within NanoTTS's supported phone inventory.
Initialize with `NANOTTS_LANG_NONE` when only IPA will be used:

```c
nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE);
nanotts_speak_ipa(&tts, "ˈo_l_a", NANOTTS_NPOS,
                  &options, audio_write, queue, &parse);
```

External eSpeak examples:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi" |
  ./build/nanotts --ipa-file - -o id-ipa.wav

espeak-ng -q -v sw --ipa=1 --sep=_ "habari yako" |
  ./build/nanotts --ipa-file - -o sw-ipa.wav

espeak-ng -q -v es-la --ipa=1 --sep=_ "hola, buenos días" |
  ./build/nanotts --ipa-file - -o es-ipa.wav
```

The finite accepted profile and aliases are documented in
[docs/IPA_PROFILE.md](docs/IPA_PROFILE.md).

## MCU-oriented configuration

A compact Spanish-only build retains direct text input while excluding the
other language modules:

```sh
cmake -S . -B build-mcu-es \
  -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_BUILD_CLI=OFF \
  -DNANOTTS_BUILD_TESTS=OFF \
  -DNANOTTS_BUILD_EXAMPLES=OFF \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=ON \
  -DNANOTTS_USE_LIBM=OFF \
  -DNANOTTS_MAX_EVENTS=128 \
  -DNANOTTS_CONTEXT_BYTES=1024 \
  -DNANOTTS_AUDIO_BLOCK=64
cmake --build build-mcu-es
```

For precomputed IPA or event arrays, disable the complete text front end:

```sh
-DNANOTTS_ENABLE_TEXT_FRONTEND=OFF
```

The public context has a compile-time size check. An undersized
`NANOTTS_CONTEXT_BYTES` setting fails compilation rather than corrupting
memory. Final flash and RAM use depend on compiler, ABI, floating-point runtime,
and linker garbage collection; measured configurations are recorded in
[docs/VALIDATION.md](docs/VALIDATION.md).

## Language-module architecture

A language module is only a bounded UTF-8 text-to-event converter:

```text
selected text module ──┐
                       ├─ common phone events → shared renderer → PCM
nanotts-ipa/1 parser ──┘
```

It does not own an audio backend, oscillator, filter, or voice. Shared helpers
provide UTF-8 decoding, pauses, word-boundary flags, bounded temporary events,
and stress helpers. The module interface is one internal function:

```c
nanotts_result_t nanotts_lang_xx_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info);
```

A multi-language build performs one `switch` when `nanotts_parse_text` is
called. The renderer never checks the language. In a single-language build,
preprocessing removes the switch and calls that parser directly.

See [docs/ADDING_A_LANGUAGE.md](docs/ADDING_A_LANGUAGE.md) and
`src/lang/nanotts_lang_template.c.example` for the extension process.
Language-specific behavior and limitations are in
[docs/LANGUAGES.md](docs/LANGUAGES.md).

## Important limitations

- The supplied voice is deliberately synthetic. The Indonesian renderer has
  undergone objective regression analysis, but no formal native-listener score
  is claimed. Kiswahili and Spanish still require native-listener transcription
  studies.
- Text modules are intentionally compact, not complete linguistic analyzers.
  Proper names, foreign words, abbreviations, code-switching, and lexical
  ambiguity may require explicit IPA or application-side overrides.
- All languages share one neutral acoustic table. Phones introduced for
  Kiswahili and Spanish use compact independently authored approximations, not
  speaker-specific voices.
- The Spanish module intentionally defaults to broadly used seseo and yeísmo.
  Distinción, lleísmo, dialect-specific aspiration, and lexical exceptions are
  best supplied through explicit IPA.
- The renderer uses single-precision floating point. `NANOTTS_USE_LIBM=OFF`
  removes transcendental library calls but is not a fixed-point backend.
- Input is normalized into a bounded event array before rendering; it is not a
  streaming text parser.

## Documentation

- [C API](docs/API.md)
- [Supported languages](docs/LANGUAGES.md)
- [Adding a language](docs/ADDING_A_LANGUAGE.md)
- [Architecture](docs/DESIGN.md)
- [IPA profile](docs/IPA_PROFILE.md)
- [MCU porting](docs/PORTING.md)
- `examples/nanotts_language_select.c`: select a text module before parsing
- `examples/nanotts_mcu_callback.c`: language-neutral IPA callback integration
- [Validation](docs/VALIDATION.md)
- [Indonesian intelligibility analysis](docs/INTELLIGIBILITY.md)
- [Clean-room provenance](PROVENANCE.md)

## License

MIT. See [LICENSE](LICENSE).
