/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#ifndef NANOTTS_CLI_HAVE_PWM
#define NANOTTS_CLI_HAVE_PWM 0
#endif
#ifndef NANOTTS_CLI_HAVE_PCM
#define NANOTTS_CLI_HAVE_PCM 0
#endif

#if NANOTTS_CLI_HAVE_PWM
#include "nanotts/nanotts_pwm.h"
#endif
#if NANOTTS_CLI_HAVE_PCM
#include "nanotts/nanotts_pcm.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum output_format {
    OUTPUT_WAV = 0,
    OUTPUT_PCM16LE,
    OUTPUT_PWM16LE
} output_format_t;

typedef struct wav_writer {
    FILE *file;
    uint32_t samples;
    int failed;
} wav_writer_t;

#if NANOTTS_CLI_HAVE_PCM || NANOTTS_CLI_HAVE_PWM
typedef struct raw_writer {
    FILE *file;
    int close_file;
    int failed;
} raw_writer_t;
#endif

#if NANOTTS_CLI_HAVE_PCM
typedef struct pcm_file_output {
    raw_writer_t raw;
    nanotts_pcm16le_output_t adapter;
    uint8_t scratch[256];
} pcm_file_output_t;
#endif

#if NANOTTS_CLI_HAVE_PWM
typedef struct pwm_file_output {
    raw_writer_t raw;
    nanotts_pwm_output_t adapter;
    uint16_t scratch[128];
} pwm_file_output_t;
#endif

static void put_u16le(FILE *file, uint16_t value)
{
    fputc((int)(value & 0xffu), file);
    fputc((int)((value >> 8) & 0xffu), file);
}

static void put_u32le(FILE *file, uint32_t value)
{
    fputc((int)(value & 0xffu), file);
    fputc((int)((value >> 8) & 0xffu), file);
    fputc((int)((value >> 16) & 0xffu), file);
    fputc((int)((value >> 24) & 0xffu), file);
}

static int wav_begin(wav_writer_t *writer, const char *path, uint32_t sample_rate)
{
    FILE *file;
    if (!strcmp(path, "-")) {
        fprintf(stderr, "WAV output needs a seekable file; use --format pcm or pwm for stdout\n");
        return 0;
    }
    file = fopen(path, "wb");
    if (file == NULL) return 0;
    writer->file = file;
    writer->samples = 0u;
    writer->failed = 0;

    fwrite("RIFF", 1u, 4u, file);
    put_u32le(file, 0u);
    fwrite("WAVEfmt ", 1u, 8u, file);
    put_u32le(file, 16u);
    put_u16le(file, 1u);
    put_u16le(file, 1u);
    put_u32le(file, sample_rate);
    put_u32le(file, sample_rate * 2u);
    put_u16le(file, 2u);
    put_u16le(file, 16u);
    fwrite("data", 1u, 4u, file);
    put_u32le(file, 0u);
    return ferror(file) == 0;
}

static int wav_write(void *user, const int16_t *samples, size_t count)
{
    wav_writer_t *writer = (wav_writer_t *)user;
    size_t index;
    if (writer->failed) return 1;
    for (index = 0u; index < count; ++index) {
        put_u16le(writer->file, (uint16_t)samples[index]);
    }
    if (ferror(writer->file) || count > UINT32_MAX - writer->samples) {
        writer->failed = 1;
        return 1;
    }
    writer->samples += (uint32_t)count;
    return 0;
}

static int wav_end(wav_writer_t *writer)
{
    uint32_t data_bytes = writer->samples * 2u;
    if (fseek(writer->file, 4L, SEEK_SET) != 0) writer->failed = 1;
    put_u32le(writer->file, 36u + data_bytes);
    if (fseek(writer->file, 40L, SEEK_SET) != 0) writer->failed = 1;
    put_u32le(writer->file, data_bytes);
    if (fclose(writer->file) != 0) writer->failed = 1;
    return !writer->failed;
}

#if NANOTTS_CLI_HAVE_PCM || NANOTTS_CLI_HAVE_PWM
static int raw_begin(raw_writer_t *writer, const char *path)
{
    writer->failed = 0;
    if (!strcmp(path, "-")) {
        writer->file = stdout;
        writer->close_file = 0;
    } else {
        writer->file = fopen(path, "wb");
        writer->close_file = 1;
    }
    return writer->file != NULL;
}
#endif

#if NANOTTS_CLI_HAVE_PCM
static int raw_bytes_write(void *user, const uint8_t *bytes, size_t count)
{
    raw_writer_t *writer = (raw_writer_t *)user;
    if (writer == NULL || writer->file == NULL || writer->failed) return 1;
    if (fwrite(bytes, 1u, count, writer->file) != count) {
        writer->failed = 1;
        return 1;
    }
    return 0;
}
#endif

#if NANOTTS_CLI_HAVE_PWM
static int raw_pwm_write(void *user, const uint16_t *values, size_t count)
{
    raw_writer_t *writer = (raw_writer_t *)user;
    size_t index;
    if (writer == NULL || writer->file == NULL || writer->failed) return 1;
    for (index = 0u; index < count; ++index) put_u16le(writer->file, values[index]);
    if (ferror(writer->file)) {
        writer->failed = 1;
        return 1;
    }
    return 0;
}
#endif

#if NANOTTS_CLI_HAVE_PCM || NANOTTS_CLI_HAVE_PWM
static int raw_end(raw_writer_t *writer)
{
    if (fflush(writer->file) != 0) writer->failed = 1;
    if (writer->close_file && fclose(writer->file) != 0) writer->failed = 1;
    return !writer->failed;
}
#endif

static char *read_all(FILE *file, size_t *length)
{
    size_t capacity = 4096u;
    size_t used = 0u;
    char *buffer = (char *)malloc(capacity + 1u);
    if (buffer == NULL) return NULL;

    while (!feof(file)) {
        size_t got;
        if (used == capacity) {
            size_t next_capacity = capacity * 2u;
            char *grown = (char *)realloc(buffer, next_capacity + 1u);
            if (grown == NULL) {
                free(buffer);
                return NULL;
            }
            buffer = grown;
            capacity = next_capacity;
        }
        got = fread(buffer + used, 1u, capacity - used, file);
        used += got;
        if (ferror(file)) {
            free(buffer);
            return NULL;
        }
    }
    buffer[used] = '\0';
    *length = used;
    return buffer;
}

static long parse_long(const char *text, const char *name, long minimum, long maximum)
{
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0' ||
        value < minimum || value > maximum) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(2);
    }
    return value;
}

static output_format_t parse_format(const char *value)
{
    if (!strcmp(value, "wav")) return OUTPUT_WAV;
    if (!strcmp(value, "pcm") || !strcmp(value, "pcm16le") ||
        !strcmp(value, "raw")) return OUTPUT_PCM16LE;
    if (!strcmp(value, "pwm") || !strcmp(value, "pwm16le")) {
        return OUTPUT_PWM16LE;
    }
    fprintf(stderr, "invalid output format: %s (use wav, pcm16le, or pwm16le)\n", value);
    exit(2);
}

static int has_suffix(const char *text, const char *suffix)
{
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

static output_format_t infer_format(const char *path)
{
    if (has_suffix(path, ".pcm") || has_suffix(path, ".pcm16le") ||
        has_suffix(path, ".raw")) {
        return OUTPUT_PCM16LE;
    }
    if (has_suffix(path, ".pwm") || has_suffix(path, ".pwm16le")) {
        return OUTPUT_PWM16LE;
    }
    return OUTPUT_WAV;
}

static void print_usage(FILE *file)
{
    fprintf(file,
        "NanoTTS - compact multilingual IPA/text-to-speech\n\n"
        "Usage:\n"
        "  nanotts --lang CODE --text TEXT -o speech.wav [options]\n"
        "  nanotts --ipa IPA --format pwm -o speech.pwm [options]\n"
        "  nanotts --list-languages\n\n"
        "Input:\n"
        "  --lang CODE             Select text language; with IPA, select prosody only\n"
        "  --ipa STRING            Speak the supported IPA subset\n"
        "  --text STRING           Use the selected text module\n"
        "  --ipa-file PATH         Read IPA from PATH, or - for stdin\n"
        "  --text-file PATH        Read text from PATH, or - for stdin\n"
        "  --list-languages        Show compiled text modules\n\n"
        "Output:\n"
        "  -o, --output PATH       Output path; - is allowed for raw formats\n"
        "  --format FORMAT         wav, pcm16le, or pwm16le\n"
        "                          Inferred from .pcm/.pcm16le/.raw or\n"
        "                          .pwm/.pwm16le; otherwise WAV\n"
        "  --pwm-top N             PWM timer TOP/ARR, 1..65535; default 255\n"
        "  --pwm-invert            Invert PWM compare values\n"
        "                          PWM output is uint16 little-endian duty values,\n"
        "                          one update per audio sample, not carrier bits\n\n"
        "Voice:\n"
        "  --sample-rate HZ        8000..24000; default 16000\n"
        "  --speed PERCENT         38..250; default 100\n"
        "  --pitch CENTS           -1200..1200; default 0\n"
        "  --volume PERCENT        0..150; default about 86\n"
        "  --rise | --fall | --continue | --level\n"
        "  --permissive-ipa        Ignore unsupported IPA symbols\n"
        "  --no-autopause          Do not turn IPA spaces into pauses\n"
        "  --dump-phones           Parse and print events without audio\n"
        "  --version\n"
        "  -h, --help\n");
}

static void list_languages(void)
{
    size_t index;
    size_t count = nanotts_compiled_language_count();
    if (count == 0u) {
        puts("No text language modules are compiled in (IPA remains available).");
        return;
    }
    for (index = 0u; index < count; ++index) {
        nanotts_language_t language = nanotts_compiled_language_at(index);
        printf("%-3s  %s\n", nanotts_language_code(language),
               nanotts_language_name(language));
    }
}

static void dump_events(const nanotts_t *tts)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t count = nanotts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        const nanotts_event_t *event = &events[index];
        printf("%3lu  %-12s flags=0x%02x duration=%u%% pitch_q4=%d\n",
               (unsigned long)index,
               nanotts_phone_name((nanotts_phone_t)event->phone),
               (unsigned)event->flags,
               (unsigned)event->duration_percent,
               (int)event->pitch_semitones_q4);
    }
}

int main(int argc, char **argv)
{
    enum input_mode { INPUT_NONE, INPUT_IPA, INPUT_TEXT } mode = INPUT_NONE;
    const char *inline_input = NULL;
    const char *input_path = NULL;
    const char *output_path = NULL;
    const char *format_name = NULL;
    char *owned_input = NULL;
    size_t input_length = 0u;
    uint32_t sample_rate = 16000u;
#if NANOTTS_CLI_HAVE_PWM
    uint16_t pwm_top = 255u;
    int pwm_invert = 0;
#endif
    nanotts_language_t language = NANOTTS_LANG_NONE;
    int language_given = 0;
    nanotts_options_t options;
    nanotts_t tts;
    nanotts_parse_info_t info;
    nanotts_result_t result = NANOTTS_OK;
    wav_writer_t wav;
#if NANOTTS_CLI_HAVE_PCM
    pcm_file_output_t pcm;
#endif
#if NANOTTS_CLI_HAVE_PWM
    pwm_file_output_t pwm;
#endif
    output_format_t format = OUTPUT_WAV;
    nanotts_write_fn audio_write = NULL;
    void *audio_user = NULL;
    int output_open = 0;
    int output_ok = 1;
    int dump_phones = 0;
    int index;

    info.event_count = 0u;
    info.error_byte = NANOTTS_NPOS;
    info.error_codepoint = 0u;
    nanotts_default_options(&options);

    for (index = 1; index < argc; ++index) {
        const char *arg = argv[index];
        if (!strcmp(arg, "--ipa") || !strcmp(arg, "--text") ||
            !strcmp(arg, "--ipa-file") || !strcmp(arg, "--text-file") ||
            !strcmp(arg, "-o") || !strcmp(arg, "--output") ||
            !strcmp(arg, "--sample-rate") || !strcmp(arg, "--speed") ||
            !strcmp(arg, "--pitch") || !strcmp(arg, "--volume") ||
            !strcmp(arg, "--lang") || !strcmp(arg, "--format") ||
            !strcmp(arg, "--pwm-top")) {
            const char *value;
            if (++index >= argc) {
                fprintf(stderr, "%s requires a value\n", arg);
                return 2;
            }
            value = argv[index];
            if (!strcmp(arg, "--lang")) {
                language = nanotts_language_from_code(value);
                language_given = 1;
                if (language == NANOTTS_LANG_COUNT || language == NANOTTS_LANG_NONE) {
                    fprintf(stderr, "invalid text language: %s\n", value);
                    return 2;
                }
            } else if (!strcmp(arg, "--ipa")) {
                mode = INPUT_IPA; inline_input = value;
            } else if (!strcmp(arg, "--text")) {
                mode = INPUT_TEXT; inline_input = value;
            } else if (!strcmp(arg, "--ipa-file")) {
                mode = INPUT_IPA; input_path = value;
            } else if (!strcmp(arg, "--text-file")) {
                mode = INPUT_TEXT; input_path = value;
            } else if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
                output_path = value;
            } else if (!strcmp(arg, "--format")) {
                format_name = value;
            } else if (!strcmp(arg, "--pwm-top")) {
#if NANOTTS_CLI_HAVE_PWM
                pwm_top = (uint16_t)parse_long(value, "PWM top", 1, 65535);
#else
                fprintf(stderr, "PWM support was disabled at build time\n");
                return 2;
#endif
            } else if (!strcmp(arg, "--sample-rate")) {
                sample_rate = (uint32_t)parse_long(value, "sample rate", 8000, 24000);
            } else if (!strcmp(arg, "--speed")) {
                long percent = parse_long(value, "speed", 38, 250);
                options.rate_q8 = (uint16_t)((percent * 256L + 50L) / 100L);
            } else if (!strcmp(arg, "--pitch")) {
                options.pitch_cents = (int16_t)parse_long(value, "pitch", -1200, 1200);
            } else if (!strcmp(arg, "--volume")) {
                long percent = parse_long(value, "volume", 0, 150);
                long q = (percent * 255L + 50L) / 100L;
                options.volume = (uint8_t)(q > 255L ? 255L : q);
            }
        } else if (!strcmp(arg, "--pwm-invert")) {
#if NANOTTS_CLI_HAVE_PWM
            pwm_invert = 1;
#else
            fprintf(stderr, "PWM support was disabled at build time\n");
            return 2;
#endif
        } else if (!strcmp(arg, "--rise")) {
            options.final_tone = (uint8_t)NANOTTS_FINAL_RISE;
        } else if (!strcmp(arg, "--fall")) {
            options.final_tone = (uint8_t)NANOTTS_FINAL_FALL;
        } else if (!strcmp(arg, "--continue")) {
            options.final_tone = (uint8_t)NANOTTS_FINAL_CONTINUE;
        } else if (!strcmp(arg, "--level")) {
            options.final_tone = (uint8_t)NANOTTS_FINAL_LEVEL;
        } else if (!strcmp(arg, "--permissive-ipa")) {
            options.flags |= NANOTTS_OPT_PERMISSIVE_IPA;
        } else if (!strcmp(arg, "--no-autopause")) {
            options.flags |= NANOTTS_OPT_NO_AUTOPAUSE;
        } else if (!strcmp(arg, "--dump-phones")) {
            dump_phones = 1;
        } else if (!strcmp(arg, "--list-languages")) {
            list_languages(); return 0;
        } else if (!strcmp(arg, "--version")) {
            printf("nanotts %s\n", nanotts_version_string()); return 0;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_usage(stdout); return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", arg);
            print_usage(stderr);
            return 2;
        }
    }

    if (mode == INPUT_NONE || (inline_input == NULL && input_path == NULL) ||
        (!dump_phones && output_path == NULL)) {
        print_usage(stderr);
        return 2;
    }
    if (mode == INPUT_TEXT && !language_given) {
        fprintf(stderr, "--lang CODE is required for text input\n");
        return 2;
    }
    if (language_given && !nanotts_language_available(language)) {
        fprintf(stderr, "language '%s' is not compiled into this binary\n",
                nanotts_language_code(language));
        return 2;
    }
    if (inline_input != NULL && input_path != NULL) {
        fprintf(stderr, "choose inline input or file input, not both\n");
        return 2;
    }

    if (inline_input != NULL) {
        input_length = strlen(inline_input);
    } else {
        FILE *input = !strcmp(input_path, "-") ? stdin : fopen(input_path, "rb");
        if (input == NULL) { perror(input_path); return 1; }
        owned_input = read_all(input, &input_length);
        if (input != stdin) fclose(input);
        if (owned_input == NULL) {
            fprintf(stderr, "could not read input\n"); return 1;
        }
        inline_input = owned_input;
    }

    result = nanotts_init(&tts, sample_rate,
        language_given ? language : NANOTTS_LANG_NONE);
    if (result != NANOTTS_OK) {
        fprintf(stderr, "%s\n", nanotts_strerror(result));
        free(owned_input);
        return 1;
    }

    if (dump_phones) {
        result = mode == INPUT_IPA
            ? nanotts_parse_ipa(&tts, inline_input, input_length, options.flags, &info)
            : nanotts_parse_text(&tts, inline_input, input_length, &info);
        if (result == NANOTTS_OK) dump_events(&tts);
    } else {
        format = format_name != NULL ? parse_format(format_name) : infer_format(output_path);
        switch (format) {
        case OUTPUT_WAV:
            if (!wav_begin(&wav, output_path, sample_rate)) {
                if (strcmp(output_path, "-") != 0) perror(output_path);
                free(owned_input); return 1;
            }
            audio_write = wav_write;
            audio_user = &wav;
            output_open = 1;
            break;
        case OUTPUT_PCM16LE:
#if NANOTTS_CLI_HAVE_PCM
            if (!raw_begin(&pcm.raw, output_path) ||
                nanotts_pcm16le_output_init(&pcm.adapter, pcm.scratch,
                    sizeof(pcm.scratch), raw_bytes_write, &pcm.raw) != NANOTTS_OK) {
                if (strcmp(output_path, "-") != 0) perror(output_path);
                free(owned_input); return 1;
            }
            audio_write = nanotts_pcm16le_write_pcm;
            audio_user = &pcm.adapter;
            output_open = 1;
#else
            fprintf(stderr, "raw PCM support was disabled at build time\n");
            free(owned_input); return 1;
#endif
            break;
        case OUTPUT_PWM16LE:
#if NANOTTS_CLI_HAVE_PWM
            if (!raw_begin(&pwm.raw, output_path) ||
                nanotts_pwm_output_init(&pwm.adapter, pwm_top, pwm_invert,
                    pwm.scratch, sizeof(pwm.scratch) / sizeof(pwm.scratch[0]),
                    raw_pwm_write, &pwm.raw) != NANOTTS_OK) {
                if (strcmp(output_path, "-") != 0) perror(output_path);
                free(owned_input); return 1;
            }
            audio_write = nanotts_pwm_write_pcm;
            audio_user = &pwm.adapter;
            output_open = 1;
#else
            fprintf(stderr, "PWM support was disabled at build time\n");
            free(owned_input); return 1;
#endif
            break;
        }

        result = mode == INPUT_IPA
            ? nanotts_speak_ipa(&tts, inline_input, input_length, &options,
                               audio_write, audio_user, &info)
            : nanotts_speak_text(&tts, inline_input, input_length, &options,
                                audio_write, audio_user, &info);

        if (output_open) {
            if (format == OUTPUT_WAV) output_ok = wav_end(&wav);
#if NANOTTS_CLI_HAVE_PCM
            else if (format == OUTPUT_PCM16LE) output_ok = raw_end(&pcm.raw);
#endif
#if NANOTTS_CLI_HAVE_PWM
            else if (format == OUTPUT_PWM16LE) output_ok = raw_end(&pwm.raw);
#endif
        }
        if (!output_ok && result == NANOTTS_OK) {
            fprintf(stderr, "failed while writing %s\n", output_path);
            result = NANOTTS_ERR_CALLBACK_ABORTED;
        }
    }

    if (result != NANOTTS_OK) {
        fprintf(stderr, "%s", nanotts_strerror(result));
        if (info.error_byte != NANOTTS_NPOS) {
            fprintf(stderr, " at byte %lu", (unsigned long)info.error_byte);
            if (info.error_codepoint != 0u) {
                fprintf(stderr, " (U+%04lX)", (unsigned long)info.error_codepoint);
            }
        }
        fputc('\n', stderr);
        free(owned_input);
        return 1;
    }

    free(owned_input);
    return 0;
}
