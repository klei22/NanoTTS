/* SPDX-License-Identifier: MIT */
#include "idtts/idtts.h"

#include <assert.h>
#include <stdint.h>

#ifndef IDTTS_EXPECT_TEXT_FRONTEND
#define IDTTS_EXPECT_TEXT_FRONTEND 1
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
    idtts_t tts;
    idtts_options_t options;
    idtts_parse_info_t info;
    idtts_result_t result;
    idtts_event_t event;

    assert(idtts_init(NULL, 16000u) == IDTTS_ERR_ARGUMENT);
    assert(idtts_init(&tts, 7999u) == IDTTS_ERR_SAMPLE_RATE);
    assert(idtts_init(&tts, 24001u) == IDTTS_ERR_SAMPLE_RATE);
    assert(idtts_init(&tts, 16000u) == IDTTS_OK);
    assert(idtts_event_capacity() == IDTTS_MAX_EVENTS);
    assert(idtts_context_bytes() == IDTTS_CONTEXT_BYTES);
    assert(idtts_sample_rate(&tts) == 16000u);
    assert(idtts_event_count(&tts) == 0u);

    assert(idtts_parse_ipa(&tts, "a", IDTTS_NPOS, 0u, &info) == IDTTS_OK);
    assert(info.event_count == 1u);
    idtts_reset(&tts);
    assert(idtts_event_count(&tts) == 0u);
    assert(idtts_sample_rate(&tts) == 16000u);

    idtts_default_options(&options);
    result = idtts_speak_ipa(&tts, "a", IDTTS_NPOS, &options,
                             abort_immediately, NULL, &info);
    assert(result == IDTTS_ERR_CALLBACK_ABORTED);

    options.rate_q8 = 95u;
    assert(idtts_parse_ipa(&tts, "a", IDTTS_NPOS, 0u, &info) == IDTTS_OK);
    assert(idtts_render_events(&tts, &options, abort_immediately, NULL) ==
           IDTTS_ERR_ARGUMENT);

    options.rate_q8 = 256u;
    options.pitch_cents = 1201;
    assert(idtts_render_events(&tts, &options, abort_immediately, NULL) ==
           IDTTS_ERR_ARGUMENT);
    options.pitch_cents = 0;

    event.phone = (uint8_t)IDTTS_PH_A;
    event.flags = IDTTS_EVENT_STRESS_PRIMARY;
    event.duration_percent = 100u;
    event.pitch_semitones_q4 = 0;
    assert(idtts_set_events(&tts, &event, 1u) == IDTTS_OK);
    assert(idtts_event_count(&tts) == 1u);
    event.phone = (uint8_t)IDTTS_PH_COUNT;
    assert(idtts_set_events(&tts, &event, 1u) == IDTTS_ERR_ARGUMENT);
    assert(idtts_set_events(&tts, NULL, 0u) == IDTTS_OK);
    assert(idtts_event_count(&tts) == 0u);

    assert(idtts_text_frontend_available() == IDTTS_EXPECT_TEXT_FRONTEND);
    assert(idtts_version_string() != NULL);
    assert(idtts_phone_name(IDTTS_PH_NG) != NULL);
    assert(idtts_strerror(IDTTS_ERR_UTF8) != NULL);
    return 0;
}
