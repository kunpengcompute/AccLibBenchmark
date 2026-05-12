#include <benchmark/benchmark.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>

#include "gen-cpp2/arr_data_types.h"

namespace {

enum class ProtocolKind {
  kCompact,
  kBinary,
};

ProtocolKind readProtocolFromEnv() {
  const char* value = std::getenv("FBTHRIFT_PROTOCOL");
  if (value == nullptr) {
    return ProtocolKind::kCompact;
  }
  std::string protocol(value);
  std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (protocol == "binary") {
    return ProtocolKind::kBinary;
  }
  return ProtocolKind::kCompact;
}

const ProtocolKind kProtocol = readProtocolFromEnv();

template <class T>
std::string serializeData(const T& data) {
  if (kProtocol == ProtocolKind::kBinary) {
    return apache::thrift::BinarySerializer::serialize<std::string>(data);
  }
  return apache::thrift::CompactSerializer::serialize<std::string>(data);
}

template <class T>
void deserializeData(const std::string& encoded, T& data) {
  if (kProtocol == ProtocolKind::kBinary) {
    apache::thrift::BinarySerializer::deserialize(encoded, data);
    return;
  }
  apache::thrift::CompactSerializer::deserialize(encoded, data);
}

uint64_t genValueByVarintBytes(uint8_t bytes) {
  static thread_local std::mt19937_64 engine(std::random_device{}());
  if (bytes == 1) {
    std::uniform_int_distribution<uint64_t> dist(0, (1ULL << 7) - 1);
    return dist(engine);
  }
  const uint64_t lower = 1ULL << ((bytes - 1) * 7);
  if (bytes >= 10) {
    std::uniform_int_distribution<uint64_t> dist(lower, std::numeric_limits<uint64_t>::max());
    return dist(engine);
  }
  const uint64_t upper = (1ULL << (bytes * 7)) - 1;
  std::uniform_int_distribution<uint64_t> dist(lower, upper);
  return dist(engine);
}

int32_t uint32BitsToInt32(uint32_t v) {
  int32_t out = 0;
  std::memcpy(&out, &v, sizeof(out));
  return out;
}

int64_t uint64BitsToInt64(uint64_t v) {
  int64_t out = 0;
  std::memcpy(&out, &v, sizeof(out));
  return out;
}

uint32_t genVarint32LikeUnsignedValue() {
  static thread_local std::mt19937 engine(std::random_device{}());
  // Pick 1..5 bytes uniformly: each bucket is 20%.
  std::uniform_int_distribution<int> bytesDist(1, 5);
  const uint8_t bytes = static_cast<uint8_t>(bytesDist(engine));
  if (bytes == 1) {
    std::uniform_int_distribution<uint32_t> dist(0U, (1U << 7) - 1);
    return dist(engine);
  }
  const uint32_t lower = 1U << ((bytes - 1) * 7);
  const uint32_t upper = (bytes == 5)
      ? std::numeric_limits<uint32_t>::max()
      : ((1U << (bytes * 7)) - 1);
  std::uniform_int_distribution<uint32_t> dist(lower, upper);
  return dist(engine);
}

int32_t genVarint32LikeValue() {
  return uint32BitsToInt32(genVarint32LikeUnsignedValue());
}

int64_t genVarint64LikeValue() {
  static thread_local std::mt19937 engine(std::random_device{}());
  // Pick 1..10 bytes uniformly for varint64.
  std::uniform_int_distribution<int> bytesDist(1, 10);
  const uint8_t bytes = static_cast<uint8_t>(bytesDist(engine));
  const uint64_t v = genValueByVarintBytes(bytes);
  return uint64BitsToInt64(v);
}

double genDouble() {
  static thread_local std::mt19937 engine(std::random_device{}());
  std::uniform_real_distribution<double> dist(-1000000.0, 1000000.0);
  return dist(engine);
}

std::string genString(size_t baseLength) {
  static thread_local std::mt19937 engine(std::random_device{}());
  std::uniform_int_distribution<int> charDist(0, 25);
  std::uniform_int_distribution<size_t> lenDist(
      std::max<size_t>(1, baseLength / 2),
      std::max<size_t>(2, baseLength * 2));
  size_t len = lenDist(engine);
  std::string result;
  result.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    result.push_back(static_cast<char>('a' + charDist(engine)));
  }
  return result;
}

benchmark::fbthrift::Varint32Data genVarint32Data(size_t arrSize) {
  benchmark::fbthrift::Varint32Data data;
  auto& values = *data.numbers_ref();
  values.reserve(arrSize);
  for (size_t i = 0; i < arrSize; ++i) {
    values.push_back(genVarint32LikeValue());
  }
  return data;
}

benchmark::fbthrift::Varint64Data genVarint64Data(size_t arrSize) {
  benchmark::fbthrift::Varint64Data data;
  auto& values = *data.numbers_ref();
  values.reserve(arrSize);
  for (size_t i = 0; i < arrSize; ++i) {
    values.push_back(genVarint64LikeValue());
  }
  return data;
}

benchmark::fbthrift::DoubleData genDoubleData(size_t arrSize) {
  benchmark::fbthrift::DoubleData data;
  auto& values = *data.numbers_ref();
  values.reserve(arrSize);
  for (size_t i = 0; i < arrSize; ++i) {
    values.push_back(genDouble());
  }
  return data;
}

benchmark::fbthrift::StringData genStringData(size_t arrSize, size_t baseLength) {
  benchmark::fbthrift::StringData data;
  auto& values = *data.numbers_ref();
  values.reserve(arrSize);
  for (size_t i = 0; i < arrSize; ++i) {
    values.push_back(genString(baseLength));
  }
  return data;
}

std::string res;

#define DEFINE_VARINT32_BENCHMARK(size)                                               \
  benchmark::fbthrift::Varint32Data varint32Data##size = genVarint32Data(size);      \
  static void BM_varint32_##size(benchmark::State& state) {                           \
    for (auto _ : state) {                                                             \
      (void)_;                                                                         \
      res = serializeData(varint32Data##size);                                         \
    }                                                                                  \
  }                                                                                    \
  BENCHMARK(BM_varint32_##size);

#define DEFINE_VARINT32_DESERIALIZE_BENCHMARK(size)                                    \
  benchmark::fbthrift::Varint32Data varint32DataDe##size = genVarint32Data(size);     \
  benchmark::fbthrift::Varint32Data varint32DataDe2##size;                             \
  std::string varint32Res##size = serializeData(varint32DataDe##size);                 \
  static void BM_Deserialize_varint32_##size(benchmark::State& state) {                \
    for (auto _ : state) {                                                              \
      (void)_;                                                                          \
      deserializeData(varint32Res##size, varint32DataDe2##size);                       \
    }                                                                                   \
  }                                                                                     \
  BENCHMARK(BM_Deserialize_varint32_##size);

#define DEFINE_VARINT64_BENCHMARK(size)                                               \
  benchmark::fbthrift::Varint64Data varint64Data##size = genVarint64Data(size);      \
  static void BM_varint64_##size(benchmark::State& state) {                           \
    for (auto _ : state) {                                                             \
      (void)_;                                                                         \
      res = serializeData(varint64Data##size);                                         \
    }                                                                                  \
  }                                                                                    \
  BENCHMARK(BM_varint64_##size);

#define DEFINE_VARINT64_DESERIALIZE_BENCHMARK(size)                                    \
  benchmark::fbthrift::Varint64Data varint64DataDe##size = genVarint64Data(size);     \
  benchmark::fbthrift::Varint64Data varint64DataDe2##size;                             \
  std::string varint64Res##size = serializeData(varint64DataDe##size);                 \
  static void BM_Deserialize_varint64_##size(benchmark::State& state) {                \
    for (auto _ : state) {                                                              \
      (void)_;                                                                          \
      deserializeData(varint64Res##size, varint64DataDe2##size);                       \
    }                                                                                   \
  }                                                                                     \
  BENCHMARK(BM_Deserialize_varint64_##size);

#define DEFINE_DOUBLE_BENCHMARK(size)                                                    \
  benchmark::fbthrift::DoubleData doubleData##size = genDoubleData(size);                \
  static void BM_double_##size(benchmark::State& state) {                                \
    for (auto _ : state) {                                                                \
      (void)_;                                                                            \
      res = serializeData(doubleData##size);                                              \
    }                                                                                     \
  }                                                                                       \
  BENCHMARK(BM_double_##size);

#define DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(size)                                         \
  benchmark::fbthrift::DoubleData doubleDataDe##size = genDoubleData(size);               \
  benchmark::fbthrift::DoubleData doubleDataDe2##size;                                    \
  std::string doubleRes##size = serializeData(doubleDataDe##size);                        \
  static void BM_Deserialize_double_##size(benchmark::State& state) {                     \
    for (auto _ : state) {                                                                 \
      (void)_;                                                                             \
      deserializeData(doubleRes##size, doubleDataDe2##size);                              \
    }                                                                                      \
  }                                                                                        \
  BENCHMARK(BM_Deserialize_double_##size);

#define DEFINE_STRING_BENCHMARK(size, baseLen)                                             \
  benchmark::fbthrift::StringData stringData##size##_##baseLen = genStringData(size, baseLen); \
  static void BM_string_##size##_##baseLen(benchmark::State& state) {                      \
    for (auto _ : state) {                                                                  \
      (void)_;                                                                              \
      res = serializeData(stringData##size##_##baseLen);                                   \
    }                                                                                       \
  }                                                                                         \
  BENCHMARK(BM_string_##size##_##baseLen);

#define DEFINE_STRING_DESERIALIZE_BENCHMARK(size, baseLen)                                 \
  benchmark::fbthrift::StringData stringDataDe##size##_##baseLen = genStringData(size, baseLen); \
  benchmark::fbthrift::StringData stringDataDe2##size##_##baseLen;                         \
  std::string stringRes##size##_##baseLen = serializeData(stringDataDe##size##_##baseLen); \
  static void BM_Deserialize_string_##size##_##baseLen(benchmark::State& state) {          \
    for (auto _ : state) {                                                                  \
      (void)_;                                                                              \
      deserializeData(stringRes##size##_##baseLen, stringDataDe2##size##_##baseLen);       \
    }                                                                                       \
  }                                                                                         \
  BENCHMARK(BM_Deserialize_string_##size##_##baseLen);

DEFINE_VARINT32_BENCHMARK(40)
DEFINE_VARINT32_BENCHMARK(100)
DEFINE_VARINT32_BENCHMARK(500)
DEFINE_VARINT32_BENCHMARK(1000)
DEFINE_VARINT32_BENCHMARK(10000)

DEFINE_VARINT32_DESERIALIZE_BENCHMARK(40)
DEFINE_VARINT32_DESERIALIZE_BENCHMARK(100)
DEFINE_VARINT32_DESERIALIZE_BENCHMARK(500)
DEFINE_VARINT32_DESERIALIZE_BENCHMARK(1000)
DEFINE_VARINT32_DESERIALIZE_BENCHMARK(10000)

DEFINE_VARINT64_BENCHMARK(40)
DEFINE_VARINT64_BENCHMARK(100)
DEFINE_VARINT64_BENCHMARK(500)
DEFINE_VARINT64_BENCHMARK(1000)
DEFINE_VARINT64_BENCHMARK(10000)

DEFINE_VARINT64_DESERIALIZE_BENCHMARK(40)
DEFINE_VARINT64_DESERIALIZE_BENCHMARK(100)
DEFINE_VARINT64_DESERIALIZE_BENCHMARK(500)
DEFINE_VARINT64_DESERIALIZE_BENCHMARK(1000)
DEFINE_VARINT64_DESERIALIZE_BENCHMARK(10000)

DEFINE_DOUBLE_BENCHMARK(40)
DEFINE_DOUBLE_BENCHMARK(100)
DEFINE_DOUBLE_BENCHMARK(500)
DEFINE_DOUBLE_BENCHMARK(1000)
DEFINE_DOUBLE_BENCHMARK(10000)

DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(40)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(100)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(500)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(1000)
DEFINE_DOUBLE_DESERIALIZE_BENCHMARK(10000)

DEFINE_STRING_BENCHMARK(40, 8)
DEFINE_STRING_BENCHMARK(100, 12)
DEFINE_STRING_BENCHMARK(500, 16)
DEFINE_STRING_BENCHMARK(1000, 20)
DEFINE_STRING_BENCHMARK(10000, 32)

DEFINE_STRING_DESERIALIZE_BENCHMARK(40, 8)
DEFINE_STRING_DESERIALIZE_BENCHMARK(100, 12)
DEFINE_STRING_DESERIALIZE_BENCHMARK(500, 16)
DEFINE_STRING_DESERIALIZE_BENCHMARK(1000, 20)
DEFINE_STRING_DESERIALIZE_BENCHMARK(10000, 32)

} // namespace

BENCHMARK_MAIN();
