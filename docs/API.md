# C API guide

Include the public header:

```c
#include <idtts/idtts.h>
```

## Context lifecycle

`idtts_t` is an opaque, caller-owned object. It may be static, global, or on a
task stack large enough for the selected build configuration.

```c
idtts_t tts;
idtts_result_t result = idtts_init(&tts, 16000u);
```

Supported sample rates are 8000 through 24000 Hz. `idtts_reset` clears events
and renderer state while preserving the sample rate. No destroy function is
needed because the library allocates nothing.

Introspection helpers return the build's public context size, event capacity,
and whether the optional text front end is present:

```c
size_t bytes = idtts_context_bytes();
size_t phones = idtts_event_capacity();
int has_text = idtts_text_frontend_available();
```

## Parse then render

Separating parsing from rendering allows inspection or modification of the
phone events:

```c
idtts_parse_info_t info;
idtts_result_t r = idtts_parse_ipa(
    &tts, ipa, IDTTS_NPOS, 0u, &info);
if (r == IDTTS_OK) {
    const idtts_event_t *events = idtts_events(&tts);
    size_t count = idtts_event_count(&tts);
    /* inspect events[0..count) */
}
```

`IDTTS_NPOS` requests NUL-terminated input; otherwise supply the exact byte
length. The parsers do not retain an input pointer.

`idtts_parse_text` invokes the independent Indonesian text front end. A build
with `IDTTS_ENABLE_TEXT_FRONTEND=OFF` returns
`IDTTS_ERR_FEATURE_DISABLED`.

## Precomputed events

`idtts_set_events` copies a caller-provided array into the context. This is
useful when firmware stores compact prevalidated prompts instead of UTF-8 text
or IPA:

```c
static const idtts_event_t ready[] = {
    { IDTTS_PH_S, 0, 100, 0 },
    { IDTTS_PH_I, IDTTS_EVENT_STRESS_PRIMARY, 100, 0 },
    { IDTTS_PH_A, 0, 100, 0 },
    { IDTTS_PH_P, IDTTS_EVENT_WORD_END, 100, 0 }
};

idtts_set_events(&tts, ready, sizeof ready / sizeof ready[0]);
idtts_render_events(&tts, &options, audio_write, audio_user);
```

Passing `NULL, 0` clears the current event sequence. Invalid phone IDs are
rejected.

## One-call helpers

`idtts_speak_ipa` and `idtts_speak_text` combine parsing and rendering. They are
synchronous: the function returns only after all PCM has been delivered or the
callback aborts.

## Options

Initialize options before changing fields:

```c
idtts_options_t options;
idtts_default_options(&options);
options.rate_q8 = 320;                    /* 125% speed */
options.pitch_cents = -100;               /* one semitone lower */
options.volume = 220;                     /* 0..255 */
options.final_tone = IDTTS_FINAL_AUTO;        /* default: punctuation-aware */
```

`rate_q8` is an unsigned Q8 ratio, where 256 is normal speed. Zero also means
normal speed. Accepted nonzero values are 96 through 640. `pitch_cents` is a
global pitch offset from -1200 through +1200 cents. `final_tone` selects fall,
rise, continuation, level, or `IDTTS_FINAL_AUTO`. Automatic mode is the
default: a phrase ending in a parsed question mark rises, while other complete
phrases fall. An explicit tone applies to every phrase in the render call.

For IPA parsing, `options.flags` may contain:

- `IDTTS_OPT_PERMISSIVE_IPA`: skip unsupported symbols;
- `IDTTS_OPT_NO_AUTOPAUSE`: do not convert whitespace to short pauses.

## Callback contract

```c
typedef int (*idtts_write_fn)(
    void *user,
    const int16_t *samples,
    size_t count);
```

The samples are mono signed PCM in native integer representation at the rate
passed to `idtts_init`. The pointer is valid only for the duration of the
callback. Return zero to continue or nonzero to abort. Callback abortion is
reported as `IDTTS_ERR_CALLBACK_ABORTED`.

## Errors

`idtts_strerror` returns a stable human-readable description. Parser errors
also fill `idtts_parse_info_t`:

- `event_count`: events accepted before the error;
- `error_byte`: UTF-8 byte offset, or `IDTTS_NPOS` when unavailable;
- `error_codepoint`: rejected Unicode scalar value when applicable.

The context remains reusable after an error; parse another input or call
`idtts_reset`.

## Concurrency

Different contexts are independent and may render concurrently. A single
context is not thread-safe or reentrant. Do not call another `idtts` function
on a context from its audio callback.
