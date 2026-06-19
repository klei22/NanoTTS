# Clean-room provenance

## Runtime implementation

All C source, public headers, tests, build files, documentation, and numeric
voice tables shipped in this repository were newly authored for NanoTTS in 2026
and are offered under the MIT license in `LICENSE`.

The repository does **not** contain or derive from source code, pronunciation
rules, dictionaries, voice tables, recorded units, or other assets from:

- eSpeak or eSpeak NG;
- SAM / Software Automatic Mouth or its ports;
- Flite/Festival;
- Klatt synthesizer program implementations;
- proprietary or neural TTS engines.

The voice parameters in `src/nanotts_voice.c` are original hand-tuned values.
They are not copied from an existing program, generated voice, or language
package. The Kiswahili `/θ/`, `/ð/`, and `/ɣ/` entries were independently
added for NanoTTS 0.3. The Spanish `/β/` entry was independently added for
NanoTTS 0.4. All use the same original source/filter model.

## Language-module provenance

The Indonesian, Kiswahili, and Spanish modules are independently authored,
bounded G2P implementations. They do not translate, extract, or mechanically
reproduce an existing TTS engine's language files.

The Kiswahili scope was informed by descriptive linguistic references rather
than implementation data. In particular:

- LDC's *Language Specific Peculiarities Document for Swahili as Spoken in
  Kenya* describes the basic Latin orthography, five-vowel inventory,
  orthographic correspondences such as `ch`, `sh`, `ny`, `ng'`, `th`, `dh`,
  `kh`, and `gh`, syllabic nasals, and the usual penultimate stress pattern:
  https://catalog.ldc.upenn.edu/docs/LDC2017S05/LSP_202_final.pdf
- Kentalis's *Swahili* phonology guide describes separate pronunciation of
  successive vowels, possible syllabic nasals, and usual penultimate stress:
  https://www.kentalis.nl/file-download/download/public/2108

Those references describe a language. No source code, rule table, lexicon,
recording, or numeric acoustic data was imported from them. The test corpus in
`tests/data/sw_smoke.txt` consists of short independently selected phrases and
words used to exercise implemented features.

The Spanish module's scope was informed by public descriptive material from the
Real Academia Española and Instituto Cervantes concerning the five-vowel
inventory, common spelling-to-sound correspondences, majority seseo and yeísmo,
written stress conventions, and diphthong/hiatus behavior:

- RAE, *Pronunciación*:
  https://www.rae.es/libro-estilo-lengua-espa%C3%B1ola/pronunciaci%C3%B3n
- RAE, *Reglas generales de acentuación*:
  https://www.rae.es/ortograf%C3%ADa-b%C3%A1sica/uso-de-la-tilde/las-reglas-de-acentuaci%C3%B3n-gr%C3%A1fica/reglas-generales
- RAE, *Los fonemas del español*:
  https://www.rae.es/ortograf%C3%ADa-b%C3%A1sica/uso-de-las-letras/los-fonemas-del-espa%C3%B1ol
- Instituto Cervantes, *Pronunciación. Inventario A1-A2*:
  https://cvc.cervantes.es/ensenanza/biblioteca_ele/plan_curricular/niveles/03_pronunciacion_inventario_a1-a2.htm

Those sources describe Spanish. No prose, source code, pronunciation table,
lexicon, recording, or numeric acoustic data was copied into NanoTTS. The rules
and `tests/data/es_smoke.txt` were independently authored for this project.

## Role of eSpeak

eSpeak is an optional, external interoperability tool. During development it
may be executed as a black-box IPA producer:

```sh
espeak -q -v id --ipa=1 --sep=_ TEXT
espeak -q -v sw --ipa=1 --sep=_ TEXT
espeak -q -v es-la --ipa=1 --sep=_ TEXT
```

Its UTF-8 stdout can be passed through a process boundary to the MIT-licensed
parser. NanoTTS does not link to eSpeak, load its data files, call its API, or
require it at runtime. No eSpeak executable, data file, pronunciation table,
voice, or generated audio corpus is redistributed in the source package.

Small hand-written strings demonstrate the public IPA interface. Compatibility
checks execute the locally installed producer at test time; their output is not
committed as language data.

## Scientific references

The project uses general ideas described in public scientific literature:
source/filter speech production, digital resonators, formant trajectories,
frication sources, and phoneme inventories. Scientific descriptions influence
the architecture, but no accompanying implementation code was used. Principal
references are listed in `docs/DESIGN.md` and `docs/LANGUAGES.md`.

## Development rule for future contributions

Contributors should preserve the clean boundary:

- do not paste, translate, or mechanically port code or tables from a
  non-MIT-compatible TTS engine;
- do not import eSpeak language rules, dictionaries, or word lists;
- document the source and license of every added numeric voice dataset;
- use independently recorded material only with an explicit redistribution
  grant compatible with the project;
- keep optional compatibility producers outside the runtime library;
- document each new language module's linguistic scope and known ambiguities.

This document records engineering provenance and is not legal advice.
