#ifndef UARCH_BENCHMARKS_H
#define UARCH_BENCHMARKS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef UARCH_ENABLE_INSTRUCTION_BENCH
void bench_add_latency(uint64_t iterations);
void bench_add_throughput(uint64_t iterations);
void bench_sub_latency(uint64_t iterations);
void bench_sub_throughput(uint64_t iterations);
void bench_mul_latency(uint64_t iterations);
void bench_mul_throughput(uint64_t iterations);
void bench_div_latency(uint64_t iterations);
void bench_div_throughput(uint64_t iterations);
void bench_mov_latency(uint64_t iterations);
void bench_mov_throughput(uint64_t iterations);
void bench_load_latency(uint64_t iterations, void* buffer);
void bench_load_throughput(uint64_t iterations, void* buffer);
void bench_store_latency(uint64_t iterations, void* buffer);
void bench_store_throughput(uint64_t iterations, void* buffer);
void bench_branch_taken(uint64_t iterations);
void bench_branch_not_taken(uint64_t iterations);
void bench_neon_add_latency(uint64_t iterations, void* buffer);
void bench_neon_add_throughput(uint64_t iterations, void* buffer);
#endif

#ifdef UARCH_ENABLE_MICROARCH
void measure_cache_latency(size_t working_size, size_t iterations);
void measure_cache_bandwidth(size_t working_size, size_t iterations);
void measure_pipeline_characteristics(size_t iterations);
void measure_prefetch_effect(size_t working_size, size_t iterations, size_t stride);
void measure_store_to_load_forwarding(uint64_t iterations, double cpu_freq_ghz);
#endif

#ifdef __cplusplus
}
#endif

#endif
