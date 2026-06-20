# NanoTTS text-language modules

Every source file here is a bounded UTF-8 text-to-event frontend. Modules do
not render samples, allocate memory, open files, or load runtime plug-ins.
Build-time selection keeps unused parsers and prosody profiles out of the
binary.

Current modules:

- `nanotts_lang_id.c`: Indonesian (`id`)
- `nanotts_lang_sw.c`: Kiswahili (`sw`)
- `nanotts_lang_es.c`: Spanish (`es`)
- `nanotts_lang_ms.c`: Malay (`ms`)
- `nanotts_lang_mi.c`: Māori wrapper (`mi`)
- `nanotts_lang_haw.c`: Hawaiian wrapper (`haw`)
- `nanotts_lang_polynesian.c`: private mechanics shared by `mi` and `haw`
- `nanotts_lang_common.c`: neutral bounded helpers for text builds
- `nanotts_lang_template.c.example`: starting shape for another module

Public IDs, codes, aliases, parser symbols, and prosody association are declared
in `include/nanotts/nanotts_languages.def`. CMake/Make still select source files
explicitly so a target links only requested modules.

See `docs/ADDING_A_LANGUAGE.md` for the integration checklist and
`docs/LANGUAGES.md` for module scope and limitations.
