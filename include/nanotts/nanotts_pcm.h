/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_NANOTTS_PCM_H
#define NANOTTS_NANOTTS_PCM_H

#include "nanotts/nanotts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Byte sink used by the portable raw-PCM adapter. */
typedef int (*nanotts_pcm_bytes_write_fn)(
    void *user,
    const uint8_t *bytes,
    size_t count);

typedef struct nanotts_pcm16le_output {
    nanotts_pcm_bytes_write_fn write;
    void *user;
    uint8_t *scratch;
    size_t scratch_bytes;
} nanotts_pcm16le_output_t;

/*
 * Initialize a zero-allocation signed 16-bit little-endian PCM adapter.
 * scratch_bytes must be at least two. The caller owns the scratch storage.
 */
nanotts_result_t nanotts_pcm16le_output_init(
    nanotts_pcm16le_output_t *output,
    uint8_t *scratch,
    size_t scratch_bytes,
    nanotts_pcm_bytes_write_fn write,
    void *user);

/* Pass this directly as nanotts_write_fn to any render/speak function. */
int nanotts_pcm16le_write_pcm(
    void *output,
    const int16_t *samples,
    size_t count);

#ifdef __cplusplus
}
#endif

#endif
