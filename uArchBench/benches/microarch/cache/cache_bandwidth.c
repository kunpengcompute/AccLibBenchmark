#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timer.h"

typedef struct {
    uint64_t data[8];
} cache_line_t;

static cache_line_t* read_buffer = NULL;
static cache_line_t* write_buffer = NULL;
static size_t buffer_count = 0;

void cache_bandwidth_setup(size_t size) {
    buffer_count = size / sizeof(cache_line_t);
    read_buffer = (cache_line_t*)aligned_alloc(64, buffer_count * sizeof(cache_line_t));
    write_buffer = (cache_line_t*)aligned_alloc(64, buffer_count * sizeof(cache_line_t));
    memset(read_buffer, 0xAA, buffer_count * sizeof(cache_line_t));
    memset(write_buffer, 0, buffer_count * sizeof(cache_line_t));
}

uint64_t measure_read_bandwidth(size_t iterations) {
    uint64_t start = uarch_nanoseconds();
    volatile uint64_t sum = 0;
    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < buffer_count; i++) {
            sum += read_buffer[i].data[0];
        }
    }
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

uint64_t measure_write_bandwidth(size_t iterations) {
    uint64_t start = uarch_nanoseconds();
    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < buffer_count; i++) {
            write_buffer[i].data[0] = 0x55;
        }
    }
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

void cache_bandwidth_teardown(void) {
    if (read_buffer) {
        free(read_buffer);
        read_buffer = NULL;
    }
    if (write_buffer) {
        free(write_buffer);
        write_buffer = NULL;
    }
}

void measure_cache_bandwidth(size_t working_size, size_t iterations) {
    cache_bandwidth_setup(working_size);
    
    uint64_t read_ns = measure_read_bandwidth(iterations);
    uint64_t write_ns = measure_write_bandwidth(iterations);
    
    double bytes_per_ns_read = (double)(working_size * iterations) / read_ns;
    double bytes_per_ns_write = (double)(working_size * iterations) / write_ns;
    
    printf("Working set: %zu KB\n", working_size / 1024);
    printf("Read bandwidth: %.2f GB/s\n", bytes_per_ns_read);
    printf("Write bandwidth: %.2f GB/s\n", bytes_per_ns_write);
    
    cache_bandwidth_teardown();
}
