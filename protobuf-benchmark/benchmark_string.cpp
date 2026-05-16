#include "benchmark_common.h"

// --- Serialization Benchmark ---
static void BM_StrSerialization(benchmark::State &state) {
    auto data_size = state.range(0);
    auto scene = static_cast<Scene>(state.range(1));

    std::string content = GenerateFlexibleData(data_size, scene);
    pb::test::Person test_data = makeStrData(content);

    std::string serialized_data;
    for (auto _ : state) {
        serialized_data.clear();
        test_data.SerializeToString(&serialized_data);
        benchmark::DoNotOptimize(serialized_data);
    }
    state.SetBytesProcessed(state.iterations() * data_size);
}

// --- Deserialization Benchmark ---
static void BM_StrDeserialization(benchmark::State &state) {
    auto data_size = state.range(0);
    auto scene = static_cast<Scene>(state.range(1));

    std::string content = GenerateFlexibleData(data_size, scene);
    pb::test::Person test_data = makeStrData(content);

    std::string serialized_data;
    test_data.SerializeToString(&serialized_data);

    pb::test::Person test_data2;
    for (auto _ : state) {
        test_data2.Clear();
        test_data2.ParseFromString(serialized_data);
        benchmark::DoNotOptimize(test_data2);
    }
    state.SetBytesProcessed(state.iterations() * serialized_data.size());
}

// --- 注册参数：覆盖 12 个长度和 3 个场景 ---
void CustomArguments(benchmark::Benchmark *b) {
    std::vector<int64_t> lengths = {4, 8, 16, 32, 64, 128, 160, 256, 512, 1024, 1048576, 4194304};
    for (auto len : lengths) {
        for (int scene = 0; scene <= 2; ++scene) {
            b->Args({len, scene});
        }
    }
}

BENCHMARK(BM_StrSerialization)->Apply(CustomArguments)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_StrDeserialization)->Apply(CustomArguments)->Unit(benchmark::kNanosecond);
