/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#include <string.h>

static int ascii_equal_case(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

bool nanotts_language_is_compiled(nanotts_language_t language)
{
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_ENABLE_LANG_ID
    if (language == NANOTTS_LANG_INDONESIAN) return true;
#endif
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_ENABLE_LANG_SW
    if (language == NANOTTS_LANG_KISWAHILI) return true;
#endif
    (void)language;
    return false;
}

int nanotts_language_available(nanotts_language_t language)
{
    return nanotts_language_is_compiled(language) ? 1 : 0;
}

size_t nanotts_compiled_language_count(void)
{
    size_t count = 0u;
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_ENABLE_LANG_ID
    count++;
#endif
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_ENABLE_LANG_SW
    count++;
#endif
    return count;
}

nanotts_language_t nanotts_compiled_language_at(size_t index)
{
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_ENABLE_LANG_ID
    if (index == 0u) return NANOTTS_LANG_INDONESIAN;
    index--;
#endif
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_ENABLE_LANG_SW
    if (index == 0u) return NANOTTS_LANG_KISWAHILI;
    index--;
#endif
    (void)index;
    return NANOTTS_LANG_NONE;
}

nanotts_language_t nanotts_language_from_code(const char *code)
{
    if (ascii_equal_case(code, "id") ||
        ascii_equal_case(code, "id-id") ||
        ascii_equal_case(code, "id_id") ||
        ascii_equal_case(code, "indonesian") ||
        ascii_equal_case(code, "bahasa") ||
        ascii_equal_case(code, "bahasa-indonesia") ||
        ascii_equal_case(code, "bahasa_indonesia")) {
        return NANOTTS_LANG_INDONESIAN;
    }
    if (ascii_equal_case(code, "sw") ||
        ascii_equal_case(code, "sw-ke") ||
        ascii_equal_case(code, "sw_ke") ||
        ascii_equal_case(code, "sw-tz") ||
        ascii_equal_case(code, "sw_tz") ||
        ascii_equal_case(code, "swahili") ||
        ascii_equal_case(code, "kiswahili")) {
        return NANOTTS_LANG_KISWAHILI;
    }
    if (ascii_equal_case(code, "none") || ascii_equal_case(code, "ipa")) {
        return NANOTTS_LANG_NONE;
    }
    return NANOTTS_LANG_COUNT;
}

const char *nanotts_language_code(nanotts_language_t language)
{
    switch (language) {
    case NANOTTS_LANG_NONE: return "none";
    case NANOTTS_LANG_INDONESIAN: return "id";
    case NANOTTS_LANG_KISWAHILI: return "sw";
    default: return "unknown";
    }
}

const char *nanotts_language_name(nanotts_language_t language)
{
    switch (language) {
    case NANOTTS_LANG_NONE: return "IPA only";
    case NANOTTS_LANG_INDONESIAN: return "Indonesian";
    case NANOTTS_LANG_KISWAHILI: return "Kiswahili";
    default: return "Unknown";
    }
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
    if (info != NULL) {
        info->event_count = 0u;
        info->error_byte = NANOTTS_NPOS;
        info->error_codepoint = 0u;
    }
    return NANOTTS_ERR_FEATURE_DISABLED;
#else
#if NANOTTS_ENABLE_LANG_ID && !NANOTTS_ENABLE_LANG_SW
    if ((nanotts_language_t)impl->language != NANOTTS_LANG_INDONESIAN) {
        return NANOTTS_ERR_LANGUAGE_UNAVAILABLE;
    }
    return nanotts_lang_id_parse_text(impl, text, length, info);
#elif NANOTTS_ENABLE_LANG_SW && !NANOTTS_ENABLE_LANG_ID
    if ((nanotts_language_t)impl->language != NANOTTS_LANG_KISWAHILI) {
        return NANOTTS_ERR_LANGUAGE_UNAVAILABLE;
    }
    return nanotts_lang_sw_parse_text(impl, text, length, info);
#else
    switch ((nanotts_language_t)impl->language) {
#if NANOTTS_ENABLE_LANG_ID
    case NANOTTS_LANG_INDONESIAN:
        return nanotts_lang_id_parse_text(impl, text, length, info);
#endif
#if NANOTTS_ENABLE_LANG_SW
    case NANOTTS_LANG_KISWAHILI:
        return nanotts_lang_sw_parse_text(impl, text, length, info);
#endif
    default:
        if (info != NULL) {
            info->event_count = 0u;
            info->error_byte = NANOTTS_NPOS;
            info->error_codepoint = 0u;
        }
        return NANOTTS_ERR_LANGUAGE_UNAVAILABLE;
    }
#endif
#endif
}
