#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define PAGE_SIZE 4096
#define DEFAULT_ITERATIONS 10000000
#define DEFAULT_OFFSET (PAGE_SIZE - 1)

static inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void test_page_boundary_load_normal(void *base, uint64_t offset, uint64_t iterations) {
    char *addr = (char *)base + offset;
    uint64_t result = 0;

    uint64_t start = get_ns();

    for (uint64_t i = 0; i < iterations; i++) {
        __asm__ volatile (
            "ldr %[res], [%[addr]]\n"
            : [res] "=r" (result)
            : [addr] "r" (addr)
            : "memory"
        );
    }

    uint64_t end = get_ns();
    printf("\nNormal ldr at offset %lu:\n", offset);
    printf("  Total time: %lu ns\n", end - start);
    printf("  Per iteration: %.2f ns\n", (double)(end - start) / iterations);
    printf("  Result (prevent optimization): %lu\n", result);
}

void test_page_boundary_load_sve_whilelo(void *base, uint64_t offset, uint64_t iterations) {
    char *addr = (char *)base + offset;
    uint64_t result = 0;
    uint64_t limit = 8;
    uint64_t start = get_ns();

    for (uint64_t i = 0; i < iterations; i++) {
        __asm__ volatile (
            "whilelo p0.b, xzr, %[limit]\n"
            "ld1b {z0.b}, p0/z, [%[addr]]\n"
            : [res] "=r" (result)
            : [addr] "r" (addr), [limit] "r" (limit)
            : "p0", "z0", "memory"
        );
    }

    uint64_t end = get_ns();
    printf("\nSVE load with whilelo mask at offset %lu:\n", offset);
    printf("  Total time: %lu ns\n", end - start);
    printf("  Per iteration: %.2f ns\n", (double)(end - start) / iterations);
    printf("  Result (prevent optimization): %lu\n", result);
}

void test_page_boundary_load_sve_whilelo_start(void *base, uint64_t offset, uint64_t iterations) {
    char *addr = (char *)base;
    uint64_t result = 0;
    uint64_t limit = 8;
    uint64_t start = get_ns();

    for (uint64_t i = 0; i < iterations; i++) {
        __asm__ volatile (
            "whilelo p0.b, xzr, %[limit]\n"
            "ld1b {z0.b}, p0/z, [%[addr]]\n"
            : [res] "=r" (result)
            : [addr] "r" (addr), [limit] "r" (limit)
            : "p0", "z0", "memory"
        );
    }

    uint64_t end = get_ns();
    printf("\nSVE load with whilelo mask at offset %lu:\n", offset);
    printf("  Total time: %lu ns\n", end - start);
    printf("  Per iteration: %.2f ns\n", (double)(end - start) / iterations);
    printf("  Result (prevent optimization): %lu\n", result);
}

void print_usage(const char *prog) {
    printf("Usage: %s [iterations] [offset]\n", prog);
    printf("  iterations: number of test iterations (default: %d)\n", DEFAULT_ITERATIONS);
    printf("  offset:     offset from page boundary (default: %d, i.e., PAGE_SIZE-1)\n", DEFAULT_OFFSET);
    printf("\nExamples:\n");
    printf("  %s                      # use defaults\n", prog);
    printf("  %s 5000000              # 5M iterations, default offset\n", prog);
    printf("  %s 10000000 4095        # 10M iterations, offset 4095\n", prog);
    printf("  %s 10000000 4096        # 10M iterations, offset 4096 (cross page)\n", prog);
}

int main(int argc, char *argv[]) {
    uint64_t iterations = DEFAULT_ITERATIONS;
    uint64_t offset = DEFAULT_OFFSET;

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 1) {
        iterations = strtoull(argv[1], NULL, 10);
    }
    if (argc > 2) {
        offset = strtoull(argv[2], NULL, 10);
    }

    void *base = NULL;
    if (posix_memalign(&base, PAGE_SIZE, PAGE_SIZE * 2) != 0) {
        fprintf(stderr, "Failed to allocate aligned memory\n");
        return 1;
    }

    memset(base, 0xAA, PAGE_SIZE * 2);

    printf("=== Page Boundary Load Performance Test ===\n");
    printf("Page size: %d bytes\n", PAGE_SIZE);
    printf("Iterations: %lu\n", iterations);
    printf("Test offset: %lu (from base)\n", offset);
    printf("Base address: %p\n", base);
    printf("Page boundary address: %p\n", (char *)base + PAGE_SIZE);
    printf("Test address: %p\n", (char *)base + offset);

    if (offset >= PAGE_SIZE) {
        printf("  -> Address crosses into second page!\n");
    } else if (offset == PAGE_SIZE - 1) {
        printf("  -> Address at last byte of first page\n");
    }

    test_page_boundary_load_normal(base, offset, iterations);
    // test_page_boundary_load_sve_whilelo(base, offset, iterations);
    // test_page_boundary_load_sve_whilelo_start(base, offset, iterations);
    free(base);
    return 0;
}
