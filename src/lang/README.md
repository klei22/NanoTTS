# NanoTTS language modules

Each `nanotts_lang_XX.c` file is a compile-time optional UTF-8 text-to-event
front end. Modules share `nanotts_lang_common.*` and the common renderer.

The parser entry-point contract and registration checklist are documented in
`docs/ADDING_A_LANGUAGE.md`. Start new work from
`nanotts_lang_template.c.example`.

Current modules:

- `nanotts_lang_id.c`: Indonesian (`id`)
- `nanotts_lang_sw.c`: Kiswahili (`sw`)

Do not put audio rendering, platform code, heap allocation, or copied third-
party language tables in this directory.
