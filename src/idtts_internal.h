/* SPDX-License-Identifier: MIT */
#ifndef IDTTS_INTERNAL_H
#define IDTTS_INTERNAL_H

#include "idtts/idtts.h"

#include <stdbool.h>
#include <string.h>

#ifndef IDTTS_AUDIO_BLOCK
#define IDTTS_AUDIO_BLOCK 128u
#endif
#define IDTTS_FRAME_MS 5u
#define IDTTS_PI_F 3.14159265358979323846f

#define IDTTS_MIN_SAMPLE_RATE 8000u
#define IDTTS_MAX_SAMPLE_RATE 24000u

typedef enum idtts_phone_kind {
    IDTTS_KIND_PAUSE = 0,
    IDTTS_KIND_VOWEL,
    IDTTS_KIND_DIPHTHONG,
    IDTTS_KIND_STOP_VOICELESS,
    IDTTS_KIND_STOP_VOICED,
    IDTTS_KIND_AFFRICATE_VOICELESS,
    IDTTS_KIND_AFFRICATE_VOICED,
    IDTTS_KIND_FRICATIVE_VOICELESS,
    IDTTS_KIND_FRICATIVE_VOICED,
    IDTTS_KIND_NASAL,
    IDTTS_KIND_LIQUID,
    IDTTS_KIND_GLIDE,
    IDTTS_KIND_TRILL,
    IDTTS_KIND_TAP
} idtts_phone_kind_t;

typedef struct idtts_phone_def {
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
} idtts_phone_def_t;

typedef struct idtts_biquad {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
} idtts_biquad_t;

typedef struct idtts_params {
    float f[4];
    float bw[4];
    float voiced;
    float noise;
    float aspiration;
    float amplitude;
    float noise_center;
    float noise_bw;
    float nasal;
} idtts_params_t;

typedef struct idtts_impl {
    uint32_t magic;
    uint32_t sample_rate;
    uint32_t rng;

    size_t event_count;
    idtts_event_t events[IDTTS_MAX_EVENTS];

    float phase;
    float previous_glottal_flow;
    float source_tilt_1;
    float source_tilt_2;
    float previous_noise;
    float dc_prev_x;
    float dc_prev_y;

    idtts_biquad_t voiced_filters[4];
    idtts_biquad_t voiced_detail_filter;
    idtts_biquad_t noise_filters[3];
    idtts_biquad_t nasal_filter;

    int16_t audio_block[IDTTS_AUDIO_BLOCK];
    size_t audio_count;
} idtts_impl_t;

#define IDTTS_MAGIC 0x49545453u /* ITTS */

typedef char idtts_context_size_check[
    (sizeof(idtts_impl_t) <= IDTTS_CONTEXT_BYTES) ? 1 : -1];

static inline idtts_impl_t *idtts_impl(idtts_t *tts)
{
    return (idtts_impl_t *)(void *)tts->bytes;
}

static inline const idtts_impl_t *idtts_impl_const(const idtts_t *tts)
{
    return (const idtts_impl_t *)(const void *)tts->bytes;
}

static inline float idtts_clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline float idtts_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline float idtts_smoothstep(float t)
{
    t = idtts_clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

const idtts_phone_def_t *idtts_phone_def(idtts_phone_t phone);
bool idtts_phone_is_vowel(idtts_phone_t phone);
bool idtts_phone_is_pause(idtts_phone_t phone);
bool idtts_phone_is_sonorant(idtts_phone_t phone);

idtts_result_t idtts_ipa_parse_impl(
    idtts_impl_t *impl,
    const char *ipa,
    size_t length,
    uint8_t parse_flags,
    idtts_parse_info_t *info);

idtts_result_t idtts_text_parse_impl(
    idtts_impl_t *impl,
    const char *text,
    size_t length,
    idtts_parse_info_t *info);

idtts_result_t idtts_synth_render(
    idtts_impl_t *impl,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user);

bool idtts_push_event(
    idtts_impl_t *impl,
    idtts_phone_t phone,
    uint8_t flags,
    uint8_t duration_percent,
    int8_t pitch_q4);

#endif
