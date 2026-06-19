/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#include <ctype.h>

static bool decode_utf8_at(
    const char *text,
    size_t length,
    size_t offset,
    uint32_t *codepoint,
    size_t *width)
{
    const unsigned char *s = (const unsigned char *)text;
    uint32_t cp;
    unsigned char c0;

    if (offset >= length) {
        return false;
    }
    c0 = s[offset];
    if (c0 < 0x80u) {
        *codepoint = c0;
        *width = 1u;
        return true;
    }
    if (c0 >= 0xc2u && c0 <= 0xdfu) {
        if (offset + 1u >= length || (s[offset + 1u] & 0xc0u) != 0x80u) {
            return false;
        }
        cp = ((uint32_t)(c0 & 0x1fu) << 6) |
             (uint32_t)(s[offset + 1u] & 0x3fu);
        *codepoint = cp;
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
        if ((c1 & 0xc0u) != 0x80u || (c2 & 0xc0u) != 0x80u) {
            return false;
        }
        if ((c0 == 0xe0u && c1 < 0xa0u) ||
            (c0 == 0xedu && c1 >= 0xa0u)) {
            return false;
        }
        cp = ((uint32_t)(c0 & 0x0fu) << 12) |
             ((uint32_t)(c1 & 0x3fu) << 6) |
             (uint32_t)(c2 & 0x3fu);
        *codepoint = cp;
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
            (c3 & 0xc0u) != 0x80u) {
            return false;
        }
        if ((c0 == 0xf0u && c1 < 0x90u) ||
            (c0 == 0xf4u && c1 >= 0x90u)) {
            return false;
        }
        cp = ((uint32_t)(c0 & 0x07u) << 18) |
             ((uint32_t)(c1 & 0x3fu) << 12) |
             ((uint32_t)(c2 & 0x3fu) << 6) |
             (uint32_t)(c3 & 0x3fu);
        *codepoint = cp;
        *width = 4u;
        return true;
    }
    return false;
}

static bool is_tie(uint32_t cp)
{
    return cp == 0x0361u || cp == 0x035cu || cp == 0x200du;
}

static bool is_separator(uint32_t cp)
{
    return cp == (uint32_t)'_' || cp == 0x00b7u || cp == 0x2027u ||
           cp == 0x2060u || cp == 0xfeffu;
}

static bool is_profile_modifier(uint32_t cp)
{
    /* IPA non-syllabic marks and harmless token-format controls. */
    return cp == 0x032fu || cp == 0x0311u || cp == 0x0329u ||
           cp == 0x034fu || cp == 0xfe0eu || cp == 0xfe0fu;
}

static bool is_space_cp(uint32_t cp)
{
    return cp == (uint32_t)' ' || cp == (uint32_t)'\t' ||
           cp == 0x00a0u || cp == 0x2009u || cp == 0x202fu;
}

static bool add_pause(
    nanotts_impl_t *impl,
    nanotts_phone_t pause,
    uint8_t flags,
    uint8_t duration_percent)
{
    if (impl->event_count == 0u) {
        return true;
    }
    if (nanotts_phone_is_pause((nanotts_phone_t)impl->events[impl->event_count - 1u].phone)) {
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

static bool map_single_phone(uint32_t cp, nanotts_phone_t *phone)
{
    switch (cp) {
    case 'a': case 0x0250u: *phone = NANOTTS_PH_A; return true;
    case 'e': *phone = NANOTTS_PH_E; return true;
    case 0x0259u: case 0x025cu: case 0x025du: *phone = NANOTTS_PH_SCHWA; return true;
    case 0x025bu: *phone = NANOTTS_PH_OPEN_E; return true;
    case 'i': *phone = NANOTTS_PH_I; return true;
    case 0x026au: *phone = NANOTTS_PH_SMALL_I; return true;
    case 'o': *phone = NANOTTS_PH_O; return true;
    case 0x0254u: *phone = NANOTTS_PH_OPEN_O; return true;
    case 'u': *phone = NANOTTS_PH_U; return true;
    case 0x028au: *phone = NANOTTS_PH_SMALL_U; return true;

    case 'p': *phone = NANOTTS_PH_P; return true;
    case 'b': case 0x0253u: *phone = NANOTTS_PH_B; return true;
    case 't': *phone = NANOTTS_PH_T; return true;
    case 'd': case 0x0257u: *phone = NANOTTS_PH_D; return true;
    case 'k': case 'q': *phone = NANOTTS_PH_K; return true;
    case 'g': case 0x0261u: case 0x0260u: *phone = NANOTTS_PH_G; return true;
    case 0x0294u: *phone = NANOTTS_PH_GLOTTAL_STOP; return true;

    case 'f': case 0x0278u: *phone = NANOTTS_PH_F; return true;
    case 'v': *phone = NANOTTS_PH_V; return true;
    case 0x03b2u: *phone = NANOTTS_PH_BETA; return true;
    case 's': *phone = NANOTTS_PH_S; return true;
    case 'z': *phone = NANOTTS_PH_Z; return true;
    case 0x03b8u: *phone = NANOTTS_PH_TH; return true;
    case 0x00f0u: *phone = NANOTTS_PH_DH; return true;
    case 0x0283u: case 0x00e7u: case 0x0255u: *phone = NANOTTS_PH_SH; return true;
    case 'x': case 0x03c7u: *phone = NANOTTS_PH_X; return true;
    case 0x0263u: *phone = NANOTTS_PH_GH; return true;
    case 'h': case 0x0266u: *phone = NANOTTS_PH_H; return true;

    case 'm': *phone = NANOTTS_PH_M; return true;
    case 'n': *phone = NANOTTS_PH_N; return true;
    case 0x0272u: case 0x0144u: *phone = NANOTTS_PH_NY; return true;
    case 0x014bu: *phone = NANOTTS_PH_NG; return true;

    case 'l': *phone = NANOTTS_PH_L; return true;
    case 'r': case 0x0280u: *phone = NANOTTS_PH_R; return true;
    case 0x027eu: *phone = NANOTTS_PH_TAP; return true;
    case 'w': case 0x028bu: *phone = NANOTTS_PH_W; return true;
    case 'j': case 0x0265u: case 0x029du: case 0x028eu: /* ʎ≈j */
        *phone = NANOTTS_PH_Y; return true;
    case 0x025fu: case 0x02a4u: *phone = NANOTTS_PH_J; return true;
    case 0x02a7u: *phone = NANOTTS_PH_CH; return true;
    default: return false;
    }
}

static void set_info(
    nanotts_parse_info_t *info,
    size_t count,
    size_t error_byte,
    uint32_t error_cp)
{
    if (info == NULL) {
        return;
    }
    info->event_count = count;
    info->error_byte = error_byte;
    info->error_codepoint = error_cp;
}

nanotts_result_t nanotts_ipa_parse_impl(
    nanotts_impl_t *impl,
    const char *ipa,
    size_t length,
    uint8_t parse_flags,
    nanotts_parse_info_t *info)
{
    size_t i = 0u;
    uint8_t pending_stress = 0u;
    bool saw_phone = false;

    impl->event_count = 0u;
    set_info(info, 0u, NANOTTS_NPOS, 0u);

    while (i < length) {
        uint32_t cp;
        size_t width;
        nanotts_phone_t phone;
        uint8_t event_flags = 0u;
        size_t consumed;

        if (!decode_utf8_at(ipa, length, i, &cp, &width)) {
            set_info(info, impl->event_count, i, 0u);
            return NANOTTS_ERR_UTF8;
        }
        consumed = width;

        if (cp == 0x0329u) { /* combining syllabic mark */
            if (impl->event_count > 0u) {
                nanotts_event_t *previous =
                    &impl->events[impl->event_count - 1u];
                nanotts_phone_t previous_phone =
                    (nanotts_phone_t)previous->phone;
                if (previous_phone == NANOTTS_PH_M ||
                    previous_phone == NANOTTS_PH_N ||
                    previous_phone == NANOTTS_PH_NG) {
                    previous->flags |= NANOTTS_EVENT_SYLLABIC | pending_stress;
                    pending_stress = 0u;
                    if (previous->duration_percent < 125u) {
                        previous->duration_percent = 125u;
                    }
                }
            }
            i += consumed;
            continue;
        }
        if (is_separator(cp) || is_tie(cp) || is_profile_modifier(cp)) {
            i += consumed;
            continue;
        }
        if (cp == 0x02c8u) { /* primary stress */
            pending_stress &= (uint8_t)~NANOTTS_EVENT_STRESS_SECONDARY;
            pending_stress |= NANOTTS_EVENT_STRESS_PRIMARY;
            i += consumed;
            continue;
        }
        if (cp == 0x02ccu) { /* secondary stress */
            if ((pending_stress & NANOTTS_EVENT_STRESS_PRIMARY) == 0u) {
                pending_stress |= NANOTTS_EVENT_STRESS_SECONDARY;
            }
            i += consumed;
            continue;
        }
        if (cp == 0x02d0u || cp == 0x02d1u) { /* length */
            if (impl->event_count > 0u &&
                !nanotts_phone_is_pause((nanotts_phone_t)impl->events[impl->event_count - 1u].phone)) {
                nanotts_event_t *previous = &impl->events[impl->event_count - 1u];
                previous->flags |= NANOTTS_EVENT_LONG;
                previous->duration_percent = (uint8_t)(cp == 0x02d0u ? 150u : 125u);
            }
            i += consumed;
            continue;
        }
        if (is_space_cp(cp)) {
            if ((parse_flags & NANOTTS_OPT_NO_AUTOPAUSE) == 0u) {
                if (impl->event_count > 0u &&
                    !nanotts_phone_is_pause((nanotts_phone_t)impl->events[impl->event_count - 1u].phone)) {
                    impl->events[impl->event_count - 1u].flags |= NANOTTS_EVENT_WORD_END;
                }
                if (!add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u, 50u)) {
                    set_info(info, impl->event_count, i, cp);
                    return NANOTTS_ERR_TOO_MANY_EVENTS;
                }
            }
            i += consumed;
            continue;
        }
        if (cp == '\n' || cp == '\r' || cp == '.' || cp == '!' || cp == '?' ||
            cp == ';' || cp == ':' || cp == 0x2014u) {
            uint8_t flags = NANOTTS_EVENT_PHRASE_END;
            if (cp == '?') flags |= NANOTTS_EVENT_QUESTION;
            if (!add_pause(impl, NANOTTS_PH_PHRASE_PAUSE, flags, 100u)) {
                set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            pending_stress = 0u;
            i += consumed;
            continue;
        }
        if (cp == ',' || cp == 0x2013u || cp == '/') {
            uint8_t duration = cp == ',' ? 165u : 120u;
            if (!add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u, duration)) {
                set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            i += consumed;
            continue;
        }

        /* Greedy affricate recognition: t͡ʃ / tʃ and d͡ʒ / dʒ. */
        if (cp == 't' || cp == 'd') {
            size_t j = i + width;
            uint32_t cp2 = 0u;
            size_t width2 = 0u;
            if (j < length && decode_utf8_at(ipa, length, j, &cp2, &width2)) {
                if (is_tie(cp2)) {
                    j += width2;
                    if (j < length && !decode_utf8_at(ipa, length, j, &cp2, &width2)) {
                        set_info(info, impl->event_count, j, 0u);
                        return NANOTTS_ERR_UTF8;
                    }
                }
                if ((cp == 't' && (cp2 == 0x0283u || cp2 == 0x00e7u)) ||
                    (cp == 'd' && cp2 == 0x0292u)) {
                    phone = cp == 't' ? NANOTTS_PH_CH : NANOTTS_PH_J;
                    consumed = (j + width2) - i;
                    goto push_phone;
                }
            }
        }

        /* eSpeak emits Indonesian diphthongs without a separator. */
        if (cp == 'a' || cp == 'o') {
            uint32_t cp2;
            size_t width2;
            if (i + width < length &&
                decode_utf8_at(ipa, length, i + width, &cp2, &width2)) {
                if (cp == 'a' && cp2 == 0x026au) {
                    phone = NANOTTS_PH_AI;
                    consumed += width2;
                    goto push_phone;
                }
                if (cp == 'a' && cp2 == 0x028au) {
                    phone = NANOTTS_PH_AU;
                    consumed += width2;
                    goto push_phone;
                }
                if (cp == 'o' && cp2 == 0x026au) {
                    phone = NANOTTS_PH_OI;
                    consumed += width2;
                    goto push_phone;
                }
            }
        }

        if (cp == 0x0292u || cp == 0x025fu) { /* standalone voiced postalveolar */
            phone = NANOTTS_PH_J;
            goto push_phone;
        }
        if (!map_single_phone(cp, &phone)) {
            if ((parse_flags & NANOTTS_OPT_PERMISSIVE_IPA) != 0u) {
                i += consumed;
                continue;
            }
            set_info(info, impl->event_count, i, cp);
            return NANOTTS_ERR_UNKNOWN_IPA;
        }

push_phone:
        if (nanotts_phone_is_vowel(phone) && pending_stress != 0u) {
            event_flags |= pending_stress;
            pending_stress = 0u;
        }
        if (!nanotts_push_event(impl, phone, event_flags, 100u, 0)) {
            set_info(info, impl->event_count, i, cp);
            return NANOTTS_ERR_TOO_MANY_EVENTS;
        }
        saw_phone = true;
        i += consumed;
    }

    while (impl->event_count > 0u &&
           impl->events[impl->event_count - 1u].phone == (uint8_t)NANOTTS_PH_SHORT_PAUSE) {
        impl->event_count--;
    }

    set_info(info, impl->event_count, NANOTTS_NPOS, 0u);
    return saw_phone ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
