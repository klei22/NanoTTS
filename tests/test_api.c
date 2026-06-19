/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#ifndef NANOTTS_EXPECT_TEXT_FRONTEND
#define NANOTTS_EXPECT_TEXT_FRONTEND 1
#endif
#ifndef NANOTTS_EXPECT_LANG_ID
#define NANOTTS_EXPECT_LANG_ID 1
#endif
#ifndef NANOTTS_EXPECT_LANG_SW
#define NANOTTS_EXPECT_LANG_SW 1
#endif
#ifndef NANOTTS_EXPECT_LANG_ES
#define NANOTTS_EXPECT_LANG_ES 1
#endif

static int abort_immediately(void *user, const int16_t *samples, size_t count)
{
    (void)user;
    (void)samples;
    (void)count;
    return 1;
}

int main(void)
{
    nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t info;
    nanotts_result_t result;
    nanotts_event_t event;

    assert(nanotts_init(NULL, 16000u, NANOTTS_LANG_NONE) == NANOTTS_ERR_ARGUMENT);
    assert(nanotts_init(&tts, 7999u, NANOTTS_LANG_NONE) == NANOTTS_ERR_SAMPLE_RATE);
    assert(nanotts_init(&tts, 24001u, NANOTTS_LANG_NONE) == NANOTTS_ERR_SAMPLE_RATE);
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_COUNT) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE) == NANOTTS_OK);
    assert(nanotts_event_capacity() == NANOTTS_MAX_EVENTS);
    assert(nanotts_context_bytes() == NANOTTS_CONTEXT_BYTES);
    assert(nanotts_sample_rate(&tts) == 16000u);
    assert(nanotts_language(&tts) == NANOTTS_LANG_NONE);
    assert(nanotts_event_count(&tts) == 0u);

    assert(nanotts_language_available(NANOTTS_LANG_INDONESIAN) ==
           NANOTTS_EXPECT_LANG_ID);
    assert(nanotts_language_available(NANOTTS_LANG_KISWAHILI) ==
           NANOTTS_EXPECT_LANG_SW);
    assert(nanotts_language_available(NANOTTS_LANG_SPANISH) ==
           NANOTTS_EXPECT_LANG_ES);
    assert(nanotts_language_from_code("id") == NANOTTS_LANG_INDONESIAN);
    assert(nanotts_language_from_code("Kiswahili") == NANOTTS_LANG_KISWAHILI);
    assert(nanotts_language_from_code("es-MX") == NANOTTS_LANG_SPANISH);
    assert(nanotts_language_from_code("español") == NANOTTS_LANG_SPANISH);
    assert(nanotts_language_from_code("ESPAÑOL") == NANOTTS_LANG_SPANISH);
    assert(nanotts_language_from_code("castellano") == NANOTTS_LANG_SPANISH);
    assert(nanotts_language_from_code("bogus") == NANOTTS_LANG_COUNT);
    assert(strcmp(nanotts_language_code(NANOTTS_LANG_KISWAHILI), "sw") == 0);
    assert(strcmp(nanotts_language_code(NANOTTS_LANG_SPANISH), "es") == 0);
    assert(strstr(nanotts_language_name(NANOTTS_LANG_SPANISH), "Spanish") != NULL);
    assert(NANOTTS_LANG_SWAHILI == NANOTTS_LANG_KISWAHILI);
    assert(NANOTTS_LANG_SPANISH_LATIN_AMERICAN == NANOTTS_LANG_SPANISH);
    assert(nanotts_compiled_language_count() ==
           (size_t)(NANOTTS_EXPECT_LANG_ID + NANOTTS_EXPECT_LANG_SW +
                    NANOTTS_EXPECT_LANG_ES));
    if (nanotts_compiled_language_count() > 0u) {
        assert(nanotts_compiled_language_at(0u) != NANOTTS_LANG_NONE);
    }
    assert(nanotts_compiled_language_at(nanotts_compiled_language_count()) ==
           NANOTTS_LANG_NONE);

#if NANOTTS_EXPECT_LANG_ID
    assert(nanotts_set_language(&tts, NANOTTS_LANG_INDONESIAN) == NANOTTS_OK);
    assert(nanotts_language(&tts) == NANOTTS_LANG_INDONESIAN);
#else
    assert(nanotts_set_language(&tts, NANOTTS_LANG_INDONESIAN) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
#if NANOTTS_EXPECT_LANG_SW
    assert(nanotts_set_language(&tts, NANOTTS_LANG_KISWAHILI) == NANOTTS_OK);
    assert(nanotts_language(&tts) == NANOTTS_LANG_KISWAHILI);
#else
    assert(nanotts_set_language(&tts, NANOTTS_LANG_KISWAHILI) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
#if NANOTTS_EXPECT_LANG_ES
    assert(nanotts_set_language(&tts, NANOTTS_LANG_SPANISH) == NANOTTS_OK);
    assert(nanotts_language(&tts) == NANOTTS_LANG_SPANISH);
#else
    assert(nanotts_set_language(&tts, NANOTTS_LANG_SPANISH) ==
           NANOTTS_ERR_LANGUAGE_UNAVAILABLE);
#endif
    assert(nanotts_set_language(&tts, NANOTTS_LANG_NONE) == NANOTTS_OK);

    assert(nanotts_parse_ipa(&tts, "a", NANOTTS_NPOS, 0u, &info) == NANOTTS_OK);
    assert(info.event_count == 1u);
    nanotts_reset(&tts);
    assert(nanotts_event_count(&tts) == 0u);
    assert(nanotts_sample_rate(&tts) == 16000u);
    assert(nanotts_language(&tts) == NANOTTS_LANG_NONE);

    nanotts_default_options(&options);
    result = nanotts_speak_ipa(&tts, "a", NANOTTS_NPOS, &options,
                             abort_immediately, NULL, &info);
    assert(result == NANOTTS_ERR_CALLBACK_ABORTED);

    options.rate_q8 = 95u;
    assert(nanotts_parse_ipa(&tts, "a", NANOTTS_NPOS, 0u, &info) == NANOTTS_OK);
    assert(nanotts_render_events(&tts, &options, abort_immediately, NULL) ==
           NANOTTS_ERR_ARGUMENT);

    options.rate_q8 = 256u;
    options.pitch_cents = 1201;
    assert(nanotts_render_events(&tts, &options, abort_immediately, NULL) ==
           NANOTTS_ERR_ARGUMENT);
    options.pitch_cents = 0;

    event.phone = (uint8_t)NANOTTS_PH_A;
    event.flags = NANOTTS_EVENT_STRESS_PRIMARY;
    event.duration_percent = 100u;
    event.pitch_semitones_q4 = 0;
    assert(nanotts_set_events(&tts, &event, 1u) == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 1u);
    event.phone = (uint8_t)NANOTTS_PH_COUNT;
    assert(nanotts_set_events(&tts, &event, 1u) == NANOTTS_ERR_ARGUMENT);
    assert(nanotts_set_events(&tts, NULL, 0u) == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 0u);

    assert(nanotts_text_frontend_available() == NANOTTS_EXPECT_TEXT_FRONTEND);
    assert(nanotts_version_string() != NULL);
    assert(nanotts_phone_name(NANOTTS_PH_GH) != NULL);
    assert(strcmp(nanotts_phone_name(NANOTTS_PH_BETA), "beta") == 0);
    assert(nanotts_strerror(NANOTTS_ERR_LANGUAGE_UNAVAILABLE) != NULL);
    return 0;
}
