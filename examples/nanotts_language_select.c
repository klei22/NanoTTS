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

static const char *sample_for(nanotts_language_t language)
{
    switch (language) {
    case NANOTTS_LANG_INDONESIAN: return "selamat pagi";
    case NANOTTS_LANG_KISWAHILI: return "habari yako";
    case NANOTTS_LANG_SPANISH: return "hola, buenos días";
    case NANOTTS_LANG_MALAY: return "selamat datang";
    case NANOTTS_LANG_MAORI: return "kia ora, Aotearoa";
    case NANOTTS_LANG_HAWAIIAN: return "aloha Hawaiʻi";
    default: return NULL;
    }
}

int main(void)
{
    nanotts_language_t selected;
    const char *text;
    static nanotts_t tts;
    nanotts_options_t options;
    nanotts_parse_info_t parse;
    uint64_t total_samples = 0u;
    nanotts_result_t result;

    if (nanotts_compiled_language_count() == 0u) {
        fprintf(stderr, "no text language module was compiled in\n");
        return 2;
    }
    selected = nanotts_compiled_language_at(0u);
    text = sample_for(selected);
    if (text == NULL) return 2;

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
