#include <stdint.h>
#include <stdio.h>
#include "timer.h"

uint64_t measure_pipeline_depth(size_t iterations) {
    uint64_t start = uarch_nanoseconds();
    
    volatile uint64_t a = 0, b = 0, c = 0, d = 0;
    for (size_t i = 0; i < iterations; i++) {
        a += 1;
        b += 1;
        c += 1;
        d += 1;
    }
    
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

uint64_t measure_dependency_chain(size_t iterations) {
    uint64_t start = uarch_nanoseconds();
    
    volatile uint64_t x = 1;
    for (size_t i = 0; i < iterations; i++) {
        x = x + 1;
    }
    
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

uint64_t measure_independent_ops(size_t iterations) {
    uint64_t start = uarch_nanoseconds();
    
    volatile uint64_t a = 1, b = 2, c = 3, d = 4;
    for (size_t i = 0; i < iterations; i++) {
        a = a + 1;
        b = b + 1;
        c = c + 1;
        d = d + 1;
    }
    
    uint64_t end = uarch_nanoseconds();
    return end - start;
}

void measure_pipeline_characteristics(size_t iterations) {
    uint64_t dep_ns = measure_dependency_chain(iterations);
    uint64_t indep_ns = measure_independent_ops(iterations);
    uint64_t parallel_ns = measure_pipeline_depth(iterations);
    
    printf("Dependency chain: %lu ns (%.2f per iteration)\n", 
           dep_ns, (double)dep_ns / iterations);
    printf("Independent ops: %lu ns (%.2f per iteration)\n", 
           indep_ns, (double)indep_ns / iterations);
    printf("Parallel ops: %lu ns (%.2f per iteration)\n", 
           parallel_ns, (double)parallel_ns / iterations);
}
