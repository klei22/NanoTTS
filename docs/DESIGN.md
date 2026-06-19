# Design

## Goals

NanoTTS targets open-ended speech on resource-constrained systems where
prerecorded phrases are insufficient. Its priorities are:

1. a small, auditable, MIT-licensed runtime;
2. deterministic behavior and bounded memory;
3. no operating-system, filesystem, threading, or allocator dependency;
4. language modules that can be compiled in or out independently;
5. no modularity branch in the real-time renderer;
6. intelligibility before naturalness.

It is not a clone of eSpeak. eSpeak may optionally produce separated IPA in a
host process, while NanoTTS independently parses and renders that IPA.

## Pipeline

```text
Indonesian text ── id module ─┐
Kiswahili text ─── sw module ───┤
Spanish text ───── es module ────┼─ normalized phone events
                                │             │
separated IPA ───── IPA parser──┘             ▼
                                  duration, prominence, F0
                                              ▼
                              glottal/noise excitation
                                              ▼
                           formant and noise resonators
                                              ▼
                             radiation / DC / limiter
                                              ▼
                                signed 16-bit mono PCM
```

Language modules stop at the event boundary. The acoustic renderer never knows
whether events came from Indonesian, Kiswahili, Spanish, IPA, or a precomputed array.

## Language-module dispatch

The selected language is one byte in `nanotts_t`. `nanotts_parse_text` performs
a single compile-time-controlled selection:

- multi-language build: one `switch` per text utterance;
- Indonesian-only build: direct `nanotts_lang_id_parse_text` call;
- Kiswahili-only build: direct `nanotts_lang_sw_parse_text` call;
- Spanish-only build: direct `nanotts_lang_es_parse_text` call;
- IPA-only build: text parser and shared text helpers omitted.

There is no function-pointer dispatch per phone, frame, or sample. The synthesis
path is identical for every language. This makes the module boundary useful for
flash selection without changing real-time performance.

## Bounded event representation

Each parsed item is four bytes:

```c
typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t  pitch_semitones_q4;
} nanotts_event_t;
```

The default context holds 512 events. The scheduler can inspect neighboring
phones for coarticulation and phrase position without allocation. Capacity is a
compile-time parameter.

Language modules may use a bounded word-local event array before copying into
the context. Overflow is reported rather than truncated.

## Parsers

### IPA parser

The IPA parser incrementally decodes UTF-8 and greedily recognizes supported
multi-code-point affricates and diphthongs. It converts Unicode immediately to
small phone IDs. Unknown code points are reported with byte offsets in strict
mode.

The accepted inventory is a finite cross-language profile rather than arbitrary
IPA. It includes the phones used by current Indonesian, Kiswahili, and Spanish modules,
plus unambiguous aliases observed from separated external IPA producers.

### Text modules

Each text module owns its orthographic normalization, word rules, number words,
stress, and compact exceptions. Shared code is limited to neutral mechanics
such as UTF-8 decoding, pause merging, bounded temporary events, and parse
diagnostics.

This division prevents a large universal rule engine from being dragged into a
one-language MCU build. It also keeps ambiguous linguistic decisions local and
testable.

## Acoustic model

The renderer is a compact frame-updated source/filter synthesizer. Parameters
are refreshed every 5 ms, then held within that frame.

### Excitation

- Voiced phones use a deterministic differentiated glottal-flow waveform.
- Fricatives and releases use deterministic xorshift noise plus spectral
  shaping.
- Aspiration mixes broadband and shaped noise.
- Voiced fricatives combine periodic and noise excitation.

### Filters

Four all-pole resonators in cascade provide a stable vocal-tract envelope for
voiced output. A broad parallel F2 band-pass restores a front/back vowel cue
without making level depend entirely on narrow harmonic/formant alignment.
Three parallel filters shape frication. A low nasal resonance is mixed for
nasals. A high-pass/radiation stage suppresses DC and adds speech-like tilt.

Filter frequencies and bandwidths are recalculated every 5 ms. With
`NANOTTS_USE_LIBM=OFF`, internal sine/cosine and base-2 exponential
approximations replace transcendental library calls. The renderer still uses
single-precision floating point.

### Phone behavior

- Vowels hold or interpolate formant targets.
- Indonesian diphthong events interpolate smoothly to a second target.
- Kiswahili adjacent vowels remain separate events.
- Spanish weak-vowel groups use glide events, while `ai`, `au`, and `oi`
  reuse smooth diphthong trajectories and accented weak vowels force hiatus.
- Stops stage closure, burst, and release/aspiration.
- Affricates stage closure followed by frication.
- `/h/` borrows a following sonorant's formants.
- Nasals add a low resonance and broader formants.
- The trill gates voicing; the tap uses one short closure.
- Immediate neighbors pull vowel edges toward place-dependent loci.
- Dedicated compact definitions cover `/θ/`, `/ð/`, and `/ɣ/`; Spanish also
  uses an independently tuned `/β/` phone for common intervocalic allophony.

## Prosody

The scheduler assigns base duration by phone class and applies mild prominence
to stressed vowels. Text modules set lexical stress; IPA stress marks set the
same event flags. Global speed and pitch are caller-controlled.

Phrase F0 supports fall, rise, continuation, or level contours. Automatic mode
uses phrase-level question flags produced by text or IPA punctuation. This
prosody layer is intentionally language-light; future language-specific
prosody can add event annotations without changing the PCM callback contract.

## Determinism and concurrency

The noise generator resets to a fixed seed at the start of every render.
Identical events, options, sample rate, build configuration, and compiler
floating-point behavior therefore produce identical output. Separate contexts
may be used concurrently. One context must not be entered concurrently.

## Voice data

`src/nanotts_voice.c` contains an original hand-tuned neutral table of phone
classes, durations, source levels, formants, and noise bands. It is shared by
all current languages and is not extracted from another synthesizer or
recording.

A future voice-pack abstraction could replace these tables, but it should
remain orthogonal to text-language selection: language chooses pronunciation;
voice chooses acoustics.

## Portability boundary

Only the public callback touches application audio code. The library has no
SDL, POSIX, filesystem, device, or threading calls. Host WAV writing belongs to
the CLI. MCU I2S/DAC/PWM code belongs to the application callback.

## References and provenance

The architecture uses general source/filter and formant-synthesis concepts from
public speech literature. Language modules are independently authored from
linguistic descriptions, not copied TTS rules. See `PROVENANCE.md` for the
clean-room policy and reference links.
