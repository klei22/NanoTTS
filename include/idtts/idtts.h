/* SPDX-License-Identifier: MIT */
#ifndef IDTTS_IDTTS_H
#define IDTTS_IDTTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDTTS_VERSION_MAJOR 0
#define IDTTS_VERSION_MINOR 2
#define IDTTS_VERSION_PATCH 1

#ifndef IDTTS_MAX_EVENTS
#define IDTTS_MAX_EVENTS 512u
#endif

#ifndef IDTTS_CONTEXT_BYTES
#define IDTTS_CONTEXT_BYTES 3072u
#endif

#ifndef IDTTS_NPOS
#define IDTTS_NPOS ((size_t)-1)
#endif

typedef enum idtts_result {
    IDTTS_OK = 0,
    IDTTS_ERR_ARGUMENT,
    IDTTS_ERR_SAMPLE_RATE,
    IDTTS_ERR_UTF8,
    IDTTS_ERR_UNKNOWN_IPA,
    IDTTS_ERR_TOO_MANY_EVENTS,
    IDTTS_ERR_EMPTY_INPUT,
    IDTTS_ERR_CALLBACK_ABORTED,
    IDTTS_ERR_OUTPUT_TOO_SMALL,
    IDTTS_ERR_FEATURE_DISABLED,
    IDTTS_ERR_INTERNAL
} idtts_result_t;

typedef enum idtts_final_tone {
    IDTTS_FINAL_FALL = 0,
    IDTTS_FINAL_RISE = 1,
    IDTTS_FINAL_CONTINUE = 2,
    IDTTS_FINAL_LEVEL = 3,
    IDTTS_FINAL_AUTO = 4
} idtts_final_tone_t;

enum {
    IDTTS_OPT_PERMISSIVE_IPA = 1u << 0,
    IDTTS_OPT_NO_AUTOPAUSE   = 1u << 1
};

enum {
    IDTTS_EVENT_STRESS_PRIMARY   = 1u << 0,
    IDTTS_EVENT_STRESS_SECONDARY = 1u << 1,
    IDTTS_EVENT_LONG             = 1u << 2,
    IDTTS_EVENT_WORD_END         = 1u << 3,
    IDTTS_EVENT_PHRASE_END       = 1u << 4,
    IDTTS_EVENT_QUESTION         = 1u << 5
};

typedef enum idtts_phone {
    IDTTS_PH_SILENCE = 0,
    IDTTS_PH_SHORT_PAUSE,
    IDTTS_PH_PHRASE_PAUSE,

    IDTTS_PH_A,
    IDTTS_PH_E,
    IDTTS_PH_SCHWA,
    IDTTS_PH_OPEN_E,
    IDTTS_PH_I,
    IDTTS_PH_SMALL_I,
    IDTTS_PH_O,
    IDTTS_PH_OPEN_O,
    IDTTS_PH_U,
    IDTTS_PH_SMALL_U,
    IDTTS_PH_AI,
    IDTTS_PH_AU,
    IDTTS_PH_OI,

    IDTTS_PH_P,
    IDTTS_PH_B,
    IDTTS_PH_T,
    IDTTS_PH_D,
    IDTTS_PH_K,
    IDTTS_PH_G,
    IDTTS_PH_GLOTTAL_STOP,

    IDTTS_PH_CH,
    IDTTS_PH_J,

    IDTTS_PH_F,
    IDTTS_PH_V,
    IDTTS_PH_S,
    IDTTS_PH_Z,
    IDTTS_PH_SH,
    IDTTS_PH_X,
    IDTTS_PH_H,

    IDTTS_PH_M,
    IDTTS_PH_N,
    IDTTS_PH_NY,
    IDTTS_PH_NG,

    IDTTS_PH_L,
    IDTTS_PH_R,
    IDTTS_PH_TAP,
    IDTTS_PH_W,
    IDTTS_PH_Y,

    IDTTS_PH_COUNT
} idtts_phone_t;

typedef struct idtts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t pitch_semitones_q4;
} idtts_event_t;

typedef struct idtts_options {
    uint16_t rate_q8;       /* 256 = normal speed; larger is faster. */
    int16_t pitch_cents;    /* Global pitch shift. */
    uint8_t volume;         /* 0..255. */
    uint8_t final_tone;     /* idtts_final_tone_t. */
    uint8_t flags;          /* IDTTS_OPT_*. */
    uint8_t reserved[3];
} idtts_options_t;

typedef struct idtts_parse_info {
    size_t event_count;
    size_t error_byte;
    uint32_t error_codepoint;
} idtts_parse_info_t;

/* Return nonzero from the callback to stop synthesis. */
typedef int (*idtts_write_fn)(void *user, const int16_t *samples, size_t count);

/* Opaque, statically allocated context. No heap allocation is performed. */
typedef union idtts {
    uint64_t _align_u64;
    double _align_double;
    unsigned char bytes[IDTTS_CONTEXT_BYTES];
} idtts_t;

void idtts_default_options(idtts_options_t *options);
idtts_result_t idtts_init(idtts_t *tts, uint32_t sample_rate);
void idtts_reset(idtts_t *tts);
uint32_t idtts_sample_rate(const idtts_t *tts);

idtts_result_t idtts_parse_ipa(
    idtts_t *tts,
    const char *ipa_utf8,
    size_t length,
    uint8_t parse_flags,
    idtts_parse_info_t *info);

idtts_result_t idtts_parse_text(
    idtts_t *tts,
    const char *text_utf8,
    size_t length,
    idtts_parse_info_t *info);

idtts_result_t idtts_set_events(
    idtts_t *tts,
    const idtts_event_t *events,
    size_t count);

idtts_result_t idtts_render_events(
    idtts_t *tts,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user);

idtts_result_t idtts_speak_ipa(
    idtts_t *tts,
    const char *ipa_utf8,
    size_t length,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user,
    idtts_parse_info_t *info);

idtts_result_t idtts_speak_text(
    idtts_t *tts,
    const char *text_utf8,
    size_t length,
    const idtts_options_t *options,
    idtts_write_fn write,
    void *user,
    idtts_parse_info_t *info);

size_t idtts_event_count(const idtts_t *tts);
size_t idtts_event_capacity(void);
size_t idtts_context_bytes(void);
int idtts_text_frontend_available(void);
const idtts_event_t *idtts_events(const idtts_t *tts);
const char *idtts_phone_name(idtts_phone_t phone);
const char *idtts_strerror(idtts_result_t result);
const char *idtts_version_string(void);

#ifdef __cplusplus
}
#endif

#endif
