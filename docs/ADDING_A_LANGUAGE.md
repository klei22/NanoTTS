# Adding a language module

NanoTTS intentionally separates language-specific text processing from audio
synthesis. A new language module converts UTF-8 text into the existing
four-byte `nanotts_event_t` representation. The shared renderer then treats the
result exactly like IPA input.

This design keeps language additions small and prevents modularity from
entering the real-time sample loop.

## Module contract

A module implements one function:

```c
nanotts_result_t nanotts_lang_xx_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info);
```

Its responsibilities are:

1. clear `impl->event_count` before parsing;
2. validate UTF-8 and report the first bad byte;
3. normalize text, numbers, abbreviations, and punctuation as needed;
4. convert graphemes or lexical entries to common `NANOTTS_PH_*` IDs;
5. set stress, word-end, phrase-end, and question flags;
6. remain within `NANOTTS_MAX_EVENTS` and fixed temporary arrays;
7. return `NANOTTS_ERR_EMPTY_INPUT` when no speakable phone was emitted;
8. never allocate, render PCM, call an audio driver, or retain input pointers.

Start from:

```text
src/lang/nanotts_lang_template.c.example
```

Shared utilities in `nanotts_lang_common.h` provide:

- bounded UTF-8 decoding;
- event insertion and pause collapsing;
- bounded word-local event buffers;
- penultimate-vowel stress marking;
- word-end and trailing-pause handling;
- parse diagnostic initialization.

A language may use those helpers or implement stricter language-specific
behavior, but it must preserve the public error and event contracts.

## Step 1: define the orthographic scope

Before writing code, document:

- the standard or dialect being targeted;
- accepted script and Unicode normalization assumptions;
- phone inventory and mappings into NanoTTS phones;
- stress and syllabification policy;
- number, acronym, punctuation, and foreign-word policy;
- known ambiguous spellings and the override mechanism.

Prefer a finite, testable scope over silent guesses. Keep strict IPA available
as an escape hatch.

## Step 2: reuse the phone inventory

Use existing common phones whenever acoustically reasonable. Adding a phone
requires updates to:

- `nanotts_phone_t` in the public header;
- the voice table and name table in `nanotts_voice.c`;
- source/filter behavior or coarticulation groups when necessary;
- IPA aliases if the phone can be entered explicitly;
- inventory and synthesis tests.

Do not create a duplicate phone only to represent a different spelling. Text
modules are grapheme-to-phone converters; the renderer operates on sounds.

## Step 3: implement the bounded parser

A typical module collects one word into a fixed `uint32_t` array, converts the
word into a fixed temporary event array, applies stress, and copies the events
into the context. Parse punctuation outside the word conversion so all modules
produce consistent phrase flags.

Important details:

- `length == NANOTTS_NPOS` is resolved by the public API before the module call.
- Input may contain embedded NUL when an explicit byte length is supplied; do
  not rely on C-string scanning inside the main parse loop.
- Keep recursion bounded when expanding numbers.
- Treat event overflow as an error; never truncate a word silently.
- Reject malformed UTF-8 even when an unsupported visible character would
  otherwise be skipped.
- Add lexical exceptions only with documented, MIT-compatible provenance.

## Step 4: register the language

For a new code `xx`, update these intentional integration points:

1. Add a stable `NANOTTS_LANG_*` value to `nanotts_language_t`.
2. Add `NANOTTS_ENABLE_LANG_XX` defaults and a parser prototype in
   `nanotts_internal.h`.
3. Add `src/lang/nanotts_lang_xx.c` conditionally in `CMakeLists.txt` and the
   Makefile.
4. Add code/name aliases, availability, compiled enumeration, and the one-time
   dispatch case in `nanotts_language.c`.
5. Add tests in `tests/test_languages.c` or a dedicated language test.
6. Document the language in `docs/LANGUAGES.md` and provenance record.

The CLI uses the public language-enumeration functions, so it does not need a
new hard-coded listing loop.

## Performance model

The module boundary is outside the renderer:

```text
nanotts_parse_text
    └─ one language selection
        └─ language parser emits events
nanotts_render_events
    └─ no language lookup
        └─ phone loop
            └─ frame loop
                └─ sample loop
```

With two or more modules, selection is a single integer switch per text parse.
With exactly one module compiled, conditional compilation generates a direct
call. An IPA-only build omits all text helpers and modules. There are no parser
function pointers in `nanotts_t`, no virtual table per phone, and no language
condition in the DSP path.

The only always-present runtime state for language selection is one byte in the
context, already padded in the implementation structure.

## Size discipline

Measure each module separately:

```sh
cmake -S . -B build-xx \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=OFF \
  -DNANOTTS_ENABLE_LANG_XX=ON
cmake --build build-xx
```

Use `size`, the target linker map, and `--gc-sections`; host archive size alone
is not an MCU flash measurement. Keep large exception dictionaries optional or
compressed. Shared Unicode, number, or punctuation code belongs in common
helpers only when at least two modules genuinely benefit and the shared path
does not force unwanted features into a one-language build.

## Test checklist

At minimum, add tests for:

- every grapheme and multigraph;
- every added phone and IPA alias;
- word-initial, medial, and final contexts;
- stress on one-, two-, and multi-syllable words;
- adjacent vowels and consonant clusters;
- uppercase words versus acronyms;
- zero, units, tens, hundreds, thousands, and maximum supported numbers;
- punctuation and question contours;
- malformed UTF-8 and overlong words;
- language-unavailable behavior in other build profiles;
- deterministic non-silent rendering of a representative corpus;
- strict acceptance of external IPA, when interoperability is claimed.

Then test intelligibility with native listeners. Spectral and level metrics can
catch renderer regressions, but they cannot establish that a language is
understood.

## Clean-room and licensing rule

NanoTTS is MIT licensed. Do not copy, translate, mechanically port, or derive
rules, dictionaries, source, or voice tables from an incompatible TTS engine.
Use linguistic descriptions, independently authored rules, and appropriately
licensed recordings or measurements. Record the origin of new data in
`PROVENANCE.md`.
