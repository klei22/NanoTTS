# Adding a text language

A NanoTTS language module is a compile-time, bounded UTF-8 text-to-event
converter. It does not render samples and it does not own an output backend.
Every module feeds the same phone-event ABI and shared renderer.

## Design target

```text
UTF-8 text
   │
   ▼
xx text module ── normalization / G2P / stress / punctuation
   │
   ▼
nanotts_event_t[]
   │
   ├── selected language prosody profile
   ▼
shared renderer ── PCM callback ── optional PCM/PWM adapter
```

A multilingual build selects a parser once per text utterance. A build with one
compiled text module resolves to a direct parser call at compile time. Do not
add language checks inside the renderer, frame loop, or output adapters.

## 1. Freeze the intended scope

Before writing code, document:

- language code and dialect/standard;
- accepted script and Unicode normalization policy;
- phoneme inventory;
- multi-letter correspondences;
- stress/prominence policy;
- number, acronym, punctuation, and loanword policy;
- ambiguities that require a table or explicit IPA;
- features deliberately left unsupported.

Prefer a small, explainable module over a large speculative dictionary.

## 2. Reuse the common phone inventory

Map the language to `nanotts_phone_t` values in
`include/nanotts/nanotts.h`. Add a new shared phone only when an existing phone
would destroy an important contrast.

When adding a phone:

1. append it without renumbering existing values;
2. add an original acoustic definition in `src/nanotts_voice.c`;
3. add a name in `nanotts_phone_name`;
4. add IPA spelling/aliases if appropriate;
5. test isolated rendering and parser acceptance;
6. document acoustic provenance.

Do not copy tables from another TTS engine.

## 3. Implement the bounded parser

Start from `src/lang/nanotts_lang_template.c.example` and create
`src/lang/nanotts_lang_xx.c`:

```c
nanotts_result_t nanotts_lang_xx_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info);
```

The contract is:

- validate UTF-8 incrementally;
- allocate no memory;
- use bounded local arrays only;
- reset `impl->event_count` before parsing;
- return `NANOTTS_ERR_EMPTY_INPUT` when no phone is produced;
- return byte offsets through `nanotts_parse_info_t`;
- merge or trim pauses through shared helpers;
- set `NANOTTS_EVENT_WORD_END`, stress, length, phrase, and question flags;
- never silently write past event or word-local capacity.

Reusable mechanics live in `nanotts_lang_common.[ch]`. Linguistic decisions
belong in the module. Closely related languages may share a private family
frontend, as Māori and Hawaiian do, provided a one-language build still links
only the required code and no family choice reaches the renderer.

## 4. Append a registry entry

`include/nanotts/nanotts_languages.def` is the language metadata source of
truth. Append a new entry; do not renumber old entries:

```c
NANOTTS_LANGUAGE(
    XX, EXAMPLE, 7, "xx", "Example",
    "xx|xx-yy|example",
    NANOTTS_ENABLE_LANG_XX, nanotts_lang_xx_parse_text, XX)
```

The fields are:

1. short internal tag;
2. public enum token;
3. stable numeric ID;
4. canonical code;
5. display name;
6. pipe-separated aliases;
7. compile-time enable macro;
8. parser symbol;
9. prosody profile token.

The registry generates:

- public `NANOTTS_LANG_*` enum entries;
- code/name/alias metadata;
- compiled-language discovery;
- one-language direct dispatch or multilingual switch cases;
- internal parser prototypes;
- prosody association.

IDs are currently contiguous append-only values. Keep that invariant so
`NANOTTS_LANG_COUNT` remains generated from the number of entries.

## 5. Add a prosody profile

Add `NANOTTS_PROFILE_XX` in `src/nanotts_prosody.c`. The profile is a small set
of timing and F0 scales, not a replacement for lexical stress:

```c
#define NANOTTS_PROFILE_XX { \
    126u, 100u, 100u, 100u, 100u, 108u, 104u, \
    12, 5, 14, 0, -20, 7, -5, 27, 8, -2, 9 }
```

Tune conservatively and explain each significant deviation from the neutral
profile. The registry instantiates the profile only when its language module is
compiled. The renderer asks for the profile once per render.

## 6. Wire the build systems

Add a CMake option and source condition:

```cmake
option(NANOTTS_ENABLE_LANG_XX "Build the Example text module" ON)
```

Update:

- the no-language CMake validation;
- `NANOTTS_SOURCES` selection;
- private compile definitions;
- API/language test expectation definitions;
- setup-script parser/profile naming;
- Makefile `LANG_XX`, definitions, and object selection;
- CI module-selection matrix.

A future build generator may consume the registry directly, but the current
plain CMake and Make files intentionally keep source selection explicit.

## 7. Add tests

At minimum, add:

- language-code and alias tests;
- compiled/uncompiled availability tests;
- representative grapheme and digraph tests;
- stress and punctuation tests;
- precomposed and decomposed Unicode tests when relevant;
- number/acronym tests;
- malformed UTF-8 and capacity tests;
- a UTF-8 smoke corpus under `tests/data/`;
- one-language, mixed-language, and no-`libm` builds;
- sanitizer and random-input parser runs.

When an external IPA producer supports the language, use
`tools/check_espeak_ipa.py` only as a process-separated parser-compatibility
check. It is not a pronunciation oracle and its output must not become runtime
language data.

## 8. Measure modularity

Build an IPA-only baseline and a compact language-only profile with identical
compiler settings:

```sh
cmake -S . -B build-ipa \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_ENABLE_TEXT_FRONTEND=OFF \
  -DNANOTTS_USE_LIBM=OFF \
  -DNANOTTS_MAX_EVENTS=128 \
  -DNANOTTS_CONTEXT_BYTES=1024

cmake -S . -B build-xx \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_ENABLE_LANG_XX=ON \
  -D...other languages...=OFF \
  -DNANOTTS_USE_LIBM=OFF \
  -DNANOTTS_MAX_EVENTS=128 \
  -DNANOTTS_CONTEXT_BYTES=1024
```

Inspect object sections and the target linker map. Confirm that:

- no disabled parser symbols are present;
- disabled prosody tables are absent;
- the selected parser is called directly in a one-language build;
- output adapter libraries are absent unless linked;
- per-context RAM does not grow merely because another language is compiled.

## 9. Document provenance and limitations

Update `docs/LANGUAGES.md`, `PROVENANCE.md`, `docs/VALIDATION.md`, README,
CHANGELOG, and the source-module README. Record linguistic references but do not
copy source code, TTS rule tables, dictionaries, recordings, or acoustic data
from incompatible projects.

## Review checklist

- [ ] Scope and dialect are explicit.
- [ ] Stable registry ID appended.
- [ ] Module is bounded and allocation-free.
- [ ] Existing phone inventory reused where sensible.
- [ ] New acoustic data is original and documented.
- [ ] Profile selected once per render.
- [ ] Single-language build has no other parser references.
- [ ] IPA-only build still works.
- [ ] PWM/PCM adapters remain language-independent.
- [ ] Tests cover Unicode, stress, punctuation, and failure paths.
- [ ] Native-speaker validation status is stated honestly.
