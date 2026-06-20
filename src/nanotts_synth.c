/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#ifndef NANOTTS_USE_LIBM
#define NANOTTS_USE_LIBM 1
#endif

#if NANOTTS_USE_LIBM
#include <math.h>
#endif

static float nanotts_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static float nanotts_floor_positive(float value)
{
#if NANOTTS_USE_LIBM
    return floorf(value);
#else
    return (float)(uint32_t)value;
#endif
}

static void nanotts_sincos(float angle, float *sine, float *cosine)
{
#if NANOTTS_USE_LIBM
    *sine = sinf(angle);
    *cosine = cosf(angle);
#else
    /* angle is in [0, pi). Reduce to [0, pi/2], then use compact
     * Taylor polynomials. Maximum absolute error is below 2e-4 here. */
    float x = angle;
    float cos_sign = 1.0f;
    float x2;
    if (x > 0.5f * NANOTTS_PI_F) {
        x = NANOTTS_PI_F - x;
        cos_sign = -1.0f;
    }
    x2 = x * x;
    *sine = x * (1.0f + x2 * (-0.1666666667f +
             x2 * (0.0083333333f + x2 * -0.0001984127f)));
    *cosine = cos_sign * (1.0f + x2 * (-0.5f +
               x2 * (0.0416666667f + x2 * (-0.0013888889f +
               x2 * 0.0000248016f))));
#endif
}

static float nanotts_exp2(float exponent)
{
#if NANOTTS_USE_LIBM
    return powf(2.0f, exponent);
#else
    /* All call sites stay near [-1, 1]. A sixth-order exp polynomial is
     * sufficiently accurate for pitch control and avoids a libm dependency. */
    const float ln2 = 0.6931471806f;
    float y = exponent * ln2;
    float y2 = y * y;
    return 1.0f + y + y2 * (0.5f + y * (0.1666666667f +
           y * (0.0416666667f + y * (0.0083333333f +
           y * 0.0013888889f))));
#endif
}

static void biquad_clear(nanotts_biquad_t *filter)
{
    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
}

static void biquad_set_bandpass(
    nanotts_biquad_t *filter,
    float center,
    float bandwidth,
    float sample_rate)
{
    float nyquist_guard = sample_rate * 0.455f;
    float w0;
    float q;
    float alpha;
    float a0;
    float c;
    float s;

    center = nanotts_clampf(center, 40.0f, nyquist_guard);
    bandwidth = nanotts_clampf(bandwidth, 45.0f, sample_rate * 0.40f);
    q = nanotts_clampf(center / bandwidth, 0.32f, 24.0f);
    w0 = 2.0f * NANOTTS_PI_F * center / sample_rate;
    nanotts_sincos(w0, &s, &c);
    alpha = s / (2.0f * q);
    a0 = 1.0f + alpha;

    filter->b0 = alpha / a0;
    filter->b1 = 0.0f;
    filter->b2 = -alpha / a0;
    filter->a1 = (-2.0f * c) / a0;
    filter->a2 = (1.0f - alpha) / a0;
}

static void biquad_set_resonator(
    nanotts_biquad_t *filter,
    float center,
    float bandwidth,
    float sample_rate)
{
    float nyquist_guard = sample_rate * 0.455f;
    float radius;
    float angle;
    float c;
    float sine_unused;
    float feedback1;
    float feedback2;

    center = nanotts_clampf(center, 40.0f, nyquist_guard);
    bandwidth = nanotts_clampf(bandwidth, 45.0f, sample_rate * 0.40f);
    radius = nanotts_exp2((-NANOTTS_PI_F * bandwidth / sample_rate) / 0.6931471806f);
    angle = 2.0f * NANOTTS_PI_F * center / sample_rate;
    nanotts_sincos(angle, &sine_unused, &c);
    feedback1 = 2.0f * radius * c;
    feedback2 = -radius * radius;

    /* Unity gain at DC. Cascading these all-pole sections produces the
     * spectral envelope required for voiced formants without the severe
     * amplitude imbalance of summing narrow band-pass outputs. */
    filter->b0 = 1.0f - feedback1 - feedback2;
    filter->b1 = 0.0f;
    filter->b2 = 0.0f;
    filter->a1 = -feedback1;
    filter->a2 = -feedback2;
}

static float biquad_process(nanotts_biquad_t *filter, float input)
{
    float output = filter->b0 * input + filter->b1 * filter->x1 +
                   filter->b2 * filter->x2 - filter->a1 * filter->y1 -
                   filter->a2 * filter->y2;
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    return output;
}

static float q8_to_float(uint8_t value)
{
    return (float)value / 255.0f;
}

static float phone_target(const nanotts_phone_def_t *def, unsigned index, bool end)
{
    const uint16_t *start = &def->f1;
    const uint16_t *finish = &def->e1;
    return (float)(end ? finish[index] : start[index]);
}

static bool adjacent_phone(
    const nanotts_impl_t *impl,
    size_t event_index,
    int direction,
    nanotts_phone_t *phone)
{
    size_t index;

    if (direction < 0) {
        if (event_index == 0u) return false;
        index = event_index - 1u;
    } else {
        index = event_index + 1u;
        if (index >= impl->event_count) return false;
    }
    *phone = (nanotts_phone_t)impl->events[index].phone;
    return !nanotts_phone_is_pause(*phone);
}

static bool consonant_locus(
    nanotts_phone_t phone,
    const nanotts_params_t *vowel,
    float locus[4])
{
    locus[0] = 350.0f;
    locus[2] = 2550.0f;
    locus[3] = 3500.0f;

    switch (phone) {
    case NANOTTS_PH_P:
    case NANOTTS_PH_B:
    case NANOTTS_PH_BETA:
    case NANOTTS_PH_M:
    case NANOTTS_PH_W:
        locus[1] = 850.0f;
        locus[2] = 2300.0f;
        return true;

    case NANOTTS_PH_F:
    case NANOTTS_PH_V:
        locus[1] = 1150.0f;
        locus[2] = 2500.0f;
        return true;

    case NANOTTS_PH_TH:
    case NANOTTS_PH_DH:
        locus[1] = 1550.0f;
        locus[2] = 2650.0f;
        return true;

    case NANOTTS_PH_T:
    case NANOTTS_PH_D:
    case NANOTTS_PH_S:
    case NANOTTS_PH_Z:
    case NANOTTS_PH_N:
    case NANOTTS_PH_L:
    case NANOTTS_PH_R:
    case NANOTTS_PH_TAP:
        locus[1] = 1800.0f;
        locus[2] = 2750.0f;
        return true;

    case NANOTTS_PH_CH:
    case NANOTTS_PH_J:
    case NANOTTS_PH_SH:
    case NANOTTS_PH_NY:
    case NANOTTS_PH_Y:
        locus[1] = 2200.0f;
        locus[2] = 3000.0f;
        return true;

    case NANOTTS_PH_K:
    case NANOTTS_PH_G:
    case NANOTTS_PH_X:
    case NANOTTS_PH_GH:
    case NANOTTS_PH_NG:
        /* The velar locus moves with the neighboring vowel rather than being
         * a fixed spectral target. This retains /k g ng/ identity without
         * pulling every vowel toward the same unnatural transition. */
        locus[1] = nanotts_clampf(0.55f * vowel->f[1] + 950.0f, 1250.0f, 2150.0f);
        locus[2] = 2500.0f;
        return true;

    case NANOTTS_PH_GLOTTAL_STOP:
    case NANOTTS_PH_H:
        /* Glottal phones inherit the vowel tract rather than imposing an oral
         * place of articulation. */
        return false;

    default:
        return false;
    }
}

static void base_params(
    const nanotts_phone_def_t *def,
    float progress,
    nanotts_params_t *params)
{
    unsigned i;
    bool moving = def->kind == NANOTTS_KIND_DIPHTHONG ||
                  def->kind == NANOTTS_KIND_GLIDE;
    float move = moving ? nanotts_smoothstep(progress) : 0.0f;

    for (i = 0u; i < 4u; ++i) {
        params->f[i] = nanotts_lerp(
            phone_target(def, i, false),
            phone_target(def, i, true),
            move);
    }
    params->bw[0] = 92.0f;
    params->bw[1] = 92.0f;
    params->bw[2] = 125.0f;
    params->bw[3] = 190.0f;
    params->voiced = q8_to_float(def->voiced_q8);
    params->noise = q8_to_float(def->noise_q8);
    params->aspiration = 0.0f;
    params->amplitude = q8_to_float(def->amp_q8);
    params->noise_center = (float)def->noise_center;
    params->noise_bw = (float)def->noise_bandwidth;
    params->nasal = 0.0f;
}

static void apply_coarticulation(
    const nanotts_impl_t *impl,
    size_t event_index,
    float progress,
    const nanotts_phone_def_t *def,
    nanotts_params_t *params)
{
    nanotts_phone_t current = (nanotts_phone_t)impl->events[event_index].phone;
    nanotts_phone_t previous_phone = NANOTTS_PH_SILENCE;
    nanotts_phone_t next_phone = NANOTTS_PH_SILENCE;
    bool has_previous = adjacent_phone(impl, event_index, -1, &previous_phone);
    bool has_next = adjacent_phone(impl, event_index, 1, &next_phone);
    unsigned i;

    if (def->kind == NANOTTS_KIND_FRICATIVE_VOICELESS &&
        current == NANOTTS_PH_H && has_next && nanotts_phone_is_vowel(next_phone)) {
        const nanotts_phone_def_t *next = nanotts_phone_def(next_phone);
        for (i = 0u; i < 4u; ++i) {
            params->f[i] = phone_target(next, i, false);
        }
        params->noise_center = params->f[1];
        params->noise_bw = 1800.0f;
        return;
    }

    if (nanotts_phone_is_vowel(current)) {
        float locus[4];
        if (has_previous && consonant_locus(previous_phone, params, locus) &&
            progress < 0.25f) {
            float blend = nanotts_smoothstep(progress / 0.25f);
            for (i = 0u; i < 4u; ++i) {
                params->f[i] = nanotts_lerp(locus[i], params->f[i], blend);
            }
        }
        if (has_next && consonant_locus(next_phone, params, locus) &&
            progress > 0.75f) {
            float blend = nanotts_smoothstep((progress - 0.75f) / 0.25f);
            for (i = 0u; i < 4u; ++i) {
                params->f[i] = nanotts_lerp(params->f[i], locus[i], blend);
            }
        }
        return;
    }

    /* Sonorants retain local vowel transitions, but never search across an
     * intervening stop or fricative. The old three-phone look-through could
     * smear place cues by connecting vowels on opposite sides of a stop. */
    if (nanotts_phone_is_sonorant(current)) {
        if (has_previous && nanotts_phone_is_vowel(previous_phone) && progress < 0.22f) {
            const nanotts_phone_def_t *previous = nanotts_phone_def(previous_phone);
            float blend = nanotts_smoothstep(progress / 0.22f);
            for (i = 0u; i < 4u; ++i) {
                params->f[i] = nanotts_lerp(phone_target(previous, i, true), params->f[i], blend);
            }
        }
        if (has_next && nanotts_phone_is_vowel(next_phone) && progress > 0.72f) {
            const nanotts_phone_def_t *next = nanotts_phone_def(next_phone);
            float blend = nanotts_smoothstep((progress - 0.72f) / 0.28f);
            for (i = 0u; i < 4u; ++i) {
                params->f[i] = nanotts_lerp(params->f[i], phone_target(next, i, false), blend);
            }
        }
    }
}

static void apply_kind_envelope(
    nanotts_phone_t phone,
    const nanotts_phone_def_t *def,
    float progress,
    nanotts_params_t *params)
{
    float attack = nanotts_smoothstep(progress / 0.12f);
    float release = nanotts_smoothstep((1.0f - progress) / 0.10f);
    float envelope = attack < release ? attack : release;
    nanotts_phone_kind_t kind = (nanotts_phone_kind_t)def->kind;

    switch (kind) {
    case NANOTTS_KIND_PAUSE:
        params->voiced = 0.0f;
        params->noise = 0.0f;
        params->amplitude = 0.0f;
        break;

    case NANOTTS_KIND_VOWEL:
    case NANOTTS_KIND_DIPHTHONG:
        params->amplitude *= 0.70f + 0.30f * envelope;
        params->aspiration = 0.015f;
        break;

    case NANOTTS_KIND_NASAL:
        params->amplitude *= 0.72f + 0.28f * envelope;
        params->nasal = 0.88f;
        params->bw[0] = 120.0f;
        params->bw[1] = 190.0f;
        params->bw[2] = 260.0f;
        params->bw[3] = 330.0f;
        break;

    case NANOTTS_KIND_LIQUID:
    case NANOTTS_KIND_GLIDE:
        params->amplitude *= 0.64f + 0.36f * envelope;
        params->bw[0] = 110.0f;
        params->bw[1] = 155.0f;
        break;

    case NANOTTS_KIND_TRILL: {
        float cycle = progress * 3.2f;
        float fraction = cycle - nanotts_floor_positive(cycle);
        float gate = (fraction > 0.38f && fraction < 0.58f) ? 0.18f : 1.0f;
        params->voiced *= gate;
        params->amplitude *= 0.68f + 0.32f * envelope;
        params->noise += gate < 0.5f ? 0.05f : 0.0f;
        break;
    }

    case NANOTTS_KIND_TAP:
        if (progress > 0.34f && progress < 0.66f) {
            params->voiced *= 0.12f;
            params->amplitude *= 0.25f;
        } else {
            params->amplitude *= 0.72f;
        }
        break;

    case NANOTTS_KIND_FRICATIVE_VOICELESS:
    case NANOTTS_KIND_FRICATIVE_VOICED:
        params->noise *= 0.45f + 0.55f * envelope;
        params->amplitude *= 0.50f + 0.50f * envelope;
        if (phone == NANOTTS_PH_H) {
            params->aspiration = params->noise * 0.90f;
            params->noise *= 0.35f;
        }
        break;

    case NANOTTS_KIND_STOP_VOICELESS:
    case NANOTTS_KIND_STOP_VOICED:
        if (progress < 0.57f) {
            params->noise = 0.0f;
            params->aspiration = 0.0f;
            if (kind == NANOTTS_KIND_STOP_VOICED) {
                params->voiced *= 0.32f;
                params->amplitude *= 0.28f;
                params->f[0] = 220.0f;
                params->bw[0] = 180.0f;
            } else {
                params->voiced = 0.0f;
                params->amplitude = 0.0f;
            }
        } else if (progress < 0.69f) {
            float burst = 1.0f - nanotts_absf((progress - 0.63f) / 0.06f);
            params->voiced *= 0.15f;
            params->noise = nanotts_clampf(params->noise * (0.65f + burst), 0.0f, 1.0f);
            params->amplitude *= 0.85f + 0.30f * burst;
        } else {
            float tail = 1.0f - (progress - 0.69f) / 0.31f;
            params->voiced *= kind == NANOTTS_KIND_STOP_VOICED ? 0.42f : 0.0f;
            params->noise *= 0.30f * tail;
            params->aspiration = 0.34f * tail;
            params->amplitude *= 0.55f + 0.45f * tail;
        }
        if (phone == NANOTTS_PH_GLOTTAL_STOP && progress > 0.69f) {
            params->noise *= 0.20f;
            params->aspiration *= 0.25f;
        }
        break;

    case NANOTTS_KIND_AFFRICATE_VOICELESS:
    case NANOTTS_KIND_AFFRICATE_VOICED:
        if (progress < 0.30f) {
            if (kind == NANOTTS_KIND_AFFRICATE_VOICED) {
                params->voiced *= 0.30f;
                params->amplitude *= 0.30f;
            } else {
                params->voiced = 0.0f;
                params->amplitude = 0.0f;
            }
            params->noise = 0.0f;
        } else {
            float fric_progress = (progress - 0.30f) / 0.70f;
            float fric_attack = nanotts_smoothstep(fric_progress / 0.18f);
            float fric_release = nanotts_smoothstep((1.0f - fric_progress) / 0.18f);
            float fric_env = fric_attack < fric_release ? fric_attack : fric_release;
            params->noise *= 0.55f + 0.45f * fric_env;
            params->voiced *= kind == NANOTTS_KIND_AFFRICATE_VOICED ? 0.40f : 0.0f;
            params->amplitude *= 0.70f + 0.30f * fric_env;
        }
        break;
    }
}

static void parameters_for_event(
    const nanotts_impl_t *impl,
    size_t event_index,
    float progress,
    nanotts_params_t *params)
{
    nanotts_phone_t phone = (nanotts_phone_t)impl->events[event_index].phone;
    const nanotts_phone_def_t *def = nanotts_phone_def(phone);
    base_params(def, progress, params);
    apply_coarticulation(impl, event_index, progress, def, params);
    apply_kind_envelope(phone, def, progress, params);
}

static void update_filters(
    nanotts_impl_t *impl,
    const nanotts_params_t *params)
{
    static const float noise_ratios[3] = { 0.70f, 1.0f, 1.30f };
    static const float noise_widths[3] = { 1.10f, 0.75f, 1.15f };
    static const float detail_widths[3] = { 230.0f, 340.0f, 480.0f };
    unsigned i;
    float sample_rate = (float)impl->sample_rate;

    for (i = 0u; i < 4u; ++i) {
        biquad_set_resonator(
            &impl->voiced_filters[i],
            params->f[i],
            params->bw[i],
            sample_rate);
    }
    biquad_set_bandpass(
        &impl->voiced_detail_filter,
        params->f[1],
        230.0f,
        sample_rate);
    for (i = 0u; i < 3u; ++i) {
        float center = params->noise_center > 0.0f
            ? params->noise_center * noise_ratios[i]
            : params->f[i + 1u];
        float width = params->noise_bw > 0.0f
            ? params->noise_bw * noise_widths[i]
            : detail_widths[i];
        biquad_set_bandpass(
            &impl->noise_filters[i],
            center,
            width,
            sample_rate);
    }
    biquad_set_bandpass(&impl->nasal_filter, 270.0f, 180.0f, sample_rate);
}

static uint32_t rng_next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float white_noise(nanotts_impl_t *impl)
{
    uint32_t bits = rng_next(&impl->rng);
    int32_t signed_bits = (int32_t)(bits >> 8) - 8388608;
    return (float)signed_bits / 8388608.0f;
}

static float glottal_source(nanotts_impl_t *impl, float f0)
{
    float flow;
    float derivative;
    float p;

    impl->phase += f0 / (float)impl->sample_rate;
    if (impl->phase >= 1.0f) {
        impl->phase -= nanotts_floor_positive(impl->phase);
    }
    p = impl->phase;
    if (p < 0.42f) {
        float u = p / 0.42f;
        flow = nanotts_smoothstep(u);
    } else if (p < 0.72f) {
        float u = (p - 0.42f) / 0.30f;
        flow = 1.0f - nanotts_smoothstep(u);
    } else {
        flow = 0.0f;
    }

    derivative = flow - impl->previous_glottal_flow;
    impl->previous_glottal_flow = flow;
    impl->source_tilt_1 = 0.58f * impl->source_tilt_1 + 0.42f * derivative;
    impl->source_tilt_2 = 0.80f * impl->source_tilt_2 + 0.20f * impl->source_tilt_1;

    /* Preserve substantially more harmonic energy than the original two-pole
     * smoothing path. Indonesian vowel identity depends strongly on a clear
     * second formant, and excessive source tilt masked it. */
    return (0.72f * derivative + 0.28f * impl->source_tilt_1) * 5.0f;
}

static float event_pitch_multiplier(
    const nanotts_event_t *event,
    const nanotts_prosody_profile_t *profile)
{
    int pitch_q4 = (int)event->pitch_semitones_q4;
    if ((event->flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u) {
        pitch_q4 += (int)profile->primary_stress_pitch_q4;
    } else if ((event->flags & NANOTTS_EVENT_STRESS_SECONDARY) != 0u) {
        pitch_q4 += (int)profile->secondary_stress_pitch_q4;
    }
    return nanotts_exp2(((float)pitch_q4 / 16.0f) / 12.0f);
}

static float event_pitch_at(
    const nanotts_impl_t *impl,
    const nanotts_prosody_profile_t *profile,
    size_t event_index,
    float progress)
{
    float current = event_pitch_multiplier(&impl->events[event_index], profile);
    float previous = current;
    float next = current;

    if (event_index > 0u &&
        !nanotts_phone_is_pause((nanotts_phone_t)impl->events[event_index - 1u].phone)) {
        previous = event_pitch_multiplier(&impl->events[event_index - 1u], profile);
    }
    if (event_index + 1u < impl->event_count &&
        !nanotts_phone_is_pause((nanotts_phone_t)impl->events[event_index + 1u].phone)) {
        next = event_pitch_multiplier(&impl->events[event_index + 1u], profile);
    }

    if (progress < 0.25f) {
        return nanotts_lerp(previous, current, nanotts_smoothstep(progress / 0.25f));
    }
    if (progress > 0.75f) {
        return nanotts_lerp(current, next,
            nanotts_smoothstep((progress - 0.75f) / 0.25f));
    }
    return current;
}

static float contour_multiplier(
    const nanotts_prosody_profile_t *profile,
    nanotts_final_tone_t final_tone,
    float phrase_progress)
{
    float p = nanotts_clampf(phrase_progress, 0.0f, 1.0f);
    float local;
    float q4;
    int start_q4;
    int middle_q4;
    int end_q4;

    switch (final_tone) {
    case NANOTTS_FINAL_RISE:
        start_q4 = profile->rise_start_q4;
        middle_q4 = profile->rise_middle_q4;
        end_q4 = profile->rise_end_q4;
        break;
    case NANOTTS_FINAL_CONTINUE:
        start_q4 = profile->continue_start_q4;
        middle_q4 = profile->continue_middle_q4;
        end_q4 = profile->continue_end_q4;
        break;
    case NANOTTS_FINAL_LEVEL:
        return 1.0f;
    case NANOTTS_FINAL_AUTO:
    case NANOTTS_FINAL_FALL:
    default:
        start_q4 = profile->fall_start_q4;
        middle_q4 = profile->fall_middle_q4;
        end_q4 = profile->fall_end_q4;
        break;
    }

    if (p < 0.65f) {
        local = nanotts_smoothstep(p / 0.65f);
        q4 = nanotts_lerp((float)start_q4, (float)middle_q4, local);
    } else {
        local = nanotts_smoothstep((p - 0.65f) / 0.35f);
        q4 = nanotts_lerp((float)middle_q4, (float)end_q4, local);
    }
    return nanotts_exp2((q4 / 16.0f) / 12.0f);
}

static size_t event_samples(
    const nanotts_event_t *event,
    const nanotts_phone_def_t *def,
    uint32_t sample_rate,
    const nanotts_options_t *options,
    const nanotts_prosody_profile_t *profile)
{
    float ms = (float)def->duration_ms;
    float rate = options->rate_q8 == 0u ? 1.0f : (float)options->rate_q8 / 256.0f;
    bool syllabic = (event->flags & NANOTTS_EVENT_SYLLABIC) != 0u;
    bool vowel_like = def->kind == NANOTTS_KIND_VOWEL ||
                      def->kind == NANOTTS_KIND_DIPHTHONG || syllabic;

    ms *= (float)(event->duration_percent == 0u ? 100u : event->duration_percent) / 100.0f;
    if (def->kind == NANOTTS_KIND_PAUSE) {
        unsigned pause_scale = event->phone == (uint8_t)NANOTTS_PH_PHRASE_PAUSE
            ? profile->phrase_pause_percent
            : profile->word_pause_percent;
        ms *= (float)pause_scale / 100.0f;
    } else if (vowel_like) {
        ms *= (float)profile->vowel_duration_percent / 100.0f;
    } else {
        ms *= (float)profile->consonant_duration_percent / 100.0f;
    }

    if ((event->flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u && vowel_like) {
        ms *= (float)profile->primary_stress_duration_percent / 100.0f;
    } else if ((event->flags & NANOTTS_EVENT_STRESS_SECONDARY) != 0u &&
               vowel_like) {
        ms *= (float)profile->secondary_stress_duration_percent / 100.0f;
    }
    ms /= nanotts_clampf(rate, 0.45f, 2.50f);
    ms = nanotts_clampf(ms, 12.0f, 600.0f);
    return (size_t)(ms * (float)sample_rate / 1000.0f + 0.5f);
}

static int flush_audio(nanotts_impl_t *impl, nanotts_write_fn write, void *user)
{
    int result;
    if (impl->audio_count == 0u) {
        return 0;
    }
    result = write(user, impl->audio_block, impl->audio_count);
    impl->audio_count = 0u;
    return result;
}

static int emit_audio_sample(
    nanotts_impl_t *impl,
    int16_t sample,
    nanotts_write_fn write,
    void *user)
{
    impl->audio_block[impl->audio_count++] = sample;
    if (impl->audio_count == NANOTTS_AUDIO_BLOCK) {
        return flush_audio(impl, write, user);
    }
    return 0;
}

static float synth_sample(
    nanotts_impl_t *impl,
    const nanotts_params_t *params,
    float f0,
    float volume)
{
    static const float noise_weights[3] = { 0.40f, 1.00f, 0.48f };
    float source = glottal_source(impl, f0);
    float noise = white_noise(impl);
    float high_noise = noise - 0.65f * impl->previous_noise;
    float cascade_out = source;
    float detail_out;
    float noise_out = 0.0f;
    float nasal_out;
    float voiced_out;
    float mixed;
    float dc;
    float absolute;
    unsigned i;

    impl->previous_noise = noise;

    for (i = 0u; i < 4u; ++i) {
        cascade_out = biquad_process(&impl->voiced_filters[i], cascade_out);
    }
    detail_out = biquad_process(&impl->voiced_detail_filter, source);
    for (i = 0u; i < 3u; ++i) {
        noise_out += noise_weights[i] *
            biquad_process(&impl->noise_filters[i], high_noise);
    }
    nasal_out = biquad_process(&impl->nasal_filter, source);

    /* The cascade gives stable vowel energy while one broad F2 detail path
     * restores the most important front/back vowel cue. Keeping it separate
     * from the frication filters avoids coefficient-switching transients. */
    voiced_out = 0.72f * cascade_out + 24.0f * detail_out;
    mixed = params->voiced * ((1.0f - 0.52f * params->nasal) * voiced_out +
             params->nasal * 0.72f * nasal_out);
    mixed += params->noise * 0.34f * noise_out;
    mixed += params->aspiration * (0.08f * noise + 0.16f * noise_out);
    mixed *= params->amplitude;

    /* Light lip-radiation/DC blocking characteristic. */
    dc = mixed - impl->dc_prev_x + 0.992f * impl->dc_prev_y;
    impl->dc_prev_x = mixed;
    impl->dc_prev_y = dc;

    /* Gentle, deterministic safety limiter. Normal speech should remain in
     * the near-linear region; the limiter only catches release transients. */
    absolute = nanotts_absf(dc);
    dc = dc / (1.0f + 0.22f * absolute);
    return nanotts_clampf(dc * 1.42f * volume, -0.96f, 0.96f);
}

static void decay_runtime_state(nanotts_impl_t *impl)
{
    unsigned i;
    float dc;

    for (i = 0u; i < 4u; ++i) {
        (void)biquad_process(&impl->voiced_filters[i], 0.0f);
    }
    (void)biquad_process(&impl->voiced_detail_filter, 0.0f);
    for (i = 0u; i < 3u; ++i) {
        (void)biquad_process(&impl->noise_filters[i], 0.0f);
    }
    (void)biquad_process(&impl->nasal_filter, 0.0f);

    impl->source_tilt_1 *= 0.985f;
    impl->source_tilt_2 *= 0.985f;
    impl->previous_noise *= 0.92f;

    /* Decay the radiation/DC-blocker memory while emitting exact digital
     * silence. Freezing this state through a pause caused a click or stale
     * formant transient at the start of the next word. */
    dc = -impl->dc_prev_x + 0.992f * impl->dc_prev_y;
    impl->dc_prev_x = 0.0f;
    impl->dc_prev_y = dc;
}

static void clear_runtime_state(nanotts_impl_t *impl)
{
    unsigned i;
    impl->rng = 0x6d2b79f5u;
    impl->phase = 0.0f;
    impl->previous_glottal_flow = 0.0f;
    impl->source_tilt_1 = 0.0f;
    impl->source_tilt_2 = 0.0f;
    impl->previous_noise = 0.0f;
    impl->dc_prev_x = 0.0f;
    impl->dc_prev_y = 0.0f;
    impl->audio_count = 0u;
    for (i = 0u; i < 4u; ++i) biquad_clear(&impl->voiced_filters[i]);
    biquad_clear(&impl->voiced_detail_filter);
    for (i = 0u; i < 3u; ++i) biquad_clear(&impl->noise_filters[i]);
    biquad_clear(&impl->nasal_filter);
}

static size_t phrase_end_index(const nanotts_impl_t *impl, size_t start)
{
    size_t i;
    for (i = start; i < impl->event_count; ++i) {
        if (impl->events[i].phone == (uint8_t)NANOTTS_PH_PHRASE_PAUSE ||
            (impl->events[i].flags & NANOTTS_EVENT_PHRASE_END) != 0u) {
            return i;
        }
    }
    return impl->event_count;
}

static nanotts_final_tone_t phrase_tone(
    const nanotts_impl_t *impl,
    size_t phrase_end,
    uint8_t requested)
{
    if (requested != (uint8_t)NANOTTS_FINAL_AUTO) {
        return (nanotts_final_tone_t)requested;
    }
    if (phrase_end < impl->event_count &&
        (impl->events[phrase_end].flags & NANOTTS_EVENT_QUESTION) != 0u) {
        return NANOTTS_FINAL_RISE;
    }
    return NANOTTS_FINAL_FALL;
}

static size_t phrase_total_samples(
    const nanotts_impl_t *impl,
    size_t start,
    size_t end,
    const nanotts_options_t *options,
    const nanotts_prosody_profile_t *profile)
{
    size_t i;
    size_t total = 0u;
    for (i = start; i < end && i < impl->event_count; ++i) {
        nanotts_phone_t phone = (nanotts_phone_t)impl->events[i].phone;
        total += event_samples(&impl->events[i], nanotts_phone_def(phone),
                               impl->sample_rate, options, profile);
    }
    return total > 0u ? total : 1u;
}

static nanotts_result_t emit_silence(
    nanotts_impl_t *impl,
    size_t samples,
    nanotts_write_fn write,
    void *user)
{
    size_t i;
    for (i = 0u; i < samples; ++i) {
        if (emit_audio_sample(impl, 0, write, user) != 0) {
            return NANOTTS_ERR_CALLBACK_ABORTED;
        }
    }
    return NANOTTS_OK;
}

nanotts_result_t nanotts_synth_render(
    nanotts_impl_t *impl,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user)
{
    size_t event_index;
    size_t current_phrase_start = 0u;
    size_t current_phrase_end = phrase_end_index(impl, 0u);
    size_t current_phrase_total = 1u;
    size_t current_phrase_elapsed = 0u;
    nanotts_final_tone_t current_phrase_tone = NANOTTS_FINAL_FALL;
    const nanotts_prosody_profile_t *profile;
    float global_pitch;
    float volume;
    nanotts_result_t result;

    if (options->rate_q8 != 0u &&
        (options->rate_q8 < 96u || options->rate_q8 > 640u)) {
        return NANOTTS_ERR_ARGUMENT;
    }
    if (options->final_tone > (uint8_t)NANOTTS_FINAL_AUTO ||
        options->pitch_cents < -1200 || options->pitch_cents > 1200) {
        return NANOTTS_ERR_ARGUMENT;
    }

    profile = nanotts_language_prosody((nanotts_language_t)impl->language);
    if (profile == NULL) profile = nanotts_language_prosody(NANOTTS_LANG_NONE);

    clear_runtime_state(impl);
    global_pitch = nanotts_exp2((float)options->pitch_cents / 1200.0f);
    volume = (float)options->volume / 255.0f;
    current_phrase_total = phrase_total_samples(
        impl, current_phrase_start, current_phrase_end, options, profile);
    current_phrase_tone = phrase_tone(
        impl, current_phrase_end, options->final_tone);

    result = emit_silence(impl, impl->sample_rate / 50u, write, user); /* 20 ms */
    if (result != NANOTTS_OK) return result;

    for (event_index = 0u; event_index < impl->event_count; ++event_index) {
        const nanotts_event_t *event = &impl->events[event_index];
        nanotts_phone_t phone = (nanotts_phone_t)event->phone;
        const nanotts_phone_def_t *def = nanotts_phone_def(phone);
        size_t total_samples = event_samples(
            event, def, impl->sample_rate, options, profile);
        size_t frame_samples = (size_t)impl->sample_rate * NANOTTS_FRAME_MS / 1000u;
        size_t sample_index = 0u;

        if (frame_samples == 0u) frame_samples = 1u;
        if (event_index > current_phrase_end) {
            current_phrase_start = event_index;
            current_phrase_end = phrase_end_index(impl, event_index);
            current_phrase_total = phrase_total_samples(
                impl, current_phrase_start, current_phrase_end, options, profile);
            current_phrase_tone = phrase_tone(
                impl, current_phrase_end, options->final_tone);
            current_phrase_elapsed = 0u;
        }

        while (sample_index < total_samples) {
            size_t frame_count = total_samples - sample_index;
            size_t k;
            float mid_progress;
            float phrase_progress;
            nanotts_params_t params;
            float f0;

            if (frame_count > frame_samples) frame_count = frame_samples;
            mid_progress = ((float)sample_index + 0.5f * (float)frame_count) /
                           (float)(total_samples > 0u ? total_samples : 1u);
            parameters_for_event(impl, event_index, mid_progress, &params);
            update_filters(impl, &params);

            if (event_index < current_phrase_end) {
                phrase_progress = ((float)current_phrase_elapsed +
                                   (float)sample_index +
                                   0.5f * (float)frame_count) /
                                  (float)current_phrase_total;
            } else {
                phrase_progress = 1.0f;
            }
            f0 = (float)profile->base_pitch_hz * global_pitch *
                 event_pitch_at(impl, profile, event_index, mid_progress) *
                 contour_multiplier(profile, current_phrase_tone, phrase_progress);
            f0 = nanotts_clampf(f0, 65.0f, 360.0f);

            for (k = 0u; k < frame_count; ++k) {
                float output;
                int32_t pcm;
                if (def->kind == NANOTTS_KIND_PAUSE) {
                    decay_runtime_state(impl);
                    output = 0.0f;
                } else {
                    output = synth_sample(impl, &params, f0, volume);
                }
                pcm = (int32_t)(output * 32767.0f);
                if (pcm > 32767) pcm = 32767;
                if (pcm < -32768) pcm = -32768;
                if (emit_audio_sample(impl, (int16_t)pcm, write, user) != 0) {
                    return NANOTTS_ERR_CALLBACK_ABORTED;
                }
            }
            sample_index += frame_count;
        }
        if (event_index < current_phrase_end) {
            current_phrase_elapsed += total_samples;
        }
    }

    result = emit_silence(impl, impl->sample_rate / 20u, write, user); /* 50 ms */
    if (result != NANOTTS_OK) return result;
    if (flush_audio(impl, write, user) != 0) {
        return NANOTTS_ERR_CALLBACK_ABORTED;
    }
    return NANOTTS_OK;
}
