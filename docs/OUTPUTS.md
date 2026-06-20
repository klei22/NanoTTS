# Output adapters

NanoTTS core always renders blocks of signed, mono, 16-bit PCM through
`nanotts_write_fn`. Output adapters are deliberately separate libraries so an
MCU build links only the representation it needs:

```text
text or IPA -> phone events -> renderer -> int16 PCM callback
                                      |-> application I2S/DAC queue
                                      |-> NanoTTS::PCM -> little-endian bytes
                                      `-> NanoTTS::PWM -> timer compare values
```

The renderer contains no WAV, filesystem, device, PWM, or DMA code. WAV is a
host-only wrapper in the CLI. `NanoTTS::PCM` and `NanoTTS::PWM` are optional,
zero-allocation adapters.

## Build selection

```sh
# Core plus both adapters (default)
cmake -S . -B build

# PWM only
cmake -S . -B build-pwm \
  -DNANOTTS_BUILD_PWM=ON \
  -DNANOTTS_BUILD_PCM=OFF

# Core only: direct int16 callback and host WAV CLI remain available
cmake -S . -B build-core \
  -DNANOTTS_BUILD_PWM=OFF \
  -DNANOTTS_BUILD_PCM=OFF
```

The setup helper exposes the same selection:

```sh
./setup.sh --languages mi --outputs pwm
./setup.sh --languages ms,haw --outputs pwm,pcm
./setup.sh --languages ipa --outputs none
```

Installed CMake targets are:

```text
NanoTTS::NanoTTS   core renderer and language modules
NanoTTS::PWM       optional PCM-to-PWM adapter
NanoTTS::PCM       optional signed-16 little-endian serializer
```

## Direct PCM callback

For I2S, a DAC, or a native signed-PCM DMA peripheral, no adapter is needed:

```c
static int audio_write(void *user, const int16_t *samples, size_t count)
{
    audio_queue_t *queue = user;
    return audio_queue_submit(queue, samples, count) ? 0 : 1;
}

nanotts_speak_text(&tts, text, NANOTTS_NPOS, &options,
                   audio_write, &queue, &parse_info);
```

The sample buffer belongs to NanoTTS and is reused immediately after the
callback returns. Copy it or finish consuming it before returning.

## Raw PCM byte adapter

`NanoTTS::PCM` serializes the native callback into signed 16-bit
little-endian bytes without assuming host endianness:

```c
#include <nanotts/nanotts_pcm.h>

static int byte_sink(void *user, const uint8_t *bytes, size_t count)
{
    return storage_or_transport_write(user, bytes, count) ? 0 : 1;
}

uint8_t scratch[128];
nanotts_pcm16le_output_t pcm;

nanotts_pcm16le_output_init(&pcm, scratch, sizeof(scratch),
                            byte_sink, transport);
nanotts_speak_ipa(&tts, "h_a_l_o", NANOTTS_NPOS, &options,
                  nanotts_pcm16le_write_pcm, &pcm, &parse_info);
```

One audio sample becomes two output bytes. The scratch buffer may be static,
stack-allocated, or part of an application context.

CLI examples:

```sh
nanotts --lang ms --text "sistem sedia" --format pcm -o speech.pcm
nanotts --ipa "h_a_l_o" --format pcm -o - > speech.pcm
```

The byte stream has no header. Record the sample rate separately.

## PWM duty adapter

`NanoTTS::PWM` does **not** generate a carrier bitstream. It converts each
signed PCM sample into one unsigned timer compare value in `0..top`:

```c
#include <nanotts/nanotts_pwm.h>

static int pwm_dma_submit(
    void *user,
    const uint16_t *duty,
    size_t count)
{
    return timer_dma_queue(user, duty, count) ? 0 : 1;
}

uint16_t scratch[64];
nanotts_pwm_output_t pwm;

nanotts_pwm_output_init(
    &pwm,
    1023u,                 /* timer TOP/ARR: 10-bit duty range */
    0,                     /* non-inverted */
    scratch,
    sizeof(scratch) / sizeof(scratch[0]),
    pwm_dma_submit,
    &timer);

nanotts_speak_text(&tts, "bateri hampir habis", NANOTTS_NPOS,
                   &options, nanotts_pwm_write_pcm, &pwm, &parse_info);
```

The conversion is endpoint-safe and division-free per sample:

```text
PCM -32768 -> duty 0
PCM      0 -> duty approximately (top + 1) / 2
PCM  32767 -> duty top
```

Set `invert` when the output stage is active-low.

### Hardware arrangement

Use two time scales:

1. A PWM timer free-runs at a carrier frequency well above the audio band.
2. A sample-rate timer or DMA request updates the compare register at the
   exact rate passed to `nanotts_init`, normally 16 kHz.

For example, a 48 MHz timer with `top=255` has a 187.5 kHz carrier. The compare
value can be updated at 16 kHz while the carrier continues to free-run. A
higher `top` improves duty resolution but reduces carrier frequency. Choose the
trade-off from the target clock, timer width, output filter, and amplifier.

Do not configure a single 16 kHz PWM period and treat that as the carrier. A
carrier at the audio sample rate is difficult to filter and leaves little
usable audio bandwidth.

The analog path normally requires:

- an RC or active low-pass reconstruction filter;
- a properly biased amplifier or AC coupling stage;
- a transducer appropriate for the available voltage/current;
- clipping and EMI checks on the final board.

DMA is preferred. The NanoTTS callback should enqueue or copy duty blocks; it
should not wait for every PWM cycle. A nonzero downstream return aborts
synthesis as `NANOTTS_ERR_CALLBACK_ABORTED`.

CLI diagnostic output:

```sh
nanotts --lang haw --text "aloha" \
  --format pwm --pwm-top 1023 -o aloha.pwm
```

The file is headerless little-endian `uint16_t`, one compare value per audio
sample. It is useful for test vectors and firmware conversion, not direct audio
playback.

## RAM and performance

Both adapters use caller-owned scratch memory and no heap. Scratch size changes
callback granularity, not output quality. Typical starting points are:

| Adapter | Scratch | Result |
|---|---:|---|
| raw PCM | 128 bytes | 64 samples per downstream write |
| PWM | 64 `uint16_t` values | 64 duty updates per downstream write |

The PWM mapping performs a 32-bit multiply and shift per sample. There is no
integer divide and no language/output branch in the synthesizer's inner loop.
The adapter runs only after each PCM block is produced.

## Validation checklist

- Confirm timer carrier frequency and compare range.
- Confirm compare DMA updates exactly at the NanoTTS sample rate.
- Verify every emitted duty value is at most `top`.
- Scope the filtered midpoint with silence; it should sit near half scale.
- Check full-volume speech for compare saturation and amplifier clipping.
- Measure queue underruns during the longest required phrase.
- Listen through the production filter, amplifier, enclosure, and speaker.
- Keep a raw PCM reference so PWM-path distortion can be isolated from TTS
  pronunciation or synthesis problems.
