/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Replace this with a blocking DMA/I2S/PWM queue on the target MCU. */
static int audio_write(void *user, const int16_t *samples, size_t count)
{
    uint64_t *total = (uint64_t *)user;
    (void)samples;
    *total += count;
    return 0;
}

int main(void)
{
    static nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t parse;
    uint64_t total_samples = 0u;
    nanotts_result_t result;

    result = nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE);
    if (result != NANOTTS_OK) return 1;

    nanotts_default_options(&options);
    result = nanotts_speak_ipa(
        &tts,
        "s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i",
        NANOTTS_NPOS,
        &options,
        audio_write,
        &total_samples,
        &parse);
    if (result != NANOTTS_OK) {
        fprintf(stderr, "%s\n", nanotts_strerror(result));
        return 1;
    }

    printf("queued %llu PCM samples\n", (unsigned long long)total_samples);
    return 0;
}
