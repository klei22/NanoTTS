# Changelog

## 0.4.0 — 2026-06-19

- Added optional Spanish (`es`) text support with a documented neutral
  Latin-American-style scope using seseo and yeísmo.
- Added accented stress, weak-vowel glide and hiatus handling, `ch`, `ll`,
  `rr`, `ñ`, silent `h`, `qu`, `gu`, `gü`, hard/soft `c` and `g`, taps,
  trills, Spanish numbers, acronyms, and inverted punctuation handling.
- Added lightweight within-word `/b~β/`, `/d~ð/`, and `/g~ɣ/` allophony and a
  new independently tuned `/β/` renderer phone and IPA symbol.
- Added `NANOTTS_ENABLE_LANG_ES`, `NANOTTS_LANG_SPANISH`, language-code aliases,
  Spanish-only and arbitrary subset build profiles, and direct parser dispatch
  in Spanish-only builds.
- Added acceptance of both precomposed and common decomposed UTF-8 spellings
  for acute accents, `ñ`, and `ü`, without a general normalization dependency.
- Extended strict IPA input with `β` and `ʝ` aliases used by external Spanish
  IPA producers; all 54 smoke-corpus phrases passed the process-separated
  compatibility check in the release environment.
- Extended the CLI, setup helper, CMake, Makefile, CI matrix, examples, tests,
  documentation, provenance record, and a 54-utterance Spanish smoke corpus.
- Preserved zero language branching in the renderer and the clean-room MIT
  runtime boundary.

## 0.3.0 — 2026-06-19

- Renamed the project, library, executable, include path, API prefix, package,
  build options, and documentation from idtts to NanoTTS / `nanotts`.
- Added explicit per-context text-language selection through
  `nanotts_init(..., language)` and `nanotts_set_language`.
- Added compile-time language discovery and selection APIs.
- Split text processing into independently removable language modules under
  `src/lang/`, while retaining one shared IPA parser and acoustic renderer.
- Added Kiswahili (`sw`) text support with regular grapheme-to-phone rules,
  `ch/sh/ny/ng'/ng/th/dh/gh/kh`, prenasalized sequences, adjacent-vowel
  handling, penultimate stress, syllabic-nasal events, numbers, acronyms, and
  punctuation-aware phrase flags.
- Added shared phones and IPA aliases for `/θ/`, `/ð/`, and `/ɣ/`, plus the
  combining syllabic mark for `m̩`, `n̩`, and `ŋ̍`.
- Added dual-language, Indonesian-only, Kiswahili-only, and IPA-only build
  profiles. A dual-language build dispatches once per text parse; a
  one-language build compiles to a direct parser call; the renderer has no
  language-dependent branch.
- Added `--lang`, `--list-languages`, language-selectable `setup.sh` profiles,
  modular CMake/Make options, a language-module template, a language porting
  guide, Kiswahili smoke corpus, and profile-specific CI coverage.
- Preserved the clean-room MIT runtime boundary: no eSpeak code, rules,
  dictionaries, voice data, or generated corpora are included.

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
- Added automatic punctuation-aware rising and falling contours.
- Added Indonesian spelling of all-uppercase acronyms such as `GPS`.
- Added regression tests, a 50-case Indonesian evaluation corpus, listening
  guidance, and a detailed objective analysis record.

## 0.1.0 — 2026-06-19

- Initial MIT-licensed Indonesian-only C99 TTS runtime.
- Added strict separated-IPA parser and compact Indonesian text front end.
- Added original formant/source-filter voice and synchronous PCM callback API.
- Added optional no-`libm` math path and configurable event/context/audio sizes.
- Added WAV CLI, MCU example, CMake package, Makefile, tests, and documentation.
