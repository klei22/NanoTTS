/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts_pcm.h"

#include <string.h>

nanotts_result_t nanotts_pcm16le_output_init(
    nanotts_pcm16le_output_t *output,
    uint8_t *scratch,
    size_t scratch_bytes,
    nanotts_pcm_bytes_write_fn write,
    void *user)
{
    if (output == NULL || scratch == NULL || scratch_bytes < 2u ||
        write == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    output->write = write;
    output->user = user;
    output->scratch = scratch;
    output->scratch_bytes = scratch_bytes;
    return NANOTTS_OK;
}

int nanotts_pcm16le_write_pcm(
    void *opaque,
    const int16_t *samples,
    size_t count)
{
    nanotts_pcm16le_output_t *output = (nanotts_pcm16le_output_t *)opaque;
    size_t offset = 0u;
    size_t sample_capacity;

    if (output == NULL || samples == NULL || output->write == NULL ||
        output->scratch == NULL || output->scratch_bytes < 2u) {
        return 1;
    }
    sample_capacity = output->scratch_bytes / 2u;
    while (offset < count) {
        size_t chunk = count - offset;
        size_t index;
        if (chunk > sample_capacity) chunk = sample_capacity;
        for (index = 0u; index < chunk; ++index) {
            uint16_t value = (uint16_t)samples[offset + index];
            output->scratch[index * 2u] = (uint8_t)(value & 0xffu);
            output->scratch[index * 2u + 1u] = (uint8_t)(value >> 8);
        }
        if (output->write(output->user, output->scratch, chunk * 2u) != 0) {
            return 1;
        }
        offset += chunk;
    }
    return 0;
}
