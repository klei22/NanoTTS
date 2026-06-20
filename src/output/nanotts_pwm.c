/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts_pwm.h"

#include <string.h>

uint16_t nanotts_pcm16_to_pwm(
    int16_t sample,
    uint16_t top,
    int invert)
{
    uint32_t unsigned_sample = (uint32_t)((int32_t)sample + 32768);
    uint32_t duty;
    if (top == 0u) return 0u;
    /* Uniform 16-bit quantization without a per-sample integer divide. The
     * endpoint-safe (top + 1) scale maps 0 -> 0 and 65535 -> top. */
    duty = (unsigned_sample * ((uint32_t)top + 1u)) >> 16;
    if (invert != 0) duty = (uint32_t)top - duty;
    return (uint16_t)duty;
}

nanotts_result_t nanotts_pwm_output_init(
    nanotts_pwm_output_t *output,
    uint16_t top,
    int invert,
    uint16_t *scratch,
    size_t scratch_count,
    nanotts_pwm_write_fn write,
    void *user)
{
    if (output == NULL || top == 0u || scratch == NULL ||
        scratch_count == 0u || write == NULL) {
        return NANOTTS_ERR_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    output->write = write;
    output->user = user;
    output->scratch = scratch;
    output->scratch_count = scratch_count;
    output->top = top;
    output->invert = invert != 0 ? 1u : 0u;
    return NANOTTS_OK;
}

int nanotts_pwm_write_pcm(
    void *opaque,
    const int16_t *samples,
    size_t count)
{
    nanotts_pwm_output_t *output = (nanotts_pwm_output_t *)opaque;
    size_t offset = 0u;

    if (output == NULL || samples == NULL || output->write == NULL ||
        output->scratch == NULL || output->scratch_count == 0u ||
        output->top == 0u) {
        return 1;
    }
    while (offset < count) {
        size_t chunk = count - offset;
        size_t index;
        if (chunk > output->scratch_count) chunk = output->scratch_count;
        for (index = 0u; index < chunk; ++index) {
            output->scratch[index] = nanotts_pcm16_to_pwm(
                samples[offset + index], output->top, output->invert != 0u);
        }
        if (output->write(output->user, output->scratch, chunk) != 0) {
            return 1;
        }
        offset += chunk;
    }
    return 0;
}
