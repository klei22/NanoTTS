/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_LANG_POLYNESIAN_H
#define NANOTTS_LANG_POLYNESIAN_H

#include "nanotts_internal.h"

typedef enum nanotts_polynesian_language {
    NANOTTS_POLYNESIAN_MAORI = 0,
    NANOTTS_POLYNESIAN_HAWAIIAN = 1
} nanotts_polynesian_language_t;

nanotts_result_t nanotts_lang_polynesian_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info,
    nanotts_polynesian_language_t language);

#endif
