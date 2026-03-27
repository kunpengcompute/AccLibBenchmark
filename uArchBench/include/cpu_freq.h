#ifndef CPU_FREQ_H
#define CPU_FREQ_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

double measure_cpu_frequency(void);
void get_cpu_info(char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif
