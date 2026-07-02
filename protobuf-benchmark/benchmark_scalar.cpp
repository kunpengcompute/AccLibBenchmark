#include "benchmark_common.h"

// Scalar type benchmarks (packed=false)
// Test sizes: 10, 100, 1000, 10000

#define DEFINE_SCALAR_SERIALIZE_BENCHMARK(Type, TypeName) \
    static void BM_Serialize_Scalar_##TypeName(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::Scalar##TypeName input = genScalar##TypeName(size); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        const int64_t total_bytes = size * sizeof(Type); \
        for (auto _ : state) { \
            size_t byte_size = input.ByteSizeLong(); \
            input.SerializeToArray(buffer, byte_size); \
            benchmark::DoNotOptimize(buffer); \
        } \
        state.SetBytesProcessed(state.iterations() * total_bytes); \
    } \
    BENCHMARK(BM_Serialize_Scalar_##TypeName)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

#define DEFINE_SCALAR_DESERIALIZE_BENCHMARK(Type, TypeName) \
    static void BM_Deserialize_Scalar_##TypeName(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::Scalar##TypeName input = genScalar##TypeName(size); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        size_t byte_size = input.ByteSizeLong(); \
        input.SerializeToArray(buffer, byte_size); \
        const int64_t total_bytes = size * sizeof(Type); \
        for (auto _ : state) { \
            pb::test::Scalar##TypeName output; \
            output.ParseFromArray(buffer, byte_size); \
            benchmark::DoNotOptimize(output); \
        } \
        state.SetBytesProcessed(state.iterations() * total_bytes); \
    } \
    BENCHMARK(BM_Deserialize_Scalar_##TypeName)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

// 1. int32
DEFINE_SCALAR_SERIALIZE_BENCHMARK(int32_t, Int32)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(int32_t, Int32)

// 2. uint32
DEFINE_SCALAR_SERIALIZE_BENCHMARK(uint32_t, UInt32)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(uint32_t, UInt32)

// 3. sint32
DEFINE_SCALAR_SERIALIZE_BENCHMARK(int32_t, SInt32)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(int32_t, SInt32)

// 4. int64
DEFINE_SCALAR_SERIALIZE_BENCHMARK(int64_t, Int64)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(int64_t, Int64)

// 5. uint64
DEFINE_SCALAR_SERIALIZE_BENCHMARK(uint64_t, UInt64)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(uint64_t, UInt64)

// 6. sint64
DEFINE_SCALAR_SERIALIZE_BENCHMARK(int64_t, SInt64)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(int64_t, SInt64)

// 7. bool
DEFINE_SCALAR_SERIALIZE_BENCHMARK(bool, Bool)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(bool, Bool)

// 8. enum
DEFINE_SCALAR_SERIALIZE_BENCHMARK(int32_t, Enum)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(int32_t, Enum)

// 9. double
DEFINE_SCALAR_SERIALIZE_BENCHMARK(double, Double)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(double, Double)

// 10. float
DEFINE_SCALAR_SERIALIZE_BENCHMARK(float, Float)
DEFINE_SCALAR_DESERIALIZE_BENCHMARK(float, Float)
