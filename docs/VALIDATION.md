# Validation record for 0.2.0

Validation date: 2026-06-19.

## Build matrix

| Compiler/configuration | Text front end | Math path | Events/context/block | Result |
|---|---:|---|---|---|
| GCC MinSizeRel | yes | system `libm` | 512 / 3072 B / 128 | pass |
| GCC MinSizeRel | yes | internal approximations | 512 / 3072 B / 128 | pass |
| Clang MinSizeRel | yes | internal approximations | 512 / 3072 B / 128 | pass |
| GCC IPA-only | no | system `libm` | 512 / 3072 B / 128 | pass |
| GCC compact IPA-only | no | internal approximations | 128 / 1024 B / 64 | pass |

The CTest suite covers API validation, IPA inventory and aliases, malformed
UTF-8, Indonesian G2P, uppercase acronyms, boundary timing, question flags,
deterministic rendering, all phone classes, callback abortion, 8/16/24 kHz
operation, and level-balance regressions. Assertions are explicitly active in
release-like test builds.

## Dynamic and static checks

- GCC AddressSanitizer plus UndefinedBehaviorSanitizer: all tests passed.
- 200,000 randomized byte strings exercised both parsers; accepted short IPA
  sequences were rendered under the same sanitizer build.
- GCC `-fanalyzer`: no diagnostics on runtime translation units.
- Strict warning builds passed under GCC and Clang.
- The compact configuration's internal context is exactly 1024 bytes on the
  tested x86-64 ABI, matching its public context.

## eSpeak interface checks

The 50-utterance evaluation corpus was converted with locally installed eSpeak
1.48.15 using `--ipa=1 --sep=_`; every line was accepted in strict IPA mode.
The runtime neither links nor loads eSpeak.

The unchanged parser design had also accepted an earlier synthetic scan of
18,278 eSpeak outputs for every one-, two-, and three-letter lowercase ASCII
input. That scan is a format-coverage stress test, not a pronunciation-quality
claim or a guarantee for every eSpeak NG release.

## Size observations

GCC `-Os` host object section totals (`text + data`) were:

| Build | Approximate object total |
|---|---:|
| full, system `libm` | 20,374 B |
| full, no-`libm` | 20,487 B |
| IPA-only, system `libm` | 14,737 B |
| 128-event compact IPA-only, no-`libm` | 14,840 B |

The tested internal context sizes were 2,688 B for the default configuration
and 1,024 B for 128 events with a 64-sample callback block. Public contexts are
3,072 B and 1,024 B respectively.

Object totals exclude final linker/runtime contributions and are not MCU flash
or cycle guarantees.

## Audio evaluation

A paired 50-utterance sweep compared 0.1.0, 0.2.0 text, 0.2.0 externally
produced IPA, and an eSpeak reference. The 0.2.0 corpus had no sample at the
limiter threshold. Mean peak fell from 0.929 to 0.488 while vowel and sonorant
levels became much more consistent. Mean long-term spectral centroid changed
from 2922 Hz to 828 Hz, eliminating the baseline's broadband-noise dominance.

Isolated-phone tests, G2P comparison, question-contour checks, limitations, and
reproduction instructions are in `docs/INTELLIGIBILITY.md`.

## Release limitation

No native-listener transcription study has yet been completed. The current
voice should be considered a substantially improved embedded formant voice,
not a natural or production-validated Indonesian voice.
