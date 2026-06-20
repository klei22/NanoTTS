# Text-language modules

NanoTTS text modules are deliberately small orthography-to-event frontends.
They normalize UTF-8 text, choose common phone IDs, mark lexical prominence and
phrase boundaries, and expand a bounded set of numbers/acronyms. They do not
contain an acoustic renderer or audio driver.

The modules share one voice and one strict IPA override path. They are useful
for ordinary embedded prompts, but they are not full linguistic dictionaries.
Names, code-switching, uncommon loanwords, regional variants, and lexical
ambiguities may require explicit IPA.

## Build and selection table

| Code | Public ID | CMake option | Setup example |
|---|---|---|---|
| `id` | `NANOTTS_LANG_INDONESIAN` | `NANOTTS_ENABLE_LANG_ID` | `--languages id` |
| `sw` | `NANOTTS_LANG_KISWAHILI` | `NANOTTS_ENABLE_LANG_SW` | `--languages sw` |
| `es` | `NANOTTS_LANG_SPANISH` | `NANOTTS_ENABLE_LANG_ES` | `--languages es` |
| `ms` | `NANOTTS_LANG_MALAY` | `NANOTTS_ENABLE_LANG_MS` | `--languages ms` |
| `mi` | `NANOTTS_LANG_MAORI` | `NANOTTS_ENABLE_LANG_MI` | `--languages mi` |
| `haw` | `NANOTTS_LANG_HAWAIIAN` | `NANOTTS_ENABLE_LANG_HAW` | `--languages haw` |

Language IDs are stable append-only values declared through
`include/nanotts/nanotts_languages.def`. Applications should store canonical
language codes rather than the position returned by
`nanotts_compiled_language_at`.

## Indonesian — `id`

Implemented scope:

- Latin-script Indonesian text.
- `ng`, `ny`, `sy`, and `kh`.
- `c` as the shared `/tʃ/` phone and `j` as `/dʒ/`.
- `ai`, `au`, and `oi` diphthong events in common positions.
- A small independently authored table for common plain-`e` choices.
- Explicit `é`, `è`, and `ê` overrides.
- Indonesian cardinal integers, common acronym behavior, and punctuation.
- Mild language-specific timing and intonation profile.

Limitations:

- Plain `e` remains lexically ambiguous outside the bounded table.
- Some final `k` realizations and regional allophones need IPA.
- Foreign names and code-switching are not dictionary-backed.

## Kiswahili — `sw`

Implemented scope:

- Five-vowel Latin orthography.
- `ch`, `sh`, `ny`, `th`, `dh`, `gh`, and `kh`.
- `ng'`/`ng’` as `/ŋ/` and plain `ng` as `/ŋɡ/`.
- Common prenasalized sequences and compact syllabic-initial-nasal handling.
- Adjacent vowels remain separate events.
- Usual penultimate prominence.
- Kiswahili cardinal integers, acronyms, and punctuation.

Limitations:

- Arabic loans, dialectal variants, proper names, and implosive distinctions
  are not exhaustively represented.
- The shared renderer approximates several prenasalized clusters as event
  sequences rather than dedicated acoustic units.

## Spanish — `es`

The current module intentionally models a neutral Latin-American-style
baseline rather than every Spanish dialect.

Implemented scope:

- Five-vowel system, written accents, default stress, and common decomposed
  Unicode accent sequences.
- Seseo and yeísmo.
- `ch`, `ll`, `rr`, `ñ`, silent `h`, `qu`, `gu`, and `gü`.
- Contextual hard/soft `c` and `g`.
- Tap versus trill.
- Common weak-vowel groups, diphthongs, and accented hiatus.
- Lightweight within-word `/b~β/`, `/d~ð/`, and `/g~ɣ/` allophony.
- Spanish cardinal integers, acronyms, and inverted punctuation.

Limitations:

- Castilian `/θ/` for `c/z`, lleísmo, regional `s` aspiration, and dialectal
  prosody are not selected automatically.
- Lexical stress exceptions without a written accent may need IPA.
- Proper names and foreign words are not dictionary-backed.

`NANOTTS_LANG_SPANISH_LATIN_AMERICAN` is a source alias for the same module.

## Malay — `ms`

Implemented scope:

- Latin-script Malay text.
- Digraphs `ng`, `ny`, `sy`, `kh`, and `gh`.
- Diphthongs `ai`, `au`, and `oi`.
- `c` as `/tʃ/`, `j` as `/dʒ/`, and common fallback letters for names.
- Plain `e` defaults to schwa with a small independently authored exception
  table for common close/open realizations.
- Explicit `é` for close `/e/`, `è` for open `/ɛ/`, and `ê` for schwa.
- Penultimate prominence, Malay cardinal integers, acronyms, and punctuation.

Limitations:

- The two pronunciations traditionally written with `e` are lexical; the small
  exception set cannot cover every word.
- Malaysian, Bruneian, Singaporean, and other regional pronunciation details
  are not independently selectable.
- Arabic and English loans may require IPA.

`NANOTTS_LANG_BAHASA_MELAYU` is a source alias for the same module.

## Māori — `mi`

Implemented scope:

- The five short vowels and precomposed or decomposed macrons.
- `ng` as `/ŋ/`.
- `wh` as the common `/f/`-like default.
- `r` as a tap.
- Common Māori vowel groups; `ai`, `au`, and `oi` reuse compact moving-vowel
  events, while other groups are represented by paired vowel events.
- A compact prominence rule: first long vowel, otherwise first recognized
  vowel group, otherwise first vowel; long words may receive secondary stress.
- Digit-by-digit Māori number names and punctuation.

Limitations:

- Iwi-specific `wh`, `r`, vowel, and prosodic variation is not modeled.
- Compounds and morphologically complex words can have lexical stress not
  recoverable from spelling alone.
- Number handling is digit-by-digit rather than full Māori cardinal grammar.
- Loanword consonants are accepted through pragmatic fallbacks, not a lexicon.

`NANOTTS_LANG_TE_REO_MAORI` is a source alias for the same module.

## Hawaiian — `haw`

Implemented scope:

- Five vowels and precomposed or decomposed kahakō/macrons.
- U+02BB ʻokina as a glottal stop, plus several common input approximations.
- Common Hawaiian vowel groups; `ai`, `au`, and `oi` reuse compact moving-vowel
  events and other groups use paired events.
- Heavy long vowels and diphthongs receive prominence; otherwise the compact
  rule selects the penultimate mora.
- Digit-by-digit Hawaiian number names and punctuation.
- A language-specific slower vowel and question-contour profile.

Limitations:

- Regional and lexical `w`/`v`, `k`/`t`, and other pronunciation variation is
  not inferred.
- Compound stress and lexical exceptions may require IPA.
- Number handling is digit-by-digit rather than full Hawaiian cardinal grammar.
- ASCII apostrophe is accepted as a convenience, but U+02BB is the canonical
  ʻokina for new text.

`NANOTTS_LANG_OLELO_HAWAII` is a source alias for the same module.

## Shared Polynesian implementation

Māori and Hawaiian use a shared removable source file for UTF-8 normalization,
macron handling, bounded vowel-nucleus analysis, digit spelling, punctuation,
and event emission. The selected dialect is resolved once when parsing a word.
This sharing reduces flash in a dual `mi,haw` build; it does not add a branch to
the renderer or sample loop.

## Per-language prosody

Each compiled module contributes one small immutable
`nanotts_prosody_profile_t`. It controls:

- base pitch;
- vowel and consonant duration scales;
- word and phrase pause scales;
- primary and secondary stress duration/pitch;
- fall, rise, and continuation contours.

The profile is selected once at the start of `nanotts_render_events`. A disabled
language's profile is not instantiated, and querying it returns the neutral IPA
profile. Pronunciation decisions remain in the text module; the profile cannot
fix an incorrect phone sequence.

## Choosing text versus IPA

Use text mode for ordinary prompts whose spelling is regular:

```sh
nanotts --lang ms --text "bateri hampir habis" -o warning.wav
```

Use explicit IPA when exact pronunciation matters:

```sh
nanotts --ipa "b_a_t_ə_r_ˈaɪ h_ˈa_m_p_i_r h_a_b_ˈi_s" -o warning.wav
```

For critical alarms, names, or public-facing language, validate the output with
native speakers and keep an explicit IPA spelling for any word that the compact
frontend does not render reliably.
