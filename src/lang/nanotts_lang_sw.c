/* SPDX-License-Identifier: MIT */
#include "lang/nanotts_lang_common.h"

#include <string.h>

static bool is_letter_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}

static bool is_upper_cp(uint32_t cp)
{
    return cp >= 'A' && cp <= 'Z';
}

static uint32_t lower_cp(uint32_t cp)
{
    if (cp >= 'A' && cp <= 'Z') {
        return cp + ('a' - 'A');
    }
    if (cp == 0x2019u || cp == 0x02bcu) {
        return '\'';
    }
    return cp;
}

static bool push_with_duration(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t phone,
    uint8_t duration_percent)
{
    if (!nanotts_lang_temp_push(events, count, phone)) {
        return false;
    }
    events[*count - 1u].duration_percent = duration_percent;
    return true;
}

static bool push_pair(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t first,
    uint8_t first_duration,
    nanotts_phone_t second)
{
    return push_with_duration(events, count, first, first_duration) &&
           nanotts_lang_temp_push(events, count, second);
}

/*
 * Apply ordinary penultimate-vowel stress, plus a compact treatment of common
 * monosyllabic stems with an initial syllabic nasal (mti, mtu, nchi, mbwa).
 * The latter have one written vowel but two spoken syllable nuclei.
 */
static void apply_swahili_stress(nanotts_event_t *events, size_t count)
{
    size_t vowel_count = 0u;
    size_t i;

    nanotts_lang_apply_penultimate_stress(events, count);
    for (i = 0u; i < count; ++i) {
        if (nanotts_phone_is_vowel((nanotts_phone_t)events[i].phone)) {
            vowel_count++;
        }
    }
    if (vowel_count == 1u && count > 1u &&
        (events[0].phone == (uint8_t)NANOTTS_PH_M ||
         events[0].phone == (uint8_t)NANOTTS_PH_N ||
         events[0].phone == (uint8_t)NANOTTS_PH_NG) &&
        !nanotts_phone_is_vowel((nanotts_phone_t)events[1].phone)) {
        for (i = 0u; i < count; ++i) {
            events[i].flags &= (uint8_t)~(NANOTTS_EVENT_STRESS_PRIMARY |
                                          NANOTTS_EVENT_STRESS_SECONDARY);
        }
        events[0].flags |= NANOTTS_EVENT_STRESS_PRIMARY |
                           NANOTTS_EVENT_SYLLABIC;
        if (events[0].duration_percent < 125u) {
            events[0].duration_percent = 125u;
        }
    }
}

/*
 * Standard Kiswahili is close to grapheme-to-phoneme. Vowel sequences remain
 * separate syllables; unlike Indonesian, this module deliberately creates no
 * diphthong phones. Arabic-loan digraphs are preserved when written.
 */
static bool emit_word(
    nanotts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    nanotts_event_t phones[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t phone_count = 0u;
    size_t i = 0u;
    size_t j;

    while (i < length) {
        uint32_t c0 = word[i];
        uint32_t c1 = i + 1u < length ? word[i + 1u] : 0u;
        uint32_t c2 = i + 2u < length ? word[i + 2u] : 0u;
        nanotts_phone_t phone;

        if (c0 == 'n' && c1 == 'g' && c2 == '\'') {
            if (!nanotts_lang_temp_push(phones, &phone_count, NANOTTS_PH_NG)) {
                return false;
            }
            i += 3u;
            continue;
        }
        if (c0 == 'n' && c1 == 'g') {
            if (!push_pair(phones, &phone_count, NANOTTS_PH_NG, 72u,
                    NANOTTS_PH_G)) {
                return false;
            }
            i += 2u;
            continue;
        }
        if (c0 == 'n' && c1 == 'y') {
            phone = NANOTTS_PH_NY;
            i += 2u;
        } else if (c0 == 'n' && c1 == 'j') {
            if (!push_pair(phones, &phone_count, NANOTTS_PH_N, 72u,
                    NANOTTS_PH_J)) {
                return false;
            }
            i += 2u;
            continue;
        } else if (c0 == 'm' && c1 == 'b') {
            if (!push_pair(phones, &phone_count, NANOTTS_PH_M, 72u,
                    NANOTTS_PH_B)) {
                return false;
            }
            i += 2u;
            continue;
        } else if (c0 == 'n' && c1 == 'd') {
            if (!push_pair(phones, &phone_count, NANOTTS_PH_N, 72u,
                    NANOTTS_PH_D)) {
                return false;
            }
            i += 2u;
            continue;
        } else if (c0 == 'c' && c1 == 'h') {
            phone = NANOTTS_PH_CH;
            i += 2u;
        } else if (c0 == 's' && c1 == 'h') {
            phone = NANOTTS_PH_SH;
            i += 2u;
        } else if (c0 == 't' && c1 == 'h') {
            phone = NANOTTS_PH_TH;
            i += 2u;
        } else if (c0 == 'd' && c1 == 'h') {
            phone = NANOTTS_PH_DH;
            i += 2u;
        } else if (c0 == 'g' && c1 == 'h') {
            phone = NANOTTS_PH_GH;
            i += 2u;
        } else if (c0 == 'k' && c1 == 'h') {
            phone = NANOTTS_PH_X;
            i += 2u;
        } else if (c0 == 't' && c1 == 's') {
            if (!push_pair(phones, &phone_count, NANOTTS_PH_T, 68u,
                    NANOTTS_PH_S)) {
                return false;
            }
            i += 2u;
            continue;
        } else {
            switch (c0) {
            case 'a': phone = NANOTTS_PH_A; break;
            case 'e': phone = NANOTTS_PH_OPEN_E; break;
            case 'i': phone = NANOTTS_PH_I; break;
            case 'o': phone = NANOTTS_PH_OPEN_O; break;
            case 'u': phone = NANOTTS_PH_U; break;
            case 'p': phone = NANOTTS_PH_P; break;
            case 'b': phone = NANOTTS_PH_B; break;
            case 't': phone = NANOTTS_PH_T; break;
            case 'd': phone = NANOTTS_PH_D; break;
            case 'k': case 'q': phone = NANOTTS_PH_K; break;
            case 'g': phone = NANOTTS_PH_G; break;
            case 'c': phone = NANOTTS_PH_CH; break;
            case 'j': phone = NANOTTS_PH_J; break;
            case 'f': phone = NANOTTS_PH_F; break;
            case 'v': phone = NANOTTS_PH_V; break;
            case 's': phone = NANOTTS_PH_S; break;
            case 'z': phone = NANOTTS_PH_Z; break;
            case 'h': phone = NANOTTS_PH_H; break;
            case 'm': phone = NANOTTS_PH_M; break;
            case 'n': phone = NANOTTS_PH_N; break;
            case 'l': phone = NANOTTS_PH_L; break;
            case 'r': phone = NANOTTS_PH_R; break;
            case 'w': phone = NANOTTS_PH_W; break;
            case 'y': phone = NANOTTS_PH_Y; break;
            case 'x':
                if (!nanotts_lang_temp_push(
                        phones, &phone_count, NANOTTS_PH_K)) {
                    return false;
                }
                phone = NANOTTS_PH_S;
                break;
            case '\'':
                i++;
                continue;
            default:
                i++;
                continue;
            }
            i++;
        }

        if (!nanotts_lang_temp_push(phones, &phone_count, phone)) {
            return false;
        }
    }

    apply_swahili_stress(phones, phone_count);
    for (j = 0u; j < phone_count; ++j) {
        if (!nanotts_push_event(
                impl,
                (nanotts_phone_t)phones[j].phone,
                phones[j].flags,
                phones[j].duration_percent,
                phones[j].pitch_semitones_q4)) {
            return false;
        }
    }
    nanotts_lang_mark_word_end(impl);
    return true;
}

static bool emit_ascii_word(nanotts_impl_t *impl, const char *word)
{
    uint32_t letters[NANOTTS_LANG_WORD_MAX];
    size_t count = 0u;
    while (*word != '\0' && count < NANOTTS_LANG_WORD_MAX) {
        letters[count++] = (uint32_t)(unsigned char)*word++;
    }
    return emit_word(impl, letters, count);
}

static bool emit_letter_name(nanotts_impl_t *impl, uint32_t letter)
{
    static const char *const names[26] = {
        "a", "ba", "cha", "da", "e", "fa", "ga", "ha", "i",
        "ja", "ka", "la", "ma", "na", "o", "pa", "kyu", "ra",
        "sa", "ta", "u", "va", "wa", "eksi", "ya", "za"
    };
    if (letter < 'a' || letter > 'z') {
        return true;
    }
    return emit_ascii_word(impl, names[letter - 'a']);
}

static bool emit_acronym(
    nanotts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        if (word[i] == '\'') {
            continue;
        }
        if (!emit_letter_name(impl, word[i])) {
            return false;
        }
        if (i + 1u < length &&
            !nanotts_lang_add_pause_scaled(
                impl, NANOTTS_PH_SHORT_PAUSE, 0u, 30u)) {
            return false;
        }
    }
    nanotts_lang_mark_word_end(impl);
    return true;
}

static bool emit_number(nanotts_impl_t *impl, uint32_t value, unsigned depth);

static bool emit_number_join(
    nanotts_impl_t *impl,
    uint32_t value,
    unsigned depth,
    bool include_na)
{
    if (!nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u)) {
        return false;
    }
    if (include_na) {
        if (!emit_ascii_word(impl, "na") ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u)) {
            return false;
        }
    }
    return emit_number(impl, value, depth + 1u);
}

static bool emit_number(nanotts_impl_t *impl, uint32_t value, unsigned depth)
{
    static const char *const ones[] = {
        "sifuri", "moja", "mbili", "tatu", "nne",
        "tano", "sita", "saba", "nane", "tisa"
    };
    static const char *const tens[] = {
        "", "kumi", "ishirini", "thelathini", "arobaini",
        "hamsini", "sitini", "sabini", "themanini", "tisini"
    };
    uint32_t remainder;

    if (depth > 12u) {
        return false;
    }
    if (value < 10u) {
        return emit_ascii_word(impl, ones[value]);
    }
    if (value < 100u) {
        if (!emit_ascii_word(impl, tens[value / 10u])) {
            return false;
        }
        remainder = value % 10u;
        return remainder == 0u || emit_number_join(impl, remainder, depth, true);
    }
    if (value < 1000u) {
        if (!emit_ascii_word(impl, "mia") ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_number(impl, value / 100u, depth + 1u)) {
            return false;
        }
        remainder = value % 100u;
        return remainder == 0u || emit_number_join(impl, remainder, depth, true);
    }
    if (value < 100000u) {
        if (!emit_ascii_word(impl, "elfu") ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_number(impl, value / 1000u, depth + 1u)) {
            return false;
        }
        remainder = value % 1000u;
        return remainder == 0u || emit_number_join(impl, remainder, depth, true);
    }
    if (value < 1000000u) {
        if (!emit_ascii_word(impl, "laki") ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_number(impl, value / 100000u, depth + 1u)) {
            return false;
        }
        remainder = value % 100000u;
        return remainder == 0u || emit_number_join(impl, remainder, depth, true);
    }
    if (value < 1000000000u) {
        if (!emit_ascii_word(impl, "milioni") ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_number(impl, value / 1000000u, depth + 1u)) {
            return false;
        }
        remainder = value % 1000000u;
        return remainder == 0u || emit_number_join(impl, remainder, depth, true);
    }
    if (!emit_ascii_word(impl, "bilioni") ||
        !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
        !emit_number(impl, value / 1000000000u, depth + 1u)) {
        return false;
    }
    remainder = value % 1000000000u;
    return remainder == 0u || emit_number_join(impl, remainder, depth, true);
}

nanotts_result_t nanotts_lang_sw_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info)
{
    uint32_t word[NANOTTS_LANG_WORD_MAX];
    size_t word_length = 0u;
    size_t i = 0u;
    bool saw_phone = false;
    bool word_all_upper = true;

    impl->event_count = 0u;
    nanotts_lang_set_info(info, 0u, NANOTTS_NPOS, 0u);

#define FLUSH_WORD() \
    do { \
        if (word_length > 0u) { \
            bool ok_ = word_all_upper && \
                    nanotts_lang_should_spell_acronym(word, word_length) \
                ? emit_acronym(impl, word, word_length) \
                : emit_word(impl, word, word_length); \
            if (!ok_) { \
                nanotts_lang_set_info(info, impl->event_count, i, 0u); \
                return NANOTTS_ERR_TOO_MANY_EVENTS; \
            } \
            saw_phone = true; \
            word_length = 0u; \
            word_all_upper = true; \
        } \
    } while (0)

    while (i < length) {
        uint32_t cp;
        size_t width;
        if (!nanotts_lang_decode_utf8(text, length, i, &cp, &width)) {
            nanotts_lang_set_info(info, impl->event_count, i, 0u);
            return NANOTTS_ERR_UTF8;
        }

        if (is_letter_cp(cp) ||
            ((cp == '\'' || cp == 0x2019u || cp == 0x02bcu) &&
             word_length > 0u)) {
            if (word_length >= NANOTTS_LANG_WORD_MAX) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            if (is_letter_cp(cp) && !is_upper_cp(cp)) {
                word_all_upper = false;
            }
            word[word_length++] = lower_cp(cp);
            i += width;
            continue;
        }

        if (cp >= '0' && cp <= '9') {
            uint64_t value = 0u;
            FLUSH_WORD();
            while (i < length) {
                uint32_t digit_cp;
                size_t digit_width;
                if (!nanotts_lang_decode_utf8(
                        text, length, i, &digit_cp, &digit_width) ||
                    digit_cp < '0' || digit_cp > '9') {
                    break;
                }
                value = value * 10u + (uint64_t)(digit_cp - '0');
                if (value > UINT32_MAX) {
                    value = UINT32_MAX;
                }
                i += digit_width;
            }
            if (!emit_number(impl, (uint32_t)value, 0u)) {
                nanotts_lang_set_info(info, impl->event_count, i, 0u);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            saw_phone = true;
            continue;
        }

        FLUSH_WORD();
        if (cp == '.' || cp == '!' || cp == '?' || cp == ';' || cp == ':' ||
            cp == '\n' || cp == '\r' || cp == 0x2014u) {
            uint8_t duration = (cp == ';' || cp == ':') ? 76u : 100u;
            uint8_t flags = NANOTTS_EVENT_PHRASE_END;
            if (cp == '?') flags |= NANOTTS_EVENT_QUESTION;
            if (!nanotts_lang_add_pause_scaled(
                    impl, NANOTTS_PH_PHRASE_PAUSE, flags, duration)) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        } else if (cp == ',' || cp == '-' || cp == 0x2013u || cp == '/' ||
                   cp == ' ' || cp == '\t' || cp == 0x00a0u) {
            uint8_t duration = 50u;
            if (cp == ',') duration = 165u;
            else if (cp == '/' || cp == 0x2013u) duration = 120u;
            else if (cp == '-') duration = 80u;
            if (!nanotts_lang_add_pause_scaled(
                    impl, NANOTTS_PH_SHORT_PAUSE, 0u, duration)) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        }
        i += width;
    }
    FLUSH_WORD();
#undef FLUSH_WORD

    nanotts_lang_trim_trailing_short_pause(impl);
    nanotts_lang_set_info(info, impl->event_count, NANOTTS_NPOS, 0u);
    return saw_phone ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
