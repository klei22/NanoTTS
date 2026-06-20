/* SPDX-License-Identifier: MIT */
#include "lang/nanotts_lang_common.h"

#include <string.h>

static bool is_ascii_letter_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
           cp == 0x00e9u || cp == 0x00c9u || /* é */
           cp == 0x00e8u || cp == 0x00c8u || /* è */
           cp == 0x00eau || cp == 0x00cau;   /* ê */
}

static bool is_upper_letter_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') ||
           cp == 0x00c9u || cp == 0x00c8u || cp == 0x00cau;
}

static uint32_t lower_text_cp(uint32_t cp)
{
    if (cp >= 'A' && cp <= 'Z') {
        return cp + ('a' - 'A');
    }
    if (cp == 0x00c9u) return 0x00e9u;
    if (cp == 0x00c8u) return 0x00e8u;
    if (cp == 0x00cau) return 0x00eau;
    return cp;
}

typedef struct e_exception {
    const char *word;
    uint64_t open_mask;
    uint64_t close_mask;
} e_exception_t;

/*
 * Small Malay ambiguity aid. Plain 'e' defaults to schwa. Standard Rumi
 * spelling does not distinguish e pepet /ə/ from e taling /e/. Applications
 * can force é=/e/, è=/ɛ/, and ê=/ə/, or use the IPA API.
 */
static const e_exception_t E_EXCEPTIONS[] = {
    { "ekor",    0u, (UINT64_C(1) << 0) },
    { "elak",    0u, (UINT64_C(1) << 0) },
    { "erat",    0u, (UINT64_C(1) << 0) },
    { "esok",    0u, (UINT64_C(1) << 0) },
    { "kereta",  0u, (UINT64_C(1) << 3) },
    { "leher",   0u, (UINT64_C(1) << 1) | (UINT64_C(1) << 3) },
    { "leka",    0u, (UINT64_C(1) << 1) },
    { "leper",   0u, (UINT64_C(1) << 1) | (UINT64_C(1) << 3) },
    { "meja",    0u, (UINT64_C(1) << 1) },
    { "nenek",   0u, (UINT64_C(1) << 1) | (UINT64_C(1) << 3) },
    { "sate",    0u, (UINT64_C(1) << 3) },
    { "teksi",   0u, (UINT64_C(1) << 1) }
};

static void lookup_e_masks(
    const uint32_t *word,
    size_t length,
    uint64_t *open_mask,
    uint64_t *close_mask)
{
    char ascii[NANOTTS_LANG_WORD_MAX + 1u];
    size_t i;
    size_t n = length < sizeof(ascii) - 1u ? length : sizeof(ascii) - 1u;
    *open_mask = 0u;
    *close_mask = 0u;
    for (i = 0u; i < n; ++i) {
        uint32_t cp = word[i];
        ascii[i] = cp < 128u ? (char)cp : 'e';
    }
    ascii[n] = '\0';
    for (i = 0u; i < sizeof(E_EXCEPTIONS) / sizeof(E_EXCEPTIONS[0]); ++i) {
        if (strcmp(ascii, E_EXCEPTIONS[i].word) == 0) {
            *open_mask = E_EXCEPTIONS[i].open_mask;
            *close_mask = E_EXCEPTIONS[i].close_mask;
            return;
        }
    }
}

static nanotts_phone_t choose_plain_e(
    size_t length,
    size_t position,
    uint64_t open_mask,
    uint64_t close_mask)
{
    if (position < 64u && (open_mask & (UINT64_C(1) << position)) != 0u) {
        return NANOTTS_PH_OPEN_E;
    }
    if (position < 64u && (close_mask & (UINT64_C(1) << position)) != 0u) {
        return NANOTTS_PH_E;
    }
    /* Word-final written e is normally a loanword/open-syllable /e/. */
    if (position + 1u == length) return NANOTTS_PH_E;
    return NANOTTS_PH_SCHWA;
}

static bool emit_word(
    nanotts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    nanotts_event_t phones[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t phone_count = 0u;
    size_t i = 0u;
    uint8_t vowel_indices[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t vowel_count = 0u;
    uint64_t e_open_mask;
    uint64_t e_close_mask;
    size_t j;

    lookup_e_masks(word, length, &e_open_mask, &e_close_mask);

    while (i < length) {
        uint32_t c0 = word[i];
        uint32_t c1 = i + 1u < length ? word[i + 1u] : 0u;
        nanotts_phone_t phone;

        if (c0 == 'n' && c1 == 'g') {
            phone = NANOTTS_PH_NG;
            i += 2u;
        } else if (c0 == 'n' && c1 == 'y') {
            phone = NANOTTS_PH_NY;
            i += 2u;
        } else if (c0 == 's' && c1 == 'y') {
            phone = NANOTTS_PH_SH;
            i += 2u;
        } else if (c0 == 'k' && c1 == 'h') {
            phone = NANOTTS_PH_X;
            i += 2u;
        } else if (c0 == 'g' && c1 == 'h') {
            phone = NANOTTS_PH_GH;
            i += 2u;
        } else if (i + 2u == length && c0 == 'a' && c1 == 'i') {
            phone = NANOTTS_PH_AI;
            i += 2u;
        } else if (i + 2u == length && c0 == 'a' && c1 == 'u') {
            phone = NANOTTS_PH_AU;
            i += 2u;
        } else if (i + 2u == length && c0 == 'o' && c1 == 'i') {
            phone = NANOTTS_PH_OI;
            i += 2u;
        } else {
            switch (c0) {
            case 'a': phone = NANOTTS_PH_A; break;
            case 'e': phone = choose_plain_e(length, i, e_open_mask, e_close_mask); break;
            case 0x00e9u: phone = NANOTTS_PH_E; break;       /* é */
            case 0x00e8u: phone = NANOTTS_PH_OPEN_E; break;  /* è */
            case 0x00eau: phone = NANOTTS_PH_SCHWA; break;   /* ê */
            case 'i': phone = NANOTTS_PH_I; break;
            case 'o': phone = NANOTTS_PH_O; break;
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
                if (!nanotts_lang_temp_push(phones, &phone_count, NANOTTS_PH_K)) {
                    return false;
                }
                phone = NANOTTS_PH_S;
                break;
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

    for (j = 0u; j < phone_count; ++j) {
        if (nanotts_phone_is_vowel((nanotts_phone_t)phones[j].phone)) {
            vowel_indices[vowel_count++] = (uint8_t)j;
        }
    }
    if (vowel_count > 0u) {
        size_t stressed = vowel_count == 1u ? 0u : vowel_count - 2u;
        phones[vowel_indices[stressed]].flags |= NANOTTS_EVENT_STRESS_PRIMARY;
    }

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
    if (phone_count > 0u) {
        impl->events[impl->event_count - 1u].flags |= NANOTTS_EVENT_WORD_END;
    }
    return true;
}

static bool emit_letter_name(nanotts_impl_t *impl, uint32_t letter)
{
    nanotts_phone_t phones[4];
    size_t count = 0u;
    size_t stress = NANOTTS_NPOS;
    size_t i;

#define LETTER_PHONE(phone_) do { phones[count++] = (phone_); } while (0)
#define LETTER_VOWEL(phone_) do { stress = count; phones[count++] = (phone_); } while (0)
    switch (letter) {
    case 'a': LETTER_VOWEL(NANOTTS_PH_A); break;
    case 'b': LETTER_PHONE(NANOTTS_PH_B); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'c': LETTER_PHONE(NANOTTS_PH_CH); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'd': LETTER_PHONE(NANOTTS_PH_D); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'e': LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'f': LETTER_VOWEL(NANOTTS_PH_OPEN_E); LETTER_PHONE(NANOTTS_PH_F); break;
    case 'g': LETTER_PHONE(NANOTTS_PH_G); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'h': LETTER_PHONE(NANOTTS_PH_H); LETTER_VOWEL(NANOTTS_PH_A); break;
    case 'i': LETTER_VOWEL(NANOTTS_PH_I); break;
    case 'j': LETTER_PHONE(NANOTTS_PH_J); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'k': LETTER_PHONE(NANOTTS_PH_K); LETTER_VOWEL(NANOTTS_PH_A); break;
    case 'l': LETTER_VOWEL(NANOTTS_PH_OPEN_E); LETTER_PHONE(NANOTTS_PH_L); break;
    case 'm': LETTER_VOWEL(NANOTTS_PH_OPEN_E); LETTER_PHONE(NANOTTS_PH_M); break;
    case 'n': LETTER_VOWEL(NANOTTS_PH_OPEN_E); LETTER_PHONE(NANOTTS_PH_N); break;
    case 'o': LETTER_VOWEL(NANOTTS_PH_O); break;
    case 'p': LETTER_PHONE(NANOTTS_PH_P); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'q': LETTER_PHONE(NANOTTS_PH_K); LETTER_VOWEL(NANOTTS_PH_I); break;
    case 'r': LETTER_VOWEL(NANOTTS_PH_OPEN_E); LETTER_PHONE(NANOTTS_PH_R); break;
    case 's': LETTER_VOWEL(NANOTTS_PH_SCHWA); LETTER_PHONE(NANOTTS_PH_S); break;
    case 't': LETTER_PHONE(NANOTTS_PH_T); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'u': LETTER_VOWEL(NANOTTS_PH_U); break;
    case 'v': LETTER_PHONE(NANOTTS_PH_V); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'w': LETTER_PHONE(NANOTTS_PH_W); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'x': LETTER_VOWEL(NANOTTS_PH_OPEN_E); LETTER_PHONE(NANOTTS_PH_K); LETTER_PHONE(NANOTTS_PH_S); break;
    case 'y': LETTER_PHONE(NANOTTS_PH_Y); LETTER_VOWEL(NANOTTS_PH_E); break;
    case 'z': LETTER_PHONE(NANOTTS_PH_Z); LETTER_VOWEL(NANOTTS_PH_E); LETTER_PHONE(NANOTTS_PH_T); break;
    default: return true;
    }
#undef LETTER_VOWEL
#undef LETTER_PHONE

    for (i = 0u; i < count; ++i) {
        uint8_t flags = i == stress ? NANOTTS_EVENT_STRESS_SECONDARY : 0u;
        if (!nanotts_push_event(impl, phones[i], flags, 100u, 0)) {
            return false;
        }
    }
    return true;
}

static bool emit_acronym(
    nanotts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        if (!emit_letter_name(impl, word[i])) {
            return false;
        }
        if (i + 1u < length &&
            !nanotts_lang_add_pause_scaled(impl, NANOTTS_PH_SHORT_PAUSE, 0u, 30u)) {
            return false;
        }
    }
    if (impl->event_count > 0u &&
        !nanotts_phone_is_pause((nanotts_phone_t)impl->events[impl->event_count - 1u].phone)) {
        impl->events[impl->event_count - 1u].flags |= NANOTTS_EVENT_WORD_END;
    }
    return true;
}

static bool emit_ascii_word(nanotts_impl_t *impl, const char *word)
{
    uint32_t letters[NANOTTS_LANG_WORD_MAX];
    size_t n = 0u;
    while (*word != '\0' && n < NANOTTS_LANG_WORD_MAX) {
        letters[n++] = (uint32_t)(unsigned char)*word++;
    }
    return emit_word(impl, letters, n);
}

static bool emit_number(nanotts_impl_t *impl, uint32_t value, unsigned depth)
{
    static const char *const ones[] = {
        "sifar", "satu", "dua", "tiga", "empat",
        "lima", "enam", "tujuh", "lapan", "sembilan"
    };
    if (depth > 12u) {
        return false;
    }
    if (value < 10u) {
        return emit_ascii_word(impl, ones[value]);
    }
    if (value == 10u) return emit_ascii_word(impl, "sepuluh");
    if (value == 11u) return emit_ascii_word(impl, "sebelas");
    if (value < 20u) {
        return emit_number(impl, value - 10u, depth + 1u) &&
               nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
               emit_ascii_word(impl, "belas");
    }
    if (value < 100u) {
        if (!emit_number(impl, value / 10u, depth + 1u) ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "puluh")) {
            return false;
        }
        if ((value % 10u) != 0u) {
            return nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
                   emit_number(impl, value % 10u, depth + 1u);
        }
        return true;
    }
    if (value < 200u) {
        if (!emit_ascii_word(impl, "seratus")) return false;
        return (value == 100u) ||
               (nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value - 100u, depth + 1u));
    }
    if (value < 1000u) {
        if (!emit_number(impl, value / 100u, depth + 1u) ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "ratus")) {
            return false;
        }
        return (value % 100u == 0u) ||
               (nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value % 100u, depth + 1u));
    }
    if (value < 2000u) {
        if (!emit_ascii_word(impl, "seribu")) return false;
        return (value == 1000u) ||
               (nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value - 1000u, depth + 1u));
    }
    if (value < 1000000u) {
        if (!emit_number(impl, value / 1000u, depth + 1u) ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "ribu")) {
            return false;
        }
        return (value % 1000u == 0u) ||
               (nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value % 1000u, depth + 1u));
    }
    if (value < 1000000000u) {
        if (!emit_number(impl, value / 1000000u, depth + 1u) ||
            !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "juta")) {
            return false;
        }
        return (value % 1000000u == 0u) ||
               (nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value % 1000000u, depth + 1u));
    }
    if (!emit_number(impl, value / 1000000000u, depth + 1u) ||
        !nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) ||
        !emit_ascii_word(impl, "bilion")) {
        return false;
    }
    return (value % 1000000000u == 0u) ||
           (nanotts_lang_add_pause(impl, NANOTTS_PH_SHORT_PAUSE, 0u) &&
            emit_number(impl, value % 1000000000u, depth + 1u));
}

nanotts_result_t nanotts_lang_ms_parse_text(
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

        if (is_ascii_letter_cp(cp)) {
            if (word_length >= NANOTTS_LANG_WORD_MAX) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            if (!is_upper_letter_cp(cp)) {
                word_all_upper = false;
            }
            word[word_length++] = lower_text_cp(cp);
            i += width;
            continue;
        }

        if (cp >= '0' && cp <= '9') {
            uint64_t value = 0u;
            FLUSH_WORD();
            while (i < length) {
                uint32_t digit_cp;
                size_t digit_width;
                if (!nanotts_lang_decode_utf8(text, length, i, &digit_cp, &digit_width) ||
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
            if (!nanotts_lang_add_pause_scaled(impl, NANOTTS_PH_PHRASE_PAUSE,
                    flags, duration)) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        } else if (cp == ',' || cp == '-' || cp == 0x2013u || cp == '/' ||
                   cp == ' ' || cp == '\t' || cp == 0x00a0u) {
            uint8_t duration = 50u;
            if (cp == ',') duration = 165u;
            else if (cp == '/' || cp == 0x2013u) duration = 120u;
            else if (cp == '-') duration = 80u;
            if (!nanotts_lang_add_pause_scaled(impl, NANOTTS_PH_SHORT_PAUSE, 0u, duration)) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        }
        /* Apostrophes and other symbols are treated as zero-width boundaries. */
        i += width;
    }
    FLUSH_WORD();
#undef FLUSH_WORD

    while (impl->event_count > 0u &&
           impl->events[impl->event_count - 1u].phone == (uint8_t)NANOTTS_PH_SHORT_PAUSE) {
        impl->event_count--;
    }
    nanotts_lang_set_info(info, impl->event_count, NANOTTS_NPOS, 0u);
    return saw_phone ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
