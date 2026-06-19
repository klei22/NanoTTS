# Validation record for NanoTTS 0.3.0

Validation date: 2026-06-19.

## Build and test matrix

Fresh out-of-tree CMake/Ninja builds were tested with the following profiles:

| Compiler/configuration | Text modules | Math path | Events/context/block | Result |
|---|---|---|---|---|
| GCC 14.2 MinSizeRel | `id`, `sw` | system `libm` | 512 / 3072 B / 128 | pass, 4 tests |
| GCC 14.2 MinSizeRel | `id`, `sw` | internal approximations | 512 / 3072 B / 128 | pass, 4 tests |
| Clang 17 MinSizeRel | `id`, `sw` | internal approximations | 512 / 3072 B / 128 | pass, 4 tests |
| GCC 14.2 compact | Indonesian only | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | Kiswahili only | internal approximations | 128 / 1024 B / 64 | pass, 4 tests |
| GCC 14.2 compact | IPA only | internal approximations | 128 / 1024 B / 64 | pass, 3 tests |

The suite covers API validation, compile-time language discovery, unavailable
module behavior, language switching, strict and permissive IPA, malformed
UTF-8, Indonesian and Kiswahili G2P, stress and syllabic-nasal flags, number
expansion, acronym handling, punctuation timing, question contours,
deterministic rendering, every phone class, callback abortion, 8/16/24 kHz
operation, and acoustic level regressions. Assertions remain active in
release-like test builds.

The plain Makefile and `setup.sh` profile wrapper were also exercised for
`all`, `id`, `sw`, and `ipa` configurations.

## Dynamic and static checks

- GCC AddressSanitizer plus UndefinedBehaviorSanitizer: all four tests passed.
- 250,000 randomized byte strings were passed independently to the Indonesian,
  Kiswahili, and permissive IPA parsers; accepted samples were periodically
  rendered under the sanitizer build.
- GCC `-fanalyzer`: no diagnostics on the eight runtime translation units.
- Strict warning builds passed under GCC and Clang.
- The compact profile's internal context is exactly 1024 bytes on the tested
  x86-64 ABI, matching the public context.

These checks improve confidence in bounds and state handling but are not a
formal proof of correctness.

## Build-system integration

The dual-language build was installed into a clean prefix. A separate consumer
project successfully used:

```cmake
find_package(NanoTTS 0.3 CONFIG REQUIRED)
target_link_libraries(app PRIVATE NanoTTS::NanoTTS)
```

The installed consumer initialized the Kiswahili module and rendered non-empty
PCM. The exported target propagates the public context-size definitions and the
system math dependency when required.

## External separated-IPA checks

Using the locally installed eSpeak 1.48.15 as a process-separated producer:

| Corpus | Producer voice | Lines | Strict IPA failures |
|---|---|---:|---:|
| `tests/data/id_smoke.txt` | `id` | 15 | 0 |
| `tests/data/sw_smoke.txt` | `sw` | 50 | 0 |

The command was equivalent to:

```sh
python3 tools/check_espeak_ipa.py \
  --nanotts build/nanotts --espeak espeak --lang sw \
  tests/data/sw_smoke.txt
```

This validates the finite input profile against those corpora and producer
version. It is not a pronunciation-quality claim and does not guarantee every
symbol emitted by every eSpeak or eSpeak NG release. NanoTTS neither links nor
loads the external program.

## Host object-section observations

GCC `-Os` archive-member section totals (`text + initialized data`) were:

| Build | Object total | Incremental language code |
|---|---:|---:|
| IPA-only compact | 16,288 B | baseline |
| Kiswahili-only compact | 21,747 B | +5,459 B including shared text helpers |
| Indonesian-only compact | 22,588 B | +6,300 B including shared text helpers |
| Dual-language, no `libm` | 26,673 B | +4,085 B over Indonesian-only |
| Dual-language, system `libm` | 26,560 B | — |

Individual `-Os` text/data observations were approximately 1,248 B for shared
language helpers, 4,026 B for the Kiswahili module, and 4,871 B for the
Indonesian module. The one-time language metadata/dispatch code is included in
the totals.

The default internal context was 2,688 B with a 3,072 B public allocation. All
128-event compact text and IPA profiles used a 1,024 B internal and public
context. Compiling another language does not add per-context RAM.

These are x86-64 host object measurements, not MCU flash guarantees. Final size
depends on ABI, C runtime, floating-point support, link-time optimization,
section garbage collection, and the target linker map.

## Performance architecture check

The dual-language text path performs one integer `switch` in
`nanotts_parse_text`. A build containing exactly one language preprocesses to a
direct call to that module. After events are produced, all profiles execute the
same phone, 5 ms frame, and sample loops; no language lookup or function-pointer
dispatch occurs in the renderer.

## Audio and intelligibility status

The shared renderer retains the 0.2 Indonesian acoustic corrections documented
in `docs/INTELLIGIBILITY.md`, including removal of the original severe
fricative/vowel imbalance. Both NanoTTS 0.3 sample WAVs were verified as mono,
16-bit, 16 kHz PCM and non-silent.

No formal native-listener transcription study has yet been completed for either
release language. The Kiswahili module has parser, inventory, renderer, corpus,
and external-IPA coverage, but it does not yet have a native-listener word-error
rate or clarity score. Treat the supplied voice as an experimental compact
formant voice, not a natural or production-validated voice.
