# Indonesian intelligibility evaluation inherited from 0.2.0

Validation date: 2026-06-19.


> **Scope in NanoTTS 0.4:** this document records the Indonesian acoustic work
> that produced the shared renderer now used by all three text modules. It is
> not a Kiswahili or Spanish listening study. Their parsers and phone inventories
> have automated validation, but native-listener transcription studies remain
> required before claiming language-level intelligibility.

## Scope and caveat

This evaluation was designed to find gross intelligibility failures before an
MCU port. It combines a varied Indonesian corpus, isolated-phone measurements,
waveform and spectral inspection, direct-text versus external-IPA comparison,
and regression tests.

No claim in this document is a native-listener intelligibility score. Objective
signal measurements can reveal clipping, masking, missing vowel energy, and
bad timing, but they cannot determine what a person actually understood. A
blind listening protocol is given below and should be completed before a
production release.

## Corpus

`tests/data/id_intelligibility.tsv` contains 50 utterances covering:

- the vowel inventory and ambiguous written `e`;
- voiced and voiceless stops, affricates, and fricatives;
- `m n ny ng`, liquids, glides, tap, and trill;
- `ai au oi`, final consonants, `ny ng sy kh`, and clusters;
- common phrases, device alerts, numbers, questions, and long sentences;
- loanwords, uppercase acronyms, and mixed orthography.

Every sentence was rendered through both paths:

```text
text -> built-in Indonesian G2P -> renderer
text -> external eSpeak IPA -> IPA parser -> renderer
```

The 0.1.0 binary was retained as a paired baseline. eSpeak 1.48.15 was also
rendered as an external reference. eSpeak is not treated as ground truth; it is
a useful independent pronunciation and acoustic comparison.

## Baseline problems found

The 0.1.0 output had an extreme source balance problem. Narrow voiced filters
were summed in parallel, so vowel level depended strongly on whether a glottal
harmonic happened to align with a formant. Frication and release noise did not
have the same failure mode and often dominated the utterance.

Examples from isolated phones at 16 kHz:

| Contrast | 0.1 RMS | Difference |
|---|---:|---:|
| `/s/` versus `/a/` | 0.403 versus 0.018 | 27.0 dB |
| `/sh/` versus `/open-e/` | 0.514 versus 0.021 | 27.8 dB |
| `/l/` versus `/s/` | 0.059 versus 0.403 | -16.7 dB |

This makes a word sound like a sequence of hiss/burst events with weak vowel
nuclei. It also drives the safety limiter repeatedly and masks place and vowel
cues.

Other baseline issues were:

- every ordinary space inserted the full 28 ms short pause, producing a choppy
  word-by-word cadence;
- coarticulation could look through several phones and blur consonant place;
- filter memory was frozen during digital silence, allowing stale transients at
  the next word onset;
- primary stress added too much pitch and duration for this small voice;
- phrase pitch followed event count rather than elapsed audio time;
- question behavior was CLI-specific rather than represented in phone events.

## 0.2.0 acoustic changes

The voiced path now uses four stable, all-pole cascade resonators. A broad
parallel F2 detail filter restores the strongest front/back vowel cue without
reintroducing the amplitude instability of four independently summed narrow
bands. Glottal differentiation retains more useful harmonics.

The source mix was then recalibrated:

- frication and aspiration gains were reduced;
- sonorant and vowel energy was raised into the same operating range;
- stop closures, bursts, and releases remain staged but no longer hit a hard
  limiter in normal use;
- a gentler deterministic limiter is now only a transient safety net;
- the default output has headroom for MCU DAC/I2S gain downstream.

Timing and prosody changes include immediate-neighbor coarticulation,
place-dependent transition loci, 14 ms ordinary word pauses, longer comma and
phrase boundaries, state decay during pauses, milder prominence, time-based
phrase contours, and punctuation-aware per-phrase final tone.

## Paired 50-utterance results

The following are means over the direct-text path. Spectral metrics are
long-term diagnostics and do not by themselves measure intelligibility.

| Diagnostic | 0.1.0 | 0.2.0 | Change |
|---|---:|---:|---:|
| duration | 1.533 s | 1.493 s | -2.6% |
| absolute peak | 0.929 | 0.488 | -47.4% |
| RMS | 0.1337 | 0.1145 | -14.4% |
| active RMS | 0.1418 | 0.1214 | -14.4% |
| spectral centroid | 2922 Hz | 828 Hz | -71.7% |
| power at/above 3 kHz | 0.511 | 0.104 | -79.6% |
| samples at limiter threshold | 0.007% | 0.000% | eliminated in corpus |

The external eSpeak reference had an 866 Hz mean centroid in this corpus. The
similar 0.2.0 centroid is not evidence of equal quality, but it confirms that
the new output is no longer globally noise-dominated.

The isolated-phone balance also changed materially:

| Phone | 0.1 RMS | 0.2 RMS |
|---|---:|---:|
| `/a/` | 0.018 | 0.144 |
| schwa | 0.027 | 0.109 |
| `/open-e/` | 0.021 | 0.112 |
| `/l/` | 0.059 | 0.114 |
| trill `/r/` | 0.034 | 0.098 |
| `/s/` | 0.403 | 0.139 |
| `/sh/` | 0.514 | 0.186 |

The `/s/` to `/a/` level difference is now approximately -0.3 dB instead of
+27 dB. This removes the most severe masking mechanism found in the baseline.

## Pronunciation-front-end comparison

Normalized phone sequences from the built-in G2P were compared with the phone
sequences parsed from eSpeak-generated IPA for the same 50 lines:

```text
exact sequence matches: 34 / 50
phone edits:            30 / 792 reference phones
reference-relative PER: 3.79%
```

This is a compatibility comparison, not a correctness score. The remaining
mismatches are concentrated in known Indonesian orthographic ambiguities:

- plain `e` selecting `/e/`, `/open-e/`, or schwa;
- diphthong versus hiatus, such as `oi` in a particular word;
- word-final `k` versus a glottal realization;
- loanword vowel allophones;
- number-word and proper-name pronunciations.

Explicit accented text (`é`, `è`, `ê`) or the IPA API remains the recommended
path for critical prompts.

## Question contour check

For `Apakah pintu depan sudah terkunci?`, a diagnostic F0 tracker measured the
last voiced quarter approximately 2.7 semitones above the first quarter with
automatic punctuation handling. The same utterance forced to `--fall` ended
approximately 3.0 semitones below its first quarter. A declarative sentence in
automatic mode also fell. The exact values depend on the tracker, but the test
confirms that `?` now reaches the core scheduler and changes the final contour.

## Residual comprehension risks

The new output is clearer but remains a small formant voice. The largest
remaining risks are:

1. **Lexical ambiguity.** Written `e`, vowel hiatus, names, abbreviations, and
   code-switching cannot be solved reliably by a tiny context-free rule set.
2. **Compressed vowel allophones.** `/e/`, schwa, and `/open-e/`, and the close
   versus open back-vowel variants, are less separated than a recorded voice.
3. **Nasal place.** A single nasal resonance plus oral formants gives useful
   but still weak distinctions among `m`, `n`, `ny`, and `ng`.
4. **Sparse high formants.** The compact renderer emphasizes F1/F2. It does not
   reproduce the rich F3/F4 amplitude pattern of a natural speaker.
5. **Stop details.** Voicing onset time, glottalization, and release spectra are
   stylized rather than speaker-measured.
6. **Prosody.** Syllable weight, focus, syntax, and speaking style are not
   modeled; long sentences can still sound mechanical.
7. **No native-listener score yet.** Signal analysis cannot reveal all phone
   confusions or dialect expectations.

## Recommended listening study

Use at least 10 native Indonesian listeners who did not tune the voice. Present
randomized clips without showing the source text or system label. Balance the
number of 0.1.0 and 0.2.0 clips per listener and avoid presenting the same text
in both systems to one listener during transcription testing.

Collect:

- the exact transcription;
- required-keyword accuracy for alerts;
- a 1–5 ease-of-understanding score;
- optional phone-confusion notes;
- playback condition: headphones, phone speaker, or target MCU speaker.

Report word error rate, keyword accuracy, and median clarity by category, with
confidence intervals or listener-level bootstrap intervals. A second A/B test
may show the same sentence in both systems and ask only which is clearer.

The separately supplied listening pack implements a local randomized browser
worksheet and includes the full corpus.

## Reproduction

Build the current CLI, then run:

```sh
python3 tools/evaluate_intelligibility.py \
  --nanotts build/nanotts \
  --corpus tests/data/id_intelligibility.tsv \
  --output build/intelligibility-eval \
  --espeak espeak-ng
```

To compare another binary:

```sh
python3 tools/evaluate_intelligibility.py \
  --nanotts build/nanotts \
  --baseline /path/to/older/nanotts \
  --output build/intelligibility-eval
```

The tool needs NumPy. eSpeak is optional and remains an external process.
