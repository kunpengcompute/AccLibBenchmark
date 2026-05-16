#pragma once

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "arr_data32.pb.h"
#include "arr_data64.pb.h"
#include "arr_double.pb.h"
#include "arr_fixed32.pb.h"
#include "arr_fixed64.pb.h"
#include "arr_float.pb.h"
#include "arr_sfixed32.pb.h"
#include "arr_sfixed64.pb.h"
#include "string.pb.h"

inline std::uint64_t genIntRandomUniformBytes(std::uint8_t maxByteNum) {
    static std::random_device rd;
    static std::mt19937 engine(rd());

    std::uniform_int_distribution<int> byteDist(1, maxByteNum);
    int selectedBytes = byteDist(engine);

    std::uint64_t min_val = (selectedBytes == 1) ? 0 : (1ULL << ((selectedBytes - 1) * 7));
    std::uint64_t max_val;

    if (selectedBytes >= 10) {
        max_val = std::numeric_limits<std::uint64_t>::max();
    } else {
        max_val = (1ULL << (selectedBytes * 7)) - 1;
    }

    std::uniform_int_distribution<std::uint64_t> valDist(min_val, max_val);
    return valDist(engine);
}

inline pb::test::ArrDouble genFullRangeDoubleData(std::size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_real_distribution<double> dist(
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::max()
    );

    pb::test::ArrDouble data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

inline pb::test::ArrFloat genFullRangeFloatData(std::size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_real_distribution<float> dist(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max()
    );

    pb::test::ArrFloat data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

inline pb::test::ArrFixed32 genFixed32Data(std::size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_int_distribution<std::uint32_t> dist(
        std::numeric_limits<std::uint32_t>::min(),
        std::numeric_limits<std::uint32_t>::max()
    );

    pb::test::ArrFixed32 data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

inline pb::test::ArrFixed64 genFixed64Data(std::size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_int_distribution<std::uint64_t> dist(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max()
    );

    pb::test::ArrFixed64 data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

inline pb::test::ArrSFixed32 genSFixed32Data(std::size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_int_distribution<std::int32_t> dist(
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()
    );

    pb::test::ArrSFixed32 data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

inline pb::test::ArrSFixed64 genSFixed64Data(std::size_t arrSize) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::uniform_int_distribution<std::int64_t> dist(
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max()
    );

    pb::test::ArrSFixed64 data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        data.add_numbers(dist(engine));
    }
    return data;
}

inline pb::test::ArrData32 genData32(std::size_t arrSize, std::uint8_t byteNum) {
    pb::test::ArrData32 data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(byteNum);
        data.add_numbers(static_cast<std::uint32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))));
    }
    return data;
}

inline pb::test::ArrData64 genData64(std::size_t arrSize, std::uint8_t byteNum) {
    pb::test::ArrData64 data;
    for (std::size_t i = 0; i < arrSize; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(byteNum);
        data.add_numbers(n);
    }
    return data;
}

/* --- Varint Macros --- */
#define DEFINE_INT_SERIALIZE_BENCHMARK(type, size, len)                       \
    static void BM_Serialize_INT##type##_##size##_##len(benchmark::State &state) \
    {                                                                         \
        pb::test::ArrData##type input = genData##type(size, len);             \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint##type##_t); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_INT##type##_##size##_##len);

#define DEFINE_INT_DESERIALIZE_BENCHMARK(type, size, len)                     \
    static void BM_Deserialize_INT##type##_##size##_##len(benchmark::State &state) \
    {                                                                         \
        pb::test::ArrData##type input = genData##type(size, len);             \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint##type##_t); \
        for (auto _ : state) {                                                \
            pb::test::ArrData##type output;                                   \
            output.ParseFromString(serialized);                               \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_INT##type##_##size##_##len);

/* --- Double Macros (Randomized) --- */
#define DEFINE_DOUBLE_SERIALIZE_BENCHMARK(size)                               \
    static void BM_Serialize_Double_##size(benchmark::State &state)           \
    {                                                                         \
        pb::test::ArrDouble input = genFullRangeDoubleData(size);             \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(double); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_Double_##size);

#define DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(size)                             \
    static void BM_Deserialize_Double_##size(benchmark::State &state)         \
    {                                                                         \
        pb::test::ArrDouble input = genFullRangeDoubleData(size);             \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(double); \
        for (auto _ : state) {                                                \
            pb::test::ArrDouble output;                                       \
            output.ParseFromString(serialized);                                \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_Double_##size);

/* --- Float Macros (Randomized) --- */
#define DEFINE_FLOAT_SERIALIZE_BENCHMARK(size)                                \
    static void BM_Serialize_Float_##size(benchmark::State &state)            \
    {                                                                         \
        pb::test::ArrFloat input = genFullRangeFloatData(size);               \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(float); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_Float_##size);

#define DEFINE_FLOAT_DESERIALIZE_BENCHMARK(size)                              \
    static void BM_Deserialize_Float_##size(benchmark::State &state)          \
    {                                                                         \
        pb::test::ArrFloat input = genFullRangeFloatData(size);               \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(float); \
        for (auto _ : state) {                                                \
            pb::test::ArrFloat output;                                        \
            output.ParseFromString(serialized);                                \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_Float_##size);

/* --- Fixed32 Macros --- */
#define DEFINE_FIXED32_SERIALIZE_BENCHMARK(size)                              \
    static void BM_Serialize_Fixed32_##size(benchmark::State &state)          \
    {                                                                         \
        pb::test::ArrFixed32 input = genFixed32Data(size);                    \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint32_t); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_Fixed32_##size);

#define DEFINE_FIXED32_DESERIALIZE_BENCHMARK(size)                            \
    static void BM_Deserialize_Fixed32_##size(benchmark::State &state)        \
    {                                                                         \
        pb::test::ArrFixed32 input = genFixed32Data(size);                    \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint32_t); \
        for (auto _ : state) {                                                \
            pb::test::ArrFixed32 output;                                      \
            output.ParseFromString(serialized);                                \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_Fixed32_##size);

/* --- Fixed64 Macros --- */
#define DEFINE_FIXED64_SERIALIZE_BENCHMARK(size)                              \
    static void BM_Serialize_Fixed64_##size(benchmark::State &state)          \
    {                                                                         \
        pb::test::ArrFixed64 input = genFixed64Data(size);                    \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint64_t); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_Fixed64_##size);

#define DEFINE_FIXED64_DESERIALIZE_BENCHMARK(size)                            \
    static void BM_Deserialize_Fixed64_##size(benchmark::State &state)        \
    {                                                                         \
        pb::test::ArrFixed64 input = genFixed64Data(size);                    \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(uint64_t); \
        for (auto _ : state) {                                                \
            pb::test::ArrFixed64 output;                                      \
            output.ParseFromString(serialized);                                \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_Fixed64_##size);

/* --- SFixed32 Macros --- */
#define DEFINE_SFIXED32_SERIALIZE_BENCHMARK(size)                             \
    static void BM_Serialize_SFixed32_##size(benchmark::State &state)         \
    {                                                                         \
        pb::test::ArrSFixed32 input = genSFixed32Data(size);                  \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(int32_t); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_SFixed32_##size);

#define DEFINE_SFIXED32_DESERIALIZE_BENCHMARK(size)                           \
    static void BM_Deserialize_SFixed32_##size(benchmark::State &state)       \
    {                                                                         \
        pb::test::ArrSFixed32 input = genSFixed32Data(size);                  \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(int32_t); \
        for (auto _ : state) {                                                \
            pb::test::ArrSFixed32 output;                                     \
            output.ParseFromString(serialized);                                \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_SFixed32_##size);

/* --- SFixed64 Macros --- */
#define DEFINE_SFIXED64_SERIALIZE_BENCHMARK(size)                             \
    static void BM_Serialize_SFixed64_##size(benchmark::State &state)         \
    {                                                                         \
        pb::test::ArrSFixed64 input = genSFixed64Data(size);                  \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(int64_t); \
        for (auto _ : state) {                                                \
            std::string serialized;                                           \
            input.SerializeToString(&serialized);                             \
            benchmark::DoNotOptimize(serialized);                             \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Serialize_SFixed64_##size);

#define DEFINE_SFIXED64_DESERIALIZE_BENCHMARK(size)                           \
    static void BM_Deserialize_SFixed64_##size(benchmark::State &state)       \
    {                                                                         \
        pb::test::ArrSFixed64 input = genSFixed64Data(size);                  \
        std::string serialized;                                               \
        input.SerializeToString(&serialized);                                 \
        const int64_t total_bytes = static_cast<int64_t>(size) * sizeof(int64_t); \
        for (auto _ : state) {                                                \
            pb::test::ArrSFixed64 output;                                     \
            output.ParseFromString(serialized);                                \
            benchmark::DoNotOptimize(output);                                 \
        }                                                                     \
        state.SetBytesProcessed(state.iterations() * total_bytes);            \
    }                                                                         \
    BENCHMARK(BM_Deserialize_SFixed64_##size);

inline pb::test::Person makeStrData(std::string data) {
    pb::test::Person person;
    person.set_email(data);
    return person;
}

inline std::string loadTestBuf(int len, std::string cycle) {
    const int c_len = static_cast<int>(cycle.length());
    std::string data;
    while (len >= c_len) {
        data.append(cycle);
        len -= c_len;
    }
    for (int i = 0; i < len; i++) {
        data.append("\x7F");
    }
    return data;
}

enum class Scene { kPureAscii = 0, kPureUtf8 = 1, kMixed = 2 };

inline std::string GenerateFlexibleData(std::size_t target_size, Scene scene) {
    std::string result;
    result.reserve(target_size);

    std::mt19937 gen(42);

    float ascii_ratio = (scene == Scene::kPureAscii) ? 1.0f :
                        (scene == Scene::kPureUtf8)  ? 0.0f : 0.5f;

    std::vector<std::string> utf8_samples = {
        "\xC2\xA2",
        "\xE2\x82\xAC",
        "\xF0\x90\xBF\x80"
    };

    std::uniform_real_distribution<float> dist(0.0, 1.0);

    while (result.size() < target_size) {
        if (dist(gen) < ascii_ratio) {
            result.push_back(static_cast<char>(gen() % 128));
        } else {
            const std::string &sample = utf8_samples[gen() % utf8_samples.size()];
            if (result.size() + sample.size() <= target_size) {
                result.append(sample);
            } else {
                result.push_back(' ');
            }
        }
    }
    return result;
}
