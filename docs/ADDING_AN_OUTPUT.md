# Adding an output adapter

NanoTTS core has exactly one audio ABI: synchronous blocks of mono native
`int16_t` PCM at `nanotts_sample_rate()`. An output adapter transforms those
blocks into a transport- or peripheral-oriented representation without changing
language parsing or synthesis.

```text
text / IPA / events → renderer → nanotts_write_fn (int16 PCM)
                                      │
                                      └── adapter → application sink
```

Existing examples are:

- `NanoTTS::PWM`: PCM to native `uint16_t` timer compare values;
- `NanoTTS::PCM`: PCM to signed 16-bit little-endian bytes;
- host WAV: CLI-only container writer, not a runtime library.

## Adapter design rules

An MCU-oriented adapter should:

1. expose a small public header under `include/nanotts/`;
2. live in its own source file and library target;
3. accept caller-owned scratch storage;
4. allocate no heap memory;
5. perform no language lookup;
6. avoid filesystem, threads, locks, and vendor SDK dependencies;
7. propagate downstream back-pressure by returning nonzero;
8. document ownership and lifetime of every supplied buffer;
9. remain removable at compile/link time;
10. test conversion endpoints, chunking, and end-to-end rendering.

The application sink must consume or copy each adapter block before returning.
Adapters may reuse their scratch buffer immediately after the callback returns.

## Template shape

For an unsigned N-bit DAC adapter, for example:

```c
typedef int (*nanotts_dac_write_fn)(
    void *user,
    const uint16_t *values,
    size_t count);

typedef struct nanotts_dac_output {
    nanotts_dac_write_fn write;
    void *user;
    uint16_t *scratch;
    size_t scratch_count;
    uint16_t maximum;
} nanotts_dac_output_t;

nanotts_result_t nanotts_dac_output_init(...);
int nanotts_dac_write_pcm(
    void *output,
    const int16_t *samples,
    size_t count);
```

`nanotts_dac_write_pcm` then has the same function type as `nanotts_write_fn`
and can be passed directly to `nanotts_speak_text`, `nanotts_speak_ipa`, or
`nanotts_render_events`.

## Build integration

Use an independent option and target:

```cmake
option(NANOTTS_BUILD_DAC "Build the optional PCM-to-DAC adapter" OFF)

if(NANOTTS_BUILD_DAC)
    add_library(nanotts_dac src/output/nanotts_dac.c)
    add_library(NanoTTS::DAC ALIAS nanotts_dac)
    target_link_libraries(nanotts_dac PUBLIC NanoTTS::NanoTTS)
endif()
```

Add the target to installation/export only when enabled. Do not put adapter
state into `nanotts_t`; otherwise every application would pay its RAM cost.

## Performance boundary

Conversion occurs after a PCM block is synthesized. An adapter must not add a
branch to the oscillator, filter, frame, or phone loops. For high-rate formats,
prefer integer arithmetic and block conversion. Keep target-specific register
packing and DMA descriptors in the application callback so the portable adapter
remains auditable.

## Validation checklist

- minimum, zero, and maximum PCM map correctly;
- output values stay inside the advertised range;
- odd scratch capacities and multi-chunk writes work;
- zero/invalid capacities are rejected;
- downstream cancellation returns `NANOTTS_ERR_CALLBACK_ABORTED` through the
  public render API;
- the adapter target is absent when disabled;
- core-only, adapter-only combinations, installation, and external CMake
  consumption succeed;
- target DMA ownership is documented and tested on the actual MCU.
