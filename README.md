# NanoTTS

NanoTTS is a compact, clean-room, MIT-licensed text-to-speech library for
microcontrollers and other resource-constrained systems. It is written in
portable C99, performs no heap allocation, and emits signed 16-bit mono PCM
through a caller-provided callback.

The 0.3 series supports two independent text front ends:

| Code | Language | Text module |
|---|---|---|
| `id` | Indonesian | compact rule-based G2P, numbers, acronym handling |
| `sw` | Kiswahili | regular orthography, penultimate stress, numbers, loan digraphs |

Both languages share one small IPA parser, event representation, prosody layer,
and source/filter formant renderer. A caller selects the text language before
parsing. Unused language modules can be left out of the binary entirely.

## Highlights

- Portable C99 library with no operating-system or audio-driver dependency.
- No heap allocation, global mutable state, files, threads, or locale use.
- Explicit language selection at initialization or with `nanotts_set_language`.
- Compile-time Indonesian-only, Kiswahili-only, dual-language, or IPA-only
  configurations.
- One language dispatch per text utterance; no language dispatch in the phone,
  frame, or sample loops. A single-language build uses a direct parser call.
- Strict UTF-8 parser for the finite `nanotts-ipa/1` profile.
- Original formant/source-filter renderer with stops, affricates, fricatives,
  nasals, diphthongs, coarticulation, and punctuation-aware phrase contours.
- Synchronous PCM callback suitable for WAV output, I2S, DAC, or PWM queues.
- Optional no-`libm` build using compact internal approximations.
- CMake package, plain Makefile, setup helper, CLI, tests, and MCU example.

NanoTTS does not link to eSpeak and does not contain eSpeak rules, dictionaries,
voices, source, or generated audio. eSpeak can optionally be used as an
external IPA producer during development or on a host.

## Quick start

Build both language modules and run the tests:

```sh
./setup.sh
```

Build one language or an IPA-only renderer:

```sh
./setup.sh --languages id
./setup.sh --languages sw
./setup.sh --languages ipa
```

The default dual-language CLI is placed at:

```text
build-make-all/nanotts
```

Speak Indonesian:

```sh
./build-make-all/nanotts \
  --lang id --text "selamat pagi, sistem siap" \
  -o indonesia.wav
```

Speak Kiswahili:

```sh
./build-make-all/nanotts \
  --lang sw --text "habari yako, mfumo uko tayari" \
  -o kiswahili.wav
```

List modules compiled into a binary:

```sh
./build-make-all/nanotts --list-languages
```

Inspect normalized events without rendering:

```sh
./build-make-all/nanotts \
  --lang sw --text "ng'ombe na chakula" --dump-phones
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
  -DNANOTTS_ENABLE_LANG_SW=OFF

# Kiswahili only
cmake -S . -B build-sw \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=ON

# IPA only: no text normalizer or G2P module
cmake -S . -B build-ipa \
  -DNANOTTS_ENABLE_TEXT_FRONTEND=OFF
```

`NANOTTS_ENABLE_LANG_ID` and `NANOTTS_ENABLE_LANG_SW` affect flash only; they do
not add per-sample renderer cost. CMake rejects a text-enabled configuration
with no selected language.

## Library use

```c
#include <nanotts/nanotts.h>

static int audio_write(void *user, const int16_t *pcm, size_t count)
{
    audio_queue_t *queue = (audio_queue_t *)user;
    return audio_queue_write(queue, pcm, count) ? 0 : 1;
}

int speak_kiswahili(audio_queue_t *queue)
{
    static nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t parse;

    if (nanotts_init(&tts, 16000u, NANOTTS_LANG_KISWAHILI) != NANOTTS_OK) {
        return -1;
    }

    nanotts_default_options(&options);
    return nanotts_speak_text(
        &tts,
        "habari yako",
        NANOTTS_NPOS,
        &options,
        audio_write,
        queue,
        &parse) == NANOTTS_OK ? 0 : -1;
}
```

The selected language is stored in the caller-owned context. It can be changed
while idle:

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
nanotts_speak_ipa(&tts, "h_a_b_ˈa_r_i", NANOTTS_NPOS,
                  &options, audio_write, queue, &parse);
```

External eSpeak examples:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi" |
  ./build/nanotts --ipa-file - -o id-ipa.wav

espeak-ng -q -v sw --ipa=1 --sep=_ "habari yako" |
  ./build/nanotts --ipa-file - -o sw-ipa.wav
```

The finite accepted profile and aliases are documented in
[docs/IPA_PROFILE.md](docs/IPA_PROFILE.md).

## MCU-oriented configuration

A compact one-language build retains direct text input while excluding the
other language:

```sh
cmake -S . -B build-mcu-sw \
  -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_BUILD_CLI=OFF \
  -DNANOTTS_BUILD_TESTS=OFF \
  -DNANOTTS_BUILD_EXAMPLES=OFF \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=ON \
  -DNANOTTS_USE_LIBM=OFF \
  -DNANOTTS_MAX_EVENTS=128 \
  -DNANOTTS_CONTEXT_BYTES=1024 \
  -DNANOTTS_AUDIO_BLOCK=64
cmake --build build-mcu-sw
```

For precomputed IPA or event arrays, disable all text modules:

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
and penultimate-stress marking. The module interface is one internal function:

```c
nanotts_result_t nanotts_lang_xx_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info);
```

The dual-language build performs one `switch` when `nanotts_parse_text` is
called. The renderer never checks the language. In an Indonesian-only or
Kiswahili-only build, preprocessing removes the switch and calls the one parser
directly.

See [docs/ADDING_A_LANGUAGE.md](docs/ADDING_A_LANGUAGE.md) and the compilable
shape in `src/lang/nanotts_lang_template.c.example` for the extension process.
Language-specific behavior and current limitations are in
[docs/LANGUAGES.md](docs/LANGUAGES.md).

## Important limitations

- The supplied voice is deliberately synthetic. The Indonesian renderer has
  undergone objective regression analysis, but no formal native-listener score
  is claimed. Kiswahili requires its own native-listener transcription study.
- Text modules are intentionally compact, not complete linguistic analyzers.
  Proper names, foreign words, abbreviations, code-switching, and lexical
  ambiguity may require explicit IPA or application-side overrides.
- Both languages currently share one neutral acoustic table. Kiswahili sounds
  absent from the original Indonesian inventory have compact independently
  authored approximations; they are not a speaker-specific voice.
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
