# NanoTTS

NanoTTS is a compact, clean-room, MIT-licensed text-to-speech library for
microcontrollers and other resource-constrained systems. It is portable C99,
uses no heap, and renders signed 16-bit mono PCM through a caller-provided
callback.

NanoTTS 0.5 has six optional text frontends:

| Code | Language | Compact frontend scope |
|---|---|---|
| `id` | Indonesian | rule-based G2P, numbers, acronyms, explicit `é/è/ê` |
| `sw` | Kiswahili | regular orthography, loan digraphs, syllabic nasals, numbers |
| `es` | Spanish | Latin-American-style seseo/yeísmo, stress, numbers, acronyms |
| `ms` | Malay | Malay Rumi, `e` ambiguity aid, final diphthongs, numbers |
| `mi` | Māori | macrons/doubled long vowels, `wh`, `ng`, compact stress |
| `haw` | Hawaiian | ʻokina, kahakō, common diphthongs, compact moraic stress |

Every language feeds the same small phone-event representation and formant
renderer. Unused frontends are removed at build time. A central X-macro
registry supplies the public IDs, codes, names, aliases, parser dispatch, and
prosody-profile mapping.

## Highlights

- Portable C99; no OS, filesystem, audio library, threads, locale, or heap.
- Caller-owned opaque context with compile-time size validation.
- Strict bounded UTF-8 and `nanotts-ipa/1` parsing.
- Original source/filter renderer with stops, affricates, fricatives, nasals,
  diphthongs, coarticulation, taps/trills, and phrase contours.
- Per-language timing and pitch profiles selected once before rendering.
- No language dispatch in phone, 5 ms frame, or sample loops.
- Single-language builds preprocess to a direct parser call.
- Optional raw PCM and division-free PWM duty adapters for MCU deployment.
- Host CLI can produce WAV, signed PCM16LE, or PWM compare streams.
- Optional no-`libm` build using internal approximations.
- CMake package, Makefile, setup helper, tests, examples, and CI profiles.

NanoTTS does not link to eSpeak and does not include its code, rules,
dictionaries, voices, or generated audio. eSpeak may be used externally as an
optional IPA producer or compatibility oracle.

## Quick start

Build all languages and output adapters, then run tests:

```sh
./setup.sh
```

Select only what the firmware needs:

```sh
./setup.sh --languages ms --outputs pwm
./setup.sh --languages mi,haw --outputs pwm,pcm
./setup.sh --languages id,es --outputs none
./setup.sh --languages ipa --outputs pwm
```

The profile name appears in the build directory, for example:

```text
build-make-ms-pwm/nanotts
build-make-mi-haw-all/nanotts
build-make-ipa-none/nanotts
```

Generate speech:

```sh
BIN=./build-make-all-all/nanotts

$BIN --lang id  --text "selamat pagi" -o id.wav
$BIN --lang sw  --text "habari yako" -o sw.wav
$BIN --lang es  --text "hola, buenos días" -o es.wav
$BIN --lang ms  --text "apa khabar" -o ms.wav
$BIN --lang mi  --text "kia ora, Aotearoa" -o mi.wav
$BIN --lang haw --text "aloha Hawaiʻi" -o haw.wav
```

List compiled modules:

```sh
$BIN --list-languages
```

Inspect events without rendering:

```sh
$BIN --lang mi --text "whānau Māori" --dump-phones
```

## WAV, PCM, and PWM output

WAV is a host-only CLI container. The library itself always emits PCM callback
blocks. Optional adapters convert those blocks without changing the renderer:

```text
renderer -> int16 PCM callback -> application I2S/DAC
                              -> NanoTTS::PCM -> PCM16LE bytes
                              -> NanoTTS::PWM -> timer compare values
```

CLI examples:

```sh
# Headerless signed 16-bit little-endian PCM
$BIN --lang ms --text "sistem sedia" \
  --format pcm -o speech.pcm

# Headerless uint16 little-endian timer duty values, one per audio sample
$BIN --lang haw --text "aloha" \
  --format pwm --pwm-top 1023 -o speech.pwm
```

PWM output is **not** a carrier bitstream. The MCU timer supplies a
high-frequency carrier and DMA updates its compare register at the NanoTTS
sample rate. See [docs/OUTPUTS.md](docs/OUTPUTS.md) and
`examples/nanotts_mcu_pwm.c`.

## CMake

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Important options:

```text
NANOTTS_ENABLE_TEXT_FRONTEND
NANOTTS_ENABLE_LANG_ID
NANOTTS_ENABLE_LANG_SW
NANOTTS_ENABLE_LANG_ES
NANOTTS_ENABLE_LANG_MS
NANOTTS_ENABLE_LANG_MI
NANOTTS_ENABLE_LANG_HAW
NANOTTS_BUILD_PWM
NANOTTS_BUILD_PCM
NANOTTS_USE_LIBM
NANOTTS_MAX_EVENTS
NANOTTS_CONTEXT_BYTES
NANOTTS_AUDIO_BLOCK
```

A compact Māori-plus-PWM cross-build:

```sh
cmake -S . -B build-arm-mi \
  -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_BUILD_CLI=OFF \
  -DNANOTTS_BUILD_TESTS=OFF \
  -DNANOTTS_BUILD_EXAMPLES=OFF \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=OFF \
  -DNANOTTS_ENABLE_LANG_MS=OFF \
  -DNANOTTS_ENABLE_LANG_MI=ON \
  -DNANOTTS_ENABLE_LANG_HAW=OFF \
  -DNANOTTS_BUILD_PWM=ON \
  -DNANOTTS_BUILD_PCM=OFF \
  -DNANOTTS_USE_LIBM=OFF \
  -DNANOTTS_MAX_EVENTS=128 \
  -DNANOTTS_CONTEXT_BYTES=1024 \
  -DNANOTTS_AUDIO_BLOCK=64
cmake --build build-arm-mi
```

Installed targets:

```cmake
find_package(NanoTTS 0.5 CONFIG REQUIRED)
target_link_libraries(app PRIVATE NanoTTS::NanoTTS NanoTTS::PWM)
```

`NanoTTS::PCM` is available when the raw PCM adapter was built.

## Core C API

```c
#include <nanotts/nanotts.h>

static int audio_write(void *user, const int16_t *samples, size_t count)
{
    audio_queue_t *queue = user;
    return audio_queue_submit(queue, samples, count) ? 0 : 1;
}

int speak(audio_queue_t *queue)
{
    static nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t parse;

    if (nanotts_init(&tts, 16000u, NANOTTS_LANG_MALAY) != NANOTTS_OK) {
        return -1;
    }
    nanotts_default_options(&options);
    return nanotts_speak_text(
        &tts, "bateri hampir habis", NANOTTS_NPOS,
        &options, audio_write, queue, &parse) == NANOTTS_OK ? 0 : -1;
}
```

The language may be changed while the context is idle:

```c
nanotts_set_language(&tts, NANOTTS_LANG_HAWAIIAN);
```

The same selection also controls the prosody profile when rendering explicit
IPA:

```c
nanotts_init(&tts, 16000u, NANOTTS_LANG_MAORI);
nanotts_speak_ipa(&tts, "k_i_ˈa o_ɾ_a", NANOTTS_NPOS,
                  &options, audio_write, queue, &parse);
```

## PWM API

```c
#include <nanotts/nanotts.h>
#include <nanotts/nanotts_pwm.h>

static int submit_compare_values(
    void *user,
    const uint16_t *values,
    size_t count)
{
    return pwm_dma_submit(user, values, count) ? 0 : 1;
}

uint16_t scratch[64];
nanotts_pwm_output_t pwm;

nanotts_pwm_output_init(
    &pwm, 1023u, 0,
    scratch, sizeof(scratch) / sizeof(scratch[0]),
    submit_compare_values, &pwm_device);

nanotts_speak_text(
    &tts, "aloha", NANOTTS_NPOS, &options,
    nanotts_pwm_write_pcm, &pwm, &parse);
```

The adapter maps `-32768..32767` to `0..top` with a 32-bit multiply and shift,
not a per-sample division. It uses caller-owned scratch memory and propagates
back-pressure through `NANOTTS_ERR_CALLBACK_ABORTED`.

## IPA input

The IPA path is language-neutral within the supported phone inventory. Use
explicit separators for predictable parsing:

```sh
$BIN --ipa "s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i" -o speech.wav
```

A language may be supplied with IPA to select its prosody profile:

```sh
$BIN --lang mi --ipa "k_i_ˈa o_ɾ_a" -o kia-ora.wav
```

External producer example:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi" |
  $BIN --ipa-file - -o speech.wav
```

See [docs/IPA_PROFILE.md](docs/IPA_PROFILE.md).

## Architecture and extension

```text
              compile-time language registry
                         |
selected frontend -> phone events -> selected prosody -> shared renderer
IPA parser -------^                                      |
                                                         +-> PCM callback
                                                             +-> PWM adapter
                                                             +-> PCM adapter
```

A language frontend is a bounded UTF-8 text-to-event function. It contains no
voice, oscillator, filter, audio driver, or runtime plug-in loader. Adding a
module does not introduce a language branch into the renderer. The complete
process is documented in [docs/ADDING_A_LANGUAGE.md](docs/ADDING_A_LANGUAGE.md).

## Important limitations

- The supplied voice is deliberately robotic. No naturalness claim is made.
- Text frontends are compact approximations, not full dictionaries or
  morphological analyzers. Proper names, code-switching, abbreviations, and
  lexical ambiguity may need explicit IPA.
- Malay plain `e` is inherently ambiguous; a small exception set is included.
- Māori stress, doubled-vowel spelling, and common diphthongs are supported,
  but compounds, iwi-specific `wh`, and lexical exceptions may need IPA.
- Hawaiian ʻokina, kahakō, common diphthongs, and a compact moraic stress model
  are supported; `w` variation and compound stress may need IPA.
- Polynesian number input is currently spoken digit by digit, while Indonesian,
  Kiswahili, Spanish, and Malay include cardinal expansion.
- All languages share one neutral acoustic phone table.
- The renderer uses `float`; no fixed-point backend is included yet.
- Text is normalized into a bounded event array before rendering.

## Documentation

- [C API](docs/API.md)
- [Output adapters](docs/OUTPUTS.md)
- [Adding an output adapter](docs/ADDING_AN_OUTPUT.md)
- [Supported languages](docs/LANGUAGES.md)
- [Adding a language](docs/ADDING_A_LANGUAGE.md)
- [Architecture](docs/DESIGN.md)
- [IPA profile](docs/IPA_PROFILE.md)
- [MCU porting](docs/PORTING.md)
- [Validation](docs/VALIDATION.md)
- [Clean-room provenance](PROVENANCE.md)

## License

MIT. See [LICENSE](LICENSE).
