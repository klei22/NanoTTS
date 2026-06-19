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

int main(void)
{
    test_indonesian();
    test_swahili();
    return 0;
}
