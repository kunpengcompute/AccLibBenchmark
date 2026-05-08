#include <benchmark/benchmark.h>
#include <string>
#include <random>
#include <vector>
#include <cstring>
#include <iostream>
#include <limits>
#include "arr_data32.pb.h"
#include "arr_data64.pb.h"
#include "string.pb.h"
#include "arr_double.pb.h"

std::string res;

uint64_t genIntRandomUniformBytes(uint8_t maxByteNum) {
    static std::random_device rd;
    static std::mt19937 engine(rd());

    std::uniform_int_distribution<int> byteDist(1, maxByteNum);
    int selectedBytes = byteDist(engine);

    uint64_t min_val = (selectedBytes == 1) ? 0 : (1ULL << ((selectedBytes - 1) * 7));
    uint64_t max_val;

    if (selectedBytes >= 10) {
        max_val = std::numeric_limits<uint64_t>::max();
    } else {
        max_val = (1ULL << (selectedBytes * 7)) - 1;
    }

    std::uniform_int_distribution<uint64_t> valDist(min_val, max_val);
    return valDist(engine);
}

pb::test::ArrDouble genFullRangeDoubleData(size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_real_distribution<double> dist(
        std::numeric_limits<double>::lowest(), 
        std::numeric_limits<double>::max()
    );

    pb::test::ArrDouble data;
    for (size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

pb::test::ArrData32 genData32(size_t arrSize, uint8_t byteNum) {
    pb::test::ArrData32 data;
    for (size_t i = 0; i < arrSize; ++i) {
        uint64_t n = genIntRandomUniformBytes(byteNum);
        data.add_numbers(static_cast<uint32_t>(std::min(n, (uint64_t)std::numeric_limits<uint32_t>::max())));
    }
    return data;
}

pb::test::ArrData64 genData64(size_t arrSize, uint8_t byteNum) {
    pb::test::ArrData64 data;
    for (size_t i = 0; i < arrSize; ++i) {
        uint64_t n = genIntRandomUniformBytes(byteNum);
        data.add_numbers(n);
    }
    return data;
}

/* --- Varint Macros --- */
#define DEFINE_INT_SERIALIZE_BENCHMARK(type, size, len)                      \
    static void BM_Serialize_INT##type##_##size##_##len(benchmark::State &state) \
    {                                                                        \
        pb::test::ArrData##type input = genData##type(size, len);            \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint##type##_t); \
        for (auto _ : state) {                                               \
            std::string serialized;                                          \
            input.SerializeToString(&serialized);                            \
            benchmark::DoNotOptimize(serialized);                            \
        }                                                                    \
        state.SetBytesProcessed(state.iterations() * total_bytes);           \
    }                                                                        \
    BENCHMARK(BM_Serialize_INT##type##_##size##_##len);

#define DEFINE_INT_DESERIALIZE_BENCHMARK(type, size, len)                    \
    static void BM_Deserialize_INT##type##_##size##_##len(benchmark::State &state) \
    {                                                                        \
        pb::test::ArrData##type input = genData##type(size, len);            \
        std::string serialized;                                              \
        input.SerializeToString(&serialized);                                \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint##type##_t); \
        for (auto _ : state) {                                               \
            pb::test::ArrData##type output;                                  \
            output.ParseFromString(serialized);                              \
            benchmark::DoNotOptimize(output);                                \
        }                                                                    \
        state.SetBytesProcessed(state.iterations() * total_bytes);           \
    }                                                                        \
    BENCHMARK(BM_Deserialize_INT##type##_##size##_##len);

/* --- Double Macros (Randomized) --- */
#define DEFINE_DOUBLE_SERIALIZE_BENCHMARK(size)                              \
    static void BM_Serialize_Double_##size(benchmark::State &state)          \
    {                                                                        \
        pb::test::ArrDouble input = genFullRangeDoubleData(size);            \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(double); \
        for (auto _ : state) {                                               \
            std::string serialized;                                          \
            input.SerializeToString(&serialized);                            \
            benchmark::DoNotOptimize(serialized);                            \
        }                                                                    \
        state.SetBytesProcessed(state.iterations() * total_bytes);           \
    }                                                                        \
    BENCHMARK(BM_Serialize_Double_##size);

#define DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(size)                            \
    static void BM_Deserialize_Double_##size(benchmark::State &state)        \
    {                                                                        \
        pb::test::ArrDouble input = genFullRangeDoubleData(size);            \
        std::string serialized;                                              \
        input.SerializeToString(&serialized);                                \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(double); \
        for (auto _ : state) {                                               \
            pb::test::ArrDouble output;                                      \
            output.ParseFromString(serialized);                              \
            benchmark::DoNotOptimize(output);                                \
        }                                                                    \
        state.SetBytesProcessed(state.iterations() * total_bytes);           \
    }                                                                        \
    BENCHMARK(BM_Deserialize_Double_##size);

/* --- String Utilities --- */
pb::test::Person makeStrData(std::string data) {
    pb::test::Person person;
    person.set_email(data);
    return person;
}

std::string loadTestBuf(int len, std::string cycle) {
    const int c_len = cycle.length();
    std::string data;
    while (len >= c_len) {
        data.append(cycle);
        len -= c_len;
    }
    for (int i = 0; i < len; i ++) data.append("\x7F");
    return data;
}

static void BM_StrSerialization(benchmark::State& state) {
    auto data_size = state.range(0);
    pb::test::Person test_data = makeStrData(loadTestBuf(data_size, "\xF0\x90\xBF\x80"));
    std::string serialized_data;
    for (auto _ : state) {
        test_data.SerializeToString(&serialized_data);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data_size));
}

static void BM_StrDeserialization(benchmark::State& state) {
    auto data_size = state.range(0);
    pb::test::Person test_data = makeStrData(loadTestBuf(data_size, "\xF0\x90\xBF\x80"));
    pb::test::Person test_data2;
    std::string serialized_data;
    test_data.SerializeToString(&serialized_data);
    for (auto _ : state) {
        test_data2.ParseFromString(serialized_data);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data_size));
}

// 1. Varint 32 Serialize
DEFINE_INT_SERIALIZE_BENCHMARK(32, 20, 5)
DEFINE_INT_SERIALIZE_BENCHMARK(32, 40, 5)
DEFINE_INT_SERIALIZE_BENCHMARK(32, 100, 5)
DEFINE_INT_SERIALIZE_BENCHMARK(32, 500, 5)
DEFINE_INT_SERIALIZE_BENCHMARK(32, 1000, 5)
DEFINE_INT_SERIALIZE_BENCHMARK(32, 5000, 5)
DEFINE_INT_SERIALIZE_BENCHMARK(32, 10000, 5)

// 2. Varint 64 Serialize
DEFINE_INT_SERIALIZE_BENCHMARK(64, 20, 10)
DEFINE_INT_SERIALIZE_BENCHMARK(64, 40, 10)
DEFINE_INT_SERIALIZE_BENCHMARK(64, 100, 10)
DEFINE_INT_SERIALIZE_BENCHMARK(64, 500, 10)
DEFINE_INT_SERIALIZE_BENCHMARK(64, 1000, 10)
DEFINE_INT_SERIALIZE_BENCHMARK(64, 5000, 10)
DEFINE_INT_SERIALIZE_BENCHMARK(64, 10000, 10)

// 3. Varint 32 Deserialize
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 20, 5)
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 40, 5)
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 100, 5)
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 500, 5)
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 1000, 5)
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 5000, 5)
DEFINE_INT_DESERIALIZE_BENCHMARK(32, 10000, 5)

// 4. Varint 64 Deserialize
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 20, 10)
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 40, 10)
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 100, 10)
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 500, 10)
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 1000, 10)
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 5000, 10)
DEFINE_INT_DESERIALIZE_BENCHMARK(64, 10000, 10)

// 5. Double Serialize (Randomized)
DEFINE_DOUBLE_SERIALIZE_BENCHMARK(2)
DEFINE_DOUBLE_SERIALIZE_BENCHMARK(8)
DEFINE_DOUBLE_SERIALIZE_BENCHMARK(32)
DEFINE_DOUBLE_SERIALIZE_BENCHMARK(128)
DEFINE_DOUBLE_SERIALIZE_BENCHMARK(256)

// 6. Double Deserialize (Randomized)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(2)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(8)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(32)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(128)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(256)

// 7. String Benchmarks
BENCHMARK(BM_StrSerialization)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64);
BENCHMARK(BM_StrDeserialization)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64);

BENCHMARK_MAIN();