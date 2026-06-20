/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_LANG_COMMON_H
#define NANOTTS_LANG_COMMON_H

#include "nanotts_internal.h"

#define NANOTTS_LANG_WORD_MAX 80u
#define NANOTTS_LANG_WORD_PHONE_MAX 96u

bool nanotts_lang_decode_utf8(
    const char *text,
    size_t length,
    size_t offset,
    uint32_t *cp,
    size_t *width);

/* Normalize Polynesian short/long vowels without a Unicode library. */
bool nanotts_lang_macron_vowel(
    uint32_t cp,
    uint32_t *base_lower,
    bool *is_long,
    bool *is_upper);

bool nanotts_lang_apply_combining_macron(uint32_t *lower_vowel);

bool nanotts_lang_add_pause_scaled(
    nanotts_impl_t *impl,
    nanotts_phone_t pause,
    uint8_t flags,
    uint8_t duration_percent);

bool nanotts_lang_add_pause(
    nanotts_impl_t *impl,
    nanotts_phone_t pause,
    uint8_t flags);

bool nanotts_lang_temp_push(
    nanotts_event_t *events,
    size_t *count,
    nanotts_phone_t phone);

/* Conservative heuristic for all-uppercase tokens such as GPS or CPU. */
bool nanotts_lang_should_spell_acronym(
    const uint32_t *word,
    size_t length);

void nanotts_lang_apply_penultimate_stress(
    nanotts_event_t *events,
    size_t count);

void nanotts_lang_mark_word_end(nanotts_impl_t *impl);
void nanotts_lang_trim_trailing_short_pause(nanotts_impl_t *impl);

void nanotts_lang_set_info(
    nanotts_parse_info_t *info,
    size_t count,
    size_t byte,
    uint32_t cp);

#endif
