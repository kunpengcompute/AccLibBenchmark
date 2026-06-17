#include "benchmark_common.h"

// Repeated type benchmarks with packed=true
// Test sizes: 10, 100, 1000, 10000

#define DEFINE_REPEATED_SERIALIZE_BENCHMARK(Type, TypeName) \
    static void BM_Serialize_Repeated_##TypeName(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::Repeated##TypeName input = genRepeated##TypeName(size); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        const int64_t total_bytes = size * sizeof(Type); \
        for (auto _ : state) { \
            size_t byte_size = input.ByteSizeLong(); \
            input.SerializeToArray(buffer, byte_size); \
            benchmark::DoNotOptimize(buffer); \
        } \
        state.SetBytesProcessed(state.iterations() * total_bytes); \
    } \
    BENCHMARK(BM_Serialize_Repeated_##TypeName)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

#define DEFINE_REPEATED_DESERIALIZE_BENCHMARK(Type, TypeName) \
    static void BM_Deserialize_Repeated_##TypeName(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::Repeated##TypeName input = genRepeated##TypeName(size); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        size_t byte_size = input.ByteSizeLong(); \
        input.SerializeToArray(buffer, byte_size); \
        const int64_t total_bytes = size * sizeof(Type); \
        for (auto _ : state) { \
            pb::test::Repeated##TypeName output; \
            output.ParseFromArray(buffer, byte_size); \
            benchmark::DoNotOptimize(output); \
        } \
        state.SetBytesProcessed(state.iterations() * total_bytes); \
    } \
    BENCHMARK(BM_Deserialize_Repeated_##TypeName)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

// 14. repeated int32
DEFINE_REPEATED_SERIALIZE_BENCHMARK(int32_t, Int32)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(int32_t, Int32)

// 15. repeated uint32
DEFINE_REPEATED_SERIALIZE_BENCHMARK(uint32_t, UInt32)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(uint32_t, UInt32)

// 16. repeated sint32
DEFINE_REPEATED_SERIALIZE_BENCHMARK(int32_t, SInt32)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(int32_t, SInt32)

// 17. repeated int64
DEFINE_REPEATED_SERIALIZE_BENCHMARK(int64_t, Int64)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(int64_t, Int64)

// 18. repeated uint64
DEFINE_REPEATED_SERIALIZE_BENCHMARK(uint64_t, UInt64)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(uint64_t, UInt64)

// 19. repeated sint64
DEFINE_REPEATED_SERIALIZE_BENCHMARK(int64_t, SInt64)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(int64_t, SInt64)

// 20. repeated double
DEFINE_REPEATED_SERIALIZE_BENCHMARK(double, Double)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(double, Double)

// 21. repeated float
DEFINE_REPEATED_SERIALIZE_BENCHMARK(float, Float)
DEFINE_REPEATED_DESERIALIZE_BENCHMARK(float, Float)
