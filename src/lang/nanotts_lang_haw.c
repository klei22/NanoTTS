/* SPDX-License-Identifier: MIT */
#include "lang/nanotts_lang_polynesian.h"

nanotts_result_t nanotts_lang_haw_parse_text(
    nanotts_impl_t *impl,
    const char *text,
    size_t length,
    nanotts_parse_info_t *info)
{
    return nanotts_lang_polynesian_parse_text(
        impl, text, length, info, NANOTTS_POLYNESIAN_HAWAIIAN);
}
