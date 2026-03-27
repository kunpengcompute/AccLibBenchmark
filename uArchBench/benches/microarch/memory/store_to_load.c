#include <stdint.h>
#include <stdio.h>
#include "timer.h"

extern void bench_store_to_load_latency(uint64_t iterations, void* buffer);

void measure_store_to_load_forwarding(uint64_t iterations, double cpu_freq_ghz) {
    void* buffer = aligned_alloc(64, 128);
    *(void**)buffer = buffer;
    
    printf("Store-to-Load Forwarding Test\n");
    printf("Testing str + ldr dependency chain latency\n");
    
    uint64_t start = uarch_nanoseconds();
    bench_store_to_load_latency(iterations, buffer);
    uint64_t end = uarch_nanoseconds();
    
    uint64_t ns = end - start;
    uint64_t cycles = (uint64_t)((double)ns * cpu_freq_ghz);
    
    double latency_per_pair = (double)cycles / (double)(32 * iterations);
    
    printf("Total time: %lu ns\n", ns);
    printf("Latency per str+ldr pair: %.2f cycles\n", latency_per_pair);
    printf("\n");
    
    free(buffer);
}
