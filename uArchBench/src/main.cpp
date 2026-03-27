#include <iostream>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "benchmarks.h"
#include "measure.h"
#include "output.h"
#include "cpu_freq.h"

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -o <dir>    Output directory for results (default: results)\n");
    printf("  -f <freq>   CPU frequency in GHz (e.g., -f 2.6)\n");
    printf("  -h, --help  Show this help message\n");
}

int main(int argc, char* argv[]) {
    const uint64_t iterations = 10000000;
    const uint64_t ops_per_iter = 32;
    
    std::string results_dir = "results";
    double manual_freq = 0.0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            results_dir = argv[i + 1];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            manual_freq = atof(argv[i + 1]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    output_init(results_dir);
    
    void* buffer = aligned_alloc(64, 128);
    memset(buffer, 0xAA, 128);
    *(void**)buffer = buffer;
    
    double cpu_freq_ghz;
    bool is_manual = false;
    if (manual_freq > 0.0) {
        cpu_freq_ghz = manual_freq;
        is_manual = true;
    } else {
        cpu_freq_ghz = measure_cpu_frequency();
    }
    
    measure_init(cpu_freq_ghz);
    output_system_info(cpu_freq_ghz, is_manual);
    
#ifdef UARCH_ENABLE_INSTRUCTION_BENCH
    output_header("Arithmetic Instructions");
    
    {
        auto r = measure_instruction(bench_add_latency, bench_add_throughput, iterations);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("ADD", latency, throughput);
    }
    {
        auto r = measure_instruction(bench_sub_latency, bench_sub_throughput, iterations);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("SUB", latency, throughput);
    }
    {
        auto r = measure_instruction(bench_mul_latency, bench_mul_throughput, iterations);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("MUL", latency, throughput);
    }
    {
        auto r = measure_instruction(bench_div_latency, bench_div_throughput, iterations);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("DIV", latency, throughput);
    }
    {
        auto r = measure_instruction(bench_mov_latency, bench_mov_throughput, iterations);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("MOV", latency, throughput);
    }
    
    output_header("Memory Instructions");
    
    {
        auto r = measure_memory_instruction(bench_load_latency, bench_load_throughput, iterations, buffer);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("LOAD", latency, throughput);
    }
    {
        auto r = measure_memory_instruction(bench_store_latency, bench_store_throughput, iterations, buffer);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("STORE", latency, throughput);
    }
    
    output_header("Branch Instructions");
    
    {
        auto r = measure_instruction(bench_branch_taken, bench_branch_not_taken, iterations);
        double latency = (double)r.latency_cycles / (double)iterations;
        double throughput = (double)iterations / (double)r.throughput_cycles;
        output_instruction_result("BRANCH (taken)", latency, throughput);
    }
    {
        uint64_t cycles = measure_latency_only(bench_branch_not_taken, iterations);
        double latency = (double)cycles / (double)iterations;
        output_instruction_result("BRANCH (not taken)", latency, latency);
    }
    
    output_header("NEON Instructions");
    
    {
        auto r = measure_memory_instruction(bench_neon_add_latency, bench_neon_add_throughput, iterations, buffer);
        double latency = (double)r.latency_cycles / (double)(ops_per_iter * iterations);
        double throughput = (double)(ops_per_iter * iterations) / (double)r.throughput_cycles;
        output_instruction_result("NEON ADD", latency, throughput);
    }
#endif
    
#ifdef UARCH_ENABLE_MICROARCH
    output_line("\n--- Microarchitecture Tests ---\n");
    output_separator();
    
    output_line("\n--- Memory Tests ---\n");
    output_separator();
    
    measure_store_to_load_forwarding(iterations, cpu_freq_ghz);
    
    output_line("\n--- Pipeline Tests ---\n");
    output_separator();
    
    measure_pipeline_characteristics(1000000);
    
    output_line("\n--- Cache Tests ---\n");
    output_separator();
    
    measure_cache_latency(32 * 1024, 1000000);
    measure_cache_latency(256 * 1024, 1000000);
    measure_cache_latency(8 * 1024 * 1024, 1000000);
    
    output_line("\n");
    measure_cache_bandwidth(32 * 1024, 100);
    measure_cache_bandwidth(256 * 1024, 100);
    
    output_line("\n--- Prefetch Tests ---\n");
    output_separator();
    
    measure_prefetch_effect(8 * 1024 * 1024, 10, 8);
#endif
    
    free(buffer);
    
    output_line("\nDone.\n");
    output_close();
    
    return 0;
}
