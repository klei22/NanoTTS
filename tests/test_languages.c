/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef NANOTTS_EXPECT_LANG_ID
#define NANOTTS_EXPECT_LANG_ID 1
#endif
#ifndef NANOTTS_EXPECT_LANG_SW
#define NANOTTS_EXPECT_LANG_SW 1
#endif
#ifndef NANOTTS_EXPECT_LANG_ES
#define NANOTTS_EXPECT_LANG_ES 1
#endif
#ifndef NANOTTS_EXPECT_LANG_MS
#define NANOTTS_EXPECT_LANG_MS 1
#endif
#ifndef NANOTTS_EXPECT_LANG_MI
#define NANOTTS_EXPECT_LANG_MI 1
#endif
#ifndef NANOTTS_EXPECT_LANG_HAW
#define NANOTTS_EXPECT_LANG_HAW 1
#endif

static int contains(const nanotts_t *tts, nanotts_phone_t phone)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t count = nanotts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].phone == (uint8_t)phone) return 1;
    }
    return 0;
}

static size_t count_phone(const nanotts_t *tts, nanotts_phone_t phone)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t count = nanotts_event_count(tts);
    size_t total = 0u;
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].phone == (uint8_t)phone) total++;
    }
    return total;
}


static int contains_flagged(
    const nanotts_t *tts,
    nanotts_phone_t phone,
    uint8_t flag)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t count = nanotts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].phone == (uint8_t)phone &&
            (events[index].flags & flag) == flag) {
            return 1;
        }
    }
    return 0;
}

static size_t primary_stress_index(const nanotts_t *tts)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t count = nanotts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        if ((events[index].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u) {
            return index;
        }
    }
    return NANOTTS_NPOS;
}

static void test_indonesian(void)
{
#if NANOTTS_EXPECT_LANG_ID
    nanotts_t tts;
    nanotts_parse_info_t info;
    nanotts_result_t result;
    const nanotts_event_t *events;
    char long_word[90];
    const char malformed[] = { 'a', (char)0xf0, (char)0x80, (char)0x80 };

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_INDONESIAN) == NANOTTS_OK);

    result = nanotts_parse_text(&tts, "nyanyi dengan syarat khusus",
                              NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_NY));
    assert(contains(&tts, NANOTTS_PH_NG));
    assert(contains(&tts, NANOTTS_PH_SH));
    assert(contains(&tts, NANOTTS_PH_X));
    assert(contains(&tts, NANOTTS_PH_OPEN_E));

    result = nanotts_parse_text(&tts, "baterai pulau amboi", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_AU));
    assert(contains(&tts, NANOTTS_PH_OI));

    result = nanotts_parse_text(&tts, "2026", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) > 10u);

    result = nanotts_parse_text(&tts, "méja mèrah mêrah", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_E));
    assert(contains(&tts, NANOTTS_PH_OPEN_E));
    assert(contains(&tts, NANOTTS_PH_SCHWA));

    result = nanotts_parse_text(&tts, "GPS aktif", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(nanotts_event_count(&tts) >= 8u);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_G);
    assert(events[1].phone == (uint8_t)NANOTTS_PH_E);
    assert(events[2].phone == (uint8_t)NANOTTS_PH_SHORT_PAUSE);
    assert(events[2].duration_percent == 30u);

    result = nanotts_parse_text(&tts, "apa kabar?", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert((events[nanotts_event_count(&tts) - 1u].flags &
            NANOTTS_EVENT_QUESTION) != 0u);

    result = nanotts_parse_text(&tts, malformed, sizeof(malformed), &info);
    assert(result == NANOTTS_ERR_UTF8);
    assert(info.error_byte == 1u);

    memset(long_word, 'a', sizeof(long_word));
    result = nanotts_parse_text(&tts, long_word, sizeof(long_word), &info);
    assert(result == NANOTTS_ERR_TOO_MANY_EVENTS);
#else
    nanotts_t tts;
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_INDONESIAN) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
}

static void test_swahili(void)
{
#if NANOTTS_EXPECT_LANG_SW
    nanotts_t tts;
    nanotts_parse_info_t info;
    nanotts_result_t result;
    const nanotts_event_t *events;
    size_t index;
    size_t stressed = 0u;

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_KISWAHILI) == NANOTTS_OK);

    result = nanotts_parse_text(
        &tts, "habari yako, chakula kizuri", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_CH));
    assert(contains(&tts, NANOTTS_PH_SHORT_PAUSE));

    result = nanotts_parse_text(
        &tts, "dhahabu thelathini ghali kheri shati", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_DH));
    assert(contains(&tts, NANOTTS_PH_TH));
    assert(contains(&tts, NANOTTS_PH_GH));
    assert(contains(&tts, NANOTTS_PH_X));
    assert(contains(&tts, NANOTTS_PH_SH));

    result = nanotts_parse_text(
        &tts, "ng'ombe ngazi nyasi njia", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_NG);
    assert(events[1].phone == (uint8_t)NANOTTS_PH_OPEN_O);
    assert(count_phone(&tts, NANOTTS_PH_NG) >= 2u);
    assert(contains(&tts, NANOTTS_PH_G));
    assert(contains(&tts, NANOTTS_PH_NY));
    assert(contains(&tts, NANOTTS_PH_J));

    result = nanotts_parse_text(&tts, "mayai", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(!contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_Y));
    events = nanotts_events(&tts);
    for (index = 0u; index < nanotts_event_count(&tts); ++index) {
        if ((events[index].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u) {
            stressed++;
            assert(events[index].phone == (uint8_t)NANOTTS_PH_A);
        }
    }
    assert(stressed == 1u);


    result = nanotts_parse_text(&tts, "mti mtu nchi mbwa", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_M);
    assert((events[0].flags & NANOTTS_EVENT_SYLLABIC) != 0u);
    assert((events[0].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u);

    result = nanotts_parse_text(&tts, "21 100 1000", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) > 20u);

    result = nanotts_parse_text(&tts, "HABARI", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(!contains(&tts, NANOTTS_PH_SHORT_PAUSE));

    result = nanotts_parse_text(&tts, "GPS", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_SHORT_PAUSE));

    assert(nanotts_set_language(&tts, NANOTTS_LANG_KISWAHILI) == NANOTTS_OK);
#else
    nanotts_t tts;
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_KISWAHILI) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
}


static void test_spanish(void)
{
#if NANOTTS_EXPECT_LANG_ES
    nanotts_t tts;
    nanotts_parse_info_t info;
    nanotts_result_t result;
    const nanotts_event_t *events;
    nanotts_event_t precomposed[16];
    size_t precomposed_count;
    size_t index;
    char long_word[90];
    const char malformed[] = { 'a', (char)0xf0, (char)0x80, (char)0x80 };

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_SPANISH) == NANOTTS_OK);

    result = nanotts_parse_text(
        &tts, "queso guitarra pingüino", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_K));
    assert(contains(&tts, NANOTTS_PH_G));
    assert(contains(&tts, NANOTTS_PH_W));

    result = nanotts_parse_text(
        &tts, "cena zapato chico niño llave", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_S));
    assert(contains(&tts, NANOTTS_PH_CH));
    assert(contains(&tts, NANOTTS_PH_NY));
    assert(contains(&tts, NANOTTS_PH_Y));
    assert(contains(&tts, NANOTTS_PH_BETA));

    result = nanotts_parse_text(&tts, "caro", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[2].phone == (uint8_t)NANOTTS_PH_TAP);

    result = nanotts_parse_text(&tts, "carro", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[2].phone == (uint8_t)NANOTTS_PH_R);

    result = nanotts_parse_text(&tts, "cada amigo", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_DH));
    assert(contains(&tts, NANOTTS_PH_GH));

    result = nanotts_parse_text(
        &tts, "aire causa hoy rey", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_AU));
    assert(contains(&tts, NANOTTS_PH_OI));
    assert(contains(&tts, NANOTTS_PH_Y));

    result = nanotts_parse_text(&tts, "muy", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(nanotts_event_count(&tts) == 3u);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_M);
    assert(events[1].phone == (uint8_t)NANOTTS_PH_U);
    assert(events[2].phone == (uint8_t)NANOTTS_PH_Y);

    result = nanotts_parse_text(&tts, "casa", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(primary_stress_index(&tts) == 1u);
    result = nanotts_parse_text(&tts, "joven", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(primary_stress_index(&tts) == 1u);
    result = nanotts_parse_text(&tts, "reloj", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(primary_stress_index(&tts) == 3u);
    result = nanotts_parse_text(&tts, "robots", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(primary_stress_index(&tts) == 3u);
    result = nanotts_parse_text(&tts, "corazón", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(primary_stress_index(&tts) == 5u);

    result = nanotts_parse_text(&tts, "país", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    precomposed_count = nanotts_event_count(&tts);
    assert(precomposed_count <= sizeof(precomposed) / sizeof(precomposed[0]));
    memcpy(precomposed, nanotts_events(&tts),
           precomposed_count * sizeof(precomposed[0]));
    result = nanotts_parse_text(&tts, "pai\xCC\x81s", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == precomposed_count);
    assert(memcmp(nanotts_events(&tts), precomposed,
                  precomposed_count * sizeof(precomposed[0])) == 0);

    result = nanotts_parse_text(
        &tts, "pingu\xCC\x88ino nin\xCC\x83o", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_W));
    assert(contains(&tts, NANOTTS_PH_NY));

    result = nanotts_parse_text(
        &tts, "GRACIAS ÁRBOL MÉXICO", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_SHORT_PAUSE) == 2u);
    result = nanotts_parse_text(&tts, "GPS", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_SHORT_PAUSE) == 2u);

    result = nanotts_parse_text(&tts, "CPU", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_SHORT_PAUSE) == 2u);
    result = nanotts_parse_text(&tts, "ONU UNESCO", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_SHORT_PAUSE) == 1u);

    result = nanotts_parse_text(&tts, "16 22 31 1001", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) > 30u);
    result = nanotts_parse_text(&tts, "21000", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    for (index = 0u; index < nanotts_event_count(&tts) &&
                     events[index].phone != (uint8_t)NANOTTS_PH_SHORT_PAUSE;
         ++index) {
        assert(events[index].phone != (uint8_t)NANOTTS_PH_O);
    }
    assert(index < nanotts_event_count(&tts));

    result = nanotts_parse_text(&tts, "¿cómo está?", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert((events[nanotts_event_count(&tts) - 1u].flags &
            NANOTTS_EVENT_QUESTION) != 0u);

    memset(long_word, 'a', sizeof(long_word));
    long_word[sizeof(long_word) - 1u] = '\0';
    assert(nanotts_parse_text(&tts, long_word, NANOTTS_NPOS, &info) ==
           NANOTTS_ERR_TOO_MANY_EVENTS);
    assert(nanotts_parse_text(&tts, malformed, sizeof(malformed), &info) ==
           NANOTTS_ERR_UTF8);

    assert(nanotts_set_language(&tts, NANOTTS_LANG_SPANISH) == NANOTTS_OK);
#else
    nanotts_t tts;
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_SPANISH) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
}


static void test_malay(void)
{
#if NANOTTS_EXPECT_LANG_MS
    nanotts_t tts;
    nanotts_parse_info_t info;
    const nanotts_event_t *events;
    nanotts_result_t result;

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_MALAY) == NANOTTS_OK);
    result = nanotts_parse_text(
        &tts, "nganga nyanyi syarat khas ghairah cawan", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_NG));
    assert(contains(&tts, NANOTTS_PH_NY));
    assert(contains(&tts, NANOTTS_PH_SH));
    assert(contains(&tts, NANOTTS_PH_X));
    assert(contains(&tts, NANOTTS_PH_GH));
    assert(contains(&tts, NANOTTS_PH_CH));

    result = nanotts_parse_text(&tts, "kedai pulau amboi", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_AU));
    assert(contains(&tts, NANOTTS_PH_OI));

    /* Closed-syllable vowel sequences are not collapsed by the compact rule. */
    result = nanotts_parse_text(&tts, "baik kain", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(!contains(&tts, NANOTTS_PH_AI));
    assert(count_phone(&tts, NANOTTS_PH_A) == 2u);
    assert(count_phone(&tts, NANOTTS_PH_I) == 2u);

    result = nanotts_parse_text(&tts, "besar merah ekor meja", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_SCHWA));
    assert(contains(&tts, NANOTTS_PH_E));

    result = nanotts_parse_text(&tts, "méja mèrah mêrah", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_E));
    assert(contains(&tts, NANOTTS_PH_OPEN_E));
    assert(contains(&tts, NANOTTS_PH_SCHWA));

    result = nanotts_parse_text(&tts, "2026", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) > 10u);
    result = nanotts_parse_text(&tts, "apa khabar?", NANOTTS_NPOS, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert((events[nanotts_event_count(&tts) - 1u].flags &
            NANOTTS_EVENT_QUESTION) != 0u);
#else
    nanotts_t tts;
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_MALAY) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
}

static void test_maori(void)
{
#if NANOTTS_EXPECT_LANG_MI
    nanotts_t tts;
    nanotts_parse_info_t info;
    nanotts_event_t composed[32];
    const nanotts_event_t *events;
    size_t composed_count;

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_MAORI) == NANOTTS_OK);
    assert(nanotts_parse_text(&tts, "whānau ngā mihi", NANOTTS_NPOS, &info) ==
           NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_F);
    assert(events[1].phone == (uint8_t)NANOTTS_PH_A);
    assert((events[1].flags & NANOTTS_EVENT_LONG) != 0u);
    assert((events[1].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u);
    assert(contains(&tts, NANOTTS_PH_NG));

    assert(nanotts_parse_text(&tts, "kia ora Aotearoa pai mauri", NANOTTS_NPOS, &info) ==
           NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_TAP));
    assert(contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_AU));

    assert(nanotts_parse_text(&tts, "pai", NANOTTS_NPOS, &info) == NANOTTS_OK);
    assert(contains_flagged(&tts, NANOTTS_PH_AI,
                            NANOTTS_EVENT_STRESS_PRIMARY));

    assert(nanotts_parse_text(&tts, "māori", NANOTTS_NPOS, &info) == NANOTTS_OK);
    composed_count = nanotts_event_count(&tts);
    assert(composed_count <= sizeof(composed) / sizeof(composed[0]));
    memcpy(composed, nanotts_events(&tts), composed_count * sizeof(composed[0]));
    assert(nanotts_parse_text(&tts, "ma\xCC\x84ori", NANOTTS_NPOS, &info) ==
           NANOTTS_OK);
    assert(nanotts_event_count(&tts) == composed_count);
    assert(memcmp(composed, nanotts_events(&tts),
                  composed_count * sizeof(composed[0])) == 0);
    assert(nanotts_parse_text(&tts, "Maaori", NANOTTS_NPOS, &info) == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == composed_count);
    assert(memcmp(composed, nanotts_events(&tts),
                  composed_count * sizeof(composed[0])) == 0);

    assert(nanotts_parse_text(&tts, "0123456789?", NANOTTS_NPOS, &info) ==
           NANOTTS_OK);
    events = nanotts_events(&tts);
    assert((events[nanotts_event_count(&tts) - 1u].flags &
            NANOTTS_EVENT_QUESTION) != 0u);
#else
    nanotts_t tts;
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_MAORI) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
}

static void test_hawaiian(void)
{
#if NANOTTS_EXPECT_LANG_HAW
    nanotts_t tts;
    nanotts_parse_info_t info;
    nanotts_event_t composed[32];
    const nanotts_event_t *events;
    size_t composed_count;

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_HAWAIIAN) == NANOTTS_OK);
    assert(nanotts_parse_text(
        &tts, "aloha Hawaiʻi, mahalo nui loa", NANOTTS_NPOS, &info) ==
        NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_GLOTTAL_STOP));
    assert(contains(&tts, NANOTTS_PH_W));

    assert(nanotts_parse_text(&tts, "kai mau poi", NANOTTS_NPOS, &info) == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_AU));
    assert(contains(&tts, NANOTTS_PH_OI));
    assert(contains_flagged(&tts, NANOTTS_PH_OI,
                            NANOTTS_EVENT_STRESS_PRIMARY));

    assert(nanotts_parse_text(&tts, "mālama", NANOTTS_NPOS, &info) == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[1].phone == (uint8_t)NANOTTS_PH_A);
    assert((events[1].flags & NANOTTS_EVENT_LONG) != 0u);
    composed_count = nanotts_event_count(&tts);
    assert(composed_count <= sizeof(composed) / sizeof(composed[0]));
    memcpy(composed, events, composed_count * sizeof(composed[0]));
    assert(nanotts_parse_text(&tts, "ma\xCC\x84lama", NANOTTS_NPOS, &info) ==
           NANOTTS_OK);
    assert(nanotts_event_count(&tts) == composed_count);
    assert(memcmp(composed, nanotts_events(&tts),
                  composed_count * sizeof(composed[0])) == 0);

    assert(nanotts_parse_text(&tts, "ʻohana 123?", NANOTTS_NPOS, &info) ==
           NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_GLOTTAL_STOP);
    assert((events[nanotts_event_count(&tts) - 1u].flags &
            NANOTTS_EVENT_QUESTION) != 0u);
#else
    nanotts_t tts;
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_HAWAIIAN) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
}

int main(void)
{
    test_indonesian();
    test_swahili();
    test_spanish();
    test_malay();
    test_maori();
    test_hawaiian();
    return 0;
}
