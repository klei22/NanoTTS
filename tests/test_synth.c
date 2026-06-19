/* SPDX-License-Identifier: MIT */
#include "idtts/idtts.h"

#include <assert.h>
#include <stdint.h>

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct sink {
    uint64_t count;
    int32_t peak;
    uint32_t checksum;
    uint64_t sum_squares;
} sink_t;

static int collect(void *user, const int16_t *samples, size_t count)
{
    sink_t *sink = (sink_t *)user;
    size_t index;
    for (index = 0u; index < count; ++index) {
        int32_t value = samples[index];
        int32_t absolute = value < 0 ? -value : value;
        if (absolute > sink->peak) sink->peak = absolute;
        sink->checksum = (sink->checksum * 16777619u) ^ (uint16_t)value;
        sink->sum_squares += (uint64_t)(value * value);
    }
    sink->count += count;
    return 0;
}

static sink_t render_phrase(uint32_t sample_rate, idtts_final_tone_t tone)
{
    idtts_t tts;
    idtts_options_t options;
    idtts_parse_info_t info;
    sink_t sink = { 0u, 0, 2166136261u, 0u };

    assert(idtts_init(&tts, sample_rate) == IDTTS_OK);
    idtts_default_options(&options);
    options.final_tone = (uint8_t)tone;
    assert(idtts_speak_ipa(
        &tts, "s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i", IDTTS_NPOS,
        &options, collect, &sink, &info) == IDTTS_OK);
    assert(sink.count > sample_rate / 2u);
    assert(sink.peak > 100);
    assert(sink.peak <= 32767);
    return sink;
}

static sink_t render_ipa_text(const char *ipa)
{
    idtts_t tts;
    idtts_options_t options;
    idtts_parse_info_t info;
    sink_t sink = { 0u, 0, 2166136261u, 0u };

    assert(idtts_init(&tts, 16000u) == IDTTS_OK);
    idtts_default_options(&options);
    options.final_tone = (uint8_t)IDTTS_FINAL_LEVEL;
    options.flags = IDTTS_OPT_NO_AUTOPAUSE;
    assert(idtts_speak_ipa(&tts, ipa, IDTTS_NPOS, &options,
                           collect, &sink, &info) == IDTTS_OK);
    return sink;
}

static sink_t render_ipa_tone(const char *ipa, idtts_final_tone_t tone)
{
    idtts_t tts;
    idtts_options_t options;
    idtts_parse_info_t info;
    sink_t sink = { 0u, 0, 2166136261u, 0u };

    assert(idtts_init(&tts, 16000u) == IDTTS_OK);
    idtts_default_options(&options);
    options.final_tone = (uint8_t)tone;
    assert(idtts_speak_ipa(&tts, ipa, IDTTS_NPOS, &options,
                           collect, &sink, &info) == IDTTS_OK);
    return sink;
}

int main(void)
{
    idtts_t tts;
    idtts_options_t options;
    idtts_event_t all_phones[IDTTS_PH_COUNT - IDTTS_PH_A];
    sink_t first;
    sink_t second;
    sink_t all = { 0u, 0, 2166136261u, 0u };
    sink_t vowels;
    sink_t fricative;
    sink_t question_auto;
    sink_t question_fall;
    sink_t marked_auto;
    sink_t marked_rise;
    sink_t marked_fall;
    size_t index;

    first = render_phrase(16000u, IDTTS_FINAL_FALL);
    second = render_phrase(16000u, IDTTS_FINAL_FALL);
    assert(first.count == second.count);
    assert(first.checksum == second.checksum);

    (void)render_phrase(8000u, IDTTS_FINAL_LEVEL);
    (void)render_phrase(24000u, IDTTS_FINAL_RISE);

    assert(idtts_init(&tts, 16000u) == IDTTS_OK);
    for (index = 0u; index < ARRAY_COUNT(all_phones); ++index) {
        all_phones[index].phone = (uint8_t)(IDTTS_PH_A + index);
        all_phones[index].flags = 0u;
        all_phones[index].duration_percent = 35u;
        all_phones[index].pitch_semitones_q4 = 0;
    }
    assert(idtts_set_events(&tts, all_phones, ARRAY_COUNT(all_phones)) == IDTTS_OK);
    idtts_default_options(&options);
    options.rate_q8 = 400u;
    assert(idtts_render_events(&tts, &options, collect, &all) == IDTTS_OK);
    assert(all.count > 1000u);
    assert(all.peak > 100);
    assert(all.peak < 32000);

    vowels = render_ipa_text("a_a_a");
    fricative = render_ipa_text("a_s_a");
    assert(vowels.peak > 3000);
    assert(vowels.peak < 30000);
    assert(fricative.peak < 30000);
    /* Regression guard for the original noise-dominant renderer. */
    assert(fricative.sum_squares < vowels.sum_squares * 4u);
    assert(vowels.sum_squares < fricative.sum_squares * 4u);

    question_auto = render_phrase(16000u, IDTTS_FINAL_AUTO);
    question_fall = render_phrase(16000u, IDTTS_FINAL_FALL);
    /* No question punctuation is present here, so AUTO intentionally falls. */
    assert(question_auto.checksum == question_fall.checksum);

    marked_auto = render_ipa_tone("a_p_a?", IDTTS_FINAL_AUTO);
    marked_rise = render_ipa_tone("a_p_a?", IDTTS_FINAL_RISE);
    marked_fall = render_ipa_tone("a_p_a?", IDTTS_FINAL_FALL);
    assert(marked_auto.checksum == marked_rise.checksum);
    assert(marked_auto.checksum != marked_fall.checksum);
    return 0;
}
