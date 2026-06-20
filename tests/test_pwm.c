/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include "nanotts/nanotts_pwm.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#ifndef INT16_MIN
#define INT16_MIN (-32767 - 1)
#endif
#ifndef INT16_MAX
#define INT16_MAX 32767
#endif

typedef struct pwm_sink {
    uint16_t values[16];
    size_t count;
    size_t calls;
} pwm_sink_t;

typedef struct pwm_count_sink {
    uint64_t count;
    uint16_t top;
    int invalid;
    int abort_after_first;
} pwm_count_sink_t;

static int collect_pwm(void *user, const uint16_t *values, size_t count)
{
    pwm_sink_t *sink = (pwm_sink_t *)user;
    size_t index;
    if (sink->count + count > sizeof(sink->values) / sizeof(sink->values[0])) {
        return 1;
    }
    for (index = 0u; index < count; ++index) {
        sink->values[sink->count++] = values[index];
    }
    sink->calls++;
    return 0;
}

static int count_pwm(void *user, const uint16_t *values, size_t count)
{
    pwm_count_sink_t *sink = (pwm_count_sink_t *)user;
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (values[index] > sink->top) sink->invalid = 1;
    }
    sink->count += count;
    if (sink->abort_after_first != 0) {
        sink->abort_after_first = 0;
        return 1;
    }
    return 0;
}

int main(void)
{
    static const int16_t pcm[] = {
        INT16_MIN, -16384, 0, 16384, INT16_MAX
    };
    uint16_t scratch[2];
    nanotts_pwm_output_t output;
    pwm_sink_t sink = { { 0u }, 0u, 0u };
    nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t info;
    uint16_t render_scratch[17];
    pwm_count_sink_t rendered = { 0u, 1023u, 0, 0 };

    assert(nanotts_pcm16_to_pwm(INT16_MIN, 255u, 0) == 0u);
    assert(nanotts_pcm16_to_pwm(INT16_MAX, 255u, 0) == 255u);
    assert(nanotts_pcm16_to_pwm(0, 255u, 0) == 128u);
    assert(nanotts_pcm16_to_pwm(INT16_MIN, 1023u, 1) == 1023u);
    assert(nanotts_pcm16_to_pwm(INT16_MAX, 1023u, 1) == 0u);
    assert(nanotts_pcm16_to_pwm(0, 0u, 0) == 0u);

    assert(nanotts_pwm_output_init(
        &output, 1023u, 0, scratch, 2u, collect_pwm, &sink) == NANOTTS_OK);
    assert(nanotts_pwm_write_pcm(&output, pcm, 5u) == 0);
    assert(sink.count == 5u);
    assert(sink.calls == 3u);
    assert(sink.values[0] == 0u);
    assert(sink.values[2] == 512u);
    assert(sink.values[4] == 1023u);

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE) == NANOTTS_OK);
    nanotts_default_options(&options);
    assert(nanotts_pwm_output_init(
        &output, rendered.top, 0, render_scratch,
        sizeof(render_scratch) / sizeof(render_scratch[0]),
        count_pwm, &rendered) == NANOTTS_OK);
    assert(nanotts_speak_ipa(
        &tts, "a_p_a", NANOTTS_NPOS, &options,
        nanotts_pwm_write_pcm, &output, &info) == NANOTTS_OK);
    assert(rendered.count > 1000u);
    assert(rendered.invalid == 0);

    rendered.abort_after_first = 1;
    assert(nanotts_speak_ipa(
        &tts, "a", NANOTTS_NPOS, &options,
        nanotts_pwm_write_pcm, &output, &info) == NANOTTS_ERR_CALLBACK_ABORTED);

    assert(nanotts_pwm_output_init(
        NULL, 255u, 0, scratch, 2u, collect_pwm, &sink) == NANOTTS_ERR_ARGUMENT);
    assert(nanotts_pwm_output_init(
        &output, 0u, 0, scratch, 2u, collect_pwm, &sink) == NANOTTS_ERR_ARGUMENT);
    return 0;
}
