/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#include <string.h>

#define NANOTTS_PP_CAT_I(a_, b_) a_##b_
#define NANOTTS_PP_CAT(a_, b_) NANOTTS_PP_CAT_I(a_, b_)
#define NANOTTS_PP_IF_0(...)
#define NANOTTS_PP_IF_1(...) __VA_ARGS__
#define NANOTTS_PP_IF(value_) NANOTTS_PP_CAT(NANOTTS_PP_IF_, value_)

typedef struct nanotts_language_descriptor {
    const char *code;
    const char *name;
} nanotts_language_descriptor_t;

typedef struct nanotts_language_aliases {
    nanotts_language_t language;
    const char *aliases;
} nanotts_language_aliases_t;

/* Canonical codes, display names, and accepted aliases remain stable even when
 * a module is not compiled. This lets callers distinguish a known language
 * from an unknown code before reporting NANOTTS_ERR_LANGUAGE_UNAVAILABLE. */
static const nanotts_language_descriptor_t
LANGUAGE_DESCRIPTORS[NANOTTS_LANG_COUNT] = {
    [NANOTTS_LANG_NONE] = { "none", "IPA only" },
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
    [NANOTTS_LANG_##enum_] = { code_, name_ },
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
};

static const nanotts_language_aliases_t LANGUAGE_ALIASES[] = {
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
    { NANOTTS_LANG_##enum_, aliases_ },
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
};

static const nanotts_language_descriptor_t *descriptor_for(
    nanotts_language_t language)
{
    unsigned value = (unsigned)language;
    if (value >= (unsigned)NANOTTS_LANG_COUNT) return NULL;
    return &LANGUAGE_DESCRIPTORS[value];
}

static bool utf8_next(
    const char *text,
    size_t length,
    size_t *offset,
    uint32_t *codepoint)
{
    const unsigned char *s = (const unsigned char *)text;
    size_t i = *offset;
    unsigned char c0;

    if (i >= length) return false;
    c0 = s[i++];
    if (c0 < 0x80u) {
        *codepoint = c0;
    } else if (c0 >= 0xc2u && c0 <= 0xdfu && i < length &&
               (s[i] & 0xc0u) == 0x80u) {
        *codepoint = ((uint32_t)(c0 & 0x1fu) << 6) |
                     (uint32_t)(s[i++] & 0x3fu);
    } else if (c0 >= 0xe0u && c0 <= 0xefu && i + 1u < length &&
               (s[i] & 0xc0u) == 0x80u &&
               (s[i + 1u] & 0xc0u) == 0x80u) {
        unsigned char c1 = s[i++];
        unsigned char c2 = s[i++];
        if ((c0 == 0xe0u && c1 < 0xa0u) ||
            (c0 == 0xedu && c1 >= 0xa0u)) return false;
        *codepoint = ((uint32_t)(c0 & 0x0fu) << 12) |
                     ((uint32_t)(c1 & 0x3fu) << 6) |
                     (uint32_t)(c2 & 0x3fu);
    } else if (c0 >= 0xf0u && c0 <= 0xf4u && i + 2u < length &&
               (s[i] & 0xc0u) == 0x80u &&
               (s[i + 1u] & 0xc0u) == 0x80u &&
               (s[i + 2u] & 0xc0u) == 0x80u) {
        unsigned char c1 = s[i++];
        unsigned char c2 = s[i++];
        unsigned char c3 = s[i++];
        if ((c0 == 0xf0u && c1 < 0x90u) ||
            (c0 == 0xf4u && c1 >= 0x90u)) return false;
        *codepoint = ((uint32_t)(c0 & 0x07u) << 18) |
                     ((uint32_t)(c1 & 0x3fu) << 12) |
                     ((uint32_t)(c2 & 0x3fu) << 6) |
                     (uint32_t)(c3 & 0x3fu);
    } else {
        return false;
    }
    *offset = i;
    return true;
}

static uint32_t simple_casefold(uint32_t cp)
{
    if (cp >= 'A' && cp <= 'Z') return cp + ('a' - 'A');
    if ((cp >= 0x00c0u && cp <= 0x00d6u) ||
        (cp >= 0x00d8u && cp <= 0x00deu)) return cp + 0x20u;
    switch (cp) {
    case 0x0100u: return 0x0101u; /* Ā */
    case 0x0112u: return 0x0113u; /* Ē */
    case 0x012au: return 0x012bu; /* Ī */
    case 0x014cu: return 0x014du; /* Ō */
    case 0x016au: return 0x016bu; /* Ū */
    default: return cp;
    }
}

static bool utf8_case_equal_span(
    const char *input,
    const char *candidate,
    size_t candidate_length)
{
    size_t input_offset = 0u;
    size_t candidate_offset = 0u;
    size_t input_length;

    if (input == NULL) return false;
    input_length = strlen(input);
    while (input_offset < input_length && candidate_offset < candidate_length) {
        uint32_t a;
        uint32_t b;
        if (!utf8_next(input, input_length, &input_offset, &a) ||
            !utf8_next(candidate, candidate_length, &candidate_offset, &b)) {
            return false;
        }
        if (simple_casefold(a) != simple_casefold(b)) return false;
    }
    return input_offset == input_length && candidate_offset == candidate_length;
}

static bool alias_list_contains(const char *aliases, const char *code)
{
    const char *segment = aliases;
    const char *cursor = aliases;
    if (aliases == NULL || code == NULL) return false;
    for (;;) {
        if (*cursor == '|' || *cursor == '\0') {
            if (utf8_case_equal_span(code, segment, (size_t)(cursor - segment))) {
                return true;
            }
            if (*cursor == '\0') return false;
            segment = cursor + 1;
        }
        cursor++;
    }
}

static void clear_parse_info(nanotts_parse_info_t *info)
{
    if (info != NULL) {
        info->event_count = 0u;
        info->error_byte = NANOTTS_NPOS;
        info->error_codepoint = 0u;
    }
}

bool nanotts_language_is_compiled(nanotts_language_t language)
{
#if NANOTTS_ENABLE_TEXT_FRONTEND
    switch (language) {
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
        NANOTTS_PP_IF(enabled_)(case NANOTTS_LANG_##enum_: return true;)
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
    default: break;
    }
#else
    (void)language;
#endif
    return false;
}

int nanotts_language_available(nanotts_language_t language)
{
    return nanotts_language_is_compiled(language) ? 1 : 0;
}

size_t nanotts_compiled_language_count(void)
{
    return (size_t)NANOTTS_COMPILED_LANGUAGE_COUNT;
}

nanotts_language_t nanotts_compiled_language_at(size_t index)
{
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_COMPILED_LANGUAGE_COUNT > 0
    static const nanotts_language_t compiled[] = {
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
        NANOTTS_PP_IF(enabled_)(NANOTTS_LANG_##enum_,)
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
    };
    if (index < sizeof(compiled) / sizeof(compiled[0])) return compiled[index];
#else
    (void)index;
#endif
    return NANOTTS_LANG_NONE;
}

nanotts_language_t nanotts_language_from_code(const char *code)
{
    size_t i;
    if (code == NULL) return NANOTTS_LANG_COUNT;
    if (utf8_case_equal_span(code, "none", 4u) ||
        utf8_case_equal_span(code, "ipa", 3u)) return NANOTTS_LANG_NONE;

    /* Canonical codes and aliases are recognized even when the corresponding
     * module is absent. Availability is a separate query. */
    for (i = 1u; i < (size_t)NANOTTS_LANG_COUNT; ++i) {
        const char *canonical = LANGUAGE_DESCRIPTORS[i].code;
        if (utf8_case_equal_span(code, canonical, strlen(canonical))) {
            return (nanotts_language_t)i;
        }
    }
    for (i = 0u; i < sizeof(LANGUAGE_ALIASES) /
                         sizeof(LANGUAGE_ALIASES[0]); ++i) {
        if (alias_list_contains(LANGUAGE_ALIASES[i].aliases, code)) {
            return LANGUAGE_ALIASES[i].language;
        }
    }
    return NANOTTS_LANG_COUNT;
}

const char *nanotts_language_code(nanotts_language_t language)
{
    const nanotts_language_descriptor_t *descriptor;
    descriptor = descriptor_for(language);
    return descriptor != NULL ? descriptor->code : "unknown";
}

const char *nanotts_language_name(nanotts_language_t language)
{
    const nanotts_language_descriptor_t *descriptor;
    descriptor = descriptor_for(language);
    return descriptor != NULL ? descriptor->name : "Unknown";
}

nanotts_result_t nanotts_text_parse_impl(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info)
{
#if !NANOTTS_ENABLE_TEXT_FRONTEND
    (void)impl;
    (void)text;
    (void)length;
    clear_parse_info(info);
    return NANOTTS_ERR_FEATURE_DISABLED;
#elif NANOTTS_COMPILED_LANGUAGE_COUNT == 1
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
    NANOTTS_PP_IF(enabled_)( \
        if ((nanotts_language_t)impl->language != NANOTTS_LANG_##enum_) { \
            clear_parse_info(info); \
            return NANOTTS_ERR_LANGUAGE_UNAVAILABLE; \
        } \
        return parser_(impl, text, length, info);)
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
    clear_parse_info(info);
    return NANOTTS_ERR_INTERNAL;
#else
    switch ((nanotts_language_t)impl->language) {
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
        NANOTTS_PP_IF(enabled_)(case NANOTTS_LANG_##enum_: \
            return parser_(impl, text, length, info);)
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
    default:
        clear_parse_info(info);
        return NANOTTS_ERR_LANGUAGE_UNAVAILABLE;
    }
#endif
}
