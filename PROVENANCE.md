# Clean-room provenance

## Runtime implementation

All C source, public headers, tests, build files, documentation, and numeric
voice/prosody tables shipped in this repository were newly authored for
NanoTTS in 2026 and are offered under the MIT license in `LICENSE`.

The repository does **not** contain or derive from source code, pronunciation
rules, dictionaries, voice tables, recorded units, or other runtime assets
from:

- eSpeak or eSpeak NG;
- SAM / Software Automatic Mouth or its ports;
- Flite/Festival;
- Klatt synthesizer program implementations;
- proprietary or neural TTS engines.

The voice values in `src/nanotts_voice.c` and the language prosody profiles in
`src/nanotts_prosody.c` are original hand-tuned values. They were not copied
from an existing program, generated voice, recording, or language package.

The PWM and PCM adapters implement ordinary numeric format conversion over the
NanoTTS callback. They contain no vendor SDK code. Board-specific timer, DMA,
filtering, and amplifier code remains outside the repository.

## Language-module provenance

The Indonesian, Kiswahili, Spanish, Malay, Māori, and Hawaiian modules are
independently authored, bounded G2P implementations. They do not translate,
extract, or mechanically reproduce another TTS engine's language files.

### Kiswahili

The scope was informed by descriptive linguistic references including:

- LDC, *Language Specific Peculiarities Document for Swahili as Spoken in
  Kenya*: https://catalog.ldc.upenn.edu/docs/LDC2017S05/LSP_202_final.pdf
- Kentalis, *Swahili* phonology guide:
  https://www.kentalis.nl/file-download/download/public/2108

These references describe orthography, vowels, common digraphs, syllabic
nasals, and usual prominence. No source code, rule table, lexicon, recording,
or numeric acoustic data was imported.

### Spanish

The compact neutral Latin-American-style scope was informed by descriptive
material from the Real Academia Española and Instituto Cervantes:

- RAE, *Pronunciación*:
  https://www.rae.es/libro-estilo-lengua-espa%C3%B1ola/pronunciaci%C3%B3n
- RAE, *Reglas generales de acentuación*:
  https://www.rae.es/ortograf%C3%ADa-b%C3%A1sica/uso-de-la-tilde/las-reglas-de-acentuaci%C3%B3n-gr%C3%A1fica/reglas-generales
- RAE, *Los fonemas del español*:
  https://www.rae.es/ortograf%C3%ADa-b%C3%A1sica/uso-de-las-letras/los-fonemas-del-espa%C3%B1ol
- Instituto Cervantes, pronunciation inventory:
  https://cvc.cervantes.es/ensenanza/biblioteca_ele/plan_curricular/niveles/03_pronunciacion_inventario_a1-a2.htm

No prose, code, pronunciation table, lexicon, recording, or numeric acoustic
data was copied into NanoTTS.

### Malay

The Malay module was independently written from general descriptions of Rumi
orthography and pronunciation. Public material consulted included:

- Dewan Bahasa dan Pustaka PRPM, explanations of Malay digraphs and vowel
  distinctions: https://prpm.dbp.gov.my/
- Universiti Sains Malaysia, descriptive work on Malay grapheme-to-phoneme
  correspondence and the written digraph/diphthong inventory:
  https://eprints.usm.my/37711/

The small `e` aid and all normalization/number code were authored specifically
for NanoTTS. No DBP word list, pronunciation dictionary, TTS language file,
recording, or generated voice data was imported.

### Māori

The Māori module was independently authored from public descriptions of the
five vowels, vowel length, macrons, `ng`, `wh`, vowel sequences, and practical
prominence guidance. Material consulted included:

- Te Taura Whiri i te Reo Māori, *Guidelines for Māori Language Orthography*:
  https://www.tetaurawhiri.govt.nz/assets/Uploads/Corporate-docs/Orthographic-conventions/58e52e80e9/Guidelines-for-Maori-Language-Orthography.pdf
- Te Herenga Waka—Victoria University of Wellington, pronunciation guide:
  https://www.wgtn.ac.nz/maori-hub/rauemi/te-reo-at-university/maori-pronunciation
- University of Hawaiʻi Press, *Pronouncing Māori*:
  https://www.hawaii.edu/uhpress/Assets/PDF/9780824842369.pdf

The bounded vowel-group and prominence algorithm is an original engineering
approximation, not a copied linguistic table or TTS implementation.

### Hawaiian

The Hawaiian module was independently authored from public descriptions of the
ʻokina, kahakō, vowel length, diphthongs, moraic weight, and pronunciation
variation. Material consulted included:

- University of Hawaiʻi, Hawaiian language considerations and diacritics:
  https://www.hawaii.edu/offices/communications/standards/hawaiian-language-considerations/
- University of Hawaiʻi Press, *Hawaiian Dictionary* pronunciation notes:
  https://www.hawaii.edu/uhpress/Assets/PDF/9780824807030.pdf
- University of Hawaiʻi linguistic descriptions of Hawaiian stress and heavy
  syllables, used only as general design references.

The compact mora/prominence logic and accepted input aliases were authored for
NanoTTS. No dictionary, corpus, recorded voice, or preexisting TTS rule set was
imported.

### Indonesian

The Indonesian module remains an independently authored compact frontend. Its
bounded plain-`e` aid, number expansion, grapheme rules, and test data are
project-authored; no eSpeak Indonesian rules/list files were used.

## Test corpora

Files under `tests/data/` are short, independently selected words and phrases
chosen to exercise implemented parser behavior. They are not copied
pronunciation dictionaries and are not distributed as voice data.

## Role of eSpeak

eSpeak is an optional external interoperability tool. During development it
may be executed as a black-box IPA producer:

```sh
espeak -q -v id --ipa=1 --sep=_ TEXT
espeak -q -v sw --ipa=1 --sep=_ TEXT
espeak -q -v es-la --ipa=1 --sep=_ TEXT
espeak -q -v ms --ipa=1 --sep=_ TEXT
```

Its UTF-8 stdout can be passed through a process boundary to NanoTTS's
MIT-licensed parser. NanoTTS does not link to eSpeak, load its data files, call
its API, or require it at runtime. No eSpeak executable, data file,
pronunciation table, voice, or generated audio corpus is redistributed.

Compatibility checks execute a locally installed producer at test time. Their
output is neither committed as language data nor treated as authoritative
pronunciation.

## Scientific and hardware references

The acoustic architecture uses general public source/filter, formant,
resonator, frication, and phoneme-inventory concepts. Scientific descriptions
influence architecture; no accompanying implementation was copied.

The MCU PWM interface follows the generic hardware model of writing compare
values to a timer while the peripheral produces a carrier. Vendor
documentation may be consulted by port authors; for example Raspberry Pi's Pico
SDK documents double-buffered PWM compare registers:

- https://www.raspberrypi.com/documentation/pico-sdk/hardware.html

NanoTTS contains no Raspberry Pi SDK code and makes no target-specific timer
assumption beyond an unsigned compare range.

## Development rule for future contributions

Contributors must preserve the clean boundary:

- do not paste, translate, or mechanically port code/tables from an
  incompatible TTS engine;
- do not import eSpeak language rules, dictionaries, word lists, voices, or
  generated corpora;
- document the source and license of every added numeric voice dataset;
- use independently recorded material only with an explicit redistribution
  grant compatible with MIT distribution;
- keep optional compatibility producers outside the runtime;
- document every language's dialect scope, ambiguities, and validation status;
- keep MCU vendor code in examples or application integration, not the portable
  core, unless its license and abstraction are explicitly reviewed.

This document records engineering provenance and is not legal advice.
