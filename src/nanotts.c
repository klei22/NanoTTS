/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#include <string.h>

void nanotts_default_options(nanotts_options_t *options)
{
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->rate_q8 = 256u;
    options->pitch_cents = 0;
    options->volume = 220u;
    options->final_tone = (uint8_t)NANOTTS_FINAL_AUTO;
}

nanotts_result_t nanotts_init(
    nanotts_t *tts,
    uint32_t sample_rate,
    nanotts_language_t language)
{
    nanotts_impl_t *impl;

    if (tts == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (sample_rate < NANOTTS_MIN_SAMPLE_RATE ||
        sample_rate > NANOTTS_MAX_SAMPLE_RATE) {
        return NANOTTS_ERR_SAMPLE_RATE;
    }
    if (language != NANOTTS_LANG_NONE &&
        !nanotts_language_is_compiled(language)) {
        return NANOTTS_ERR_LANGUAGE_UNAVAILABLE;
    }

    memset(tts->bytes, 0, NANOTTS_CONTEXT_BYTES);
    impl = nanotts_impl(tts);
    impl->magic = NANOTTS_MAGIC;
    impl->sample_rate = sample_rate;
    impl->rng = 0x6d2b79f5u;
    impl->language = (uint8_t)language;
    return NANOTTS_OK;
}

void nanotts_reset(nanotts_t *tts)
{
    nanotts_impl_t *impl;
    uint32_t sample_rate;
    nanotts_language_t language;

    if (tts == NULL) {
        return;
    }
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return;
    }
    sample_rate = impl->sample_rate;
    language = (nanotts_language_t)impl->language;
    (void)nanotts_init(tts, sample_rate, language);
}

uint32_t nanotts_sample_rate(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (tts == NULL) {
        return 0u;
    }
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->sample_rate : 0u;
}

nanotts_result_t nanotts_set_language(
    nanotts_t *tts,
    nanotts_language_t language)
{
    nanotts_impl_t *impl;

    if (tts == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (language != NANOTTS_LANG_NONE &&
        !nanotts_language_is_compiled(language)) {
        return NANOTTS_ERR_LANGUAGE_UNAVAILABLE;
    }
    impl->language = (uint8_t)language;
    impl->event_count = 0u;
    return NANOTTS_OK;
}

nanotts_language_t nanotts_language(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (tts == NULL) {
        return NANOTTS_LANG_NONE;
    }
    impl = nanotts_impl_const(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return NANOTTS_LANG_NONE;
    }
    return (nanotts_language_t)impl->language;
}

bool nanotts_push_event(
    nanotts_impl_t *impl,
    nanotts_phone_t phone,
    uint8_t flags,
    uint8_t duration_percent,
    int8_t pitch_q4)
{
    nanotts_event_t *event;
    if (impl->event_count >= NANOTTS_MAX_EVENTS) {
        return false;
    }
    event = &impl->events[impl->event_count++];
    event->phone = (uint8_t)phone;
    event->flags = flags;
    event->duration_percent = duration_percent == 0u ? 100u : duration_percent;
    event->pitch_semitones_q4 = pitch_q4;
    return true;
}

nanotts_result_t nanotts_parse_ipa(
    nanotts_t *tts,
    const char *ipa_utf8,
    size_t length,
    uint8_t parse_flags,
    nanotts_parse_info_t *info)
{
    nanotts_impl_t *impl;
    if (tts == NULL || ipa_utf8 == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (length == NANOTTS_NPOS) {
        length = strlen(ipa_utf8);
    }
    return nanotts_ipa_parse_impl(impl, ipa_utf8, length, parse_flags, info);
}

nanotts_result_t nanotts_parse_text(
    nanotts_t *tts,
    const char *text_utf8,
    size_t length,
    nanotts_parse_info_t *info)
{
    nanotts_impl_t *impl;
    if (tts == NULL || text_utf8 == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (length == NANOTTS_NPOS) {
        length = strlen(text_utf8);
    }
#if NANOTTS_ENABLE_TEXT_FRONTEND
    return nanotts_text_parse_impl(impl, text_utf8, length, info);
#else
    (void)impl;
    (void)length;
    if (info != NULL) {
        info->event_count = 0u;
        info->error_byte = NANOTTS_NPOS;
        info->error_codepoint = 0u;
    }
    return NANOTTS_ERR_FEATURE_DISABLED;
#endif
}

nanotts_result_t nanotts_set_events(
    nanotts_t *tts,
    const nanotts_event_t *events,
    size_t count)
{
    nanotts_impl_t *impl;
    size_t index;

    if (tts == NULL || (events == NULL && count != 0u)) {
        return NANOTTS_ERR_ARGUMENT;
    }
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (count > NANOTTS_MAX_EVENTS) {
        return NANOTTS_ERR_TOO_MANY_EVENTS;
    }
    for (index = 0u; index < count; ++index) {
        if ((unsigned)events[index].phone >= (unsigned)NANOTTS_PH_COUNT) {
            return NANOTTS_ERR_ARGUMENT;
        }
    }
    if (count != 0u) {
        memcpy(impl->events, events, count * sizeof(events[0]));
    }
    impl->event_count = count;
    return NANOTTS_OK;
}

nanotts_result_t nanotts_render_events(
    nanotts_t *tts,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user)
{
    nanotts_impl_t *impl;
    nanotts_options_t defaults;

    if (tts == NULL || write == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (impl->event_count == 0u) {
        return NANOTTS_ERR_EMPTY_INPUT;
    }
    if (options == NULL) {
        nanotts_default_options(&defaults);
        options = &defaults;
    }
    return nanotts_synth_render(impl, options, write, user);
}

nanotts_result_t nanotts_speak_ipa(
    nanotts_t *tts,
    const char *ipa_utf8,
    size_t length,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user,
    nanotts_parse_info_t *info)
{
    nanotts_result_t result;
    uint8_t flags = options == NULL ? 0u : options->flags;
    result = nanotts_parse_ipa(tts, ipa_utf8, length, flags, info);
    if (result != NANOTTS_OK) {
        return result;
    }
    return nanotts_render_events(tts, options, write, user);
}

nanotts_result_t nanotts_speak_text(
    nanotts_t *tts,
    const char *text_utf8,
    size_t length,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user,
    nanotts_parse_info_t *info)
{
    nanotts_result_t result;
    result = nanotts_parse_text(tts, text_utf8, length, info);
    if (result != NANOTTS_OK) {
        return result;
    }
    return nanotts_render_events(tts, options, write, user);
}

size_t nanotts_event_capacity(void)
{
    return NANOTTS_MAX_EVENTS;
}

size_t nanotts_context_bytes(void)
{
    return NANOTTS_CONTEXT_BYTES;
}

int nanotts_text_frontend_available(void)
{
#if NANOTTS_ENABLE_TEXT_FRONTEND && NANOTTS_COMPILED_LANGUAGE_COUNT > 0
    return 1;
#else
    return 0;
#endif
}

size_t nanotts_event_count(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (tts == NULL) {
        return 0u;
    }
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->event_count : 0u;
}

const nanotts_event_t *nanotts_events(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (tts == NULL) {
        return NULL;
    }
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->events : NULL;
}

const char *nanotts_strerror(nanotts_result_t result)
{
    switch (result) {
    case NANOTTS_OK: return "success";
    case NANOTTS_ERR_ARGUMENT: return "invalid argument or uninitialized context";
    case NANOTTS_ERR_SAMPLE_RATE: return "unsupported sample rate";
    case NANOTTS_ERR_LANGUAGE_UNAVAILABLE: return "language is invalid or not compiled in";
    case NANOTTS_ERR_UTF8: return "invalid UTF-8";
    case NANOTTS_ERR_UNKNOWN_IPA: return "unsupported IPA symbol";
    case NANOTTS_ERR_TOO_MANY_EVENTS: return "utterance exceeds event capacity";
    case NANOTTS_ERR_EMPTY_INPUT: return "input contained no speakable phones";
    case NANOTTS_ERR_CALLBACK_ABORTED: return "audio callback aborted synthesis";
    case NANOTTS_ERR_OUTPUT_TOO_SMALL: return "output buffer too small";
    case NANOTTS_ERR_FEATURE_DISABLED: return "feature disabled at build time";
    case NANOTTS_ERR_INTERNAL: return "internal error";
    default: return "unknown error";
    }
}

const char *nanotts_version_string(void)
{
    return "0.5.0";
}
