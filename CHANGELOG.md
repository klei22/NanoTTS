# Changelog

## 0.2.1 — 2026-06-19

- Added a portable `setup.sh` wrapper for parallel Makefile builds and the
  CMake/CTest validation suite, with clean, no-test, and job-count options.
- Expanded `.gitignore` for CMake, native binaries, generated audio, coverage,
  Python caches, local environments, and common editor files.
- No synthesis, pronunciation, or public API behavior changed from 0.2.0.

## 0.2.0 — 2026-06-19

- Reworked the acoustic renderer after a 50-utterance Indonesian evaluation.
- Replaced the unstable parallel voiced bank with a stable four-resonator
  cascade plus a dedicated F2 detail path.
- Rebalanced vowel, sonorant, stop, and fricative levels; reduced burst/noise
  dominance; and replaced the aggressive limiter with a gentler safety stage.
- Restricted coarticulation to immediate neighbors and added place-dependent
  vowel transition loci so stops no longer smear across syllables.
- Shortened word-boundary pauses, differentiated comma/phrase timing, and
  decayed filter state through pauses to prevent stale-formant clicks.
- Reduced stress exaggeration and made phrase pitch progression time-based.
- Added `IDTTS_FINAL_AUTO` and per-phrase question flags for punctuation-aware
  rising and falling contours in both text and IPA input.
- Added Indonesian spelling of all-uppercase acronyms such as `GPS`.
- Added regression tests for level balance, punctuation timing, acronym
  expansion, question flags, and the automatic tone behavior.
- Added a 50-case intelligibility corpus, reproducible metric tool, listening
  guidance, and a detailed evaluation record.

## 0.1.0 — 2026-06-19

- Initial MIT-licensed Indonesian-only C99 TTS runtime.
- Added strict separated-IPA parser and compact Indonesian text front end.
- Added original formant/source-filter voice and synchronous PCM callback API.
- Added optional no-`libm` math path and configurable event/context/audio sizes.
- Added WAV CLI, MCU example, CMake package, Makefile, tests, and documentation.
