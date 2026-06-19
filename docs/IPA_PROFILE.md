# `espeak-id-ipa/1` input profile

This document defines the UTF-8 IPA subset accepted by `idtts` 0.1. The profile
is intentionally finite. A small embedded parser is safer and easier to test
when unsupported symbols are rejected rather than guessed.

## Recommended eSpeak command

```sh
espeak-ng -q -v id --ipa=1 --sep=_ "selamat pagi"
```

A typical result is:

```text
s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i
```

`_` separates phones, a regular space separates words, and `ˈ` marks primary
stress on the following vowel nucleus. Tied and untied affricates are accepted.

## Phones

| Class | Canonical symbols | Accepted aliases |
|---|---|---|
| vowels | `a e ə ɛ i ɪ o ɔ u ʊ` | `ɐ→a`, `ɜ ɝ→ə` |
| diphthongs | `aɪ aʊ oɪ` | optional following non-syllabic mark |
| stops | `p b t d k ɡ ʔ` | `g ɣ→ɡ`, `q→k` |
| affricates | `tʃ dʒ` | `t͡ʃ`, `t͜ʃ`, `t‍ʃ`, `d͡ʒ`, `d͜ʒ`, `d‍ʒ`; `ç` may follow `t` |
| fricatives | `f v s z ʃ x h` | `ɸ→f`, `ç ɕ→ʃ`, `χ→x`, `ɦ→h` |
| nasals | `m n ɲ ŋ` | `ń→ɲ` |
| liquids | `l r ɾ` | `ʀ→r` |
| glides | `w j` | `ʋ→w`, `ɥ→j` |

A standalone `ʒ` or `ɟ` is normalized to the internal voiced affricate phone.
This is an intentional Indonesian/eSpeak compatibility approximation, not a
claim that the symbols are universally equivalent.

## Formatting and control marks

- `_`, `·`, `‧`, word joiner, and BOM are ignored as phone separators.
- Tie bars `͡`, `͜`, and zero-width joiner are accepted in affricates.
- `ˈ` applies primary stress to the next vowel or diphthong.
- `ˌ` applies secondary stress unless a pending primary mark supersedes it.
- `ː` lengthens the preceding phone to 150 percent.
- `ˑ` lengthens the preceding phone to 125 percent.
- Combining non-syllabic marks U+032F and U+0311 are accepted and ignored after
  diphthong recognition.
- Variation selectors and combining grapheme joiner are ignored.

The parser does not run a general NFC/NFD normalizer. Producers should emit
precomposed symbols from the table above.

## Boundaries

By default:

- spaces and tabs become short pauses and mark the preceding event as a word
  end;
- comma, en dash, and slash become short pauses;
- period, exclamation mark, question mark, semicolon, colon, newline, carriage
  return, and em dash become phrase pauses; a question mark also sets the
  phrase's question flag for `IDTTS_FINAL_AUTO`;
- repeated boundaries collapse, with a phrase pause taking precedence over a
  short pause;
- leading and trailing short pauses are removed.

Set `IDTTS_OPT_NO_AUTOPAUSE` to make whitespace act only as a separator. Explicit
punctuation still creates pauses.

## Strict and permissive behavior

Strict mode is the default. An unsupported code point returns
`IDTTS_ERR_UNKNOWN_IPA`; `idtts_parse_info_t.error_byte` and
`error_codepoint` identify the first rejected symbol. Invalid UTF-8 returns
`IDTTS_ERR_UTF8`.

`IDTTS_OPT_PERMISSIVE_IPA` skips unsupported symbols. It is useful for
exploration, but strict mode is preferred in production because silent symbol
loss can hide pronunciation errors.

## Event normalization examples

```text
s_ə_l_ˈa_m_a_t  → s ə l a(primary) m a t
ɲ_ˈa_ɲ_i         → ny a(primary) ny i
t͡ʃ_a             → ch a
dʒ_a              → j a
b_ˈaɪ_k            → b ai(primary) k
p_ˈu_l_aʊ          → p u(primary) l au
```

## Versioning

The profile name is `espeak-id-ipa/1`. Minor parser releases may add aliases
that map unambiguously to an existing internal phone. Removing a symbol,
changing a mapping, or changing boundary semantics requires a new profile
version.
