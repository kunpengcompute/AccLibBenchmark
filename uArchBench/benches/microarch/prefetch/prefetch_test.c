#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timer.h"

typedef struct {
    uint64_t data[8];
} cache_line_t;

static cache_line_t* prefetch_buffer = NULL;
static size_t buffer_count = 0;

void prefetch_test_setup(size_t size) {
    buffer_count = size / sizeof(cache_line_t);
    prefetch_buffer = (cache_line_t*)aligned_alloc(64, buffer_count * sizeof(cache_line_t));
    memset(prefetch_buffer, 0x42, buffer_count * sizeof(cache_line_t));
}

static inline void prefetch_read(void* addr) {
    __asm__ volatile("prfm pldl1keep, [%0]" : : "r"(addr));
}

uint64_t measure_with_prefetch(size_t iterations, size_t stride) {
    uint64_t start = uarch_nanoseconds();
    volatile uint64_t sum = 0;
    
    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < buffer_count; i += stride) {
            if (i + stride < buffer_count) {
                prefetch_read(&prefetch_buffer[i + stride]);
            }
            sum += prefetch_buffer[i].data[0];
        }
    }
    
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

uint64_t measure_without_prefetch(size_t iterations, size_t stride) {
    uint64_t start = uarch_nanoseconds();
    volatile uint64_t sum = 0;
    
    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < buffer_count; i += stride) {
            sum += prefetch_buffer[i].data[0];
        }
    }
    
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

void prefetch_test_teardown(void) {
    if (prefetch_buffer) {
        free(prefetch_buffer);
        prefetch_buffer = NULL;
    }
}

void measure_prefetch_effect(size_t working_size, size_t iterations, size_t stride) {
    prefetch_test_setup(working_size);
    
    uint64_t with_prefetch = measure_with_prefetch(iterations, stride);
    uint64_t without_prefetch = measure_without_prefetch(iterations, stride);
    
    printf("Working set: %zu KB, Stride: %zu lines\n", working_size / 1024, stride);
    printf("With prefetch: %lu ns\n", with_prefetch);
    printf("Without prefetch: %lu ns\n", without_prefetch);
    printf("Speedup: %.2fx\n", (double)without_prefetch / with_prefetch);
    
    prefetch_test_teardown();
}
