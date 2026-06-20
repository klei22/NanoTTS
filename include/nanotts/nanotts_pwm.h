/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_NANOTTS_PWM_H
#define NANOTTS_NANOTTS_PWM_H

#include "nanotts/nanotts.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PWM here means a stream of timer-compare duty values, one value for every
 * NanoTTS PCM sample. The MCU timer supplies the high-frequency carrier; DMA
 * or an ISR updates the compare register at nanotts_sample_rate().
 */
typedef int (*nanotts_pwm_write_fn)(
    void *user,
    const uint16_t *duty_values,
    size_t count);

typedef struct nanotts_pwm_output {
    nanotts_pwm_write_fn write;
    void *user;
    uint16_t *scratch;
    size_t scratch_count;
    uint16_t top;
    uint8_t invert;
    uint8_t reserved[3];
} nanotts_pwm_output_t;

/* Map -32768 to 0, +32767 to top, and zero close to top/2. */
uint16_t nanotts_pcm16_to_pwm(
    int16_t sample,
    uint16_t top,
    int invert);

/*
 * Initialize a zero-allocation PCM-to-PWM adapter. The scratch buffer is
 * caller-owned and may live in static RAM. top is the timer period/ARR value,
 * for example 255, 1023, 4095, or 65535.
 */
nanotts_result_t nanotts_pwm_output_init(
    nanotts_pwm_output_t *output,
    uint16_t top,
    int invert,
    uint16_t *scratch,
    size_t scratch_count,
    nanotts_pwm_write_fn write,
    void *user);

/* Pass this directly as nanotts_write_fn to any render/speak function. */
int nanotts_pwm_write_pcm(
    void *output,
    const int16_t *samples,
    size_t count);

#ifdef __cplusplus
}
#endif

#endif
