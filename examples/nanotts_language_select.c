/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Replace this with a DMA/I2S/DAC/PWM queue on the target. */
static int audio_write(void *user, const int16_t *samples, size_t count)
{
    uint64_t *total = (uint64_t *)user;
    (void)samples;
    *total += count;
    return 0;
}

int main(void)
{
    /* The application chooses a text language before it parses the text. */
    nanotts_language_t selected = NANOTTS_LANG_SPANISH;
    const char *text = "hola, buenos días";
    static nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t parse;
    uint64_t total_samples = 0u;
    nanotts_result_t result;

    /* Keep this sample runnable in any one-language build profile. */
    if (!nanotts_language_available(selected)) {
        selected = NANOTTS_LANG_KISWAHILI;
        text = "habari yako";
    }
    if (!nanotts_language_available(selected)) {
        selected = NANOTTS_LANG_INDONESIAN;
        text = "selamat pagi";
    }
    if (!nanotts_language_available(selected)) {
        fprintf(stderr, "no text language module was compiled in\n");
        return 2;
    }

    result = nanotts_init(&tts, 16000u, selected);
    if (result != NANOTTS_OK) {
        fprintf(stderr, "%s\n", nanotts_strerror(result));
        return 1;
    }

    nanotts_default_options(&options);
    result = nanotts_speak_text(
        &tts, text, NANOTTS_NPOS, &options,
        audio_write, &total_samples, &parse);
    if (result != NANOTTS_OK) {
        fprintf(stderr, "%s\n", nanotts_strerror(result));
        return 1;
    }

    printf("%s: queued %llu PCM samples\n",
           nanotts_language_code(selected),
           (unsigned long long)total_samples);
    return 0;
}
