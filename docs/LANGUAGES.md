# Supported languages

NanoTTS 0.3 contains two optional text modules. They normalize UTF-8 text and
emit a shared phone-event vocabulary; both use the same renderer and public
API.

| Code | Public selector | Build option | Module |
|---|---|---|---|
| `id` | `NANOTTS_LANG_INDONESIAN` | `NANOTTS_ENABLE_LANG_ID` | `nanotts_lang_id.c` |
| `sw` | `NANOTTS_LANG_KISWAHILI` | `NANOTTS_ENABLE_LANG_SW` | `nanotts_lang_sw.c` |

`NANOTTS_LANG_SWAHILI` is retained as an alias of
`NANOTTS_LANG_KISWAHILI`. The wire/CLI language code is the ISO 639-1 code
`sw`.

## Selecting a language

Text parsing requires a selection before the call:

```c
nanotts_init(&tts, 16000u, NANOTTS_LANG_INDONESIAN);
nanotts_speak_text(&tts, "selamat pagi", NANOTTS_NPOS,
                   &options, write_pcm, user, &info);

nanotts_set_language(&tts, NANOTTS_LANG_KISWAHILI);
nanotts_speak_text(&tts, "habari yako", NANOTTS_NPOS,
                   &options, write_pcm, user, &info);
```

The CLI makes this explicit:

```sh
nanotts --lang id --text "terima kasih" -o id.wav
nanotts --lang sw --text "asante sana" -o sw.wav
```

IPA input does not require a text language. Use `NANOTTS_LANG_NONE` or omit
`--lang` with `--ipa`/`--ipa-file`.

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
- integer expansion through the 32-bit unsigned range;
- conservative spelling of likely all-uppercase acronyms;
- punctuation boundaries and question flags.

Principal limitations are lexical choice among `/e/`, `/ɛ/`, and schwa;
diphthong versus hiatus; final glottal realization; and foreign names or
code-switching. Explicit IPA is authoritative when pronunciation matters.

## Kiswahili (`sw`)

The Kiswahili module is based on standard written Kiswahili and includes:

- five stable written vowels, with adjacent vowels retained as separate nuclei;
- automatic primary stress on the penultimate vowel nucleus;
- `ch`, `sh`, `ny`, `th`, `dh`, `gh`, and `kh`;
- `ng'`/`ng’` as the velar nasal and `ng` as nasal plus voiced velar stop;
- common prenasalized sequences such as `mb`, `nd`, and `nj`;
- `c`, `q`, and `x` fallback mappings for names and loanwords;
- integer expansion using Kiswahili number words;
- conservative spelling of likely all-uppercase acronyms;
- punctuation boundaries and question flags.

The parser deliberately does not collapse adjacent vowels into diphthong
phones. For example, `mayai` retains separate vowel nuclei and receives stress
on the penultimate nucleus. Initial syllabic nasals in compact forms such as
`mti`, `mtu`, `nchi`, and `mbwa` are marked with `NANOTTS_EVENT_SYLLABIC`;
explicit IPA also accepts the combining syllabic mark.

The shared acoustic inventory provides dedicated compact phones for `/θ/`,
`/ð/`, and `/ɣ/`. It approximates the written `j` with the renderer's voiced
affricate and does not model every dialectal or Arabic-loan realization.
Implosive variants and speaker-specific timing are also outside the current
small voice.

## Build-time selection

Dual-language is the default:

```sh
cmake -S . -B build
```

One-language builds exclude the other module's code and word tables:

```sh
cmake -S . -B build-id -DNANOTTS_ENABLE_LANG_SW=OFF
cmake -S . -B build-sw -DNANOTTS_ENABLE_LANG_ID=OFF
```

An IPA-only build excludes the shared text helpers and every G2P module:

```sh
cmake -S . -B build-ipa -DNANOTTS_ENABLE_TEXT_FRONTEND=OFF
```

Language selection adds no work to synthesis. In a dual-language build, one
switch selects the text parser once per utterance. A single-language build
preprocesses to a direct parser call. Phone scheduling, formant updates, and
sample generation contain no language branch.

## External IPA producers

NanoTTS can accept separated IPA from another process:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi" |
  nanotts --ipa-file - -o id.wav

espeak-ng -q -v sw --ipa=1 --sep=_ "habari yako" |
  nanotts --ipa-file - -o sw.wav
```

External IPA is useful for names or words outside the compact built-in rules.
No external TTS engine is required on the MCU.


## Linguistic scope references

The Kiswahili rules are independently implemented from descriptive language
sources, not copied from another TTS system. The principal references are:

- LDC, *Language Specific Peculiarities Document for Swahili as Spoken in
  Kenya*: https://catalog.ldc.upenn.edu/docs/LDC2017S05/LSP_202_final.pdf
- Kentalis, *Swahili*: https://www.kentalis.nl/file-download/download/public/2108

The LDC document supplies the standard orthographic correspondences, five-vowel
inventory, syllabic nasal examples, and usual penultimate stress pattern. The
Kentalis guide independently notes that successive vowels form separate
syllables and that a nasal can be a syllable nucleus. NanoTTS deliberately
chooses a compact Standard-Kiswahili scope; regional, speaker, and loanword
variation is documented as a limitation rather than guessed silently.
