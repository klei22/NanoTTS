# NanoTTS text-language modules

Each source file in this directory is a bounded UTF-8 text-to-event frontend.
It contains no renderer, audio driver, heap allocation, or runtime plug-in
loader. Build-time selection keeps unused modules out of the binary.

Current modules:

- `nanotts_lang_id.c`: Indonesian (`id`)
- `nanotts_lang_sw.c`: Kiswahili (`sw`)
- `nanotts_lang_es.c`: Latin-American-style Spanish (`es`)
- `nanotts_lang_common.c`: neutral bounded parser helpers shared only by text builds
- `nanotts_lang_template.c.example`: starting shape for another module

See `docs/ADDING_A_LANGUAGE.md` for the contract and integration checklist, and
`docs/LANGUAGES.md` for each module's pronunciation scope and limitations.
