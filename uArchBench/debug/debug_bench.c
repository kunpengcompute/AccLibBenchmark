#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "benchmarks.h"
#include "timer.h"
#include "cpu_freq.h"

int main(int argc, char* argv[]) {
    uint64_t iterations = 10000000;
    
    if (argc > 1) {
        iterations = strtoull(argv[1], NULL, 10);
    }
    
    double cpu_freq_ghz = measure_cpu_frequency();

    void* buffer = aligned_alloc(64, 128);
    memset(buffer, 0xAA, 128);
    *(void**)buffer = buffer;
    
    printf("=== Debug: Latency and Throughput ===\n");
    printf("CPU Frequency: %.3f GHz\n", cpu_freq_ghz);
    printf("Iterations: %lu\n\n", iterations);
    
    for (int i = 0; i < 3; i++) {
        bench_store_latency(iterations / 100, buffer);
    }
    
    printf("Testing Latency...\n");
    uint64_t start = uarch_nanoseconds();
    bench_store_latency(iterations, buffer);
    uint64_t end = uarch_nanoseconds();
    uint64_t latency_ns = end - start;
    uint64_t latency_cycles = (uint64_t)((double)latency_ns * cpu_freq_ghz);
    double latency_per_op = (double)latency_cycles / (double)(iterations * 32);
    printf("Latency: %.2f cycles\n\n", latency_per_op);
    
    for (int i = 0; i < 3; i++) {
        bench_store_throughput(iterations / 100, buffer);
    }
    
    printf("Testing Throughput...\n");
    start = uarch_nanoseconds();
    bench_store_throughput(iterations, buffer);
    end = uarch_nanoseconds();
    uint64_t throughput_ns = end - start;
    uint64_t throughput_cycles = (uint64_t)((double)throughput_ns * cpu_freq_ghz);
    double ipc = (double)(iterations * 32) / (double)throughput_cycles;
    printf("Throughput(IPC): %.2f\n\n", ipc);
    
    printf("=== Perf Commands ===\n");
    printf("  perf stat -e cycles,instructions ./build/debug/debug_bench\n");
    printf("  perf record -g ./build/debug/debug_bench\n");
    printf("  perf annotate\n");
    printf("  objdump -d ./build/debug/debug_bench\n");
    
    return 0;
}
