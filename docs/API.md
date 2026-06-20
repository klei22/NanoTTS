# C API guide

NanoTTS separates three interfaces:

```text
nanotts.h       core text/IPA/event renderer → native int16 PCM callback
nanotts_pwm.h   optional PCM → timer compare adapter
nanotts_pcm.h   optional PCM → signed little-endian byte adapter
```

The optional adapters are separate libraries. Applications that drive I²S,
DAC, or a codec directly need only `NanoTTS::NanoTTS`.

## Context lifecycle

```c
#include <nanotts/nanotts.h>

nanotts_t tts;
nanotts_result_t result = nanotts_init(
    &tts, 16000u, NANOTTS_LANG_MALAY);
```

`nanotts_t` is opaque and caller-owned. It may live in static storage or on a
task stack large enough for `NANOTTS_CONTEXT_BYTES`. NanoTTS allocates nothing.
Supported sample rates are 8000 through 24000 Hz.

Use `NANOTTS_LANG_NONE` for IPA/event-only operation. A text language must be
compiled into the library or initialization returns
`NANOTTS_ERR_LANGUAGE_UNAVAILABLE`.

Change language only while the context is idle:

```c
nanotts_set_language(&tts, NANOTTS_LANG_HAWAIIAN);
```

Changing language clears parsed events. `nanotts_reset` clears event and
renderer state while preserving the sample rate and language. No destroy
function is required.

## Language discovery

```c
size_t count = nanotts_compiled_language_count();
for (size_t i = 0; i < count; ++i) {
    nanotts_language_t language = nanotts_compiled_language_at(i);
    printf("%s: %s\n",
           nanotts_language_code(language),
           nanotts_language_name(language));
}
```

Canonical codes are `id`, `sw`, `es`, `ms`, `mi`, and `haw`.
`nanotts_language_from_code` also recognizes documented aliases. It returns
`NANOTTS_LANG_COUNT` for unknown text. It can identify a known language even
when that module was not compiled; call `nanotts_language_available` before
selecting it.

Source aliases include:

```c
NANOTTS_LANG_SWAHILI
NANOTTS_LANG_SPANISH_LATIN_AMERICAN
NANOTTS_LANG_BAHASA_MELAYU
NANOTTS_LANG_TE_REO_MAORI
NANOTTS_LANG_OLELO_HAWAII
```

`nanotts_compiled_language_at` returns `NANOTTS_LANG_NONE` when out of range.
Store canonical codes, not discovery-array positions, in protocols or settings.

Other introspection:

```c
size_t bytes = nanotts_context_bytes();
size_t capacity = nanotts_event_capacity();
int has_text = nanotts_text_frontend_available();
```

## Parse and render

Text:

```c
nanotts_parse_info_t parse;
nanotts_result_t result = nanotts_parse_text(
    &tts, "kia ora", NANOTTS_NPOS, &parse);
```

IPA:

```c
result = nanotts_parse_ipa(
    &tts, "k_ˈi_a_ʔ_o_ɾ_a", NANOTTS_NPOS, 0u, &parse);
```

Render the buffered events:

```c
nanotts_options_t options;
nanotts_default_options(&options);
result = nanotts_render_events(
    &tts, &options, pcm_callback, user);
```

Convenience functions combine both stages:

```c
nanotts_speak_text(...);
nanotts_speak_ipa(...);
```

Pass `NANOTTS_NPOS` to use the input string's terminating NUL, or pass an
explicit byte length for non-NUL-terminated buffers.

## Direct event input

Each event is four bytes:

```c
typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t pitch_semitones_q4;
} nanotts_event_t;
```

Set precomputed events with:

```c
nanotts_set_events(&tts, events, count);
nanotts_render_events(&tts, &options, pcm_callback, user);
```

This is the smallest deployment path when text/IPA conversion occurs on a host.
Do not serialize the in-memory enum/event representation as a permanent wire
format without a versioned schema; public phone IDs are append-only but a
future blob format may add metadata and validation.

## Options

Initialize before changing fields:

```c
nanotts_options_t options;
nanotts_default_options(&options);
options.rate_q8 = 320u;                 /* 125% speed. */
options.pitch_cents = -100;             /* One semitone lower. */
options.volume = 220u;                  /* 0..255. */
options.final_tone = NANOTTS_FINAL_AUTO;
```

- `rate_q8`: unsigned Q8 speed ratio; 256 is normal. Zero also means normal.
  Accepted nonzero range is 96 through 640.
- `pitch_cents`: global shift from -1200 through +1200 cents.
- `volume`: 0 through 255.
- `final_tone`: fall, rise, continuation, level, or automatic.
- `flags`: IPA parser flags.

IPA flags:

- `NANOTTS_OPT_PERMISSIVE_IPA`: skip unsupported IPA symbols;
- `NANOTTS_OPT_NO_AUTOPAUSE`: do not convert IPA whitespace to pauses.

Text modules currently ignore the IPA-specific flags.

## Per-language prosody

```c
const nanotts_prosody_profile_t *profile =
    nanotts_language_prosody(NANOTTS_LANG_MAORI);
```

A profile contains immutable base pitch, duration scales, pause scales, stress
scales, and phrase-contour points. The renderer resolves it once per render.
When a language module is not compiled, its profile is also absent and this
function returns the neutral `NANOTTS_LANG_NONE` profile.

The profile pointer is read-only. Public inspection is intended for diagnostics
and integration; 0.5 does not expose runtime profile replacement.

## Core PCM callback

```c
typedef int (*nanotts_write_fn)(
    void *user,
    const int16_t *samples,
    size_t count);
```

Samples are mono signed native `int16_t` PCM at the rate passed to
`nanotts_init`. The pointer remains valid only during the callback. Return zero
to continue or nonzero to abort. NanoTTS reports an aborted sink as
`NANOTTS_ERR_CALLBACK_ABORTED`.

The callback may block or copy into a DMA/ring buffer, but it must not reenter
the same `nanotts_t` context.

## PWM adapter

Include and link the optional adapter:

```c
#include <nanotts/nanotts_pwm.h>
/* CMake target: NanoTTS::PWM */
```

The adapter maps signed PCM into timer compare values:

```c
uint16_t nanotts_pcm16_to_pwm(
    int16_t sample,
    uint16_t top,
    int invert);
```

Mapping invariants:

```text
-32768 → 0
     0 → approximately TOP / 2
 32767 → TOP
```

With inversion, each value becomes `TOP - value`.

Zero-allocation block adapter:

```c
typedef int (*nanotts_pwm_write_fn)(
    void *user,
    const uint16_t *duty_values,
    size_t count);

nanotts_result_t nanotts_pwm_output_init(
    nanotts_pwm_output_t *output,
    uint16_t top,
    int invert,
    uint16_t *scratch,
    size_t scratch_count,
    nanotts_pwm_write_fn write,
    void *user);
```

Pass `nanotts_pwm_write_pcm` directly to any render/speak function:

```c
nanotts_speak_text(
    &tts, text, NANOTTS_NPOS, &options,
    nanotts_pwm_write_pcm, &pwm, &parse);
```

The adapter produces one compare value per audio sample. It does not create a
carrier, configure a timer, start DMA, filter the signal, or amplify it. `top`
must match the target timer period/TOP/ARR. The scratch array is caller-owned
and may be static.

## PCM16LE adapter

Include and link:

```c
#include <nanotts/nanotts_pcm.h>
/* CMake target: NanoTTS::PCM */
```

The adapter converts native `int16_t` blocks to a defined signed 16-bit
little-endian byte stream:

```c
typedef int (*nanotts_pcm_bytes_write_fn)(
    void *user,
    const uint8_t *bytes,
    size_t count);

nanotts_result_t nanotts_pcm16le_output_init(
    nanotts_pcm16le_output_t *output,
    uint8_t *scratch,
    size_t scratch_bytes,
    nanotts_pcm_bytes_write_fn write,
    void *user);
```

Use `nanotts_pcm16le_write_pcm` as the core callback. The output has no WAV
header or metadata; the consumer must know the sample rate, mono channel count,
and signed PCM16LE encoding.

## Parse diagnostics and errors

`nanotts_parse_info_t` reports:

- `event_count`: events accepted before completion/error;
- `error_byte`: UTF-8 byte offset, or `NANOTTS_NPOS`;
- `error_codepoint`: rejected scalar value when available.

Common results:

| Result | Meaning |
|---|---|
| `NANOTTS_ERR_LANGUAGE_UNAVAILABLE` | language invalid or module omitted |
| `NANOTTS_ERR_FEATURE_DISABLED` | complete text frontend disabled |
| `NANOTTS_ERR_UTF8` | malformed UTF-8 input |
| `NANOTTS_ERR_UNKNOWN_IPA` | valid UTF-8 outside the strict IPA profile |
| `NANOTTS_ERR_TOO_MANY_EVENTS` | bounded event/word expansion exceeded |
| `NANOTTS_ERR_CALLBACK_ABORTED` | output callback returned nonzero |

`nanotts_strerror` returns a stable human-readable description. A context remains
reusable after an error.

## Concurrency and lifetime

Different contexts are independent and may render concurrently. One context is
not thread-safe or reentrant. Language metadata, prosody profiles, and acoustic
tables are immutable. Scratch arrays supplied to output adapters must remain
valid until the synchronous render call returns.
