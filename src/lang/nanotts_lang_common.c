/* SPDX-License-Identifier: MIT */
#include "lang/nanotts_lang_common.h"

bool nanotts_lang_decode_utf8(
    const char *text,
    size_t length,
    size_t offset,
    uint32_t *cp,
    size_t *width)
{
    const unsigned char *s = (const unsigned char *)text;
    unsigned char c0;

    if (text == NULL || cp == NULL || width == NULL || offset >= length) {
        return false;
    }
    c0 = s[offset];
    if (c0 < 0x80u) {
        *cp = c0;
        *width = 1u;
        return true;
    }
    if (c0 >= 0xc2u && c0 <= 0xdfu) {
        if (offset + 1u >= length || (s[offset + 1u] & 0xc0u) != 0x80u) {
            return false;
        }
        *cp = ((uint32_t)(c0 & 0x1fu) << 6) |
              (uint32_t)(s[offset + 1u] & 0x3fu);
        *width = 2u;
        return true;
    }
    if (c0 >= 0xe0u && c0 <= 0xefu) {
        unsigned char c1;
        unsigned char c2;
        if (offset + 2u >= length) {
            return false;
        }
        c1 = s[offset + 1u];
        c2 = s[offset + 2u];
        if ((c1 & 0xc0u) != 0x80u || (c2 & 0xc0u) != 0x80u ||
            (c0 == 0xe0u && c1 < 0xa0u) ||
            (c0 == 0xedu && c1 >= 0xa0u)) {
            return false;
        }
        *cp = ((uint32_t)(c0 & 0x0fu) << 12) |
              ((uint32_t)(c1 & 0x3fu) << 6) |
              (uint32_t)(c2 & 0x3fu);
        *width = 3u;
        return true;
    }
    if (c0 >= 0xf0u && c0 <= 0xf4u) {
        unsigned char c1;
        unsigned char c2;
        unsigned char c3;
        if (offset + 3u >= length) {
            return false;
        }
        c1 = s[offset + 1u];
        c2 = s[offset + 2u];
        c3 = s[offset + 3u];
        if ((c1 & 0xc0u) != 0x80u || (c2 & 0xc0u) != 0x80u ||
            (c3 & 0xc0u) != 0x80u ||
            (c0 == 0xf0u && c1 < 0x90u) ||
            (c0 == 0xf4u && c1 >= 0x90u)) {
            return false;
        }
        *cp = ((uint32_t)(c0 & 0x07u) << 18) |
              ((uint32_t)(c1 & 0x3fu) << 12) |
              ((uint32_t)(c2 & 0x3fu) << 6) |
              (uint32_t)(c3 & 0x3fu);
        *width = 4u;
        return true;
    }
    return false;
}

bool nanotts_lang_add_pause_scaled(
    nanotts_impl_t *impl,
    nanotts_phone_t pause,
    uint8_t flags,
    uint8_t duration_percent)
{
    if (impl->event_count == 0u) {
        return true;
    }
    if (nanotts_phone_is_pause(
            (nanotts_phone_t)impl->events[impl->event_count - 1u].phone)) {
        nanotts_event_t *previous = &impl->events[impl->event_count - 1u];
        if (pause == NANOTTS_PH_PHRASE_PAUSE) {
            previous->phone = (uint8_t)pause;
            previous->flags |= flags;
        }
        if (duration_percent > previous->duration_percent) {
            previous->duration_percent = duration_percent;
        }
        return true;
    }
    return nanotts_push_event(impl, pause, flags, duration_percent, 0);
}

bool nanotts_lang_add_pause(
    nanotts_impl_t *impl,
    nanotts_phone_t pause,
    uint8_t flags)
{
    uint8_t duration = pause == NANOTTS_PH_PHRASE_PAUSE ? 100u : 50u;
    return nanotts_lang_add_pause_scaled(impl, pause, flags, duration);
}

bool nanotts_lang_temp_push(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t phone)
{
    if (*count >= NANOTTS_LANG_WORD_PHONE_MAX) {
        return false;
    }
    events[*count].phone = (uint8_t)phone;
    events[*count].flags = 0u;
    events[*count].duration_percent = 100u;
    events[*count].pitch_semitones_q4 = 0;
    (*count)++;
    return true;
}

bool nanotts_lang_should_spell_acronym(
    const uint32_t *word,
    size_t length)
{
    size_t vowels = 0u;
    size_t letters = 0u;
    size_t i;

    if (word == NULL || length < 2u || length > 8u) {
        return false;
    }
    for (i = 0u; i < length; ++i) {
        uint32_t cp = word[i];
        if ((cp >= 'a' && cp <= 'z') || cp == 0x00e9u ||
            cp == 0x00e8u || cp == 0x00eau) {
            letters++;
            if (cp == 'a' || cp == 'e' || cp == 'i' ||
                cp == 'o' || cp == 'u' || cp == 0x00e9u ||
                cp == 0x00e8u || cp == 0x00eau) {
                vowels++;
            }
        }
    }
    return letters == length && (vowels * 2u) < letters;
}

void nanotts_lang_apply_penultimate_stress(
    nanotts_event_t *events,
    size_t count)
{
    size_t vowels[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t vowel_count = 0u;
    size_t i;

    for (i = 0u; i < count && vowel_count < NANOTTS_LANG_WORD_PHONE_MAX; ++i) {
        if (nanotts_phone_is_vowel((nanotts_phone_t)events[i].phone)) {
            vowels[vowel_count++] = i;
        }
    }
    if (vowel_count > 0u) {
        size_t stressed = vowel_count == 1u ? 0u : vowel_count - 2u;
        events[vowels[stressed]].flags |= NANOTTS_EVENT_STRESS_PRIMARY;
    }
}

void nanotts_lang_mark_word_end(nanotts_impl_t *impl)
{
    if (impl->event_count > 0u &&
        !nanotts_phone_is_pause(
            (nanotts_phone_t)impl->events[impl->event_count - 1u].phone)) {
        impl->events[impl->event_count - 1u].flags |= NANOTTS_EVENT_WORD_END;
    }
}

void nanotts_lang_trim_trailing_short_pause(nanotts_impl_t *impl)
{
    while (impl->event_count > 0u &&
           impl->events[impl->event_count - 1u].phone ==
               (uint8_t)NANOTTS_PH_SHORT_PAUSE) {
        impl->event_count--;
    }
}

void nanotts_lang_set_info(
    nanotts_parse_info_t *info,
    size_t count,
    size_t byte,
    uint32_t cp)
{
    if (info != NULL) {
        info->event_count = count;
        info->error_byte = byte;
        info->error_codepoint = cp;
    }
}
