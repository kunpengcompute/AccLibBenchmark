#include "cpu_freq.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

extern void bench_add_latency(uint64_t iterations);

double measure_cpu_frequency(void) {
    const uint64_t test_iterations = 100000000;
    const int warmup_runs = 3;
    const int measure_runs = 5;
    const uint64_t instructions_per_run = test_iterations * 32;
    
    for (int i = 0; i < warmup_runs; i++) {
        bench_add_latency(test_iterations / 10);
    }
    
    double total_ghz = 0.0;
    int valid_runs = 0;
    
    for (int i = 0; i < measure_runs; i++) {
        uint64_t start_ns = uarch_nanoseconds();
        bench_add_latency(test_iterations);
        uint64_t end_ns = uarch_nanoseconds();
        
        uint64_t elapsed_ns = end_ns - start_ns;
        
        if (elapsed_ns > 0) {
            double ghz = (double)instructions_per_run / (double)elapsed_ns;
            total_ghz += ghz;
            valid_runs++;
        }
    }
    
    if (valid_runs > 0) {
        return total_ghz / valid_runs;
    }
    
    return 2.4;
}

void get_cpu_info(char* buf, size_t size) {
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        strncpy(buf, "Unable to read CPU info", size - 1);
        return;
    }
    
    buf[0] = '\0';
    char line[256];
    int line_count = 0;
    const int max_lines = 10;
    
    while (fgets(line, sizeof(line), fp) && line_count < max_lines) {
        if (strncmp(line, "processor", 9) == 0 ||
            strncmp(line, "model name", 10) == 0 ||
            strncmp(line, "CPU implementer", 15) == 0 ||
            strncmp(line, "CPU architecture", 16) == 0 ||
            strncmp(line, "CPU variant", 11) == 0 ||
            strncmp(line, "CPU part", 8) == 0 ||
            strncmp(line, "CPU revision", 12) == 0 ||
            strncmp(line, "BogoMIPS", 8) == 0) {
            strncat(buf, line, size - strlen(buf) - 1);
            line_count++;
        }
    }
    fclose(fp);
}
