#ifndef UARCH_MEASURE_H
#define UARCH_MEASURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t latency_cycles;
    uint64_t throughput_cycles;
} MeasureResult;

void measure_init(double cpu_freq_ghz);

MeasureResult measure_instruction(
    void (*latency_func)(uint64_t),
    void (*throughput_func)(uint64_t),
    uint64_t iterations
);

MeasureResult measure_memory_instruction(
    void (*latency_func)(uint64_t, void*),
    void (*throughput_func)(uint64_t, void*),
    uint64_t iterations,
    void* buffer
);

uint64_t measure_latency_only(
    void (*func)(uint64_t),
    uint64_t iterations
);

#ifdef __cplusplus
}
#endif

#endif
