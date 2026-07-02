#pragma once

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "scalar_int32.pb.h"
#include "scalar_uint32.pb.h"
#include "scalar_sint32.pb.h"
#include "scalar_int64.pb.h"
#include "scalar_uint64.pb.h"
#include "scalar_sint64.pb.h"
#include "scalar_bool.pb.h"
#include "scalar_enum.pb.h"
#include "scalar_double.pb.h"
#include "scalar_float.pb.h"
#include "repeated_int32.pb.h"
#include "repeated_uint32.pb.h"
#include "repeated_sint32.pb.h"
#include "repeated_int64.pb.h"
#include "repeated_uint64.pb.h"
#include "repeated_sint64.pb.h"
#include "repeated_double.pb.h"
#include "repeated_float.pb.h"
#include "bytes_data.pb.h"
#include "string.pb.h"

enum class Scene { kPureAscii = 0, kChinese = 1, kMixed = 2 };

inline std::string GenerateFlexibleData(std::size_t target_size, Scene scene) {
    std::string result;
    result.reserve(target_size);

    std::mt19937 gen(42);

    // Chinese common characters pool (常用汉字)
    std::vector<std::string> chinese_chars = {
        "的", "一", "是", "在", "不", "了", "有", "和", "人", "这",
        "中", "大", "为", "上", "个", "国", "我", "以", "要", "他",
        "时", "来", "用", "们", "生", "到", "作", "地", "于", "出",
        "就", "分", "对", "成", "会", "可", "主", "发", "年", "动",
        "同", "工", "也", "能", "下", "过", "子", "说", "产", "种",
        "面", "而", "方", "后", "多", "定", "行", "学", "法", "所",
        "民", "得", "经", "十", "三", "之", "进", "着", "等", "部",
        "度", "家", "电", "力", "里", "如", "水", "化", "高", "自",
        "二", "理", "起", "小", "物", "现", "实", "加", "量", "都",
        "两", "体", "制", "机", "当", "使", "点", "从", "业", "本"
    };

    float ascii_ratio = (scene == Scene::kPureAscii) ? 1.0f :
                        (scene == Scene::kChinese)  ? 0.0f : 0.5f;

    std::uniform_real_distribution<float> dist(0.0, 1.0);

    while (result.size() < target_size) {
        if (dist(gen) < ascii_ratio) {
            result.push_back(static_cast<char>(gen() % 128));
        } else {
            const std::string &sample = chinese_chars[gen() % chinese_chars.size()];
            if (result.size() + sample.size() <= target_size) {
                result.append(sample);
            } else {
                result.push_back(' ');
            }
        }
    }
    return result;
}

// String data generator - one-step function
inline pb::test::Person makeStringData(std::size_t size, Scene scene) {
    std::string content = GenerateFlexibleData(size, scene);
    pb::test::Person person;
    person.set_email(content);
    return person;
}

// Bytes data generator - one-step function
inline pb::test::BytesData makeBytesData(std::size_t size) {
    pb::test::BytesData data;
    std::mt19937 gen(42);
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    std::string bytes;
    bytes.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes.push_back(static_cast<char>(dist(gen)));
    }
    data.add_data(bytes);
    return data;
}

// Generate integer uniformly distributed by varint byte count
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

// Scalar type generators (packed=false)

inline pb::test::ScalarInt32 genScalarInt32(std::size_t count) {
    pb::test::ScalarInt32 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(5);
        data.add_numbers(static_cast<int32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int32_t>::max()))));
    }
    return data;
}

inline pb::test::ScalarUInt32 genScalarUInt32(std::size_t count) {
    pb::test::ScalarUInt32 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(5);
        data.add_numbers(static_cast<uint32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<uint32_t>::max()))));
    }
    return data;
}

inline pb::test::ScalarSInt32 genScalarSInt32(std::size_t count) {
    pb::test::ScalarSInt32 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(5);
        data.add_numbers(static_cast<int32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int32_t>::max()))));
    }
    return data;
}

inline pb::test::ScalarInt64 genScalarInt64(std::size_t count) {
    pb::test::ScalarInt64 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(10);
        data.add_numbers(static_cast<int64_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int64_t>::max()))));
    }
    return data;
}

inline pb::test::ScalarUInt64 genScalarUInt64(std::size_t count) {
    pb::test::ScalarUInt64 data;
    for (std::size_t i = 0; i < count; ++i) {
        data.add_numbers(genIntRandomUniformBytes(10));
    }
    return data;
}

inline pb::test::ScalarSInt64 genScalarSInt64(std::size_t count) {
    pb::test::ScalarSInt64 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(10);
        data.add_numbers(static_cast<int64_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int64_t>::max()))));
    }
    return data;
}

inline pb::test::ScalarBool genScalarBool(std::size_t count) {
    pb::test::ScalarBool data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 1);
    for (std::size_t i = 0; i < count; ++i) {
        data.add_values(dist(gen) == 1);
    }
    return data;
}

inline pb::test::ScalarEnum genScalarEnum(std::size_t count) {
    pb::test::ScalarEnum data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 4);
    for (std::size_t i = 0; i < count; ++i) {
        data.add_values(static_cast<pb::test::TestEnum>(dist(gen)));
    }
    return data;
}

inline pb::test::ScalarDouble genScalarDouble(std::size_t count) {
    pb::test::ScalarDouble data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::max()
    );
    for (std::size_t i = 0; i < count; ++i) {
        data.add_numbers(dist(gen));
    }
    return data;
}

inline pb::test::ScalarFloat genScalarFloat(std::size_t count) {
    pb::test::ScalarFloat data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max()
    );
    for (std::size_t i = 0; i < count; ++i) {
        data.add_numbers(dist(gen));
    }
    return data;
}

// Repeated type generators (packed=true)
inline pb::test::RepeatedInt32 genRepeatedInt32(std::size_t count) {
    pb::test::RepeatedInt32 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(5);
        data.add_numbers(static_cast<int32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int32_t>::max()))));
    }
    return data;
}

inline pb::test::RepeatedUInt32 genRepeatedUInt32(std::size_t count) {
    pb::test::RepeatedUInt32 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(5);
        data.add_numbers(static_cast<uint32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<uint32_t>::max()))));
    }
    return data;
}

inline pb::test::RepeatedSInt32 genRepeatedSInt32(std::size_t count) {
    pb::test::RepeatedSInt32 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(5);
        data.add_numbers(static_cast<int32_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int32_t>::max()))));
    }
    return data;
}

inline pb::test::RepeatedInt64 genRepeatedInt64(std::size_t count) {
    pb::test::RepeatedInt64 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(10);
        data.add_numbers(static_cast<int64_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int64_t>::max()))));
    }
    return data;
}

inline pb::test::RepeatedUInt64 genRepeatedUInt64(std::size_t count) {
    pb::test::RepeatedUInt64 data;
    for (std::size_t i = 0; i < count; ++i) {
        data.add_numbers(genIntRandomUniformBytes(10));
    }
    return data;
}

inline pb::test::RepeatedSInt64 genRepeatedSInt64(std::size_t count) {
    pb::test::RepeatedSInt64 data;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t n = genIntRandomUniformBytes(10);
        data.add_numbers(static_cast<int64_t>(std::min(n, static_cast<std::uint64_t>(std::numeric_limits<int64_t>::max()))));
    }
    return data;
}

inline pb::test::RepeatedDouble genRepeatedDouble(std::size_t count) {
    pb::test::RepeatedDouble data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::max()
    );
    for (std::size_t i = 0; i < count; ++i) {
        data.add_numbers(dist(gen));
    }
    return data;
}

inline pb::test::RepeatedFloat genRepeatedFloat(std::size_t count) {
    pb::test::RepeatedFloat data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::max()
    );
    for (std::size_t i = 0; i < count; ++i) {
        data.add_numbers(dist(gen));
    }
    return data;
}
