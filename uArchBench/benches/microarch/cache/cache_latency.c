#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timer.h"

typedef struct {
    volatile uint64_t data[8];
} cache_line_t;

static cache_line_t* buffer = NULL;
static size_t buffer_size = 0;

void cache_latency_setup(size_t size) {
    buffer_size = size / sizeof(cache_line_t);
    buffer = (cache_line_t*)aligned_alloc(64, buffer_size * sizeof(cache_line_t));
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i].data[0] = (i + 1) % buffer_size;
    }
}

uint64_t cache_latency_test(size_t iterations) {
    uint64_t start = uarch_nanoseconds();
    volatile uint64_t index = 0;
    for (size_t i = 0; i < iterations; i++) {
        index = buffer[index].data[0];
    }
    uint64_t end = uarch_nanoseconds();
    (void)index;
    return end - start;
}

void cache_latency_teardown(void) {
    if (buffer) {
        free((void*)buffer);
        buffer = NULL;
    }
}

void measure_cache_latency(size_t working_size, size_t iterations) {
    cache_latency_setup(working_size);
    
    printf("Working set: %zu KB\n", working_size / 1024);
    
    uint64_t ns = cache_latency_test(iterations);
    double latency_per_access = (double)ns / (double)iterations;
    
    printf("Total time: %lu ns\n", ns);
    printf("Latency per access: %.2f ns\n", latency_per_access);
    
    cache_latency_teardown();
}
