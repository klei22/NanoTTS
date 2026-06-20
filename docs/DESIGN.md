# Design

## Goals

NanoTTS targets open-ended, intelligible speech on resource-constrained systems
where prerecorded prompts are insufficient. Its priorities are:

1. small, auditable, MIT-licensed runtime;
2. deterministic behavior and bounded memory;
3. no operating-system, filesystem, threading, locale, or allocator dependency;
4. removable language and output modules;
5. no language/output modularity branch in the real-time sample loop;
6. intelligibility before naturalness.

It is not an eSpeak port. An external executable may provide separated IPA over
a process boundary, but NanoTTS independently parses and renders it.

## Pipeline

```text
id ─ Indonesian ───────┐
sw ─ Kiswahili ────────┤
es ─ Spanish ──────────┤
ms ─ Malay ────────────┼── compact phone events
mi ─ Māori ───────┐    │          │
haw ─ Hawaiian ───┴ family core ──┘          │
strict IPA parser ────────────────────────────┘
                                              │
                                  selected prosody profile
                                              │
                                              ▼
                              source/filter formant renderer
                                              │
                                   signed native int16 PCM
                                              │
                     ┌────────────────────────┼───────────────────────┐
                     ▼                        ▼                       ▼
             application callback      PCM16LE adapter       PWM compare adapter
               I²S/DAC/codec          byte/file/socket         timer DMA/ISR
```

The output adapters consume PCM. They never know which language produced it.
The text modules stop at events and never know which output sink will consume
the render.

## Language registry

`include/nanotts/nanotts_languages.def` is an X-macro registry. Each append-only
entry declares:

```text
tag, public enum token, stable ID, canonical code, display name,
aliases, compile-time enable macro, parser symbol, prosody token
```

The same file is included under different macro definitions to generate:

- public enum values;
- language metadata and aliases;
- compiled-language discovery;
- internal parser prototypes;
- parser dispatch cases;
- prosody association.

Stable IDs are contiguous and append-only. Build systems still select source
files explicitly so CMake and the plain Makefile remain understandable without
a code-generation step.

### Dispatch cost

`NANOTTS_COMPILED_LANGUAGE_COUNT` is a compile-time expression.

- zero text modules: text parsing returns `FEATURE_DISABLED`;
- one text module: preprocessor expansion emits a direct parser call;
- multiple text modules: one integer switch occurs per text utterance.

After event generation, there is no language function pointer, virtual table,
or switch in the phone, 5 ms frame, or sample loops.

## Language families

Closely related modules may share a private family implementation. Māori and
Hawaiian share bounded UTF-8/macron/vowel-nucleus mechanics through
`nanotts_lang_polynesian.c`, while their wrappers pass one family selection at
parse entry. A Māori-only or Hawaiian-only build still excludes the other
public module. Family sharing does not enter synthesis.

## Event representation

Each parsed item is four bytes:

```c
typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t  pitch_semitones_q4;
} nanotts_event_t;
```

Events carry pronunciation-independent annotations such as primary/secondary
stress, long duration, word boundary, phrase boundary, question, and syllabic
status. The default context buffers 512 events; this is a compile-time setting.

Text modules use bounded word-local arrays and copy normalized events into the
context. Overflow is reported, never truncated.

## Parsers

### Strict IPA

The IPA parser decodes UTF-8 incrementally and greedily recognizes the finite
`nanotts-ipa/1` inventory, including supported multi-code-point affricates,
diphthongs, ties, stress, length, and syllabic marks. Unicode is converted to
small phone IDs immediately. Strict mode reports unknown code points and byte
offsets; permissive mode skips them.

### Text modules

A module owns:

- orthographic normalization;
- multi-letter rules;
- lexical ambiguity tables;
- number/acronym expansion;
- lexical stress/prominence flags;
- punctuation and question flags.

Shared text code is restricted to mechanical helpers such as UTF-8 decoding,
pause merging, bounded event pushes, and parse diagnostics. A universal G2P
engine is intentionally avoided because it would pull irrelevant code into a
one-language MCU build.

## Per-language prosody

Pronunciation modules mark lexical prominence, but each language also has a
small immutable `nanotts_prosody_profile_t` containing:

- base F0;
- vowel/consonant duration scales;
- word/phrase pause scales;
- primary/secondary stress scales;
- fall, rise, and continuation contour points.

The renderer resolves the profile once at the beginning of
`nanotts_render_events` and passes the pointer through event/frame scheduling.
There is no subsequent language lookup. X-macro enable gating prevents disabled
language profiles from being instantiated; queries for an uncompiled language
resolve to the neutral IPA profile.

The current profiles are deliberately subtle. They improve rhythm and prevent
one global contour from treating every language identically, but they are not
large statistical prosody models.

## Acoustic renderer

The renderer is a compact frame-updated source/filter synthesizer. Parameters
are refreshed every 5 ms and held within each frame.

### Excitation

- Voiced phones use a deterministic differentiated glottal-flow waveform.
- Fricatives and stop releases use deterministic xorshift noise.
- Aspiration mixes broadband and shaped noise.
- Voiced fricatives combine periodic and noise sources.

### Filters

- Four all-pole resonators in cascade provide the voiced vocal-tract envelope.
- A broad parallel F2 path reinforces front/back vowel identity.
- Three parallel filters shape frication.
- Nasals mix a low resonance with broadened oral formants.
- Radiation/high-pass processing removes DC and shapes spectral tilt.

Filter targets interpolate across diphthongs and glides and are adjusted by
immediate-neighbor place loci. Stops stage closure, burst, and release;
affricates stage closure and frication; taps and trills use short voicing gates.

With `NANOTTS_USE_LIBM=OFF`, compact internal approximations replace sine,
cosine, and power calls. The renderer remains single-precision floating point.

## Core output contract

The core exposes only:

```c
int write(void *user, const int16_t *samples, size_t count);
```

This is the primary portability boundary. A target can copy blocks into:

- I²S DMA;
- DAC DMA;
- an audio-codec queue;
- a host WAV writer;
- the optional PCM/PWM adapters;
- any custom transport.

The core does not know about files or devices.

## PWM adapter

`NanoTTS::PWM` is a separate static/shared target. It linearly maps PCM into
unsigned timer compare values between zero and `top`, optionally inverted, and
uses caller-supplied scratch memory.

```text
PCM block → compare block → application DMA/ISR queue → hardware PWM register
```

The adapter emits one compare value per audio sample. The target timer supplies
the much faster carrier and updates its compare register at the audio sample
rate. The adapter does not configure timer clocks, carrier frequency, DMA,
interrupts, analog filtering, or amplification.

Because it is a separate target, a core-only I²S/DAC build has no PWM symbols or
scratch requirements.

## PCM16LE adapter

`NanoTTS::PCM` is also separate. It serializes native `int16_t` samples into a
defined signed little-endian byte stream using caller-owned scratch. This is
useful for host files, UART/USB/network transfer, or an MCU peripheral that
expects bytes. It adds no WAV header.

## Memory and determinism

- No heap allocation.
- Context size and event capacity are compile-time constants.
- Output adapter scratch belongs to the caller.
- Noise starts from a fixed seed at every render.
- Identical events, options, rate, build configuration, and floating-point
  behavior produce identical PCM.
- Separate contexts may render concurrently; one context is not reentrant.

## Voice data

`src/nanotts_voice.c` contains original, hand-tuned neutral phone classes,
durations, source levels, formants, and noise bands. All languages share this
voice. Language selects pronunciation/prosody; the voice selects acoustics.

A future voice-pack system should remain orthogonal to the language registry
and output adapters.

## Clean-room boundary

Language rules and acoustic values are independently authored from descriptive
linguistic and speech-synthesis literature. No eSpeak/SAM/Flite implementation,
rule table, dictionary, voice, or recording is copied into the project. See
`PROVENANCE.md`.
