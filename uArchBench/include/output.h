#ifndef UARCH_OUTPUT_H
#define UARCH_OUTPUT_H

#include <stdint.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

void output_init(const std::string& results_dir);
void output_close(void);
void output_line(const std::string& line);
void output_separator(void);
void output_header(const char* title);
void output_instruction_result(const char* name, double latency, double throughput);
void output_system_info(double cpu_freq_ghz, bool is_manual);

#ifdef __cplusplus
}
#endif

#endif
