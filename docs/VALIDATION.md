# Validation record for NanoTTS 0.5.0

Validation date: 2026-06-19 (America/Los_Angeles).

This record describes the source tree and release archive tested in the project
workspace. It is an engineering validation report, not a formal proof of
correctness, a timing guarantee for a specific MCU, or a native-listener
intelligibility study.

## Build and test matrix

Fresh out-of-tree CMake builds were exercised with these profiles:

| Compiler/configuration | Text modules | Outputs | Math path | Events/context/block | Result |
|---|---|---|---|---|---|
| GCC 14.2 MinSizeRel, `-Werror` | all six | PWM + PCM | system `libm` | 512 / 3072 B / 128 | pass, 6 tests |
| GCC 14.2 MinSizeRel, `-Werror` | all six | PWM + PCM | internal approximations | 512 / 3072 B / 128 | pass, 6 tests |
| Clang 17 MinSizeRel, `-Werror` | all six | PWM + PCM | internal approximations | 512 / 3072 B / 128 | pass, 6 tests |
| GCC 14.2 compact | Indonesian only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 6 tests |
| GCC 14.2 compact | Kiswahili only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 6 tests |
| GCC 14.2 compact | Spanish only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 6 tests |
| GCC 14.2 compact | Malay only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 6 tests |
| GCC 14.2 compact | Māori only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 6 tests |
| GCC 14.2 compact | Hawaiian only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 6 tests |
| GCC 14.2 compact | IPA only | PWM + PCM | internal approximations | 128 / 1024 B / 64 | pass, 5 tests |
| GCC 14.2 MinSizeRel | all six | no adapters | system `libm` | default | pass, 4 tests |
| GCC 14.2 MinSizeRel | all six | PWM only | system `libm` | default | pass, 5 tests |
| GCC 14.2 MinSizeRel | all six | PCM only | system `libm` | default | pass, 5 tests |
| GCC 14.2 MinSizeRel, `-Werror` | Māori + Hawaiian | no adapters | system `libm` | default | pass, 4 tests |

The C suite covers:

- public API validation and stable language-code/alias behavior;
- compile-time language discovery and unavailable-module errors;
- one-language direct dispatch and multilingual dispatch;
- strict and permissive IPA, malformed UTF-8, and event bounds;
- all six text modules, stress flags, long vowels, diphthongs, acronyms,
  numbers, punctuation, and question contours;
- per-language prosody selection for text and explicit IPA;
- deterministic rendering, every phone class, callback abortion, and
  8/16/24 kHz operation;
- PCM16LE byte order, chunking, and downstream abort propagation;
- PWM endpoint mapping, inversion, range checking, chunking, and end-to-end
  render callback behavior.

Assertions remain active in release-like test builds.

## Makefile and setup helper

The repository Makefile and `setup.sh` were exercised from clean profile
directories for:

```text
Malay + PWM
Māori + Hawaiian + PWM + PCM
IPA-only + PWM
all six languages + no optional adapters
```

Every matching CMake/CTest profile passed. The Makefile builds completed with
`-Wall -Wextra -Wpedantic` and no warnings, including builds with either or both
optional output adapters omitted.

## Text-corpus checks

`tools/check_text_corpus.py` sent each non-comment line through the selected
compiled text frontend and required a successful, nonempty phone-event stream:

| Corpus | Language | Inputs | Failures |
|---|---|---:|---:|
| `tests/data/ms_smoke.txt` | Malay | 30 | 0 |
| `tests/data/mi_smoke.txt` | Māori | 30 | 0 |
| `tests/data/haw_smoke.txt` | Hawaiian | 30 | 0 |

These corpora exercise ordinary phrases, long vowels/diacritics, digraphs,
vowel groups, punctuation, numbers or digit names, and embedded-device prompts.
They establish parser coverage, not pronunciation correctness.

## Dynamic and static checks

- GCC AddressSanitizer plus UndefinedBehaviorSanitizer: all six tests passed.
- A sanitizer-enabled in-process harness completed 700,000 randomized parser
  calls: 100,000 each for Indonesian, Kiswahili, Spanish, Malay, Māori,
  Hawaiian, and strict/permissive IPA, with no diagnostics.
- GCC `-fanalyzer -Werror` completed without diagnostics for the full runtime,
  adapters, CLI, examples, and tests; the resulting tests passed.
- Strict warning builds passed under GCC and Clang.
- The default internal context measured 2688 bytes on the tested x86-64 ABI,
  within the 3072-byte public allocation.
- The 128-event, 64-sample-block configuration measured exactly 1024 bytes and
  passed with a 1024-byte public context for every one-language profile and
  IPA-only.

These checks improve confidence in bounds and state handling but do not replace
target-specific stack, execution-time, DMA, timer, and electrical validation.

## Build-system integration

The complete build was installed into a clean prefix. An independent consumer
successfully used:

```cmake
find_package(NanoTTS 0.5 CONFIG REQUIRED)
target_link_libraries(app PRIVATE
    NanoTTS::NanoTTS
    NanoTTS::PWM
    NanoTTS::PCM)
```

The consumer initialized the Malay frontend and rendered the same phrase
through both adapters, receiving 16,520 PWM compare samples and 33,040 PCM16LE
bytes. This also checked the exported target names, public headers, context
layout definitions, and transitive core dependency.

## External separated-IPA checks

The locally installed eSpeak 1.48.15 executable was used only as a
process-separated development producer. Strict NanoTTS IPA acceptance results:

| Corpus | Producer voice | Phrases | Strict IPA failures |
|---|---|---:|---:|
| `tests/data/id_smoke.txt` | `id` | 15 | 0 |
| `tests/data/sw_smoke.txt` | `sw` | 50 | 0 |
| `tests/data/es_smoke.txt` | `es-la` | 54 | 0 |
| `tests/data/ms_smoke.txt` | `ms` | 30 | 0 |

That installed producer did not provide Māori or Hawaiian voices, so those
frontends were validated through their independent parser tests and direct text
corpora rather than an external IPA comparison.

This check validates only the finite NanoTTS IPA input profile against those
corpora and that producer version. It is not a pronunciation-quality comparison
and does not incorporate external rules or data into the NanoTTS runtime.

## Host object-section observations

The following GCC `-Os` x86-64 totals are `text + initialized data` over core
object files. They are comparison measurements, not MCU flash guarantees:

| Build | Core object total | Increment from compact IPA-only |
|---|---:|---:|
| Compact IPA-only | 18,029 B | baseline |
| Compact Māori-only | 23,827 B | +5,798 B |
| Compact Hawaiian-only | 23,830 B | +5,801 B |
| Compact Māori + Hawaiian | 23,961 B | +5,932 B |
| Compact Kiswahili-only | 23,995 B | +5,966 B |
| Compact Malay-only | 24,650 B | +6,621 B |
| Compact Indonesian-only | 24,835 B | +6,806 B |
| Compact Spanish-only | 26,751 B | +8,722 B |
| All six, system `libm` | 44,491 B | +26,462 B |
| All six, no `libm` | 44,610 B | +26,581 B |

The unusually small increase from Māori-only to Māori-plus-Hawaiian reflects
the removable shared Polynesian normalization and nucleus-analysis source.

Optional adapter objects measured:

| Adapter | `text + initialized data` |
|---|---:|
| PWM compare adapter | 445 B |
| PCM16LE serializer | 349 B |

The all-language no-`libm` build contained these language-related objects:

| Object | `text + initialized data` |
|---|---:|
| shared text helpers | 1,686 B |
| Indonesian module | 4,871 B |
| Kiswahili module | 4,026 B |
| Spanish module | 6,783 B |
| Malay module | 4,680 B |
| shared Māori/Hawaiian family core | 3,803 B |
| Māori wrapper | 56 B |
| Hawaiian wrapper | 59 B |
| language registry/metadata | 2,366 B |
| all prosody profiles | 370 B |

Language modules and profiles add no fields to `nanotts_t`. PWM and PCM scratch
storage is caller-owned. Final target size depends on ABI, C library,
floating-point implementation, linker garbage collection, LTO, and the linker
script.

## Output sanity checks

Three release samples were generated as mono signed 16-bit 16 kHz WAV:

| Language | Samples | Duration | Absolute peak | Clipped samples |
|---|---:|---:|---:|---:|
| Malay | 43,723 | 2.733 s | 16,491 | 0 |
| Māori | 37,928 | 2.371 s | 19,644 | 0 |
| Hawaiian | 49,648 | 3.103 s | 20,500 | 0 |

For the phrase `Bateri hampir habis.`:

- WAV payload and separately generated PCM16LE output were byte-identical;
- both contained 26,648 samples;
- PWM16LE with `TOP=1023` contained the same number of compare values;
- observed PWM values were 290 through 710 and all were in range;
- inverted output satisfied `normal + inverted == 1023` for every sample;
- reconstructing approximate PCM from 10-bit duty values produced a maximum
  absolute quantization error of 63 PCM counts and RMS error of 34.03 counts.

These are file-format, range, quantization, clipping, and non-silence checks—not
native-listener intelligibility measurements or a substitute for testing the
production PWM carrier, reconstruction filter, amplifier, enclosure, and
speaker.

## Performance architecture check

- Stable public IDs and aliases come from one append-only registry.
- A one-language build preprocesses to a direct parser call.
- A multilingual build performs one integer switch per text utterance.
- The prosody profile is selected once at render start.
- No language lookup or output-format branch occurs in the phone, 5 ms frame,
  or sample loops.
- PWM conversion uses one 32-bit multiply and shift per sample, with no integer
  division.
- PCM/PWM adapters process completed core PCM blocks and can be omitted entirely.

## Audio and linguistic status

The shared formant voice remains deliberately robotic. The language profiles
adjust timing and F0 but do not create separate native voices. Malay plain `e`,
Māori compound stress and iwi variation, Hawaiian compound stress and `w`
variation, proper names, code-switching, and unusual loanwords can require
explicit IPA.

No formal native-listener transcription study has yet been completed for Malay,
Māori, or Hawaiian. Treat the modules as compact experimental frontends and
validate every public-facing or safety-critical prompt with native speakers on
the production transducer.
