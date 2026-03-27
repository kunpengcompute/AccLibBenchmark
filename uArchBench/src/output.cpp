#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <sys/stat.h>
#include "cpu_freq.h"

static std::ofstream g_result_file;
static bool g_save_to_file = false;

static std::string get_timestamp() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", t);
    return std::string(buf);
}

extern "C" {

void output_init(const std::string& results_dir) {
    mkdir(results_dir.c_str(), 0755);
    
    std::string filename = results_dir + "/benchmark_" + get_timestamp() + ".txt";
    g_result_file.open(filename);
    if (g_result_file.is_open()) {
        g_save_to_file = true;
        std::cout << "Results will be saved to: " << filename << "\n\n";
    }
}

void output_close(void) {
    if (g_save_to_file && g_result_file.is_open()) {
        g_result_file.close();
    }
}

void output_line(const std::string& line) {
    std::cout << line;
    if (g_save_to_file && g_result_file.is_open()) {
        g_result_file << line;
    }
}

void output_separator(void) {
    output_line(std::string(60, '-') + "\n");
}

void output_header(const char* title) {
    char buf[256];
    snprintf(buf, sizeof(buf), "\n--- %s ---\n", title);
    output_line(buf);
    
    snprintf(buf, sizeof(buf), "%-20s%16s%20s\n", "Instruction", "Latency(cycles)", "Throughput(IPC)");
    output_line(buf);
    output_separator();
}

void output_instruction_result(const char* name, double latency, double throughput) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%-20s%16.2f%20.2f\n", name, latency, throughput);
    output_line(buf);
}

void output_system_info(double cpu_freq_ghz, bool is_manual) {
    char buf[1024];
    
    output_line("========================================\n");
    output_line("         uArchBench - Microarchitecture\n");
    output_line("         Instruction Benchmark Suite\n");
    output_line("========================================\n\n");
    
    output_line("--- System Information ---\n");
    output_separator();
    
    snprintf(buf, sizeof(buf), "Timestamp: %s\n", get_timestamp().c_str());
    output_line(buf);
    
    if (is_manual) {
        snprintf(buf, sizeof(buf), "CPU Frequency: %.3f GHz (manually specified)\n", cpu_freq_ghz);
    } else {
        snprintf(buf, sizeof(buf), "CPU Frequency: %.3f GHz (measured)\n", cpu_freq_ghz);
    }
    output_line(buf);
    
    char cpu_info[512] = {0};
    get_cpu_info(cpu_info, sizeof(cpu_info));
    output_line("CPU Info:\n");
    output_line(cpu_info);
    output_line("\n");
    
    output_line("Note: Latency = cycles per instruction (dependency chain)\n");
    output_line("       Throughput(IPC) = instructions per cycle (independent ops)\n");
}

}
