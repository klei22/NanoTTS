/* SPDX-License-Identifier: MIT */
#include "idtts_internal.h"

#include <string.h>

#ifndef IDTTS_ENABLE_TEXT_FRONTEND
#define IDTTS_ENABLE_TEXT_FRONTEND 1
#endif

void idtts_default_options(idtts_options_t *options)
{
    if (options == NULL) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->rate_q8 = 256u;
    options->pitch_cents = 0;
    options->volume = 220u;
    options->final_tone = (uint8_t)IDTTS_FINAL_AUTO;
}

idtts_result_t idtts_init(idtts_t *tts, uint32_t sample_rate)
{
    idtts_impl_t *impl;

    if (tts == NULL) {
        return IDTTS_ERR_ARGUMENT;
    }
    if (sample_rate < IDTTS_MIN_SAMPLE_RATE ||
        sample_rate > IDTTS_MAX_SAMPLE_RATE) {
        return IDTTS_ERR_SAMPLE_RATE;
    }

    memset(tts->bytes, 0, IDTTS_CONTEXT_BYTES);
    impl = idtts_impl(tts);
    impl->magic = IDTTS_MAGIC;
    impl->sample_rate = sample_rate;
    impl->rng = 0x6d2b79f5u;
    return IDTTS_OK;
}

void idtts_reset(idtts_t *tts)
{
    idtts_impl_t *impl;
    uint32_t sample_rate;

    if (tts == NULL) {
        return;
    }
    impl = idtts_impl(tts);
    if (impl->magic != IDTTS_MAGIC) {
        return;
    }
    sample_rate = impl->sample_rate;
    (void)idtts_init(tts, sample_rate);
}

uint32_t idtts_sample_rate(const idtts_t *tts)
{
    const idtts_impl_t *impl;
    if (tts == NULL) {
        return 0u;
    }
    impl = idtts_impl_const(tts);
    return impl->magic == IDTTS_MAGIC ? impl->sample_rate : 0u;
}

bool idtts_push_event(
    idtts_impl_t *impl,
    idtts_phone_t phone,
    uint8_t flags,
    uint8_t duration_percent,
    int8_t pitch_q4)
{
    idtts_event_t *event;
    if (impl->event_count >= IDTTS_MAX_EVENTS) {
        return false;
    }
    event = &impl->events[impl->event_count++];
    event->phone = (uint8_t)phone;
    event->flags = flags;
    event->duration_percent = duration_percent == 0u ? 100u : duration_percent;
    event->pitch_semitones_q4 = pitch_q4;
    return true;
}

idtts_result_t idtts_parse_ipa(
    idtts_t *tts,
    const char *ipa_utf8,
    size_t length,
    uint8_t parse_flags,
    idtts_parse_info_t *info)
{
    idtts_impl_t *impl;
    if (tts == NULL || ipa_utf8 == NULL) {
        return IDTTS_ERR_ARGUMENT;
    }
    impl = idtts_impl(tts);
    if (impl->magic != IDTTS_MAGIC) {
        return IDTTS_ERR_ARGUMENT;
    }
    if (length == IDTTS_NPOS) {
        length = strlen(ipa_utf8);
    }
    return idtts_ipa_parse_impl(impl, ipa_utf8, length, parse_flags, info);
}

idtts_result_t idtts_parse_text(
    idtts_t *tts,
    const char *text_utf8,
    size_t length,
    idtts_parse_info_t *info)
{
    idtts_impl_t *impl;
    if (tts == NULL || text_utf8 == NULL) {
        return IDTTS_ERR_ARGUMENT;
    }
    impl = idtts_impl(tts);
    if (impl->magic != IDTTS_MAGIC) {
        return IDTTS_ERR_ARGUMENT;
    }
    if (length == IDTTS_NPOS) {
        length = strlen(text_utf8);
    }
#if IDTTS_ENABLE_TEXT_FRONTEND
    return idtts_text_parse_impl(impl, text_utf8, length, info);
#else
    (void)impl;
    (void)length;
    if (info != NULL) {
        info->event_count = 0u;
        info->error_byte = IDTTS_NPOS;
        info->error_codepoint = 0u;
    }
    return IDTTS_ERR_FEATURE_DISABLED;
#endif
}

idtts_result_t idtts_set_events(
    idtts_t *tts,
    const idtts_event_t *events,
    size_t count)
{
    idtts_impl_t *impl;
    size_t index;

    if (tts == NULL || (events == NULL && count != 0u)) {
        return IDTTS_ERR_ARGUMENT;
    }
    impl = idtts_impl(tts);
    if (impl->magic != IDTTS_MAGIC) {
        return IDTTS_ERR_ARGUMENT;
    }
    if (count > IDTTS_MAX_EVENTS) {
        return IDTTS_ERR_TOO_MANY_EVENTS;
    }
    for (index = 0u; index < count; ++index) {
        if ((unsigned)events[index].phone >= (unsigned)IDTTS_PH_COUNT) {
            return IDTTS_ERR_ARGUMENT;
        }
    }
    if (count != 0u) {
        memcpy(impl->events, events, count * sizeof(events[0]));
    }
    impl->event_count = count;
    return IDTTS_OK;
}

idtts_result_t idtts_render_events(
    idtts_t *tts,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user)
{
    idtts_impl_t *impl;
    idtts_options_t defaults;

    if (tts == NULL || write == NULL) {
        return IDTTS_ERR_ARGUMENT;
    }
    impl = idtts_impl(tts);
    if (impl->magic != IDTTS_MAGIC) {
        return IDTTS_ERR_ARGUMENT;
    }
    if (impl->event_count == 0u) {
        return IDTTS_ERR_EMPTY_INPUT;
    }
    if (options == NULL) {
        idtts_default_options(&defaults);
        options = &defaults;
    }
    return idtts_synth_render(impl, options, write, user);
}

idtts_result_t idtts_speak_ipa(
    idtts_t *tts,
    const char *ipa_utf8,
    size_t length,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user,
    idtts_parse_info_t *info)
{
    idtts_result_t result;
    uint8_t flags = options == NULL ? 0u : options->flags;
    result = idtts_parse_ipa(tts, ipa_utf8, length, flags, info);
    if (result != IDTTS_OK) {
        return result;
    }
    return idtts_render_events(tts, options, write, user);
}

idtts_result_t idtts_speak_text(
    idtts_t *tts,
    const char *text_utf8,
    size_t length,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user,
    idtts_parse_info_t *info)
{
    idtts_result_t result;
    result = idtts_parse_text(tts, text_utf8, length, info);
    if (result != IDTTS_OK) {
        return result;
    }
    return idtts_render_events(tts, options, write, user);
}

size_t idtts_event_capacity(void)
{
    return IDTTS_MAX_EVENTS;
}

size_t idtts_context_bytes(void)
{
    return IDTTS_CONTEXT_BYTES;
}

int idtts_text_frontend_available(void)
{
#if IDTTS_ENABLE_TEXT_FRONTEND
    return 1;
#else
    return 0;
#endif
}

size_t idtts_event_count(const idtts_t *tts)
{
    const idtts_impl_t *impl;
    if (tts == NULL) {
        return 0u;
    }
    impl = idtts_impl_const(tts);
    return impl->magic == IDTTS_MAGIC ? impl->event_count : 0u;
}

const idtts_event_t *idtts_events(const idtts_t *tts)
{
    const idtts_impl_t *impl;
    if (tts == NULL) {
        return NULL;
    }
    impl = idtts_impl_const(tts);
    return impl->magic == IDTTS_MAGIC ? impl->events : NULL;
}

const char *idtts_strerror(idtts_result_t result)
{
    switch (result) {
    case IDTTS_OK: return "success";
    case IDTTS_ERR_ARGUMENT: return "invalid argument or uninitialized context";
    case IDTTS_ERR_SAMPLE_RATE: return "unsupported sample rate";
    case IDTTS_ERR_UTF8: return "invalid UTF-8";
    case IDTTS_ERR_UNKNOWN_IPA: return "unsupported IPA symbol";
    case IDTTS_ERR_TOO_MANY_EVENTS: return "utterance exceeds event capacity";
    case IDTTS_ERR_EMPTY_INPUT: return "input contained no speakable phones";
    case IDTTS_ERR_CALLBACK_ABORTED: return "audio callback aborted synthesis";
    case IDTTS_ERR_OUTPUT_TOO_SMALL: return "output buffer too small";
    case IDTTS_ERR_FEATURE_DISABLED: return "feature disabled at build time";
    case IDTTS_ERR_INTERNAL: return "internal error";
    default: return "unknown error";
    }
}

const char *idtts_version_string(void)
{
    return "0.2.1";
}
