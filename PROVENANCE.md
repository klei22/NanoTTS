# Clean-room provenance

## Runtime implementation

All C source, public headers, tests, build files, documentation, and numeric
voice tables shipped in this repository were newly authored for `idtts` in
2026 and are offered under the MIT license in `LICENSE`.

The repository does **not** contain or derive from source code, pronunciation
rules, dictionaries, voice tables, recorded units, or other assets from:

- eSpeak or eSpeak NG;
- SAM / Software Automatic Mouth or its ports;
- Flite/Festival;
- Klatt synthesizer program implementations;
- proprietary or neural TTS engines.

The voice parameters in `src/idtts_voice.c` are original hand-tuned values.
They are not measurements extracted from an existing voice and are not copied
from another program.

## Role of eSpeak

eSpeak is an optional, external interoperability tool. During development it
may be executed as a black-box pronunciation oracle:

```sh
espeak-ng -q -v id --ipa=1 --sep=_ TEXT
```

Its UTF-8 stdout can be passed through a process boundary to the MIT-licensed
parser. `idtts` does not link to eSpeak, load its data files, call its API, or
require it at runtime. No eSpeak executable or data is redistributed here.

Small test strings demonstrate the public IPA interface. No generated corpus
or eSpeak audio is shipped in the source package.

## Scientific references

The project uses general ideas described in public scientific literature:
source/filter speech production, digital resonators, formant trajectories,
frication sources, and Indonesian phoneme inventories. Scientific descriptions
influence the architecture, but no accompanying implementation code was used.
Principal references are listed in `docs/DESIGN.md`.

## Development rule for future contributions

Contributors should preserve the clean boundary:

- do not paste, translate, or mechanically port code or tables from a
  non-MIT-compatible TTS engine;
- do not import eSpeak language rules or word lists;
- document the origin and license of every added numeric voice dataset;
- use independently recorded material only with an explicit license grant
  suitable for redistribution under the project's terms;
- keep external compatibility tools outside the runtime library.

This document records engineering provenance and is not legal advice.
