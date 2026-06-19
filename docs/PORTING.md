# Porting to an MCU

## Runtime requirements

The `idtts` library requires:

- a C99 compiler;
- 8-bit bytes, fixed-width integer types, and at least 32-bit arithmetic;
- single-precision floating point;
- `memset` and `strlen` from the C library;
- a caller-owned `idtts_t` context and a synchronous PCM callback.

It does not require a heap, files, sockets, threads, clocks, locale support, or
an audio library. The desktop CLI is separate and is not part of an MCU build.

## Add the sources directly

For build systems that do not consume CMake, compile:

```text
src/idtts.c
src/idtts_ipa.c
src/idtts_voice.c
src/idtts_synth.c
src/idtts_g2p.c       optional
```

Add `include` and `src` to the include path. Define:

```text
IDTTS_ENABLE_TEXT_FRONTEND=1 or 0
IDTTS_USE_LIBM=1 or 0
IDTTS_MAX_EVENTS=<capacity>
IDTTS_CONTEXT_BYTES=<opaque context size>
IDTTS_AUDIO_BLOCK=<callback block samples>
```

When `IDTTS_USE_LIBM=1`, link the target math library where required. With it
set to zero, the renderer uses internal approximations and has no
transcendental math-library calls.

## Memory tuning

The dominant RAM items are the event array, filter state, and PCM callback
block. Useful starting configurations are:

| Configuration | Events | Context | Callback block | Front end |
|---|---:|---:|---:|---|
| default | 512 | 3072 B | 128 samples | IPA + text |
| compact | 128 | 1024 B | 64 samples | IPA only |
| very short prompts | 64 | 768 B tested here | 32 samples | IPA only |

The values are compile-time parameters, and structure padding differs by ABI.
The source contains a compile-time assertion that rejects an undersized public
context. Start conservatively, compile for the actual target, inspect the map
file, and reduce `IDTTS_CONTEXT_BYTES` only after the build proves it fits.

Each event is four bytes. Reducing `IDTTS_MAX_EVENTS` from 512 to 128 therefore
saves 1536 bytes before ABI padding. Reducing `IDTTS_AUDIO_BLOCK` from 128 to 64
saves another 128 bytes.

## Audio callback

The renderer calls the callback synchronously from the task that invoked
`idtts_speak_ipa` or `idtts_speak_text`:

```c
static int audio_write(void *user, const int16_t *samples, size_t count)
{
    audio_ring_t *ring = user;

    /* Block until DMA has room, or copy immediately into a ring buffer. */
    return audio_ring_write(ring, samples, count) ? 0 : 1;
}
```

The supplied buffer belongs to `idtts` and is reused after the callback returns.
Copy or consume it before returning. Do not retain the pointer. A nonzero return
aborts rendering with `IDTTS_ERR_CALLBACK_ABORTED`.

Call synthesis from a normal task, not an interrupt. The renderer performs many
floating-point operations and may block in the callback. DMA completion
interrupts should only advance or refill a separate audio queue.

## Audio settings for clarity

Use 16 kHz when flash/CPU and the output path permit it. The Indonesian
fricatives and affricates rely on energy above the 4 kHz Nyquist limit of an
8 kHz build. Keep the analog amplifier and speaker below clipping; the 0.2.0
voice deliberately leaves digital headroom. A small loudspeaker enclosure can
remove low vowel energy or exaggerate the 3--6 kHz frication band, so perform
the final listening test on the actual transducer rather than desktop
headphones alone.

## Common output paths

### I2S

Configure mono signed 16-bit PCM at the same sample rate passed to
`idtts_init`. Queue each callback block to the I2S DMA driver. For a mono-to-
stereo peripheral, duplicate each sample or configure the peripheral's mono
slot mode.

### DAC

Convert signed PCM to the DAC's unsigned range, optionally with a simple
low-pass reconstruction filter:

```c
uint16_t dac12 = (uint16_t)(((int32_t)sample + 32768) >> 4);
```

### PWM

Bias signed PCM to unsigned duty cycle, update from a timer/DMA stream, and use
an analog low-pass filter. Keep the PWM carrier well above the audio band.

## CPU considerations

- A Cortex-M4F/M7, ESP32-class core, or MCU with an FPU is the easiest target.
- `IDTTS_USE_LIBM=OFF` avoids thousands of trigonometric library calls per
  second and is recommended for small MCUs.
- The renderer still uses float multiplies and divisions. Cortex-M0/M0+ targets
  need an on-target benchmark; they may require a lower sample rate, higher
  clock, or a future fixed-point backend.
- Build with optimization and section garbage collection, for example
  `-Os -ffunction-sections -fdata-sections` and linker `--gc-sections`.

## CMake cross-build example

```sh
cmake -S . -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DIDTTS_BUILD_CLI=OFF \
  -DIDTTS_BUILD_TESTS=OFF \
  -DIDTTS_BUILD_EXAMPLES=OFF \
  -DIDTTS_ENABLE_TEXT_FRONTEND=OFF \
  -DIDTTS_USE_LIBM=OFF \
  -DIDTTS_MAX_EVENTS=128 \
  -DIDTTS_CONTEXT_BYTES=1024 \
  -DIDTTS_AUDIO_BLOCK=64
cmake --build build-arm
```

The resulting static library is `libidtts.a`.

## Supplying IPA without eSpeak on the MCU

Several deployment patterns are possible:

1. Generate IPA on a server/phone/host and send UTF-8 IPA to the MCU.
2. Precompute IPA for prompts during the firmware build and store the strings in
   flash.
3. Enable the independent built-in Indonesian text front end on the MCU.
4. Store normalized `idtts_event_t` arrays and call `idtts_render_events`, which
   removes UTF-8 parsing and pronunciation ambiguity from runtime behavior.

The third option is the most self-contained; the fourth is the smallest and
most deterministic for a known prompt set.

## Target validation checklist

- Verify the configured sample rate and actual peripheral clock.
- Confirm callback blocks do not underrun.
- Run strict IPA tests on target, including UTF-8 symbols.
- Measure stack, static RAM, flash, and real-time factor from the linker map and
  cycle counter.
- Listen to every required phone and all critical product phrases.
- Test maximum configured event count and callback abortion.
- Test with the exact optimization and floating-point flags used in release.
