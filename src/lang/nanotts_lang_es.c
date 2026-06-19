/* SPDX-License-Identifier: MIT */
#include "lang/nanotts_lang_common.h"

#include <string.h>

/*
 * Compact neutral Latin-American Spanish frontend.
 *
 * Deliberate dialect policy:
 *   - seseo: c before e/i and z -> /s/
 *   - yeismo: ll and consonantal y -> shared palatal glide /y/
 *   - written j and soft g -> /x/
 *
 * The module also applies a lightweight set of common stop/approximant
 * allophones (/b~beta/, /d~dh/, /g~gh/) within words. It is intentionally a
 * bounded G2P frontend, not a morphology or proper-name dictionary.
 */

#ifndef NANOTTS_LANG_ES_VOWELS_MAX
#define NANOTTS_LANG_ES_VOWELS_MAX 24u
#endif

typedef struct es_vowel {
    nanotts_phone_t phone;
    uint8_t weak;
    uint8_t accented;
    uint8_t orthographic_y;
} es_vowel_t;

static bool is_letter_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
           cp == 0x00c1u || cp == 0x00e1u || /* A acute */
           cp == 0x00c9u || cp == 0x00e9u || /* E acute */
           cp == 0x00cdu || cp == 0x00edu || /* I acute */
           cp == 0x00d3u || cp == 0x00f3u || /* O acute */
           cp == 0x00dau || cp == 0x00fau || /* U acute */
           cp == 0x00dcu || cp == 0x00fcu || /* U diaeresis */
           cp == 0x00d1u || cp == 0x00f1u;   /* N tilde */
}

static bool is_upper_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') ||
           cp == 0x00c1u || cp == 0x00c9u || cp == 0x00cdu ||
           cp == 0x00d3u || cp == 0x00dau || cp == 0x00dcu ||
           cp == 0x00d1u;
}

static uint32_t lower_cp(uint32_t cp)
{
    if (cp >= 'A' && cp <= 'Z') return cp + ('a' - 'A');
    switch (cp) {
    case 0x00c1u: return 0x00e1u;
    case 0x00c9u: return 0x00e9u;
    case 0x00cdu: return 0x00edu;
    case 0x00d3u: return 0x00f3u;
    case 0x00dau: return 0x00fau;
    case 0x00dcu: return 0x00fcu;
    case 0x00d1u: return 0x00f1u;
    case 0x2019u: case 0x02bcu: return '\'';
    default: return cp;
    }
}

static uint32_t base_cp(uint32_t cp)
{
    switch (cp) {
    case 0x00e1u: return 'a';
    case 0x00e9u: return 'e';
    case 0x00edu: return 'i';
    case 0x00f3u: return 'o';
    case 0x00fau: case 0x00fcu: return 'u';
    default: return cp;
    }
}

static bool has_acute(uint32_t cp)
{
    return cp == 0x00e1u || cp == 0x00e9u || cp == 0x00edu ||
           cp == 0x00f3u || cp == 0x00fau;
}

/* Accept the common NFD spellings without bringing in Unicode normalization. */
static bool apply_combining_mark(uint32_t *cp, uint32_t mark)
{
    uint32_t base;
    if (cp == NULL) return false;
    base = base_cp(*cp);
    if (mark == 0x0301u) { /* combining acute */
        switch (base) {
        case 'a': *cp = 0x00e1u; return true;
        case 'e': *cp = 0x00e9u; return true;
        case 'i': *cp = 0x00edu; return true;
        case 'o': *cp = 0x00f3u; return true;
        case 'u': *cp = 0x00fau; return true;
        default: return false;
        }
    }
    if (mark == 0x0308u && base == 'u') { /* combining diaeresis */
        *cp = 0x00fcu;
        return true;
    }
    if (mark == 0x0303u && base == 'n') { /* combining tilde */
        *cp = 0x00f1u;
        return true;
    }
    return false;
}

static bool vowel_at(const uint32_t *word, size_t length, size_t position)
{
    uint32_t cp;
    if (position >= length) return false;
    cp = base_cp(word[position]);
    if (cp == 'a' || cp == 'e' || cp == 'i' || cp == 'o' || cp == 'u') {
        return true;
    }
    /* Orthographic y is vocalic only word-finally (including standalone y). */
    return cp == 'y' && position + 1u == length;
}

static es_vowel_t vowel_from_cp(uint32_t cp)
{
    es_vowel_t vowel;
    uint32_t base = base_cp(cp);
    vowel.accented = has_acute(cp) ? 1u : 0u;
    vowel.weak = (base == 'i' || base == 'u' || base == 'y') ? 1u : 0u;
    vowel.orthographic_y = base == 'y' ? 1u : 0u;
    switch (base) {
    case 'a': vowel.phone = NANOTTS_PH_A; break;
    case 'e': vowel.phone = NANOTTS_PH_E; break;
    case 'i': case 'y': vowel.phone = NANOTTS_PH_I; break;
    case 'o': vowel.phone = NANOTTS_PH_O; break;
    default: vowel.phone = NANOTTS_PH_U; break;
    }
    return vowel;
}

static nanotts_phone_t glide_for(const es_vowel_t *vowel)
{
    return vowel->phone == NANOTTS_PH_U ? NANOTTS_PH_W : NANOTTS_PH_Y;
}

static bool push_phone(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t phone)
{
    /* Spanish has no ordinary lexical geminates; avoid an artificial long /s/
     * in sequences such as sc before e/i. */
    if (*count > 0u && phone == NANOTTS_PH_S &&
        events[*count - 1u].phone == (uint8_t)NANOTTS_PH_S) {
        return true;
    }
    return nanotts_lang_temp_push(events, count, phone);
}

static bool push_glide(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t phone)
{
    if (!push_phone(events, count, phone)) return false;
    events[*count - 1u].duration_percent = 82u;
    return true;
}

static bool push_nucleus(
    nanotts_event_t *events,
    size_t *count,
    size_t *nuclei,
    size_t *nucleus_count,
    size_t *explicit_stress,
    nanotts_phone_t phone,
    bool accented)
{
    size_t index = *count;
    if (*nucleus_count >= NANOTTS_LANG_WORD_PHONE_MAX ||
        !nanotts_lang_temp_push(events, count, phone)) {
        return false;
    }
    nuclei[(*nucleus_count)++] = index;
    if (accented && *explicit_stress == NANOTTS_NPOS) {
        *explicit_stress = index;
    }
    return true;
}

/*
 * Emit one orthographic vowel group. Unaccented i/u act as glides beside a
 * strong vowel; accented i/u force hiatus. Strong-vowel adjacency is hiatus.
 * This compact rule also handles common triphthongs as glide+nucleus+glide.
 */
static bool emit_vowel_group(
    nanotts_event_t *events,
    size_t *phone_count,
    size_t *nuclei,
    size_t *nucleus_count,
    size_t *explicit_stress,
    const uint32_t *word,
    size_t length,
    size_t start,
    size_t *end)
{
    es_vowel_t vowels[NANOTTS_LANG_ES_VOWELS_MAX];
    size_t vowel_count = 0u;
    size_t scan = start;
    size_t position = 0u;

    while (scan < length) {
        if (vowel_at(word, length, scan)) {
            if (vowel_count >= NANOTTS_LANG_ES_VOWELS_MAX) return false;
            vowels[vowel_count++] = vowel_from_cp(word[scan]);
            scan++;
            continue;
        }
        /* Silent h does not block the compact vowel-group analysis. */
        if (word[scan] == 'h' && vowel_count > 0u &&
            scan + 1u < length && vowel_at(word, length, scan + 1u)) {
            scan++;
            continue;
        }
        break;
    }

    while (position < vowel_count) {
        size_t strong = position;
        size_t cluster_end;
        size_t k;
        bool cluster_accented = false;

        while (strong < vowel_count && vowels[strong].weak != 0u &&
               vowels[strong].accented == 0u) {
            strong++;
        }
        if (strong == vowel_count) {
            /* A remaining weak-vowel run is one nucleus. Usually the final
             * weak vowel carries it (ciudad, ruido). Word-final orthographic
             * y is instead an offglide in forms such as muy. */
            if (vowel_count - position > 1u &&
                vowels[vowel_count - 1u].orthographic_y != 0u) {
                strong = position;
            } else {
                strong = vowel_count - 1u;
            }
            cluster_end = vowel_count - 1u;
        } else {
            cluster_end = strong;
            while (cluster_end + 1u < vowel_count &&
                   vowels[cluster_end + 1u].weak != 0u &&
                   vowels[cluster_end + 1u].accented == 0u) {
                cluster_end++;
            }
        }

        for (k = position; k <= cluster_end; ++k) {
            if (vowels[k].accented != 0u) cluster_accented = true;
        }

        /* Reuse the shared smooth diphthong trajectories where they match. */
        if (cluster_end == position + 1u && strong == position) {
            nanotts_phone_t diphthong = NANOTTS_PH_COUNT;
            if (vowels[position].phone == NANOTTS_PH_A &&
                vowels[position + 1u].phone == NANOTTS_PH_I) {
                diphthong = NANOTTS_PH_AI;
            } else if (vowels[position].phone == NANOTTS_PH_A &&
                       vowels[position + 1u].phone == NANOTTS_PH_U) {
                diphthong = NANOTTS_PH_AU;
            } else if (vowels[position].phone == NANOTTS_PH_O &&
                       vowels[position + 1u].phone == NANOTTS_PH_I) {
                diphthong = NANOTTS_PH_OI;
            }
            if (diphthong != NANOTTS_PH_COUNT) {
                if (!push_nucleus(events, phone_count, nuclei, nucleus_count,
                                  explicit_stress, diphthong,
                                  cluster_accented)) {
                    return false;
                }
                position = cluster_end + 1u;
                continue;
            }
        }

        for (k = position; k <= cluster_end; ++k) {
            if (k == strong) {
                if (!push_nucleus(events, phone_count, nuclei, nucleus_count,
                                  explicit_stress, vowels[k].phone,
                                  cluster_accented)) {
                    return false;
                }
            } else if (!push_glide(events, phone_count, glide_for(&vowels[k]))) {
                return false;
            }
        }
        position = cluster_end + 1u;
    }

    *end = scan;
    return vowel_count > 0u;
}

static bool prefix_ascii(
    const uint32_t *word,
    size_t length,
    const char *prefix)
{
    size_t i = 0u;
    while (prefix[i] != '\0') {
        uint32_t cp;
        if (i >= length) return false;
        cp = base_cp(word[i]);
        if (cp > 0x7fu || cp != (uint32_t)(unsigned char)prefix[i]) return false;
        i++;
    }
    return true;
}

static bool x_is_velar(const uint32_t *word, size_t length, size_t position)
{
    static const char *const prefixes[] = {
        "mexic", "mexiqu", "oaxac", "texas", "texan"
    };
    size_t i;
    if (position == 0u) return false;
    for (i = 0u; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (prefix_ascii(word, length, prefixes[i])) return true;
    }
    return false;
}

static nanotts_phone_t previous_phone(
    const nanotts_event_t *phones,
    size_t phone_count)
{
    if (phone_count == 0u) return NANOTTS_PH_SILENCE;
    return (nanotts_phone_t)phones[phone_count - 1u].phone;
}

static nanotts_phone_t spanish_b_phone(
    const nanotts_event_t *phones,
    size_t phone_count)
{
    nanotts_phone_t previous = previous_phone(phones, phone_count);
    if (phone_count == 0u || previous == NANOTTS_PH_M ||
        previous == NANOTTS_PH_N || previous == NANOTTS_PH_NG ||
        !nanotts_phone_is_sonorant(previous)) {
        return NANOTTS_PH_B;
    }
    return NANOTTS_PH_BETA;
}

static nanotts_phone_t spanish_d_phone(
    const nanotts_event_t *phones,
    size_t phone_count)
{
    nanotts_phone_t previous = previous_phone(phones, phone_count);
    if (phone_count == 0u || previous == NANOTTS_PH_N ||
        previous == NANOTTS_PH_L || !nanotts_phone_is_sonorant(previous)) {
        return NANOTTS_PH_D;
    }
    return NANOTTS_PH_DH;
}

static nanotts_phone_t spanish_g_phone(
    const nanotts_event_t *phones,
    size_t phone_count)
{
    nanotts_phone_t previous = previous_phone(phones, phone_count);
    if (phone_count == 0u || previous == NANOTTS_PH_N ||
        previous == NANOTTS_PH_NG || !nanotts_phone_is_sonorant(previous)) {
        return NANOTTS_PH_G;
    }
    return NANOTTS_PH_GH;
}

static bool emit_word(
    nanotts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    nanotts_event_t phones[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t nuclei[NANOTTS_LANG_WORD_PHONE_MAX];
    size_t phone_count = 0u;
    size_t nucleus_count = 0u;
    size_t explicit_stress = NANOTTS_NPOS;
    size_t i = 0u;
    size_t j;

    while (i < length) {
        uint32_t c0 = word[i];
        uint32_t c1 = i + 1u < length ? word[i + 1u] : 0u;
        uint32_t c2 = i + 2u < length ? word[i + 2u] : 0u;
        uint32_t b1 = base_cp(c1);
        uint32_t b2 = base_cp(c2);
        nanotts_phone_t phone;

        if (vowel_at(word, length, i)) {
            size_t end;
            if (!emit_vowel_group(phones, &phone_count, nuclei, &nucleus_count,
                                  &explicit_stress, word, length, i, &end)) {
                return false;
            }
            i = end;
            continue;
        }

        if (c0 == 'h' || c0 == '\'') {
            i++;
            continue;
        }
        if (c0 == 'c' && c1 == 'h') {
            phone = NANOTTS_PH_CH;
            i += 2u;
        } else if (c0 == 'l' && c1 == 'l') {
            phone = NANOTTS_PH_Y;
            i += 2u;
        } else if (c0 == 'r' && c1 == 'r') {
            phone = NANOTTS_PH_R;
            i += 2u;
        } else if (c0 == 'q' && c1 == 'u' && (b2 == 'e' || b2 == 'i')) {
            phone = NANOTTS_PH_K;
            i += 2u; /* leave e/i for the next iteration */
        } else if (c0 == 'g' && c1 == 'u' && (b2 == 'e' || b2 == 'i')) {
            phone = spanish_g_phone(phones, phone_count);
            i += 2u; /* silent u */
        } else if (c0 == 'g' && c1 == 0x00fcu && (b2 == 'e' || b2 == 'i')) {
            phone = spanish_g_phone(phones, phone_count);
            i += 1u; /* pronounced ü is handled as a glide next */
        } else {
            switch (c0) {
            case 'b': case 'v':
                phone = spanish_b_phone(phones, phone_count);
                break;
            case 'c':
                phone = (b1 == 'e' || b1 == 'i') ? NANOTTS_PH_S : NANOTTS_PH_K;
                break;
            case 'd': phone = spanish_d_phone(phones, phone_count); break;
            case 'f': phone = NANOTTS_PH_F; break;
            case 'g':
                phone = (b1 == 'e' || b1 == 'i')
                    ? NANOTTS_PH_X : spanish_g_phone(phones, phone_count);
                break;
            case 'j': phone = NANOTTS_PH_X; break;
            case 'k': case 'q': phone = NANOTTS_PH_K; break;
            case 'l': phone = NANOTTS_PH_L; break;
            case 'm': phone = NANOTTS_PH_M; break;
            case 'n': phone = NANOTTS_PH_N; break;
            case 0x00f1u: phone = NANOTTS_PH_NY; break;
            case 'p': phone = NANOTTS_PH_P; break;
            case 'r':
                phone = (i == 0u || word[i - 1u] == 'n' ||
                         word[i - 1u] == 'l' || word[i - 1u] == 's')
                    ? NANOTTS_PH_R : NANOTTS_PH_TAP;
                break;
            case 's': case 'z': phone = NANOTTS_PH_S; break;
            case 't': phone = NANOTTS_PH_T; break;
            case 'w': phone = NANOTTS_PH_W; break;
            case 'y': phone = NANOTTS_PH_Y; break;
            case 'x':
                if (i == 0u) {
                    phone = NANOTTS_PH_S;
                } else if (x_is_velar(word, length, i)) {
                    phone = NANOTTS_PH_X;
                } else {
                    if (!push_phone(phones, &phone_count, NANOTTS_PH_K)) {
                        return false;
                    }
                    phone = NANOTTS_PH_S;
                }
                break;
            default:
                i++;
                continue;
            }
            i++;
        }

        if (!push_phone(phones, &phone_count, phone)) return false;
    }

    if (nucleus_count > 0u) {
        size_t stressed;
        if (explicit_stress != NANOTTS_NPOS) {
            stressed = explicit_stress;
        } else {
            uint32_t final_cp = length > 0u ? base_cp(word[length - 1u]) : 0u;
            uint32_t before_final =
                length > 1u ? base_cp(word[length - 2u]) : 0u;
            bool final_is_vowel = final_cp == 'a' || final_cp == 'e' ||
                                  final_cp == 'i' || final_cp == 'o' ||
                                  final_cp == 'u';
            bool final_ns_after_vowel =
                (final_cp == 'n' || final_cp == 's') &&
                (before_final == 'a' || before_final == 'e' ||
                 before_final == 'i' || before_final == 'o' ||
                 before_final == 'u');
            bool penultimate = final_is_vowel || final_ns_after_vowel;
            size_t nucleus_position =
                penultimate && nucleus_count > 1u
                    ? nucleus_count - 2u : nucleus_count - 1u;
            stressed = nuclei[nucleus_position];
        }
        phones[stressed].flags |= NANOTTS_EVENT_STRESS_PRIMARY;
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
    nanotts_lang_mark_word_end(impl);
    return true;
}

static bool emit_utf8_word(nanotts_impl_t *impl, const char *word)
{
    uint32_t letters[NANOTTS_LANG_WORD_MAX];
    size_t count = 0u;
    size_t offset = 0u;
    size_t length = strlen(word);

    while (offset < length) {
        uint32_t cp;
        size_t width;
        if (count >= NANOTTS_LANG_WORD_MAX ||
            !nanotts_lang_decode_utf8(word, length, offset, &cp, &width)) {
            return false;
        }
        letters[count++] = lower_cp(cp);
        offset += width;
    }
    return emit_word(impl, letters, count);
}

static bool emit_join(nanotts_impl_t *impl)
{
    return nanotts_lang_add_pause_scaled(
        impl, NANOTTS_PH_SHORT_PAUSE, 0u, 38u);
}

static bool should_spell_acronym_es(
    const uint32_t *word,
    size_t length)
{
    size_t vowels = 0u;
    size_t letters = 0u;
    size_t i;

    if (word == NULL || length < 2u || length > 8u) return false;
    for (i = 0u; i < length; ++i) {
        uint32_t cp = base_cp(word[i]);
        if ((cp >= 'a' && cp <= 'z') || cp == 0x00f1u) {
            letters++;
            if (cp == 'a' || cp == 'e' || cp == 'i' ||
                cp == 'o' || cp == 'u') {
                vowels++;
            }
        }
    }
    /* Keep ordinary all-caps words intact. Short consonant-heavy tokens are
     * treated as initialisms; pronounce vowel-rich acronyms such as ONU as
     * words. This intentionally favors false negatives over spelling headings. */
    return letters == length && length <= 4u && (vowels * 2u) < letters;
}

static bool emit_letter_name(nanotts_impl_t *impl, uint32_t letter)
{
    static const char *const names[26] = {
        "a", "be", "ce", "de", "e", "efe", "ge", "hache", "i",
        "jota", "ka", "ele", "eme", "ene", "o", "pe", "cu", "erre",
        "ese", "te", "u", "uve", "uve", "equis", "ye", "zeta"
    };
    if (letter == 0x00f1u) return emit_utf8_word(impl, "eñe");
    letter = base_cp(letter);
    if (letter < 'a' || letter > 'z') return true;
    if (letter == 'w') {
        return emit_utf8_word(impl, "doble") && emit_join(impl) &&
               emit_utf8_word(impl, "uve");
    }
    return emit_utf8_word(impl, names[letter - 'a']);
}

static bool emit_acronym(
    nanotts_impl_t *impl,
    const uint32_t *word,
    size_t length)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        if (word[i] == '\'') continue;
        if (!emit_letter_name(impl, word[i])) return false;
        if (i + 1u < length && !emit_join(impl)) return false;
    }
    nanotts_lang_mark_word_end(impl);
    return true;
}

static bool emit_number_under_100(
    nanotts_impl_t *impl,
    uint32_t value,
    bool apocopate)
{
    static const char *const small[] = {
        "cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete",
        "ocho", "nueve", "diez", "once", "doce", "trece", "catorce",
        "quince", "dieciséis", "diecisiete", "dieciocho", "diecinueve",
        "veinte", "veintiuno", "veintidós", "veintitrés", "veinticuatro",
        "veinticinco", "veintiséis", "veintisiete", "veintiocho", "veintinueve"
    };
    static const char *const tens[] = {
        "", "", "", "treinta", "cuarenta", "cincuenta", "sesenta",
        "setenta", "ochenta", "noventa"
    };
    uint32_t remainder;

    if (value == 1u && apocopate) return emit_utf8_word(impl, "un");
    if (value == 21u && apocopate) return emit_utf8_word(impl, "veintiún");
    if (value < 30u) return emit_utf8_word(impl, small[value]);
    if (!emit_utf8_word(impl, tens[value / 10u])) return false;
    remainder = value % 10u;
    if (remainder == 0u) return true;
    return emit_join(impl) && emit_utf8_word(impl, "y") &&
           emit_join(impl) &&
           emit_utf8_word(impl,
               remainder == 1u && apocopate ? "un" : small[remainder]);
}

static bool emit_number_under_1000(
    nanotts_impl_t *impl,
    uint32_t value,
    bool apocopate)
{
    static const char *const hundreds[] = {
        "", "ciento", "doscientos", "trescientos", "cuatrocientos",
        "quinientos", "seiscientos", "setecientos", "ochocientos",
        "novecientos"
    };
    uint32_t remainder;

    if (value < 100u) return emit_number_under_100(impl, value, apocopate);
    if (value == 100u) return emit_utf8_word(impl, "cien");
    if (!emit_utf8_word(impl, hundreds[value / 100u])) return false;
    remainder = value % 100u;
    return remainder == 0u ||
           (emit_join(impl) &&
            emit_number_under_100(impl, remainder, apocopate));
}

static bool emit_number_under_million(
    nanotts_impl_t *impl,
    uint32_t value,
    bool apocopate)
{
    uint32_t group;
    uint32_t remainder;

    if (value < 1000u) return emit_number_under_1000(impl, value, apocopate);
    group = value / 1000u;
    remainder = value % 1000u;
    if (group != 1u && !emit_number_under_1000(impl, group, true)) return false;
    if (group != 1u && !emit_join(impl)) return false;
    if (!emit_utf8_word(impl, "mil")) return false;
    if (remainder == 0u) return true;
    return emit_join(impl) &&
           emit_number_under_1000(impl, remainder, apocopate);
}

static bool emit_number(nanotts_impl_t *impl, uint32_t value)
{
    uint32_t group;
    uint32_t remainder;

    if (value < 1000000u) {
        return emit_number_under_million(impl, value, false);
    }

    group = value / 1000000u;
    remainder = value % 1000000u;
    if (group == 1u) {
        if (!emit_utf8_word(impl, "un") || !emit_join(impl) ||
            !emit_utf8_word(impl, "millón")) {
            return false;
        }
    } else {
        /* The largest uint32 group is 4294, so this remains bounded. */
        if (!emit_number_under_million(impl, group, true) ||
            !emit_join(impl) || !emit_utf8_word(impl, "millones")) {
            return false;
        }
    }
    if (remainder == 0u) return true;
    return emit_join(impl) &&
           emit_number_under_million(impl, remainder, false);
}

nanotts_result_t nanotts_lang_es_parse_text(
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
                    should_spell_acronym_es(word, word_length) \
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

        if ((cp == 0x0301u || cp == 0x0303u || cp == 0x0308u) &&
            word_length > 0u &&
            apply_combining_mark(&word[word_length - 1u], cp)) {
            i += width;
            continue;
        }

        if (is_letter_cp(cp) ||
            ((cp == '\'' || cp == 0x2019u || cp == 0x02bcu) &&
             word_length > 0u)) {
            if (word_length >= NANOTTS_LANG_WORD_MAX) {
                nanotts_lang_set_info(info, impl->event_count, i, cp);
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
            if (is_letter_cp(cp) && !is_upper_cp(cp)) word_all_upper = false;
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
                if (value > UINT32_MAX) value = UINT32_MAX;
                i += digit_width;
            }
            if (!emit_number(impl, (uint32_t)value)) {
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
        /* Opening inverted question/exclamation marks are intentionally silent. */
        i += width;
    }
    FLUSH_WORD();
#undef FLUSH_WORD

    nanotts_lang_trim_trailing_short_pause(impl);
    nanotts_lang_set_info(info, impl->event_count, NANOTTS_NPOS, 0u);
    return saw_phone ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
