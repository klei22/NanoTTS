/* SPDX-License-Identifier: MIT */
#include "idtts/idtts.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int contains(const idtts_t *tts, idtts_phone_t phone)
{
    const idtts_event_t *events = idtts_events(tts);
    size_t count = idtts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].phone == (uint8_t)phone) return 1;
    }
    return 0;
}

int main(void)
{
    idtts_t tts;
    idtts_parse_info_t info;
    idtts_result_t result;
    const idtts_event_t *events;
    char long_word[90];
    const char malformed[] = { 'a', (char)0xf0, (char)0x80, (char)0x80 };

    assert(idtts_init(&tts, 16000u) == IDTTS_OK);
    assert(idtts_text_frontend_available() == 1);

    result = idtts_parse_text(&tts, "nyanyi dengan syarat khusus",
                              IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_NY));
    assert(contains(&tts, IDTTS_PH_NG));
    assert(contains(&tts, IDTTS_PH_SH));
    assert(contains(&tts, IDTTS_PH_X));
    assert(contains(&tts, IDTTS_PH_OPEN_E));

    result = idtts_parse_text(&tts, "baterai pulau amboi", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_AI));
    assert(contains(&tts, IDTTS_PH_AU));
    assert(contains(&tts, IDTTS_PH_OI));

    result = idtts_parse_text(&tts, "xilofon qari", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_K));
    assert(contains(&tts, IDTTS_PH_S));

    result = idtts_parse_text(&tts, "2026", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    assert(idtts_event_count(&tts) > 10u);

    result = idtts_parse_text(&tts, "méja mèrah mêrah", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_E));
    assert(contains(&tts, IDTTS_PH_OPEN_E));
    assert(contains(&tts, IDTTS_PH_SCHWA));

    result = idtts_parse_text(&tts, "siap, jalan!", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_SHORT_PAUSE));
    assert(contains(&tts, IDTTS_PH_PHRASE_PAUSE));


    result = idtts_parse_text(&tts, "GPS aktif", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    events = idtts_events(&tts);
    assert(idtts_event_count(&tts) >= 8u);
    assert(events[0].phone == (uint8_t)IDTTS_PH_G);
    assert(events[1].phone == (uint8_t)IDTTS_PH_E);
    assert(events[2].phone == (uint8_t)IDTTS_PH_SHORT_PAUSE);
    assert(events[2].duration_percent == 30u);

    result = idtts_parse_text(&tts, "apa kabar?", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    events = idtts_events(&tts);
    assert((events[idtts_event_count(&tts) - 1u].flags &
            IDTTS_EVENT_QUESTION) != 0u);

    result = idtts_parse_text(&tts, "satu dua, tiga", IDTTS_NPOS, &info);
    assert(result == IDTTS_OK);
    events = idtts_events(&tts);
    {
        int saw_word_gap = 0;
        int saw_comma_gap = 0;
        size_t k;
        for (k = 0u; k < idtts_event_count(&tts); ++k) {
            if (events[k].phone == (uint8_t)IDTTS_PH_SHORT_PAUSE) {
                if (events[k].duration_percent == 50u) saw_word_gap = 1;
                if (events[k].duration_percent == 165u) saw_comma_gap = 1;
            }
        }
        assert(saw_word_gap);
        assert(saw_comma_gap);
    }

    result = idtts_parse_text(&tts, malformed, sizeof(malformed), &info);
    assert(result == IDTTS_ERR_UTF8);
    assert(info.error_byte == 1u);

    memset(long_word, 'a', sizeof(long_word));
    result = idtts_parse_text(&tts, long_word, sizeof(long_word), &info);
    assert(result == IDTTS_ERR_TOO_MANY_EVENTS);

    result = idtts_parse_text(&tts, "  --  ", IDTTS_NPOS, &info);
    assert(result == IDTTS_ERR_EMPTY_INPUT);
    return 0;
}
