#include "benchmark_common.h"

// String and bytes benchmarks
// Test sizes: 10, 100, 1000, 10000

#define DEFINE_STRING_SERIALIZE_BENCHMARK(SceneName, scene) \
    static void BM_Serialize_String_##SceneName(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::Person input = makeStringData(size, scene); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        for (auto _ : state) { \
            size_t byte_size = input.ByteSizeLong(); \
            input.SerializeToArray(buffer, byte_size); \
            benchmark::DoNotOptimize(buffer); \
        } \
        state.SetBytesProcessed(state.iterations() * size); \
    } \
    BENCHMARK(BM_Serialize_String_##SceneName)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

#define DEFINE_STRING_DESERIALIZE_BENCHMARK(SceneName, scene) \
    static void BM_Deserialize_String_##SceneName(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::Person input = makeStringData(size, scene); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        size_t byte_size = input.ByteSizeLong(); \
        input.SerializeToArray(buffer, byte_size); \
        for (auto _ : state) { \
            pb::test::Person output; \
            output.ParseFromArray(buffer, byte_size); \
            benchmark::DoNotOptimize(output); \
        } \
        state.SetBytesProcessed(state.iterations() * byte_size); \
    } \
    BENCHMARK(BM_Deserialize_String_##SceneName)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

// 11. String (Pure ASCII)
DEFINE_STRING_SERIALIZE_BENCHMARK(ASCII, Scene::kPureAscii)
DEFINE_STRING_DESERIALIZE_BENCHMARK(ASCII, Scene::kPureAscii)

// 12. String (Chinese)
DEFINE_STRING_SERIALIZE_BENCHMARK(Chinese, Scene::kChinese)
DEFINE_STRING_DESERIALIZE_BENCHMARK(Chinese, Scene::kChinese)

// 13. Bytes
#define DEFINE_BYTES_SERIALIZE_BENCHMARK() \
    static void BM_Serialize_Bytes(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::BytesData input = makeBytesData(size); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        for (auto _ : state) { \
            size_t byte_size = input.ByteSizeLong(); \
            input.SerializeToArray(buffer, byte_size); \
            benchmark::DoNotOptimize(buffer); \
        } \
        state.SetBytesProcessed(state.iterations() * size); \
    } \
    BENCHMARK(BM_Serialize_Bytes)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

#define DEFINE_BYTES_DESERIALIZE_BENCHMARK() \
    static void BM_Deserialize_Bytes(benchmark::State &state) { \
        auto size = state.range(0); \
        pb::test::BytesData input = makeBytesData(size); \
        alignas(64) uint8_t buffer[131072];  /* 128KB buffer, 64-byte aligned */ \
        size_t byte_size = input.ByteSizeLong(); \
        input.SerializeToArray(buffer, byte_size); \
        for (auto _ : state) { \
            pb::test::BytesData output; \
            output.ParseFromArray(buffer, byte_size); \
            benchmark::DoNotOptimize(output); \
        } \
        state.SetBytesProcessed(state.iterations() * size); \
    } \
    BENCHMARK(BM_Deserialize_Bytes)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

DEFINE_BYTES_SERIALIZE_BENCHMARK()
DEFINE_BYTES_DESERIALIZE_BENCHMARK()
