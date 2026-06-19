# Design

## Goals

`idtts` is designed for open-ended Indonesian speech on resource-constrained
systems where prerecorded phrases are insufficient. Its priorities are:

1. a small and auditable runtime;
2. deterministic behavior and bounded memory;
3. no operating-system, filesystem, threading, or allocator dependency;
4. a clean boundary from eSpeak licensing and implementation;
5. intelligibility before naturalness.

It is not a clone of eSpeak. eSpeak may optionally produce the input IPA on a
host, while `idtts` independently parses and renders that IPA.

## Pipeline

```text
Indonesian text                          separated Indonesian IPA
      │                                            │
      └── independent compact G2P                  │
                         └──────────┬──────────────┘
                                    ▼
                         normalized phone events
                                    ▼
                 duration, prominence, and F0 scheduling
                                    ▼
             glottal + aspiration + frication excitation
                                    ▼
                formant, noise, and nasal resonator banks
                                    ▼
                       radiation / DC blocker / limiter
                                    ▼
                         signed 16-bit mono PCM
```

## Bounded event representation

Each parsed item is four bytes:

```c
typedef struct idtts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t  pitch_semitones_q4;
} idtts_event_t;
```

The default array holds 512 events. This allows the scheduler to inspect nearby
phones for coarticulation and phrase position without allocating memory. The
capacity is configurable at compile time.

## Parser

The IPA parser performs incremental UTF-8 decoding and greedy recognition of
multi-code-point affricates and diphthongs. It immediately converts Unicode to
small phone IDs, so no Unicode survives into the renderer. Unknown code points
are reported with their byte offsets in strict mode.

The text front end is separate from the IPA parser. It handles Indonesian
letter mappings and a small amount of normalization. Because Indonesian plain
`e` and vowel sequences are lexically ambiguous, explicit accented vowels or
caller-provided IPA remain available for exact control.

## Acoustic model

The renderer is a compact frame-updated source/filter synthesizer. Parameters
are refreshed every 5 ms, then held within that audio frame.

### Excitation

- Voiced phones use a deterministic differentiated glottal-flow waveform.
- Fricatives and releases use deterministic xorshift white noise followed by
  spectral shaping.
- Aspiration mixes broadband and shaped noise.
- Voiced fricatives combine periodic and noise excitation.

### Filters

Four all-pole resonators in cascade provide a stable vocal-tract envelope for
voiced output. One broad parallel F2 band-pass restores a strong front/back
vowel cue without making level depend on narrow harmonic/formant alignment.
Three parallel band-pass filters shape frication energy. A low nasal resonance
is mixed for nasals. A simple high-pass/radiation stage suppresses DC and adds
speech-like spectral tilt.

Filter frequencies and bandwidths are recalculated on 5 ms boundaries. With
`IDTTS_USE_LIBM=OFF`, a range-reduced polynomial sine/cosine approximation and
a compact base-2 exponential approximation replace transcendental library
calls. In the tested reference phrase, the no-`libm` output differed from the
system-`libm` build by about -86 dB RMS and at most two 16-bit sample counts.
That comparison is implementation validation, not a formal error guarantee for
all inputs and targets.

### Phone behavior

- Vowels hold or interpolate formant targets.
- Diphthongs interpolate to a second target with smooth timing.
- Stops stage closure, burst, and release/aspiration.
- Affricates stage closure followed by a fricative release.
- `/h/` borrows the following sonorant's formants.
- Nasals add a low resonance and broader formants.
- The trill gates voicing several times; the tap uses one short closure.
- Immediate neighboring phones pull early and late vowel targets toward
  place-dependent loci; coarticulation never looks through an intervening stop
  or fricative.

## Prosody

The scheduler assigns base durations by phone class and modifies vowel duration
and pitch mildly for stress. Global speed and pitch are caller-controlled.
Phrase F0 uses fall, rise, continuation, or level contours. The default
`IDTTS_FINAL_AUTO` mode reads phrase-level question flags produced by either
parser, so punctuation can select a different contour for each phrase. Explicit
options override automatic selection.

This is intentionally modest. A later prosody module can be replaced without
changing the IPA parser or PCM callback API.

## Determinism and reentrancy

The noise generator is reset to a fixed seed at the start of every render.
Identical events, options, sample rate, build configuration, and compiler
floating-point behavior therefore produce identical output. Separate
`idtts_t` contexts may be used concurrently. A single context must not be
entered concurrently.

## Voice data

`src/idtts_voice.c` contains an original, hand-tuned neutral table of phone
classes, durations, source levels, resonant frequencies, and noise bands. It is
not extracted from another synthesizer or recording. This keeps the first
release small and unencumbered, but also explains its synthetic quality.

A future compatible backend could use independently recorded LPC/diphone data
while retaining the same parser, event scheduler, public API, and license
provenance rules.

## Scientific basis

The broad source/filter organization is informed by published formant synthesis
literature, particularly Dennis H. Klatt, “Software for a Cascade/Parallel
Formant Synthesizer,” *Journal of the Acoustical Society of America* 67(3),
1980, pp. 971–995. The implementation does not use Klatt program source.

The Indonesian inventory and the importance of allophones and lexical handling
for `e` and vowel sequences are informed by D. P. Lestari et al., “Indonesian
Phoneme Set, Vocabulary, and Pronunciation for Automatic Speech Recognition and
Speech Synthesizer,” LT4All 2019, pp. 206–209.
