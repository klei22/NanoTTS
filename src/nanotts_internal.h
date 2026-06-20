/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_INTERNAL_H
#define NANOTTS_INTERNAL_H

#include "nanotts/nanotts.h"

#include <stdbool.h>
#include <string.h>

#ifndef NANOTTS_AUDIO_BLOCK
#define NANOTTS_AUDIO_BLOCK 128u
#endif
#ifndef NANOTTS_ENABLE_TEXT_FRONTEND
#define NANOTTS_ENABLE_TEXT_FRONTEND 1
#endif
#ifndef NANOTTS_ENABLE_LANG_ID
#define NANOTTS_ENABLE_LANG_ID 1
#endif
#ifndef NANOTTS_ENABLE_LANG_SW
#define NANOTTS_ENABLE_LANG_SW 1
#endif
#ifndef NANOTTS_ENABLE_LANG_ES
#define NANOTTS_ENABLE_LANG_ES 1
#endif
#ifndef NANOTTS_ENABLE_LANG_MS
#define NANOTTS_ENABLE_LANG_MS 1
#endif
#ifndef NANOTTS_ENABLE_LANG_MI
#define NANOTTS_ENABLE_LANG_MI 1
#endif
#ifndef NANOTTS_ENABLE_LANG_HAW
#define NANOTTS_ENABLE_LANG_HAW 1
#endif

#if NANOTTS_ENABLE_TEXT_FRONTEND
#define NANOTTS_COMPILED_LANGUAGE_COUNT \
    (NANOTTS_ENABLE_LANG_ID + NANOTTS_ENABLE_LANG_SW + \
     NANOTTS_ENABLE_LANG_ES + NANOTTS_ENABLE_LANG_MS + \
     NANOTTS_ENABLE_LANG_MI + NANOTTS_ENABLE_LANG_HAW)
#else
#define NANOTTS_COMPILED_LANGUAGE_COUNT 0
#endif

#define NANOTTS_FRAME_MS 5u
#define NANOTTS_PI_F 3.14159265358979323846f

#define NANOTTS_MIN_SAMPLE_RATE 8000u
#define NANOTTS_MAX_SAMPLE_RATE 24000u

typedef enum nanotts_phone_kind {
    NANOTTS_KIND_PAUSE = 0,
    NANOTTS_KIND_VOWEL,
    NANOTTS_KIND_DIPHTHONG,
    NANOTTS_KIND_STOP_VOICELESS,
    NANOTTS_KIND_STOP_VOICED,
    NANOTTS_KIND_AFFRICATE_VOICELESS,
    NANOTTS_KIND_AFFRICATE_VOICED,
    NANOTTS_KIND_FRICATIVE_VOICELESS,
    NANOTTS_KIND_FRICATIVE_VOICED,
    NANOTTS_KIND_NASAL,
    NANOTTS_KIND_LIQUID,
    NANOTTS_KIND_GLIDE,
    NANOTTS_KIND_TRILL,
    NANOTTS_KIND_TAP
} nanotts_phone_kind_t;

typedef struct nanotts_phone_def {
    uint8_t kind;
    uint8_t duration_ms;
    uint8_t voiced_q8;
    uint8_t noise_q8;
    uint16_t f1;
    uint16_t f2;
    uint16_t f3;
    uint16_t f4;
    uint16_t e1;
    uint16_t e2;
    uint16_t e3;
    uint16_t e4;
    uint16_t noise_center;
    uint16_t noise_bandwidth;
    uint8_t amp_q8;
    uint8_t reserved;
} nanotts_phone_def_t;

typedef struct nanotts_biquad {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
} nanotts_biquad_t;

typedef struct nanotts_params {
    float f[4];
    float bw[4];
    float voiced;
    float noise;
    float aspiration;
    float amplitude;
    float noise_center;
    float noise_bw;
    float nasal;
} nanotts_params_t;

typedef struct nanotts_impl {
    uint32_t magic;
    uint32_t sample_rate;
    uint32_t rng;
    uint8_t language;
    uint8_t reserved[3];

    size_t event_count;
    nanotts_event_t events[NANOTTS_MAX_EVENTS];

    float phase;
    float previous_glottal_flow;
    float source_tilt_1;
    float source_tilt_2;
    float previous_noise;
    float dc_prev_x;
    float dc_prev_y;

    nanotts_biquad_t voiced_filters[4];
    nanotts_biquad_t voiced_detail_filter;
    nanotts_biquad_t noise_filters[3];
    nanotts_biquad_t nasal_filter;

    int16_t audio_block[NANOTTS_AUDIO_BLOCK];
    size_t audio_count;
} nanotts_impl_t;

#define NANOTTS_MAGIC 0x4e545453u /* NTTS */

typedef char nanotts_context_size_check[
    (sizeof(nanotts_impl_t) <= NANOTTS_CONTEXT_BYTES) ? 1 : -1];

static inline nanotts_impl_t *nanotts_impl(nanotts_t *tts)
{
    return (nanotts_impl_t *)(void *)tts->bytes;
}

static inline const nanotts_impl_t *nanotts_impl_const(const nanotts_t *tts)
{
    return (const nanotts_impl_t *)(const void *)tts->bytes;
}

static inline float nanotts_clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline float nanotts_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline float nanotts_smoothstep(float t)
{
    t = nanotts_clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

const nanotts_phone_def_t *nanotts_phone_def(nanotts_phone_t phone);
bool nanotts_phone_is_vowel(nanotts_phone_t phone);
bool nanotts_phone_is_pause(nanotts_phone_t phone);
bool nanotts_phone_is_sonorant(nanotts_phone_t phone);

nanotts_result_t nanotts_ipa_parse_impl(
    nanotts_impl_t *impl,
    const char *ipa,
    size_t length,
    uint8_t parse_flags,
    nanotts_parse_info_t *info);

nanotts_result_t nanotts_text_parse_impl(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info);

bool nanotts_language_is_compiled(nanotts_language_t language);

#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
    nanotts_result_t parser_( \
        nanotts_impl_t *impl, const char *text, size_t length, \
        nanotts_parse_info_t *info);
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE


nanotts_result_t nanotts_synth_render(
    nanotts_impl_t *impl,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user);

bool nanotts_push_event(
    nanotts_impl_t *impl,
    nanotts_phone_t phone,
    uint8_t flags,
    uint8_t duration_percent,
    int8_t pitch_q4);

#endif
