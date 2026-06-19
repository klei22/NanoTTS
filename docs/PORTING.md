# Porting NanoTTS to an MCU

## Runtime requirements

The library requires:

- a C99 compiler;
- 8-bit bytes, fixed-width integer types, and at least 32-bit arithmetic;
- single-precision floating point;
- `memset` and `strlen` from the C library;
- a caller-owned `nanotts_t` context;
- a synchronous signed-PCM callback.

It does not require a heap, files, sockets, threads, clocks, locale support, or
an audio library. The desktop CLI is separate from MCU builds.

## Source selection without CMake

Always compile the common runtime:

```text
src/nanotts.c
src/nanotts_language.c
src/nanotts_ipa.c
src/nanotts_voice.c
src/nanotts_synth.c
```

For text input, also compile:

```text
src/lang/nanotts_lang_common.c
src/lang/nanotts_lang_id.c       when Indonesian is enabled
src/lang/nanotts_lang_sw.c       when Kiswahili is enabled
```

Add `include` and `src` to the include path. Define consistently for the
library and every consumer of the public header:

```text
NANOTTS_ENABLE_TEXT_FRONTEND=1 or 0
NANOTTS_ENABLE_LANG_ID=1 or 0
NANOTTS_ENABLE_LANG_SW=1 or 0
NANOTTS_USE_LIBM=1 or 0
NANOTTS_MAX_EVENTS=<capacity>
NANOTTS_CONTEXT_BYTES=<opaque context bytes>
NANOTTS_AUDIO_BLOCK=<callback samples>
```

When `NANOTTS_ENABLE_TEXT_FRONTEND=0`, omit all language files. When it is one,
compile at least one language. `NANOTTS_USE_LIBM=1` may require linking the
target math library; zero uses internal approximations.

## Language profiles and overhead

Choose the smallest required build:

| Profile | Text sources | Text dispatch |
|---|---|---|
| Indonesian-only | common + `lang_id` | direct call |
| Kiswahili-only | common + `lang_sw` | direct call |
| dual-language | common + both | one switch per text utterance |
| IPA-only | none | text API disabled |

The DSP renderer is identical in every profile. Language selection is not
checked in the phone, 5 ms frame, or sample loop.

At runtime, initialize a text build with its selected language:

```c
nanotts_init(&tts, 16000u, NANOTTS_LANG_KISWAHILI);
```

Use `NANOTTS_LANG_NONE` for an IPA-only context.

## Memory tuning

The dominant RAM items are the event array, filter state, and PCM callback
block. Useful starting points are:

| Configuration | Events | Public context | Callback block | Input |
|---|---:|---:|---:|---|
| default | 512 | 3072 B | 128 samples | IPA + selected text modules |
| compact | 128 | 1024 B | 64 samples | IPA or one short-text module |
| very short prompts | 64 | target-dependent | 32 samples | precomputed IPA/events |

Structure padding differs by ABI. A compile-time assertion rejects an
undersized public context. Start conservatively, build for the actual target,
inspect the linker map, and then reduce `NANOTTS_CONTEXT_BYTES`.

Each event is four bytes. Reducing `NANOTTS_MAX_EVENTS` from 512 to 128 saves
1536 bytes before padding. Reducing `NANOTTS_AUDIO_BLOCK` from 128 to 64 saves
another 128 bytes.

The text modules use bounded word-local arrays on the call stack. Account for
those in task stack sizing, especially when number expansion recursively emits
words.

## Audio callback

The renderer calls the callback synchronously from the task that invoked
`nanotts_speak_ipa`, `nanotts_speak_text`, or `nanotts_render_events`:

```c
static int audio_write(void *user, const int16_t *samples, size_t count)
{
    audio_ring_t *ring = (audio_ring_t *)user;
    return audio_ring_write(ring, samples, count) ? 0 : 1;
}
```

The supplied buffer is reused after the callback returns. Copy or consume it
before returning. A nonzero result aborts with
`NANOTTS_ERR_CALLBACK_ABORTED`.

Call synthesis from a normal task, not an interrupt. DMA completion interrupts
should only advance or refill a separate queue.

## Audio settings for clarity

Use 16 kHz when CPU and the output path permit it. Fricatives and affricates in
both current languages rely on energy above the 4 kHz Nyquist limit of an 8 kHz
build. Keep the amplifier and speaker below clipping. Small enclosures can
remove vowel energy or exaggerate frication, so conduct final intelligibility
tests on the production transducer.

### I2S

Configure mono signed 16-bit PCM at the same rate passed to `nanotts_init`.
Queue callback blocks to DMA. For a stereo-only peripheral, duplicate samples
or use a mono slot mode.

### DAC

Convert signed PCM to the DAC's unsigned range:

```c
uint16_t dac12 = (uint16_t)(((int32_t)sample + 32768) >> 4);
```

Use an appropriate reconstruction filter and amplifier biasing.

### PWM

Bias signed PCM to unsigned duty cycle, update from timer/DMA, and low-pass the
output. Keep the PWM carrier well above the audio band.

## CPU considerations

- Cortex-M4F/M7, ESP32-class cores, and other FPU-equipped MCUs are easiest.
- `NANOTTS_USE_LIBM=OFF` avoids runtime transcendental library calls.
- The renderer still uses float multiplies and divisions; benchmark M0/M0+
  targets at the final sample rate and clock.
- Build with optimization and section garbage collection, such as
  `-Os -ffunction-sections -fdata-sections` and linker `--gc-sections`.
- Measure real-time factor during the longest supported utterance, not only an
  isolated vowel.

## CMake cross-build examples

Kiswahili-only text build:

```sh
cmake -S . -B build-arm-sw \
  -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake \
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
cmake --build build-arm-sw
```

IPA-only build:

```sh
cmake -S . -B build-arm-ipa \
  -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_BUILD_CLI=OFF \
  -DNANOTTS_BUILD_TESTS=OFF \
  -DNANOTTS_BUILD_EXAMPLES=OFF \
  -DNANOTTS_ENABLE_TEXT_FRONTEND=OFF \
  -DNANOTTS_USE_LIBM=OFF \
  -DNANOTTS_MAX_EVENTS=128 \
  -DNANOTTS_CONTEXT_BYTES=1024 \
  -DNANOTTS_AUDIO_BLOCK=64
cmake --build build-arm-ipa
```

The output is `libnanotts.a`.

## Deployment patterns

1. Generate IPA on a server, phone, or host and send it to the MCU.
2. Precompute IPA during the firmware build and store strings in flash.
3. Compile one built-in language module for open-ended on-device text.
4. Store normalized `nanotts_event_t` arrays and call
   `nanotts_render_events` for fixed, prevalidated prompts.

The third option is self-contained. The fourth is the smallest and most
deterministic for a fixed prompt set.

## Target validation checklist

- Verify sample rate and peripheral clock.
- Confirm callback blocks do not underrun.
- Confirm the intended language appears in `nanotts_language_available`.
- Test strict IPA and UTF-8 on target.
- Measure task stack, static RAM, flash, cycles, and real-time factor.
- Listen to every required phone and every safety-critical phrase.
- Test maximum event count, long words, number expansion, and callback abortion.
- Test the exact release optimization and floating-point flags.
- Run native-listener transcription tests for each shipped language.
