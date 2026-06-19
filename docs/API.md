# C API guide

Include the installed public header:

```c
#include <nanotts/nanotts.h>
```

## Context lifecycle and language selection

`nanotts_t` is an opaque, caller-owned object. It may be static, global, or on a
task stack large enough for the selected build configuration.

Text language is selected at initialization:

```c
nanotts_t tts;
nanotts_result_t result = nanotts_init(
    &tts, 16000u, NANOTTS_LANG_SPANISH);
```

Supported sample rates are 8000 through 24000 Hz. A requested text language
must be compiled into the library or initialization returns
`NANOTTS_ERR_LANGUAGE_UNAVAILABLE`. For IPA-only use, initialize with
`NANOTTS_LANG_NONE`.

Change language only while the context is idle:

```c
nanotts_set_language(&tts, NANOTTS_LANG_INDONESIAN);
```

Changing the language clears any parsed events. `nanotts_reset` clears events
and renderer state while preserving sample rate and language. No destroy
function is needed because the library allocates nothing.

## Language discovery

```c
int has_sw = nanotts_language_available(NANOTTS_LANG_KISWAHILI);
int has_es = nanotts_language_available(NANOTTS_LANG_SPANISH);
size_t count = nanotts_compiled_language_count();

for (size_t i = 0; i < count; ++i) {
    nanotts_language_t language = nanotts_compiled_language_at(i);
    const char *code = nanotts_language_code(language);
    const char *name = nanotts_language_name(language);
}
```

`nanotts_language_from_code` recognizes the principal aliases for all built-in
modules case-insensitively: `id`/`Indonesian`/`Bahasa-Indonesia`,
`sw`/`Swahili`/`Kiswahili`, and
`es`/`es-419`/`es-MX`/`Spanish`/`espanol`/`español`/`castellano`. Unknown input returns
`NANOTTS_LANG_COUNT`.

`NANOTTS_LANG_SPANISH_LATIN_AMERICAN` is a source-level alias for
`NANOTTS_LANG_SPANISH`; both serialize as the code `es`.

`nanotts_compiled_language_at` returns `NANOTTS_LANG_NONE` for an out-of-range
index. The order is stable for one build but applications should use codes,
not array positions, for persistence or protocols.

Other build introspection:

```c
size_t bytes = nanotts_context_bytes();
size_t phones = nanotts_event_capacity();
int has_any_text = nanotts_text_frontend_available();
```

## Parse then render

Separating parsing from rendering allows inspection or modification of phone
events:

```c
nanotts_parse_info_t info;
nanotts_result_t r = nanotts_parse_text(
    &tts, "hola, buenos días", NANOTTS_NPOS, &info);
if (r == NANOTTS_OK) {
    const nanotts_event_t *events = nanotts_events(&tts);
    size_t count = nanotts_event_count(&tts);
    /* Inspect events[0..count), then render. */
    r = nanotts_render_events(&tts, &options, audio_write, user);
}
```

`NANOTTS_NPOS` requests NUL-terminated input; otherwise supply the exact byte
length. The parsers do not retain the input pointer.

`nanotts_parse_text` uses the context's selected language. A context initialized
with `NANOTTS_LANG_NONE` returns `NANOTTS_ERR_LANGUAGE_UNAVAILABLE`. A build
with `NANOTTS_ENABLE_TEXT_FRONTEND=OFF` returns
`NANOTTS_ERR_FEATURE_DISABLED`.

IPA parsing is independent of the selected text language:

```c
r = nanotts_parse_ipa(
    &tts, "ˈo_l_a", NANOTTS_NPOS, 0u, &info);
```

## Precomputed events

`nanotts_set_events` copies a caller-provided event array into the context. This
is useful when firmware stores prevalidated prompts instead of UTF-8 text or
IPA:

```c
static const nanotts_event_t ready[] = {
    { NANOTTS_PH_S, 0, 100, 0 },
    { NANOTTS_PH_I, NANOTTS_EVENT_STRESS_PRIMARY, 100, 0 },
    { NANOTTS_PH_A, 0, 100, 0 },
    { NANOTTS_PH_P, NANOTTS_EVENT_WORD_END, 100, 0 }
};

nanotts_set_events(&tts, ready, sizeof ready / sizeof ready[0]);
nanotts_render_events(&tts, &options, audio_write, audio_user);
```

Passing `NULL, 0` clears the sequence. Invalid phone IDs and arrays larger than
`NANOTTS_MAX_EVENTS` are rejected.

### Event annotations

Language modules and IPA input use the same flags:

| Flag | Meaning |
|---|---|
| `NANOTTS_EVENT_STRESS_PRIMARY` | primary prominence on this nucleus |
| `NANOTTS_EVENT_STRESS_SECONDARY` | secondary prominence |
| `NANOTTS_EVENT_LONG` | explicit length mark; duration also carries the amount |
| `NANOTTS_EVENT_WORD_END` | final phone of a word |
| `NANOTTS_EVENT_PHRASE_END` | phrase boundary event |
| `NANOTTS_EVENT_QUESTION` | request an automatic question contour |
| `NANOTTS_EVENT_SYLLABIC` | consonant functions as a syllable nucleus |

`duration_percent` is 100 for the phone's default duration; zero is normalized
to 100 when using the internal parser helpers. `pitch_semitones_q4` is a signed
per-event offset in sixteenth-semitone units.

## One-call helpers

`nanotts_speak_text` and `nanotts_speak_ipa` combine parsing and rendering. They
are synchronous: the call returns only after all PCM has been delivered or the
callback aborts.

```c
nanotts_result_t r = nanotts_speak_text(
    &tts, text, NANOTTS_NPOS, &options,
    audio_write, user, &info);
```

## Options

Initialize options before changing fields:

```c
nanotts_options_t options;
nanotts_default_options(&options);
options.rate_q8 = 320u;                 /* 125% speed */
options.pitch_cents = -100;             /* one semitone lower */
options.volume = 220u;                  /* 0..255 */
options.final_tone = NANOTTS_FINAL_AUTO;
```

`rate_q8` is an unsigned Q8 ratio, where 256 is normal speed. Zero also means
normal speed. Accepted nonzero values are 96 through 640. `pitch_cents` is a
global offset from -1200 through +1200 cents. `final_tone` selects fall, rise,
continuation, level, or automatic behavior. Automatic mode uses question flags
from punctuation; explicit selection applies to every phrase in the render
call.

For IPA parsing, `options.flags` may contain:

- `NANOTTS_OPT_PERMISSIVE_IPA`: skip unsupported IPA symbols;
- `NANOTTS_OPT_NO_AUTOPAUSE`: do not turn IPA whitespace into pauses.

Text modules currently ignore these two IPA-specific flags.

## Callback contract

```c
typedef int (*nanotts_write_fn)(
    void *user,
    const int16_t *samples,
    size_t count);
```

Samples are mono signed PCM in native `int16_t` representation at the rate
passed to `nanotts_init`. The pointer is valid only during the callback. Return
zero to continue or nonzero to abort; abortion is reported as
`NANOTTS_ERR_CALLBACK_ABORTED`.

The callback may block or copy into a DMA ring, but it must not reenter NanoTTS
using the same context.

## Parse diagnostics and errors

`nanotts_strerror` returns a stable human-readable description. Parser errors
also fill `nanotts_parse_info_t`:

- `event_count`: events accepted before the error;
- `error_byte`: UTF-8 byte offset, or `NANOTTS_NPOS` when unavailable;
- `error_codepoint`: rejected scalar value when applicable.

The context remains reusable after an error. Parse another input, select a
language, or reset it.

Common distinctions:

| Result | Meaning |
|---|---|
| `NANOTTS_ERR_LANGUAGE_UNAVAILABLE` | invalid selection, `NONE` for text, or module not compiled |
| `NANOTTS_ERR_FEATURE_DISABLED` | complete text front end disabled at build time |
| `NANOTTS_ERR_UNKNOWN_IPA` | valid UTF-8 but outside `nanotts-ipa/1` |
| `NANOTTS_ERR_TOO_MANY_EVENTS` | context capacity or bounded word expansion exceeded |
| `NANOTTS_ERR_CALLBACK_ABORTED` | output callback returned nonzero |

## Concurrency

Different contexts are independent and may render concurrently. A single
context is not thread-safe or reentrant. Language metadata and voice tables are
immutable; the renderer has no global mutable state.
