/* SPDX-License-Identifier: MIT */
/*
 * Hardware-neutral PWM integration pattern. Replace pwm_dma_submit() with a
 * target driver that queues timer compare values to DMA. The PWM carrier is
 * configured separately; compare values are updated at the NanoTTS sample
 * rate (16 kHz here).
 */
#include "nanotts/nanotts.h"
#include "nanotts/nanotts_pwm.h"

#include <stddef.h>
#include <stdint.h>

typedef struct pwm_device {
    volatile uint16_t last_compare;
    size_t values_written;
} pwm_device_t;

static int pwm_dma_submit(void *user, const uint16_t *values, size_t count)
{
    pwm_device_t *device = (pwm_device_t *)user;
    if (count != 0u) device->last_compare = values[count - 1u];
    device->values_written += count;
    return 0;
}

int main(void)
{
    nanotts_t tts;
    nanotts_options_t options;
    nanotts_pwm_output_t pwm;
    pwm_device_t device = { 0u, 0u };
    uint16_t pwm_scratch[64];

    if (nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE) != NANOTTS_OK) return 1;
    if (nanotts_pwm_output_init(
            &pwm,
            1023u,                 /* 10-bit timer period/ARR. */
            0,                     /* Non-inverted duty. */
            pwm_scratch,
            sizeof(pwm_scratch) / sizeof(pwm_scratch[0]),
            pwm_dma_submit,
            &device) != NANOTTS_OK) {
        return 1;
    }

    nanotts_default_options(&options);
    if (nanotts_speak_ipa(
            &tts,
            "h_a_l_o",
            NANOTTS_NPOS,
            &options,
            nanotts_pwm_write_pcm,
            &pwm,
            NULL) != NANOTTS_OK) {
        return 1;
    }
    return device.values_written == 0u ? 1 : 0;
}
