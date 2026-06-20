/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include "nanotts/nanotts_pcm.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#ifndef INT16_MIN
#define INT16_MIN (-32767 - 1)
#endif
#ifndef INT16_MAX
#define INT16_MAX 32767
#endif

typedef struct byte_sink {
    uint8_t bytes[32];
    size_t count;
    size_t calls;
} byte_sink_t;

typedef struct byte_count_sink {
    uint64_t count;
    int abort_after_first;
} byte_count_sink_t;

static int collect_bytes(void *user, const uint8_t *bytes, size_t count)
{
    byte_sink_t *sink = (byte_sink_t *)user;
    size_t index;
    if (sink->count + count > sizeof(sink->bytes)) return 1;
    for (index = 0u; index < count; ++index) {
        sink->bytes[sink->count++] = bytes[index];
    }
    sink->calls++;
    return 0;
}

static int count_bytes(void *user, const uint8_t *bytes, size_t count)
{
    byte_count_sink_t *sink = (byte_count_sink_t *)user;
    (void)bytes;
    sink->count += count;
    if (sink->abort_after_first != 0) {
        sink->abort_after_first = 0;
        return 1;
    }
    return 0;
}

int main(void)
{
    static const int16_t samples[] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    static const uint8_t expected[] = {
        0x00u, 0x80u, 0xffu, 0xffu, 0x00u,
        0x00u, 0x01u, 0x00u, 0xffu, 0x7fu
    };
    uint8_t scratch[4];
    nanotts_pcm16le_output_t output;
    byte_sink_t sink = { { 0u }, 0u, 0u };
    size_t index;
    nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t info;
    uint8_t render_scratch[31];
    byte_count_sink_t rendered = { 0u, 0 };

    assert(nanotts_pcm16le_output_init(
        &output, scratch, sizeof(scratch), collect_bytes, &sink) == NANOTTS_OK);
    assert(nanotts_pcm16le_write_pcm(&output, samples, 5u) == 0);
    assert(sink.count == sizeof(expected));
    assert(sink.calls == 3u);
    for (index = 0u; index < sizeof(expected); ++index) {
        assert(sink.bytes[index] == expected[index]);
    }

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE) == NANOTTS_OK);
    nanotts_default_options(&options);
    assert(nanotts_pcm16le_output_init(
        &output, render_scratch, sizeof(render_scratch),
        count_bytes, &rendered) == NANOTTS_OK);
    assert(nanotts_speak_ipa(
        &tts, "a_p_a", NANOTTS_NPOS, &options,
        nanotts_pcm16le_write_pcm, &output, &info) == NANOTTS_OK);
    assert(rendered.count > 2000u);
    assert((rendered.count & 1u) == 0u);

    rendered.abort_after_first = 1;
    assert(nanotts_speak_ipa(
        &tts, "a", NANOTTS_NPOS, &options,
        nanotts_pcm16le_write_pcm, &output, &info) == NANOTTS_ERR_CALLBACK_ABORTED);

    assert(nanotts_pcm16le_output_init(
        NULL, scratch, sizeof(scratch), collect_bytes, &sink) == NANOTTS_ERR_ARGUMENT);
    assert(nanotts_pcm16le_output_init(
        &output, scratch, 1u, collect_bytes, &sink) == NANOTTS_ERR_ARGUMENT);
    return 0;
}
