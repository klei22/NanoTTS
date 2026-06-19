# Validation record for NanoTTS 0.4.0

Validation date: 2026-06-19.

## Build and test matrix

Fresh out-of-tree CMake/Ninja builds were exercised with these profiles:

| Compiler/configuration | Text modules | Math path | Events/context/block | Result |
|---|---|---|---|---|
| GCC 14.2 MinSizeRel | `id`, `sw`, `es` | system `libm` | 512 / 3072 B / 128 | pass, 4 tests |
| GCC 14.2 MinSizeRel | `id`, `sw`, `es` | internal approximations | 512 / 3072 B / 128 | pass, 4 tests |
| Clang 17 MinSizeRel | `id`, `sw`, `es` | internal approximations | 512 / 3072 B / 128 | pass, 4 tests |
| GCC 14.2 compact | Indonesian only | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | Kiswahili only | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | Spanish only | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | Indonesian + Kiswahili | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | Indonesian + Spanish | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | IPA only | internal approximations | 128 / 1024 B / 64 | pass, 3 tests |

The suite covers API validation, compile-time language discovery, unavailable
module behavior, language switching, strict and permissive IPA, malformed
UTF-8, all three G2P modules, stress and syllabic-nasal flags, number expansion,
acronym handling, punctuation timing, question contours, deterministic
rendering, every phone class, callback abortion, 8/16/24 kHz operation, and
acoustic level regressions. Assertions remain active in release-like test
builds.

The portable `setup.sh`/Makefile path was also run successfully for `all`,
`es`, `id,es`, and `ipa` profiles. Earlier module-profile CMake tests cover the
remaining one-language configurations.

## Spanish-specific checks

`tests/data/es_smoke.txt` contains 54 inputs covering:

- all five vowels and the main consonant spellings;
- soft/hard `c` and `g`, `qu`, `gu`, `gü`, silent `h`, `ch`, `ll`, `rr`, and
  `ñ`;
- taps, trills, seseo, yeísmo, and compact stop/approximant allophony;
- ordinary diphthongs, accented hiatus, default stress, written accents, and
  final consonant clusters;
- precomposed and NFD acute, tilde, and diaeresis spellings;
- numbers, acronym heuristics, punctuation, questions, alerts, and loanwords.

All 54 phrases were accepted by the built-in Spanish parser and rendered to
non-silent mono 16-bit 16 kHz WAV files. Across that corpus, output durations
ranged from 0.353 to 2.496 seconds, absolute peaks from 0.398 to 0.653 of full
scale, and RMS from 0.1066 to 0.1506. These are signal sanity checks, not
intelligibility or naturalness scores.

## Dynamic and static checks

- GCC AddressSanitizer plus UndefinedBehaviorSanitizer: all four tests passed.
- A sanitizer-enabled harness ran 250,000 randomized iterations. Each iteration
  selected one of Indonesian, Kiswahili, or Spanish, supplied either structured
  multilingual UTF-8 fragments or arbitrary bytes, and periodically rendered
  accepted input. Every third iteration also exercised permissive IPA parsing.
  The run completed without sanitizer diagnostics.
- GCC `-fanalyzer` completed with no diagnostics on all nine runtime translation
  units.
- Strict warning builds passed under GCC and Clang.
- The default internal context measured 2688 bytes on the tested x86-64 ABI,
  within the 3072-byte public allocation. The 128-event compact configuration
  measured exactly 1024 bytes and passed in a 1024-byte public context.

These checks improve confidence in bounds, state handling, and portability but
are not a formal proof of correctness.

## Build-system integration

The three-language build was installed into a clean prefix. A separate consumer
project successfully used:

```cmake
find_package(NanoTTS 0.4 CONFIG REQUIRED)
target_link_libraries(app PRIVATE NanoTTS::NanoTTS)
```

The consumer initialized `NANOTTS_LANG_SPANISH`, rendered non-empty PCM, and
exited successfully. The exported target propagates the public context-size
definitions and the system math dependency when required.

## External separated-IPA checks

Using the locally installed eSpeak 1.48.15 as a process-separated producer:

| Corpus | Producer voice | Lines | Strict IPA failures |
|---|---|---:|---:|
| `tests/data/id_smoke.txt` | `id` | 15 | 0 |
| `tests/data/sw_smoke.txt` | `sw` | 50 | 0 |
| `tests/data/es_smoke.txt` | `es-la` | 54 | 0 |

The Spanish command was equivalent to:

```sh
python3 tools/check_espeak_ipa.py \
  --nanotts build/nanotts --espeak espeak \
  --lang es --voice es-la tests/data/es_smoke.txt
```

This validates the finite input profile against those corpora and that producer
version. It is not a pronunciation-quality comparison and does not guarantee
every symbol emitted by every eSpeak/eSpeak NG version or voice. NanoTTS does
not link to or load the external program.

## Host object-section observations

GCC `-Os` object totals (`text + initialized data`) were:

| Build | Object total | Increment from IPA-only |
|---|---:|---:|
| IPA-only compact | 16,875 B | baseline |
| Kiswahili-only compact | 22,334 B | +5,459 B |
| Indonesian-only compact | 23,175 B | +6,300 B |
| Spanish-only compact | 25,090 B | +8,215 B |
| Indonesian + Kiswahili compact | 27,250 B | +10,375 B |
| Indonesian + Spanish compact | 30,009 B | +13,134 B |
| All languages, no `libm` | 34,066 B | +17,191 B |
| All languages, system `libm` | 33,953 B | +17,078 B |

In the all-language no-`libm` build, the individual text objects measured:

| Object | `text + initialized data` |
|---|---:|
| shared text helpers | 1,248 B |
| Indonesian module | 4,871 B |
| Kiswahili module | 4,026 B |
| Spanish module | 6,783 B |

The Spanish module is larger chiefly because it contains written-stress,
vowel-group, UTF-8 accent, acronym, and number handling. Compiling an additional
language does not add per-context RAM.

These are x86-64 host object measurements, not MCU flash guarantees. Final size
depends on ABI, C runtime, floating-point support, link-time optimization,
section garbage collection, and the target linker map.

## Performance architecture check

A build with two or more text modules performs one integer `switch` in
`nanotts_parse_text`. A build containing exactly one language preprocesses to a
direct parser call. After event generation, all profiles execute the same
phone, 5 ms frame, and sample loops; no language lookup or function-pointer
dispatch occurs in the renderer.

## Audio and intelligibility status

The shared renderer retains the Indonesian acoustic corrections documented in
`docs/INTELLIGIBILITY.md`. The Spanish release sample was verified as mono,
signed 16-bit, 16 kHz PCM, non-silent, unclipped, and 3.209 seconds long.

No formal native-listener transcription study has yet been completed for
Spanish or Kiswahili. The modules have parser, inventory, renderer, corpus, and
external-IPA coverage, but they do not yet have native-listener word-error-rate
or clarity scores. Treat the supplied voice as an experimental compact formant
voice, not a natural or production-validated voice.
