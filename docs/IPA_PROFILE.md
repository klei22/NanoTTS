# `nanotts-ipa/1` input profile

This document defines the finite UTF-8 IPA subset accepted by NanoTTS 0.4. It
covers the current Indonesian, Kiswahili, and Spanish phone inventory and
selected aliases used by separated external IPA output. It is intentionally not a general IPA
engine: strict rejection is smaller and safer than guessing on an MCU.

## Recommended external format

Use an explicit phone separator:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi"
espeak-ng -q -v sw --ipa=1 --sep=_ "habari yako"
espeak-ng -q -v es-la --ipa=1 --sep=_ "hola, buenos días"
```

Representative output:

```text
s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i
h_a_b_ˈa_r_i j_ˈa_k_o
```

`_` separates phones, a normal space separates words, and `ˈ` marks primary
stress on the following vowel nucleus. Tied and untied affricates are accepted.

## Phones and aliases

| Class | Canonical symbols | Accepted aliases or approximations |
|---|---|---|
| vowels | `a e ə ɛ i ɪ o ɔ u ʊ` | `ɐ→a`, `ɜ ɝ→ə` |
| diphthongs | `aɪ aʊ oɪ` | optional following non-syllabic mark |
| stops | `p b t d k ɡ ʔ` | `ɓ→b`, `ɗ→d`, `g ɠ→ɡ`, `q→k` |
| affricates | `tʃ dʒ` | tied forms; `ʧ→tʃ`; `ʤ ɟ ʒ→dʒ` approximation |
| fricatives | `f v β s z θ ð ʃ x ɣ h` | `ɸ→f`, `ç ɕ→ʃ`, `χ→x`, `ɦ→h` |
| nasals | `m n ɲ ŋ` | `ń→ɲ` |
| liquids | `l r ɾ` | `ʀ→r` |
| glides | `w j` | `ʋ→w`, `ɥ ʝ ʎ→j` approximations |

The Spanish-oriented `β` phone is distinct from stop `b`. The mappings for
standalone `ɟ`, `ʒ`, and the implosive symbols are deliberate small-renderer
approximations. They support common producer output; they are not
claims of universal phonetic equivalence.

## Multi-code-point recognition

The parser greedily recognizes:

```text
tʃ  t͡ʃ  t͜ʃ  t‍ʃ
dʒ  d͡ʒ  d͜ʒ  d‍ʒ
aɪ  aʊ  oɪ
```

The diphthong rule requires the IPA near-close symbols `ɪ` or `ʊ`. ASCII `ai`,
`au`, and `oi` remain two vowels unless a text language module emits a
diphthong event.

## Formatting and control marks

- `_`, `·`, `‧`, word joiner, and BOM are ignored as separators.
- Tie bars U+0361 and U+035C and zero-width joiner are accepted in affricates.
- `ˈ` applies primary stress to the next vowel or diphthong.
- `ˌ` applies secondary stress unless pending primary stress supersedes it.
- `ː` lengthens the preceding phone to 150 percent.
- `ˑ` lengthens the preceding phone to 125 percent.
- Combining non-syllabic marks U+032F and U+0311 are ignored after phone
  recognition.
- The syllabic mark U+0329 attaches `NANOTTS_EVENT_SYLLABIC` to a preceding
  `m`, `n`, or `ŋ`, lengthens it modestly, and accepts pending stress; this
  supports forms such as `ˈm̩`, `ˈn̩`, and `ˈŋ̍`.
- Combining grapheme joiner and text/emoji variation selectors are ignored as
  harmless profile modifiers.

The parser does not implement general Unicode normalization. Producers should
emit precomposed symbols from the table above.

## Boundaries

By default:

- spaces, tabs, narrow spaces, and non-breaking spaces become short pauses and
  mark the preceding event as a word end;
- comma, en dash, and slash become longer short pauses;
- period, exclamation mark, question mark, semicolon, colon, newline, carriage
  return, and em dash become phrase pauses;
- a question mark adds `NANOTTS_EVENT_QUESTION` for automatic rising contour;
- repeated boundaries collapse, with a phrase pause taking precedence;
- leading pauses and trailing short pauses are removed.

Set `NANOTTS_OPT_NO_AUTOPAUSE` to make whitespace a separator only. Explicit
punctuation still creates pauses.

## Strict and permissive behavior

Strict mode is the default. An unsupported code point returns
`NANOTTS_ERR_UNKNOWN_IPA`; `nanotts_parse_info_t.error_byte` and
`error_codepoint` identify the first rejected symbol. Invalid UTF-8 returns
`NANOTTS_ERR_UTF8`.

`NANOTTS_OPT_PERMISSIVE_IPA` skips unsupported symbols. It is useful for
exploration, but strict mode is preferred in production because silent symbol
loss can hide pronunciation errors.

## Normalization examples

```text
s_ə_l_ˈa_m_a_t  → s ə l a(primary) m a t
ɲ_ˈa_ɲ_i         → ny a(primary) ny i
t͡ʃ_a             → ch a
dʒ_a              → j a
θ_e_l_a_θ_ˈi_n_i → th e l a th i(primary) n i
ɣ_ˈa_l_i          → gh a(primary) l i
l_a_β_ˈi_ð_a      → l a beta i(primary) dh a
ˈm̩_t_i            → m(syllabic, primary) t i
b_ˈaɪ_k            → b ai(primary) k
```

## Versioning

The profile name is `nanotts-ipa/1`. Minor releases may add aliases that map
unambiguously to an existing internal phone. Removing a symbol, changing a
mapping, or changing boundary semantics requires a new profile version.

The former Indonesian-only documentation name `espeak-id-ipa/1` is superseded
by this language-neutral name; the Indonesian symbols accepted by 0.2 remain
supported.
