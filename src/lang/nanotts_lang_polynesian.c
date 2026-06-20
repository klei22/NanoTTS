/* SPDX-License-Identifier: MIT */
#include "lang/nanotts_lang_polynesian.h"
#include "lang/nanotts_lang_common.h"

#include <string.h>

typedef struct poly_char {
    uint8_t base;       /* normalized ASCII letter or apostrophe */
    uint8_t long_vowel;
} poly_char_t;

typedef struct poly_nucleus {
    uint8_t long_vowel;
    uint8_t diphthong;
    uint8_t moras;
} poly_nucleus_t;

typedef struct poly_config {
    nanotts_polynesian_language_t language;
    const char *const *digits;
} poly_config_t;

static const char *const MAORI_DIGITS[10] = {
    "kore", "tahi", "rua", "toru", "whā",
    "rima", "ono", "whitu", "waru", "iwa"
};

static const char *const HAWAIIAN_DIGITS[10] = {
    "ʻole", "ʻekahi", "ʻelua", "ʻekolu", "ʻehā",
    "ʻelima", "ʻeono", "ʻehiku", "ʻewalu", "ʻeiwa"
};


static bool is_vowel_base(uint32_t cp)
{
    return cp == (uint32_t)'a' || cp == (uint32_t)'e' ||
           cp == (uint32_t)'i' || cp == (uint32_t)'o' ||
           cp == (uint32_t)'u';
}

static bool normalize_letter(uint32_t cp, poly_char_t *out)
{
    out->long_vowel = 0u;
    if (cp >= (uint32_t)'A' && cp <= (uint32_t)'Z') {
        out->base = (uint8_t)(cp + ((uint32_t)'a' - (uint32_t)'A'));
        return true;
    }
    if (cp >= (uint32_t)'a' && cp <= (uint32_t)'z') {
        out->base = (uint8_t)cp;
        return true;
    }
    switch (cp) {
    case 0x0100u: case 0x0101u: out->base = (uint8_t)'a'; break; /* Ā ā */
    case 0x0112u: case 0x0113u: out->base = (uint8_t)'e'; break; /* Ē ē */
    case 0x012Au: case 0x012Bu: out->base = (uint8_t)'i'; break; /* Ī ī */
    case 0x014Cu: case 0x014Du: out->base = (uint8_t)'o'; break; /* Ō ō */
    case 0x016Au: case 0x016Bu: out->base = (uint8_t)'u'; break; /* Ū ū */
    default: return false;
    }
    out->long_vowel = 1u;
    return true;
}

static bool is_okina(uint32_t cp)
{
    return cp == 0x02BBu || cp == 0x2018u || cp == 0x2019u ||
           cp == (uint32_t)'\'';
}

static bool temp_push_event(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t phone,
    uint8_t flags,
    uint8_t duration)
{
    if (*count >= NANOTTS_LANG_WORD_PHONE_MAX) return false;
    events[*count].phone = (uint8_t)phone;
    events[*count].flags = flags;
    events[*count].duration_percent = duration == 0u ? 100u : duration;
    events[*count].pitch_semitones_q4 = 0;
    (*count)++;
    return true;
}

static nanotts_phone_t vowel_phone(uint32_t cp)
{
    switch (cp) {
    case 'a': return NANOTTS_PH_A;
    case 'e': return NANOTTS_PH_E;
    case 'i': return NANOTTS_PH_I;
    case 'o': return NANOTTS_PH_O;
    case 'u': return NANOTTS_PH_U;
    default: return NANOTTS_PH_SILENCE;
    }
}

static bool is_maori_diphthong(uint32_t first, uint32_t second)
{
    if (!is_vowel_base(first) || !is_vowel_base(second)) return false;
    if (first == 'a') {
        return second == 'e' || second == 'i' || second == 'o' || second == 'u';
    }
    if (first == 'e') return second == 'i';
    if (first == 'o') return second == 'i' || second == 'u';
    return false;
}

static bool is_hawaiian_diphthong(uint32_t first, uint32_t second)
{
    if (!is_vowel_base(first) || !is_vowel_base(second)) return false;
    if (first == 'a') {
        return second == 'e' || second == 'i' || second == 'o' || second == 'u';
    }
    if (first == 'e') return second == 'i' || second == 'u';
    if (first == 'i') return second == 'u';
    if (first == 'o') return second == 'i' || second == 'u';
    if (first == 'u') return second == 'i';
    return false;
}

static bool is_language_diphthong(
    const poly_config_t *config,
    uint32_t first,
    uint32_t second)
{
    return config->language == NANOTTS_POLYNESIAN_MAORI
        ? is_maori_diphthong(first, second)
        : is_hawaiian_diphthong(first, second);
}

static nanotts_phone_t compact_diphthong_phone(uint32_t first, uint32_t second)
{
    if (first == 'a' && second == 'i') return NANOTTS_PH_AI;
    if (first == 'a' && second == 'u') return NANOTTS_PH_AU;
    if (first == 'o' && second == 'i') return NANOTTS_PH_OI;
    return NANOTTS_PH_SILENCE;
}

static void apply_maori_stress(
    nanotts_event_t *events,
    const uint8_t *nucleus_event,
    const poly_nucleus_t *nuclei,
    size_t nucleus_count)
{
    size_t chosen = NANOTTS_NPOS;
    size_t index;

    for (index = 0u; index < nucleus_count; ++index) {
        if (nuclei[index].long_vowel != 0u) {
            chosen = index;
            break;
        }
    }
    if (chosen == NANOTTS_NPOS) {
        for (index = 0u; index < nucleus_count; ++index) {
            if (nuclei[index].diphthong != 0u) {
                chosen = index;
                break;
            }
        }
    }
    if (chosen == NANOTTS_NPOS && nucleus_count != 0u) chosen = 0u;
    if (chosen != NANOTTS_NPOS) {
        events[nucleus_event[chosen]].flags |= NANOTTS_EVENT_STRESS_PRIMARY;
    }
    if (nucleus_count >= 5u) {
        size_t secondary = nucleus_count - 2u;
        if (secondary != chosen) {
            events[nucleus_event[secondary]].flags |= NANOTTS_EVENT_STRESS_SECONDARY;
        }
    }
}

static void apply_hawaiian_stress(
    nanotts_event_t *events,
    const uint8_t *nucleus_event,
    const poly_nucleus_t *nuclei,
    size_t nucleus_count)
{
    size_t total_moras = 0u;
    size_t rightmost_heavy = NANOTTS_NPOS;
    size_t index;

    /* Long vowels and diphthongs are heavy. All heavy nuclei are prominent;
     * the rightmost receives primary stress and preceding heavy nuclei receive
     * secondary stress. This is a compact approximation for compounds and long
     * words, whose lexical stress may need an IPA override. */
    for (index = 0u; index < nucleus_count; ++index) {
        total_moras += nuclei[index].moras;
        if (nuclei[index].moras >= 2u) {
            events[nucleus_event[index]].flags |= NANOTTS_EVENT_STRESS_SECONDARY;
            rightmost_heavy = index;
        }
    }
    if (rightmost_heavy != NANOTTS_NPOS) {
        events[nucleus_event[rightmost_heavy]].flags &=
            (uint8_t)~NANOTTS_EVENT_STRESS_SECONDARY;
        events[nucleus_event[rightmost_heavy]].flags |=
            NANOTTS_EVENT_STRESS_PRIMARY;
        return;
    }

    if (total_moras != 0u) {
        size_t target = total_moras == 1u ? 0u : total_moras - 2u;
        size_t running = 0u;
        for (index = 0u; index < nucleus_count; ++index) {
            if (target < running + nuclei[index].moras) {
                events[nucleus_event[index]].flags |=
                    NANOTTS_EVENT_STRESS_PRIMARY;
                break;
            }
            running += nuclei[index].moras;
        }
    }
}

static bool emit_word(
    nanotts_impl_t *impl,
    const poly_char_t *word,
    size_t length,
    const poly_config_t *config)
{
    nanotts_event_t events[NANOTTS_LANG_WORD_PHONE_MAX];
    uint8_t nucleus_event[NANOTTS_LANG_WORD_PHONE_MAX];
    poly_nucleus_t nuclei[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t event_count = 0u;
    size_t nucleus_count = 0u;
    size_t index = 0u;
    size_t out_index;

    while (index < length) {
        uint32_t c0 = word[index].base;
        uint32_t c1 = index + 1u < length ? word[index + 1u].base : 0u;
        nanotts_phone_t phone = NANOTTS_PH_SILENCE;
        uint8_t flags = 0u;
        uint8_t duration = 100u;

        if (config->language == NANOTTS_POLYNESIAN_HAWAIIAN && c0 == '\'') {
            phone = NANOTTS_PH_GLOTTAL_STOP;
            index++;
        } else if (config->language == NANOTTS_POLYNESIAN_MAORI &&
                   c0 == 'n' && c1 == 'g') {
            phone = NANOTTS_PH_NG;
            index += 2u;
        } else if (config->language == NANOTTS_POLYNESIAN_MAORI &&
                   c0 == 'w' && c1 == 'h') {
            phone = NANOTTS_PH_F;
            index += 2u;
        } else if (is_vowel_base(c0)) {
            bool doubled_long =
                config->language == NANOTTS_POLYNESIAN_MAORI &&
                index + 1u < length && c0 == c1 &&
                word[index].long_vowel == 0u &&
                word[index + 1u].long_vowel == 0u;
            bool diphthong = !doubled_long && index + 1u < length &&
                word[index].long_vowel == 0u &&
                word[index + 1u].long_vowel == 0u &&
                is_language_diphthong(config, c0, c1);
            nanotts_phone_t compact = diphthong
                ? compact_diphthong_phone(c0, c1)
                : NANOTTS_PH_SILENCE;

            if (nucleus_count >= NANOTTS_LANG_WORD_PHONE_MAX) return false;
            nucleus_event[nucleus_count] = (uint8_t)event_count;
            nuclei[nucleus_count].long_vowel =
                (word[index].long_vowel != 0u || doubled_long) ? 1u : 0u;
            nuclei[nucleus_count].diphthong = diphthong ? 1u : 0u;
            nuclei[nucleus_count].moras =
                (word[index].long_vowel != 0u || doubled_long || diphthong)
                    ? 2u : 1u;
            nucleus_count++;

            if (diphthong && compact != NANOTTS_PH_SILENCE) {
                phone = compact;
                duration = 125u;
                index += 2u;
            } else {
                phone = vowel_phone(c0);
                if (word[index].long_vowel != 0u || doubled_long) {
                    flags |= NANOTTS_EVENT_LONG;
                    duration = 150u;
                }
                index += doubled_long ? 2u : 1u;
                if (diphthong) {
                    if (!temp_push_event(
                            events, &event_count, phone, flags, 108u) ||
                        !temp_push_event(
                            events, &event_count, vowel_phone(c1), 0u, 92u)) {
                        return false;
                    }
                    index++;
                    continue;
                }
            }
        } else {
            switch (c0) {
            case 'h': phone = NANOTTS_PH_H; break;
            case 'k': case 'c': case 'q': phone = NANOTTS_PH_K; break;
            case 'm': phone = NANOTTS_PH_M; break;
            case 'n': phone = NANOTTS_PH_N; break;
            case 'p': phone = NANOTTS_PH_P; break;
            case 'l': phone = NANOTTS_PH_L; break;
            case 'r': phone = config->language == NANOTTS_POLYNESIAN_MAORI
                                ? NANOTTS_PH_TAP : NANOTTS_PH_L; break;
            case 't': phone = NANOTTS_PH_T; break;
            case 'w': phone = NANOTTS_PH_W; break;
            case 'f': phone = NANOTTS_PH_F; break;
            case 's': phone = NANOTTS_PH_S; break;
            case 'b': phone = NANOTTS_PH_B; break;
            case 'd': phone = NANOTTS_PH_D; break;
            case 'g': phone = NANOTTS_PH_G; break;
            case 'j': case 'y': phone = NANOTTS_PH_Y; break;
            case 'v': phone = NANOTTS_PH_V; break;
            case 'z': phone = NANOTTS_PH_Z; break;
            case 'x':
                if (!temp_push_event(
                        events, &event_count, NANOTTS_PH_K, 0u, 100u)) {
                    return false;
                }
                phone = NANOTTS_PH_S;
                break;
            default:
                index++;
                continue;
            }
            index++;
        }

        if (!temp_push_event(events, &event_count, phone, flags, duration)) {
            return false;
        }
    }

    if (config->language == NANOTTS_POLYNESIAN_MAORI) {
        apply_maori_stress(events, nucleus_event, nuclei, nucleus_count);
    } else {
        apply_hawaiian_stress(events, nucleus_event, nuclei, nucleus_count);
    }

    for (out_index = 0u; out_index < event_count; ++out_index) {
        if (!nanotts_push_event(
                impl,
                (nanotts_phone_t)events[out_index].phone,
                events[out_index].flags,
                events[out_index].duration_percent,
                events[out_index].pitch_semitones_q4)) {
            return false;
        }
    }
    nanotts_lang_mark_word_end(impl);
    return true;
}

static bool append_normalized_word(
    const char *utf8,
    poly_char_t *word,
    size_t *length,
    const poly_config_t *config)
{
    size_t offset = 0u;
    size_t bytes = strlen(utf8);
    while (offset < bytes) {
        uint32_t cp;
        size_t width;
        poly_char_t value;
        if (!nanotts_lang_decode_utf8(utf8, bytes, offset, &cp, &width)) {
            return false;
        }
        if (normalize_letter(cp, &value)) {
            if (*length >= NANOTTS_LANG_WORD_MAX) return false;
            word[(*length)++] = value;
        } else if (cp == 0x0304u && *length > 0u &&
                   is_vowel_base(word[*length - 1u].base)) {
            word[*length - 1u].long_vowel = 1u;
        } else if (config->language == NANOTTS_POLYNESIAN_HAWAIIAN &&
                   is_okina(cp)) {
            if (*length >= NANOTTS_LANG_WORD_MAX) return false;
            word[*length].base = '\'';
            word[*length].long_vowel = 0u;
            (*length)++;
        }
        offset += width;
    }
    return true;
}

static bool emit_utf8_word(
    nanotts_impl_t *impl,
    const char *utf8,
    const poly_config_t *config)
{
    poly_char_t word[NANOTTS_LANG_WORD_MAX];
    size_t length = 0u;
    if (!append_normalized_word(utf8, word, &length, config)) return false;
    return emit_word(impl, word, length, config);
}

static bool emit_digits(
    nanotts_impl_t *impl,
    const char *text,
    size_t start,
    size_t end,
    const poly_config_t *config)
{
    size_t index;
    for (index = start; index < end; ++index) {
        unsigned digit = (unsigned)((unsigned char)text[index] - (unsigned char)'0');
        if (!emit_utf8_word(impl, config->digits[digit], config)) return false;
        if (index + 1u < end &&
            !nanotts_lang_add_pause_scaled(
                impl, NANOTTS_PH_SHORT_PAUSE, 0u, 32u)) {
            return false;
        }
    }
    return true;
}

nanotts_result_t nanotts_lang_polynesian_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info,
    nanotts_polynesian_language_t language)
{
    poly_char_t word[NANOTTS_LANG_WORD_MAX];
    poly_config_t config;
    size_t word_length = 0u;
    size_t offset = 0u;
    bool saw_phone = false;

    config.language = language;
    config.digits = language == NANOTTS_POLYNESIAN_MAORI
        ? MAORI_DIGITS : HAWAIIAN_DIGITS;
    impl->event_count = 0u;
    nanotts_lang_set_info(info, 0u, NANOTTS_NPOS, 0u);

#define FLUSH_WORD() \
    do { \
        if (word_length != 0u) { \
            if (!emit_word(impl, word, word_length, &config)) { \
                nanotts_lang_set_info(info, impl->event_count, offset, 0u); \
                return NANOTTS_ERR_TOO_MANY_EVENTS; \
            } \
            saw_phone = true; \
            word_length = 0u; \
        } \
    } while (0)

    while (offset < length) {
        uint32_t cp;
        size_t width;
        poly_char_t normalized;
        if (!nanotts_lang_decode_utf8(text, length, offset, &cp, &width)) {
            nanotts_lang_set_info(info, impl->event_count, offset, 0u);
            return NANOTTS_ERR_UTF8;
        }

        if (normalize_letter(cp, &normalized)) {
            if (word_length >= NANOTTS_LANG_WORD_MAX) {
                nanotts_lang_set_info(info, impl->event_count, offset, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            word[word_length++] = normalized;
            offset += width;
            continue;
        }
        if (cp == 0x0304u && word_length > 0u &&
            is_vowel_base(word[word_length - 1u].base)) {
            word[word_length - 1u].long_vowel = 1u;
            offset += width;
            continue;
        }
        if (language == NANOTTS_POLYNESIAN_HAWAIIAN && is_okina(cp)) {
            if (word_length >= NANOTTS_LANG_WORD_MAX) {
                nanotts_lang_set_info(info, impl->event_count, offset, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            word[word_length].base = '\'';
            word[word_length].long_vowel = 0u;
            word_length++;
            offset += width;
            continue;
        }

        if (cp >= (uint32_t)'0' && cp <= (uint32_t)'9') {
            size_t digit_start;
            FLUSH_WORD();
            digit_start = offset;
            while (offset < length &&
                   (unsigned char)text[offset] >= (unsigned char)'0' &&
                   (unsigned char)text[offset] <= (unsigned char)'9') {
                offset++;
            }
            if (!emit_digits(impl, text, digit_start, offset, &config)) {
                nanotts_lang_set_info(info, impl->event_count, offset, 0u);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            saw_phone = true;
            continue;
        }

        FLUSH_WORD();
        if (cp == '.' || cp == '!' || cp == '?' || cp == ';' || cp == ':' ||
            cp == '\n' || cp == '\r' || cp == 0x2014u) {
            uint8_t pause_flags = NANOTTS_EVENT_PHRASE_END;
            uint8_t duration = (cp == ';' || cp == ':') ? 76u : 100u;
            if (cp == '?') pause_flags |= NANOTTS_EVENT_QUESTION;
            if (!nanotts_lang_add_pause_scaled(
                    impl, NANOTTS_PH_PHRASE_PAUSE,
                    pause_flags, duration)) {
                nanotts_lang_set_info(info, impl->event_count, offset, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        } else if (cp == ',' || cp == '-' || cp == 0x2013u || cp == '/' ||
                   cp == ' ' || cp == '\t' || cp == 0x00A0u) {
            uint8_t duration = 50u;
            if (cp == ',') duration = 150u;
            else if (cp == '/' || cp == 0x2013u) duration = 115u;
            else if (cp == '-') duration = 75u;
            if (!nanotts_lang_add_pause_scaled(
                    impl, NANOTTS_PH_SHORT_PAUSE, 0u, duration)) {
                nanotts_lang_set_info(info, impl->event_count, offset, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        }
        offset += width;
    }
    FLUSH_WORD();
#undef FLUSH_WORD

    nanotts_lang_trim_trailing_short_pause(impl);
    nanotts_lang_set_info(info, impl->event_count, NANOTTS_NPOS, 0u);
    return saw_phone ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
