# Supported languages

NanoTTS 0.4 contains three optional text modules. Each module normalizes UTF-8
text into the same compact phone-event vocabulary and then leaves all timing,
prosody, synthesis, and PCM delivery to the shared renderer.

| Code | Public selector | Build option | Module |
|---|---|---|---|
| `id` | `NANOTTS_LANG_INDONESIAN` | `NANOTTS_ENABLE_LANG_ID` | `nanotts_lang_id.c` |
| `sw` | `NANOTTS_LANG_KISWAHILI` | `NANOTTS_ENABLE_LANG_SW` | `nanotts_lang_sw.c` |
| `es` | `NANOTTS_LANG_SPANISH` | `NANOTTS_ENABLE_LANG_ES` | `nanotts_lang_es.c` |

`NANOTTS_LANG_SWAHILI` aliases `NANOTTS_LANG_KISWAHILI`.
`NANOTTS_LANG_SPANISH_LATIN_AMERICAN` aliases `NANOTTS_LANG_SPANISH` to make
the current Spanish dialect policy explicit. CLI and wire codes remain the
ISO 639-1 forms `id`, `sw`, and `es`.

## Selecting a language

Text parsing requires a selection before the call:

```c
nanotts_init(&tts, 16000u, NANOTTS_LANG_INDONESIAN);
nanotts_speak_text(&tts, "selamat pagi", NANOTTS_NPOS,
                   &options, write_pcm, user, &info);

nanotts_set_language(&tts, NANOTTS_LANG_KISWAHILI);
nanotts_speak_text(&tts, "habari yako", NANOTTS_NPOS,
                   &options, write_pcm, user, &info);

nanotts_set_language(&tts, NANOTTS_LANG_SPANISH);
nanotts_speak_text(&tts, "hola, buenos días", NANOTTS_NPOS,
                   &options, write_pcm, user, &info);
```

The CLI makes the selection explicit:

```sh
nanotts --lang id --text "terima kasih" -o id.wav
nanotts --lang sw --text "asante sana" -o sw.wav
nanotts --lang es --text "el sistema está listo" -o es.wav
```

IPA input does not require a text language. Use `NANOTTS_LANG_NONE` or omit
`--lang` with `--ipa` and `--ipa-file`.

Available-module discovery is static and allocation-free:

```c
for (size_t i = 0; i < nanotts_compiled_language_count(); ++i) {
    nanotts_language_t lang = nanotts_compiled_language_at(i);
    printf("%s: %s\n", nanotts_language_code(lang),
           nanotts_language_name(lang));
}
```

## Indonesian (`id`)

The Indonesian module includes:

- regular Latin-letter mappings;
- `ng`, `ny`, `sy`, and `kh`;
- `c` as the voiceless affricate and `j` as the voiced affricate;
- `ai`, `au`, and `oi` diphthong events;
- a small independently authored aid for ambiguous plain `e`;
- explicit `é`, `è`, and `ê` controls;
- integer expansion through the unsigned 32-bit range;
- conservative spelling of likely all-uppercase acronyms;
- punctuation boundaries and question flags.

Principal limitations are lexical choice among `/e/`, `/ɛ/`, and schwa;
diphthong versus hiatus; final glottal realization; and foreign names or
code-switching. Explicit IPA is authoritative when pronunciation matters.

## Kiswahili (`sw`)

The Kiswahili module targets compact Standard Kiswahili and includes:

- five stable written vowels, with adjacent vowels retained as separate nuclei;
- automatic primary stress on the penultimate vowel nucleus;
- `ch`, `sh`, `ny`, `th`, `dh`, `gh`, and `kh`;
- `ng'`/`ng’` as the velar nasal and plain `ng` as nasal plus voiced velar stop;
- common prenasalized sequences such as `mb`, `nd`, and `nj`;
- `c`, `q`, and `x` fallback mappings for names and loanwords;
- integer expansion using Kiswahili number words;
- conservative spelling of likely all-uppercase acronyms;
- punctuation boundaries and question flags.

The parser deliberately does not collapse adjacent vowels into diphthong
phones. Initial syllabic nasals in compact forms such as `mti`, `mtu`, `nchi`,
and `mbwa` are marked with `NANOTTS_EVENT_SYLLABIC`.

The shared acoustic inventory provides compact phones for `/θ/`, `/ð/`, and
`/ɣ/`. Written `j` uses the renderer's voiced affricate. Regional variants,
implosives, and every Arabic-loan realization are outside this small module.

## Spanish (`es`)

The Spanish module targets a neutral, broadly useful Latin-American-style
pronunciation. Its deliberate defaults are:

- **seseo:** `z` and soft `c` use `/s/` rather than `/θ/`;
- **yeísmo:** `ll` and consonantal `y` share the renderer's palatal glide;
- five stable vowel qualities;
- written acute accents override automatic stress;
- unaccented words normally stress the penultimate nucleus when ending in a
  vowel, or in `n`/`s` after a vowel; other endings use the final nucleus;
- unaccented weak vowels form glides beside a strong vowel, while accented
  `í` and `ú` force hiatus;
- `ai`, `au`, and `oi` use the renderer's smooth diphthong events where
  applicable;
- `ch`, `ll`, `rr`, `ñ`, silent `h`, `qu`, `gu`, and `gü` handling;
- hard/soft `c` and `g`, `j → /x/`, and conservative `x` behavior;
- taps versus trills for ordinary `r` and `rr`/strong `r`;
- lightweight within-word `/b~β/`, `/d~ð/`, and `/g~ɣ/` allophony;
- integer expansion through the unsigned 32-bit range;
- Spanish letter-name expansion for likely all-uppercase acronyms;
- inverted punctuation acceptance, phrase boundaries, and question flags.

Examples:

```text
queso      → /keso/
guitarra   → /giˈtara/       (`rr` uses the trill event)
pingüino   → /pingwino/
niño       → /niɲo/
caro       → tap
carro      → trill
país       → two nuclei, stress on í
hoy        → /oi/
```

The module does not attempt every regional realization. It does not provide
Castilian `c/z → /θ/`, lleísmo, Caribbean final-`s` aspiration, voseo-specific
prosody, or a proper-name dictionary. Some learned or foreign words require
lexical knowledge for `x`, hiatus, or stress. Use explicit IPA for those cases.

## Build-time selection

All modules are enabled by default:

```sh
cmake -S . -B build
```

A one-language build excludes the other parsers and their normalization data:

```sh
cmake -S . -B build-id \
  -DNANOTTS_ENABLE_LANG_ID=ON \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=OFF

cmake -S . -B build-sw \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=ON \
  -DNANOTTS_ENABLE_LANG_ES=OFF

cmake -S . -B build-es \
  -DNANOTTS_ENABLE_LANG_ID=OFF \
  -DNANOTTS_ENABLE_LANG_SW=OFF \
  -DNANOTTS_ENABLE_LANG_ES=ON
```

Any subset may be selected. An IPA-only build excludes the common text helpers
and every G2P module:

```sh
cmake -S . -B build-ipa -DNANOTTS_ENABLE_TEXT_FRONTEND=OFF
```

Language selection adds no work to synthesis. A multi-language build performs
one switch when text is parsed. A single-language build preprocesses to a
direct parser call. Phone scheduling, formant updates, and sample generation
contain no language branch.

## External IPA producers

NanoTTS accepts separated IPA from another process:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi" |
  nanotts --ipa-file - -o id.wav

espeak-ng -q -v sw --ipa=1 --sep=_ "habari yako" |
  nanotts --ipa-file - -o sw.wav

espeak-ng -q -v es-la --ipa=1 --sep=_ "hola, buenos días" |
  nanotts --ipa-file - -o es.wav
```

External IPA is useful for names, regional variants, or words outside the
compact built-in rules. No external TTS engine is required on the MCU.

## Linguistic scope references

The modules are independently implemented from descriptive language sources,
not copied from another TTS system.

Kiswahili references:

- LDC, *Language Specific Peculiarities Document for Swahili as Spoken in
  Kenya*: https://catalog.ldc.upenn.edu/docs/LDC2017S05/LSP_202_final.pdf
- Kentalis, *Swahili*: https://www.kentalis.nl/file-download/download/public/2108

Spanish scope references:

- Real Academia Española, descriptions of Spanish stress and written accents:
  https://www.rae.es/ortograf%C3%ADa/acentuaci%C3%B3n-gr%C3%A1fica
- Real Academia Española, `seseo`:
  https://www.rae.es/dpd/seseo
- Real Academia Española, `yeísmo`:
  https://www.rae.es/dpd/ye%C3%ADsmo

These references describe languages. No source code, rule table, lexicon,
recording, or numeric acoustic data was imported from them.
