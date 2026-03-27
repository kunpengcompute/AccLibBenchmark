#include "measure.h"
#include "timer.h"

static double g_cpu_freq_ghz = 2.4;

void measure_init(double cpu_freq_ghz) {
    g_cpu_freq_ghz = cpu_freq_ghz;
}

static uint64_t ns_to_cycles(uint64_t ns_elapsed) {
    return (uint64_t)((double)ns_elapsed * g_cpu_freq_ghz);
}

static void warmup(void (*func)(uint64_t), uint64_t warmup_iters) {
    func(warmup_iters);
}

static void warmup_memory(void (*func)(uint64_t, void*), uint64_t warmup_iters, void* buffer) {
    func(warmup_iters, buffer);
}

MeasureResult measure_instruction(
    void (*latency_func)(uint64_t),
    void (*throughput_func)(uint64_t),
    uint64_t iterations
) {
    MeasureResult result;
    
    warmup(latency_func, iterations / 100);
    
    uint64_t start = uarch_nanoseconds();
    latency_func(iterations);
    uint64_t end = uarch_nanoseconds();
    result.latency_cycles = ns_to_cycles(end - start);
    
    warmup(throughput_func, iterations / 100);
    
    start = uarch_nanoseconds();
    throughput_func(iterations);
    end = uarch_nanoseconds();
    result.throughput_cycles = ns_to_cycles(end - start);
    
    return result;
}

MeasureResult measure_memory_instruction(
    void (*latency_func)(uint64_t, void*),
    void (*throughput_func)(uint64_t, void*),
    uint64_t iterations,
    void* buffer
) {
    MeasureResult result;
    
    warmup_memory(latency_func, iterations / 100, buffer);
    
    uint64_t start = uarch_nanoseconds();
    latency_func(iterations, buffer);
    uint64_t end = uarch_nanoseconds();
    result.latency_cycles = ns_to_cycles(end - start);
    
    warmup_memory(throughput_func, iterations / 100, buffer);
    
    start = uarch_nanoseconds();
    throughput_func(iterations, buffer);
    end = uarch_nanoseconds();
    result.throughput_cycles = ns_to_cycles(end - start);
    
    return result;
}

uint64_t measure_latency_only(
    void (*func)(uint64_t),
    uint64_t iterations
) {
    warmup(func, iterations / 100);
    
    uint64_t start = uarch_nanoseconds();
    func(iterations);
    uint64_t end = uarch_nanoseconds();
    return ns_to_cycles(end - start);
}
