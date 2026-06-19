/* SPDX-License-Identifier: MIT */
#include "idtts_internal.h"

#include <ctype.h>
#include <string.h>

#define IDTTS_WORD_MAX 80u
#define IDTTS_WORD_PHONE_MAX 96u

static bool decode_utf8_text(
    const char *text,
    size_t length,
    size_t offset,
    uint32_t *cp,
    size_t *width)
{
    const unsigned char *s = (const unsigned char *)text;
    unsigned char c0;
    if (offset >= length) {
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

static bool add_text_pause_scaled(
    idtts_impl_t *impl,
    idtts_phone_t pause,
    uint8_t flags,
    uint8_t duration_percent)
{
    if (impl->event_count == 0u) {
        return true;
    }
    if (idtts_phone_is_pause((idtts_phone_t)impl->events[impl->event_count - 1u].phone)) {
        idtts_event_t *previous = &impl->events[impl->event_count - 1u];
        if (pause == IDTTS_PH_PHRASE_PAUSE) {
            previous->phone = (uint8_t)pause;
            previous->flags |= flags;
        }
        if (duration_percent > previous->duration_percent) {
            previous->duration_percent = duration_percent;
        }
        return true;
    }
    return idtts_push_event(impl, pause, flags, duration_percent, 0);
}

static bool add_text_pause(idtts_impl_t *impl, idtts_phone_t pause, uint8_t flags)
{
    uint8_t duration = pause == IDTTS_PH_PHRASE_PAUSE ? 100u : 50u;
    return add_text_pause_scaled(impl, pause, flags, duration);
}

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

static bool phone_temp_push(idtts_event_t *events, size_t *count, idtts_phone_t phone)
{
    if (*count >= IDTTS_WORD_PHONE_MAX) {
        return false;
    }
    events[*count].phone = (uint8_t)phone;
    events[*count].flags = 0u;
    events[*count].duration_percent = 100u;
    events[*count].pitch_semitones_q4 = 0;
    (*count)++;
    return true;
}

typedef struct e_exception {
    const char *word;
    uint64_t open_mask;
    uint64_t close_mask;
} e_exception_t;

/*
 * Small, independently authored ambiguity aid. Plain 'e' defaults to schwa.
 * Users can always write é=/e/, è=/ɛ/, and ê=/ə/ or use the IPA API.
 */
static const e_exception_t E_EXCEPTIONS[] = {
    { "bebek",   (UINT64_C(1) << 1) | (UINT64_C(1) << 3), 0u },
    { "besar",   (UINT64_C(1) << 1), 0u },
    { "dengan",  (UINT64_C(1) << 1), 0u },
    { "enam",    (UINT64_C(1) << 0), 0u },
    { "empat",   (UINT64_C(1) << 0), 0u },
    { "ekspor",  (UINT64_C(1) << 0), 0u },
    { "ember",   (UINT64_C(1) << 0) | (UINT64_C(1) << 3), 0u },
    { "es",      0u, (UINT64_C(1) << 0) },
    { "kerja",   (UINT64_C(1) << 1), 0u },
    { "kecil",   (UINT64_C(1) << 1), 0u },
    { "leher",   0u, (UINT64_C(1) << 1) | (UINT64_C(1) << 3) },
    { "meja",    0u, (UINT64_C(1) << 1) },
    { "memang",  (UINT64_C(1) << 1), 0u },
    { "merah",   (UINT64_C(1) << 1), 0u },
    { "mereka",  (UINT64_C(1) << 3), 0u },
    { "nenek",   (UINT64_C(1) << 1) | (UINT64_C(1) << 3), 0u },
    { "pendek",  (UINT64_C(1) << 4), 0u },
    { "sore",    0u, (UINT64_C(1) << 3) },
    { "teks",    (UINT64_C(1) << 1), 0u },
    { "video",   (UINT64_C(1) << 3), 0u }
};

static void word_to_ascii(const uint32_t *word, size_t length, char *out, size_t out_size)
{
    size_t i;
    size_t n = length < out_size - 1u ? length : out_size - 1u;
    for (i = 0u; i < n; ++i) {
        uint32_t cp = word[i];
        out[i] = cp < 128u ? (char)cp : 'e';
    }
    out[n] = '\0';
}

static idtts_phone_t choose_plain_e(
    const uint32_t *word,
    size_t length,
    size_t position)
{
    char ascii[IDTTS_WORD_MAX + 1u];
    size_t i;
    word_to_ascii(word, length, ascii, sizeof(ascii));
    for (i = 0u; i < sizeof(E_EXCEPTIONS) / sizeof(E_EXCEPTIONS[0]); ++i) {
        if (strcmp(ascii, E_EXCEPTIONS[i].word) == 0) {
            if (position < 64u &&
                (E_EXCEPTIONS[i].open_mask & (UINT64_C(1) << position)) != 0u) {
                return IDTTS_PH_OPEN_E;
            }
            if (position < 64u &&
                (E_EXCEPTIONS[i].close_mask & (UINT64_C(1) << position)) != 0u) {
                return IDTTS_PH_E;
            }
            return IDTTS_PH_SCHWA;
        }
    }
    if (position + 1u == length) {
        return IDTTS_PH_E;
    }
    return IDTTS_PH_SCHWA;
}

static bool emit_word(
    idtts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    idtts_event_t phones[IDTTS_WORD_PHONE_MAX];
    size_t phone_count = 0u;
    size_t i = 0u;
    size_t vowel_indices[IDTTS_WORD_PHONE_MAX];
    size_t vowel_count = 0u;
    size_t j;

    while (i < length) {
        uint32_t c0 = word[i];
        uint32_t c1 = i + 1u < length ? word[i + 1u] : 0u;
        idtts_phone_t phone;

        if (c0 == 'n' && c1 == 'g') {
            phone = IDTTS_PH_NG;
            i += 2u;
        } else if (c0 == 'n' && c1 == 'y') {
            phone = IDTTS_PH_NY;
            i += 2u;
        } else if (c0 == 's' && c1 == 'y') {
            phone = IDTTS_PH_SH;
            i += 2u;
        } else if (c0 == 'k' && c1 == 'h') {
            phone = IDTTS_PH_X;
            i += 2u;
        } else if (c0 == 'a' && c1 == 'i') {
            phone = IDTTS_PH_AI;
            i += 2u;
        } else if (c0 == 'a' && c1 == 'u') {
            phone = IDTTS_PH_AU;
            i += 2u;
        } else if (c0 == 'o' && c1 == 'i') {
            phone = IDTTS_PH_OI;
            i += 2u;
        } else {
            switch (c0) {
            case 'a': phone = IDTTS_PH_A; break;
            case 'e': phone = choose_plain_e(word, length, i); break;
            case 0x00e9u: phone = IDTTS_PH_E; break;       /* é */
            case 0x00e8u: phone = IDTTS_PH_OPEN_E; break;  /* è */
            case 0x00eau: phone = IDTTS_PH_SCHWA; break;   /* ê */
            case 'i': phone = IDTTS_PH_I; break;
            case 'o': phone = IDTTS_PH_O; break;
            case 'u': phone = IDTTS_PH_U; break;
            case 'p': phone = IDTTS_PH_P; break;
            case 'b': phone = IDTTS_PH_B; break;
            case 't': phone = IDTTS_PH_T; break;
            case 'd': phone = IDTTS_PH_D; break;
            case 'k': case 'q': phone = IDTTS_PH_K; break;
            case 'g': phone = IDTTS_PH_G; break;
            case 'c': phone = IDTTS_PH_CH; break;
            case 'j': phone = IDTTS_PH_J; break;
            case 'f': phone = IDTTS_PH_F; break;
            case 'v': phone = IDTTS_PH_V; break;
            case 's': phone = IDTTS_PH_S; break;
            case 'z': phone = IDTTS_PH_Z; break;
            case 'h': phone = IDTTS_PH_H; break;
            case 'm': phone = IDTTS_PH_M; break;
            case 'n': phone = IDTTS_PH_N; break;
            case 'l': phone = IDTTS_PH_L; break;
            case 'r': phone = IDTTS_PH_R; break;
            case 'w': phone = IDTTS_PH_W; break;
            case 'y': phone = IDTTS_PH_Y; break;
            case 'x':
                if (!phone_temp_push(phones, &phone_count, IDTTS_PH_K)) {
                    return false;
                }
                phone = IDTTS_PH_S;
                break;
            default:
                i++;
                continue;
            }
            i++;
        }

        if (!phone_temp_push(phones, &phone_count, phone)) {
            return false;
        }
    }

    for (j = 0u; j < phone_count; ++j) {
        if (idtts_phone_is_vowel((idtts_phone_t)phones[j].phone)) {
            vowel_indices[vowel_count++] = j;
        }
    }
    if (vowel_count > 0u) {
        size_t stressed = vowel_count == 1u ? 0u : vowel_count - 2u;
        phones[vowel_indices[stressed]].flags |= IDTTS_EVENT_STRESS_PRIMARY;
    }

    for (j = 0u; j < phone_count; ++j) {
        if (!idtts_push_event(
                impl,
                (idtts_phone_t)phones[j].phone,
                phones[j].flags,
                phones[j].duration_percent,
                phones[j].pitch_semitones_q4)) {
            return false;
        }
    }
    if (phone_count > 0u) {
        impl->events[impl->event_count - 1u].flags |= IDTTS_EVENT_WORD_END;
    }
    return true;
}

static bool emit_letter_name(idtts_impl_t *impl, uint32_t letter)
{
    idtts_phone_t phones[4];
    size_t count = 0u;
    size_t stress = IDTTS_NPOS;
    size_t i;

#define LETTER_PHONE(phone_) do { phones[count++] = (phone_); } while (0)
#define LETTER_VOWEL(phone_) do { stress = count; phones[count++] = (phone_); } while (0)
    switch (letter) {
    case 'a': LETTER_VOWEL(IDTTS_PH_A); break;
    case 'b': LETTER_PHONE(IDTTS_PH_B); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'c': LETTER_PHONE(IDTTS_PH_CH); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'd': LETTER_PHONE(IDTTS_PH_D); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'e': LETTER_VOWEL(IDTTS_PH_E); break;
    case 'f': LETTER_VOWEL(IDTTS_PH_OPEN_E); LETTER_PHONE(IDTTS_PH_F); break;
    case 'g': LETTER_PHONE(IDTTS_PH_G); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'h': LETTER_PHONE(IDTTS_PH_H); LETTER_VOWEL(IDTTS_PH_A); break;
    case 'i': LETTER_VOWEL(IDTTS_PH_I); break;
    case 'j': LETTER_PHONE(IDTTS_PH_J); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'k': LETTER_PHONE(IDTTS_PH_K); LETTER_VOWEL(IDTTS_PH_A); break;
    case 'l': LETTER_VOWEL(IDTTS_PH_OPEN_E); LETTER_PHONE(IDTTS_PH_L); break;
    case 'm': LETTER_VOWEL(IDTTS_PH_OPEN_E); LETTER_PHONE(IDTTS_PH_M); break;
    case 'n': LETTER_VOWEL(IDTTS_PH_OPEN_E); LETTER_PHONE(IDTTS_PH_N); break;
    case 'o': LETTER_VOWEL(IDTTS_PH_O); break;
    case 'p': LETTER_PHONE(IDTTS_PH_P); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'q': LETTER_PHONE(IDTTS_PH_K); LETTER_VOWEL(IDTTS_PH_I); break;
    case 'r': LETTER_VOWEL(IDTTS_PH_OPEN_E); LETTER_PHONE(IDTTS_PH_R); break;
    case 's': LETTER_VOWEL(IDTTS_PH_SCHWA); LETTER_PHONE(IDTTS_PH_S); break;
    case 't': LETTER_PHONE(IDTTS_PH_T); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'u': LETTER_VOWEL(IDTTS_PH_U); break;
    case 'v': LETTER_PHONE(IDTTS_PH_V); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'w': LETTER_PHONE(IDTTS_PH_W); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'x': LETTER_VOWEL(IDTTS_PH_OPEN_E); LETTER_PHONE(IDTTS_PH_K); LETTER_PHONE(IDTTS_PH_S); break;
    case 'y': LETTER_PHONE(IDTTS_PH_Y); LETTER_VOWEL(IDTTS_PH_E); break;
    case 'z': LETTER_PHONE(IDTTS_PH_Z); LETTER_VOWEL(IDTTS_PH_E); LETTER_PHONE(IDTTS_PH_T); break;
    default: return true;
    }
#undef LETTER_VOWEL
#undef LETTER_PHONE

    for (i = 0u; i < count; ++i) {
        uint8_t flags = i == stress ? IDTTS_EVENT_STRESS_SECONDARY : 0u;
        if (!idtts_push_event(impl, phones[i], flags, 100u, 0)) {
            return false;
        }
    }
    return true;
}

static bool emit_acronym(
    idtts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        if (!emit_letter_name(impl, word[i])) {
            return false;
        }
        if (i + 1u < length &&
            !add_text_pause_scaled(impl, IDTTS_PH_SHORT_PAUSE, 0u, 30u)) {
            return false;
        }
    }
    if (impl->event_count > 0u &&
        !idtts_phone_is_pause((idtts_phone_t)impl->events[impl->event_count - 1u].phone)) {
        impl->events[impl->event_count - 1u].flags |= IDTTS_EVENT_WORD_END;
    }
    return true;
}

static bool emit_ascii_word(idtts_impl_t *impl, const char *word)
{
    uint32_t letters[IDTTS_WORD_MAX];
    size_t n = 0u;
    while (*word != '\0' && n < IDTTS_WORD_MAX) {
        letters[n++] = (uint32_t)(unsigned char)*word++;
    }
    return emit_word(impl, letters, n);
}

static bool emit_number(idtts_impl_t *impl, uint32_t value, unsigned depth)
{
    static const char *const ones[] = {
        "nol", "satu", "dua", "tiga", "empat",
        "lima", "enam", "tujuh", "delapan", "sembilan"
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
               add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
               emit_ascii_word(impl, "belas");
    }
    if (value < 100u) {
        if (!emit_number(impl, value / 10u, depth + 1u) ||
            !add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "puluh")) {
            return false;
        }
        if ((value % 10u) != 0u) {
            return add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
                   emit_number(impl, value % 10u, depth + 1u);
        }
        return true;
    }
    if (value < 200u) {
        if (!emit_ascii_word(impl, "seratus")) return false;
        return (value == 100u) ||
               (add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value - 100u, depth + 1u));
    }
    if (value < 1000u) {
        if (!emit_number(impl, value / 100u, depth + 1u) ||
            !add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "ratus")) {
            return false;
        }
        return (value % 100u == 0u) ||
               (add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value % 100u, depth + 1u));
    }
    if (value < 2000u) {
        if (!emit_ascii_word(impl, "seribu")) return false;
        return (value == 1000u) ||
               (add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value - 1000u, depth + 1u));
    }
    if (value < 1000000u) {
        if (!emit_number(impl, value / 1000u, depth + 1u) ||
            !add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "ribu")) {
            return false;
        }
        return (value % 1000u == 0u) ||
               (add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value % 1000u, depth + 1u));
    }
    if (value < 1000000000u) {
        if (!emit_number(impl, value / 1000000u, depth + 1u) ||
            !add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) ||
            !emit_ascii_word(impl, "juta")) {
            return false;
        }
        return (value % 1000000u == 0u) ||
               (add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
                emit_number(impl, value % 1000000u, depth + 1u));
    }
    if (!emit_number(impl, value / 1000000000u, depth + 1u) ||
        !add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) ||
        !emit_ascii_word(impl, "miliar")) {
        return false;
    }
    return (value % 1000000000u == 0u) ||
           (add_text_pause(impl, IDTTS_PH_SHORT_PAUSE, 0u) &&
            emit_number(impl, value % 1000000000u, depth + 1u));
}

static void set_text_info(idtts_parse_info_t *info, size_t count, size_t byte, uint32_t cp)
{
    if (info != NULL) {
        info->event_count = count;
        info->error_byte = byte;
        info->error_codepoint = cp;
    }
}

idtts_result_t idtts_text_parse_impl(
    idtts_impl_t *impl,
    const char *text,
    size_t length,
    idtts_parse_info_t *info)
{
    uint32_t word[IDTTS_WORD_MAX];
    size_t word_length = 0u;
    size_t i = 0u;
    bool saw_phone = false;
    bool word_all_upper = true;

    impl->event_count = 0u;
    set_text_info(info, 0u, IDTTS_NPOS, 0u);

#define FLUSH_WORD() \
    do { \
        if (word_length > 0u) { \
            bool ok_ = word_all_upper && word_length > 1u \
                ? emit_acronym(impl, word, word_length) \
                : emit_word(impl, word, word_length); \
            if (!ok_) { \
                set_text_info(info, impl->event_count, i, 0u); \
                return IDTTS_ERR_TOO_MANY_EVENTS; \
            } \
            saw_phone = true; \
            word_length = 0u; \
            word_all_upper = true; \
        } \
    } while (0)

    while (i < length) {
        uint32_t cp;
        size_t width;
        if (!decode_utf8_text(text, length, i, &cp, &width)) {
            set_text_info(info, impl->event_count, i, 0u);
            return IDTTS_ERR_UTF8;
        }

        if (is_ascii_letter_cp(cp)) {
            if (word_length >= IDTTS_WORD_MAX) {
                set_text_info(info, impl->event_count, i, cp);
                return IDTTS_ERR_TOO_MANY_EVENTS;
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
                if (!decode_utf8_text(text, length, i, &digit_cp, &digit_width) ||
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
                set_text_info(info, impl->event_count, i, 0u);
                return IDTTS_ERR_TOO_MANY_EVENTS;
            }
            saw_phone = true;
            continue;
        }

        FLUSH_WORD();
        if (cp == '.' || cp == '!' || cp == '?' || cp == ';' || cp == ':' ||
            cp == '\n' || cp == '\r' || cp == 0x2014u) {
            uint8_t duration = (cp == ';' || cp == ':') ? 76u : 100u;
            uint8_t flags = IDTTS_EVENT_PHRASE_END;
            if (cp == '?') flags |= IDTTS_EVENT_QUESTION;
            if (!add_text_pause_scaled(impl, IDTTS_PH_PHRASE_PAUSE,
                    flags, duration)) {
                set_text_info(info, impl->event_count, i, cp);
                return IDTTS_ERR_TOO_MANY_EVENTS;
            }
        } else if (cp == ',' || cp == '-' || cp == 0x2013u || cp == '/' ||
                   cp == ' ' || cp == '\t' || cp == 0x00a0u) {
            uint8_t duration = 50u;
            if (cp == ',') duration = 165u;
            else if (cp == '/' || cp == 0x2013u) duration = 120u;
            else if (cp == '-') duration = 80u;
            if (!add_text_pause_scaled(impl, IDTTS_PH_SHORT_PAUSE, 0u, duration)) {
                set_text_info(info, impl->event_count, i, cp);
                return IDTTS_ERR_TOO_MANY_EVENTS;
            }
        }
        /* Apostrophes and other symbols are treated as zero-width boundaries. */
        i += width;
    }
    FLUSH_WORD();
#undef FLUSH_WORD

    while (impl->event_count > 0u &&
           impl->events[impl->event_count - 1u].phone == (uint8_t)IDTTS_PH_SHORT_PAUSE) {
        impl->event_count--;
    }
    set_text_info(info, impl->event_count, IDTTS_NPOS, 0u);
    return saw_phone ? IDTTS_OK : IDTTS_ERR_EMPTY_INPUT;
}
