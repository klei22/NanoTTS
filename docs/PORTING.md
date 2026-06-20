# MCU porting guide

NanoTTS has no board dependency. A port consists of selecting a build profile,
placing a caller-owned context in RAM, and implementing one synchronous output
callback. The optional PWM adapter is intended to make the smallest no-codec
path straightforward while keeping timer/DMA details in application code.

## Choose a deployment profile

### Full text on target

```text
UTF-8 text → selected language module → events → renderer → output
```

Use this when prompts are dynamic and the MCU has enough flash for one text
module.

### IPA on target

```text
host/server G2P → separated IPA → MCU IPA parser → renderer → output
```

Disable all text modules:

```cmake
-DNANOTTS_ENABLE_TEXT_FRONTEND=OFF
```

### Events on target

```text
host compiler → nanotts_event_t[] → renderer → output
```

This removes UTF-8/G2P work from the target. Keep event data versioned in the
application; 0.5 does not yet define a permanent serialized prompt format.

## Compact build example

Māori-only with PWM and no host tools:

```sh
cmake -S . -B build-mcu \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/your-mcu.cmake \
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
cmake --build build-mcu
```

The compile-time context assertion fails if 1024 bytes is too small for the
selected ABI/configuration. Increase it rather than suppressing the check.

For I²S/DAC output, set both adapter options off and link only
`NanoTTS::NanoTTS`.

## Core callback path

```c
static int audio_write(void *user, const int16_t *pcm, size_t count)
{
    audio_device_t *device = user;
    return audio_dma_submit_blocking(device, pcm, count) ? 0 : 1;
}
```

The callback runs synchronously inside synthesis. It must either consume/copy
the block before returning or block until space is available. The supplied
pointer is not valid after return.

`NANOTTS_AUDIO_BLOCK` controls the renderer's callback block size. Smaller
blocks reduce buffering latency and scratch RAM but increase callback overhead.
A value of 32–128 samples is a practical starting range.

## PWM architecture

NanoTTS PWM output is a stream of **compare values**, not a pre-expanded GPIO
bitstream:

```text
NanoTTS at 16 kHz       MCU PWM peripheral
----------------       ------------------
PCM sample          →  compare/TOP duty value
next PCM sample     →  next compare value
                        timer generates many carrier cycles per audio sample
```

The timer carrier and sample-update timing are independent concepts:

- carrier: configured by the MCU timer clock, divider, and TOP/ARR;
- audio update: one new compare value at `nanotts_sample_rate()`;
- transfer: DMA is preferred; a bounded real-time interrupt is also possible;
- reconstruction: hardware-appropriate low-pass/AC coupling/amplification.

Do not drive a loudspeaker directly from an MCU GPIO. Use circuitry appropriate
for the board, load, voltage, and required power.

### Adapter setup

```c
#include <nanotts/nanotts.h>
#include <nanotts/nanotts_pwm.h>

static nanotts_t tts;
static nanotts_pwm_output_t pwm_output;
static uint16_t pwm_scratch[64];

static int pwm_submit(void *user, const uint16_t *values, size_t count)
{
    pwm_driver_t *driver = user;
    /* Copy or queue before returning. */
    return pwm_dma_submit_blocking(driver, values, count) ? 0 : 1;
}

void speech_init(pwm_driver_t *driver)
{
    nanotts_init(&tts, 16000u, NANOTTS_LANG_MALAY);
    nanotts_pwm_output_init(
        &pwm_output,
        1023u,             /* Same value as timer TOP/ARR. */
        0,
        pwm_scratch,
        sizeof(pwm_scratch) / sizeof(pwm_scratch[0]),
        pwm_submit,
        driver);
}
```

Render:

```c
nanotts_options_t options;
nanotts_default_options(&options);

nanotts_speak_text(
    &tts,
    "bateri hampir habis",
    NANOTTS_NPOS,
    &options,
    nanotts_pwm_write_pcm,
    &pwm_output,
    NULL);
```

### DMA ownership

`nanotts_pwm_write_pcm` fills the caller's scratch array and then invokes
`nanotts_pwm_write_fn`. The same scratch array is reused for the next chunk.
Therefore the low-level callback must:

- block until DMA has copied/consumed the block; or
- copy it into a separate application-owned ring/double buffer.

Do not retain the scratch pointer asynchronously without copying it.

A common design is:

```text
NanoTTS scratch → copy to free DMA half → return
                                      DMA completion interrupt marks half free
```

### Timer value width

The adapter uses `uint16_t` compare values and supports TOP/ARR from 1 through
65535. This matches common 16-bit timer registers and avoids target-specific
packing behavior. An 8-bit timer can use TOP 255 and write/cast the low byte in
the target callback if its peripheral API requires 8-bit data.

The mapping is linear:

```text
PCM -32768   → 0
PCM      0   → midpoint
PCM +32767   → TOP
```

Use `invert=1` or CLI `--pwm-invert` when the hardware output polarity is
inverted.

### Carrier and filtering

Choose timer frequency, resolution, and analog filtering together for the
specific MCU and output circuit. Higher TOP gives more duty resolution but, at
a fixed timer clock, reduces carrier frequency. Validate the final tradeoff on
the actual board with the actual load. NanoTTS intentionally does not hardcode
a universal carrier frequency.

For Raspberry Pi Pico-family targets, the official SDK exposes double-buffered
PWM compare registers; this is compatible with updating compare values at audio
sample boundaries. Other MCUs often name the equivalent period and compare
registers `ARR` and `CCR`.

## I²S or external codec

The simplest high-quality path uses the core PCM callback directly:

```c
static int i2s_write(void *user, const int16_t *pcm, size_t count)
{
    return i2s_dma_write_mono16(user, pcm, count) ? 0 : 1;
}
```

When hardware requires stereo, duplicate each mono sample in the callback or
configure the peripheral for mono. Keep that packing outside NanoTTS so the
core remains target-neutral.

## DAC

For an unsigned N-bit DAC, convert in the application or reuse the linear
mapping concept from `nanotts_pcm16_to_pwm`. Add the board-specific bias,
settling, DMA packing, and reconstruction filter in the driver. Do not label a
DAC stream as PWM merely because both are unsigned.

## Raw PCM adapter

`NanoTTS::PCM` is useful when the transport consumes bytes instead of native
samples:

```c
#include <nanotts/nanotts_pcm.h>

static uint8_t byte_scratch[128];
static nanotts_pcm16le_output_t pcm_le;

nanotts_pcm16le_output_init(
    &pcm_le, byte_scratch, sizeof(byte_scratch),
    usb_audio_bytes_write, usb_device);

nanotts_speak_ipa(
    &tts, ipa, NANOTTS_NPOS, &options,
    nanotts_pcm16le_write_pcm, &pcm_le, &parse);
```

The stream is mono signed PCM16LE without a header. Communicate sample rate and
encoding separately.

## Scheduler and blocking behavior

NanoTTS is synchronous and does not create threads or interrupts. On a small
MCU, choose one of these patterns:

1. run synthesis in a foreground/task context and block on DMA queue space;
2. render into a ring buffer while a high-priority ISR drains it;
3. render short prompts completely into external RAM before playback;
4. precompute event arrays and render only when audio is requested.

Do not run the full renderer inside a tight audio ISR. Floating-point filter
updates and language parsing belong in normal task/foreground context.

## Flash and RAM reduction checklist

- Compile one language or use IPA/events only.
- Disable `NANOTTS_BUILD_PCM` when using PWM/I²S/DAC.
- Disable `NANOTTS_BUILD_PWM` when using PCM/I²S/DAC.
- Disable CLI, tests, and examples.
- Use `NANOTTS_USE_LIBM=OFF` where beneficial.
- Reduce `NANOTTS_MAX_EVENTS` to the longest expected prompt.
- Reduce `NANOTTS_AUDIO_BLOCK` after measuring callback overhead.
- Set the smallest `NANOTTS_CONTEXT_BYTES` that passes the compile-time check.
- Enable compiler/linker section garbage collection and inspect the map.
- Use LTO only after validating numerical output and toolchain stability.

Compiling more languages does not add per-context RAM. Each selected text
module and prosody profile adds flash only.

## Endianness and files

The core callback uses native `int16_t`; it is not a file format. The PCM
adapter explicitly emits little-endian bytes. The PWM CLI output also stores
little-endian `uint16_t` values for reproducible host inspection, while the MCU
adapter passes native `uint16_t` arrays directly to application code.

## Error handling

Return nonzero from an output callback when the target queue fails or playback
is cancelled. NanoTTS stops and returns `NANOTTS_ERR_CALLBACK_ABORTED`.

Always check initialization, parser, adapter, and render return codes. For
critical prompts, keep a fallback alarm tone or prerecorded message independent
of the open-ended TTS path.
