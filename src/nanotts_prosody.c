/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

/*
 * Profile values are intentionally compact and conservative. They shape only
 * timing and F0; pronunciation remains entirely in each text module. Pitch
 * values are sixteenth-semitones. A profile is selected once per render.
 * Profiles belonging to disabled text modules are not instantiated.
 */
#define NANOTTS_PROFILE_DEFAULT { \
    126u, 100u, 100u, 100u, 100u, 108u, 104u, \
    12, 5, 14, 0, -20, 7, -5, 27, 8, -2, 9 }
#define NANOTTS_PROFILE_ID { \
    126u, 100u, 99u, 100u, 100u, 107u, 103u, \
    10, 4, 12, 0, -18, 6, -4, 24, 7, -2, 8 }
#define NANOTTS_PROFILE_SW { \
    124u, 104u, 98u, 96u, 101u, 112u, 105u, \
    14, 5, 12, 1, -18, 7, -4, 25, 8, -1, 10 }
#define NANOTTS_PROFILE_ES { \
    130u, 97u, 96u, 90u, 96u, 111u, 104u, \
    15, 5, 16, 0, -23, 8, -6, 30, 10, -2, 12 }
#define NANOTTS_PROFILE_MS { \
    124u, 101u, 99u, 99u, 100u, 106u, 103u, \
    9, 3, 11, 0, -17, 5, -4, 23, 7, -2, 8 }
#define NANOTTS_PROFILE_MI { \
    128u, 108u, 96u, 94u, 102u, 113u, 105u, \
    15, 5, 13, 1, -19, 8, -4, 26, 8, 0, 10 }
#define NANOTTS_PROFILE_HAW { \
    132u, 111u, 95u, 92u, 103u, 112u, 105u, \
    14, 5, 14, 1, -20, 9, -4, 28, 9, 0, 11 }

#define NANOTTS_PP_CAT_I(a_, b_) a_##b_
#define NANOTTS_PP_CAT(a_, b_) NANOTTS_PP_CAT_I(a_, b_)
#define NANOTTS_PP_IF_0(...)
#define NANOTTS_PP_IF_1(...) __VA_ARGS__
#define NANOTTS_PP_IF(value_) NANOTTS_PP_CAT(NANOTTS_PP_IF_, value_)

static const nanotts_prosody_profile_t PROFILE_DEFAULT =
    NANOTTS_PROFILE_DEFAULT;

#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
    NANOTTS_PP_IF(enabled_)( \
        static const nanotts_prosody_profile_t PROFILE_##profile_ = \
            NANOTTS_PROFILE_##profile_;)
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE

const nanotts_prosody_profile_t *nanotts_language_prosody(
    nanotts_language_t language)
{
#if NANOTTS_ENABLE_TEXT_FRONTEND
    switch (language) {
#define NANOTTS_LANGUAGE(tag_, enum_, value_, code_, name_, aliases_, enabled_, parser_, profile_) \
        NANOTTS_PP_IF(enabled_)( \
            case NANOTTS_LANG_##enum_: return &PROFILE_##profile_;)
#include "nanotts/nanotts_languages.def"
#undef NANOTTS_LANGUAGE
    default:
        break;
    }
#else
    (void)language;
#endif
    return &PROFILE_DEFAULT;
}
