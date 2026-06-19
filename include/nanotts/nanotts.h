/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_NANOTTS_H
#define NANOTTS_NANOTTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NANOTTS_VERSION_MAJOR 0
#define NANOTTS_VERSION_MINOR 3
#define NANOTTS_VERSION_PATCH 0

#ifndef NANOTTS_MAX_EVENTS
#define NANOTTS_MAX_EVENTS 512u
#endif

#ifndef NANOTTS_CONTEXT_BYTES
#define NANOTTS_CONTEXT_BYTES 3072u
#endif

#ifndef NANOTTS_NPOS
#define NANOTTS_NPOS ((size_t)-1)
#endif

typedef enum nanotts_result {
    NANOTTS_OK = 0,
    NANOTTS_ERR_ARGUMENT,
    NANOTTS_ERR_SAMPLE_RATE,
    NANOTTS_ERR_LANGUAGE_UNAVAILABLE,
    NANOTTS_ERR_UTF8,
    NANOTTS_ERR_UNKNOWN_IPA,
    NANOTTS_ERR_TOO_MANY_EVENTS,
    NANOTTS_ERR_EMPTY_INPUT,
    NANOTTS_ERR_CALLBACK_ABORTED,
    NANOTTS_ERR_OUTPUT_TOO_SMALL,
    NANOTTS_ERR_FEATURE_DISABLED,
    NANOTTS_ERR_INTERNAL
} nanotts_result_t;

/*
 * Text languages are selected once, before parsing text. NANOTTS_LANG_NONE is
 * intended for IPA-only builds and IPA-only contexts.
 */
typedef enum nanotts_language {
    NANOTTS_LANG_NONE = 0,
    NANOTTS_LANG_INDONESIAN = 1,
    NANOTTS_LANG_KISWAHILI = 2,
    /* Source compatibility alias; ISO 639-1 code remains "sw". */
    NANOTTS_LANG_SWAHILI = NANOTTS_LANG_KISWAHILI,
    NANOTTS_LANG_COUNT = 3
} nanotts_language_t;

typedef enum nanotts_final_tone {
    NANOTTS_FINAL_FALL = 0,
    NANOTTS_FINAL_RISE = 1,
    NANOTTS_FINAL_CONTINUE = 2,
    NANOTTS_FINAL_LEVEL = 3,
    NANOTTS_FINAL_AUTO = 4
} nanotts_final_tone_t;

enum {
    NANOTTS_OPT_PERMISSIVE_IPA = 1u << 0,
    NANOTTS_OPT_NO_AUTOPAUSE   = 1u << 1
};

enum {
    NANOTTS_EVENT_STRESS_PRIMARY   = 1u << 0,
    NANOTTS_EVENT_STRESS_SECONDARY = 1u << 1,
    NANOTTS_EVENT_LONG             = 1u << 2,
    NANOTTS_EVENT_WORD_END         = 1u << 3,
    NANOTTS_EVENT_PHRASE_END       = 1u << 4,
    NANOTTS_EVENT_QUESTION         = 1u << 5,
    NANOTTS_EVENT_SYLLABIC         = 1u << 6
};

typedef enum nanotts_phone {
    NANOTTS_PH_SILENCE = 0,
    NANOTTS_PH_SHORT_PAUSE,
    NANOTTS_PH_PHRASE_PAUSE,

    NANOTTS_PH_A,
    NANOTTS_PH_E,
    NANOTTS_PH_SCHWA,
    NANOTTS_PH_OPEN_E,
    NANOTTS_PH_I,
    NANOTTS_PH_SMALL_I,
    NANOTTS_PH_O,
    NANOTTS_PH_OPEN_O,
    NANOTTS_PH_U,
    NANOTTS_PH_SMALL_U,
    NANOTTS_PH_AI,
    NANOTTS_PH_AU,
    NANOTTS_PH_OI,

    NANOTTS_PH_P,
    NANOTTS_PH_B,
    NANOTTS_PH_T,
    NANOTTS_PH_D,
    NANOTTS_PH_K,
    NANOTTS_PH_G,
    NANOTTS_PH_GLOTTAL_STOP,

    NANOTTS_PH_CH,
    NANOTTS_PH_J,

    NANOTTS_PH_F,
    NANOTTS_PH_V,
    NANOTTS_PH_S,
    NANOTTS_PH_Z,
    NANOTTS_PH_TH,
    NANOTTS_PH_DH,
    NANOTTS_PH_SH,
    NANOTTS_PH_X,
    NANOTTS_PH_GH,
    NANOTTS_PH_H,

    NANOTTS_PH_M,
    NANOTTS_PH_N,
    NANOTTS_PH_NY,
    NANOTTS_PH_NG,

    NANOTTS_PH_L,
    NANOTTS_PH_R,
    NANOTTS_PH_TAP,
    NANOTTS_PH_W,
    NANOTTS_PH_Y,

    NANOTTS_PH_COUNT
} nanotts_phone_t;

typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t pitch_semitones_q4;
} nanotts_event_t;

typedef struct nanotts_options {
    uint16_t rate_q8;       /* 256 = normal speed; larger is faster. */
    int16_t pitch_cents;    /* Global pitch shift. */
    uint8_t volume;         /* 0..255. */
    uint8_t final_tone;     /* nanotts_final_tone_t. */
    uint8_t flags;          /* NANOTTS_OPT_*. */
    uint8_t reserved[3];
} nanotts_options_t;

typedef struct nanotts_parse_info {
    size_t event_count;
    size_t error_byte;
    uint32_t error_codepoint;
} nanotts_parse_info_t;

/* Return nonzero from the callback to stop synthesis. */
typedef int (*nanotts_write_fn)(void *user, const int16_t *samples, size_t count);

/* Opaque, statically allocated context. No heap allocation is performed. */
typedef union nanotts {
    uint64_t _align_u64;
    double _align_double;
    unsigned char bytes[NANOTTS_CONTEXT_BYTES];
} nanotts_t;

void nanotts_default_options(nanotts_options_t *options);

/* The language is selected at initialization and may later be changed while idle. */
nanotts_result_t nanotts_init(
    nanotts_t *tts,
    uint32_t sample_rate,
    nanotts_language_t language);
void nanotts_reset(nanotts_t *tts);
uint32_t nanotts_sample_rate(const nanotts_t *tts);

nanotts_result_t nanotts_set_language(
    nanotts_t *tts,
    nanotts_language_t language);
nanotts_language_t nanotts_language(const nanotts_t *tts);
int nanotts_language_available(nanotts_language_t language);
size_t nanotts_compiled_language_count(void);
nanotts_language_t nanotts_compiled_language_at(size_t index);
nanotts_language_t nanotts_language_from_code(const char *code);
const char *nanotts_language_code(nanotts_language_t language);
const char *nanotts_language_name(nanotts_language_t language);

nanotts_result_t nanotts_parse_ipa(
    nanotts_t *tts,
    const char *ipa_utf8,
    size_t length,
    uint8_t parse_flags,
    nanotts_parse_info_t *info);

nanotts_result_t nanotts_parse_text(
    nanotts_t *tts,
    const char *text_utf8,
    size_t length,
    nanotts_parse_info_t *info);

nanotts_result_t nanotts_set_events(
    nanotts_t *tts,
    const nanotts_event_t *events,
    size_t count);

nanotts_result_t nanotts_render_events(
    nanotts_t *tts,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user);

nanotts_result_t nanotts_speak_ipa(
    nanotts_t *tts,
    const char *ipa_utf8,
    size_t length,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user,
    nanotts_parse_info_t *info);

nanotts_result_t nanotts_speak_text(
    nanotts_t *tts,
    const char *text_utf8,
    size_t length,
    const nanotts_options_t *options,
    nanotts_write_fn write,
    void *user,
    nanotts_parse_info_t *info);

size_t nanotts_event_count(const nanotts_t *tts);
size_t nanotts_event_capacity(void);
size_t nanotts_context_bytes(void);
int nanotts_text_frontend_available(void);
const nanotts_event_t *nanotts_events(const nanotts_t *tts);
const char *nanotts_phone_name(nanotts_phone_t phone);
const char *nanotts_strerror(nanotts_result_t result);
const char *nanotts_version_string(void);

#ifdef __cplusplus
}
#endif

#endif
