#pragma once

#define SONIC_ARCH_ARM 1
#include "sonic/sonic.h"
#include <thrift/lib/cpp/protocol/TBase64Utils.h>
#include <string>
#include <string_view>
#include <stdexcept>
#include "gen-cpp2/press_types.h"

namespace press_test {

// ═══════════ Serialize: C++ struct → sonic-cpp DOM → JSON string ═══════════

static std::string sonicSerializeRequest(const Request& req) {
    sonic_json::Document doc;
    auto& alloc = doc.GetAllocator();
    doc.SetObject();

    {
        sonic_json::Node inner;
        inner.SetObject();
        inner.AddMember("tf", sonic_json::Node(
            static_cast<int64_t>(*req.echo_attachment() ? 1 : 0)), alloc);
        doc.AddMember("1", std::move(inner), alloc);
    }

    {
        sonic_json::Node inner;
        inner.SetObject();
        if (req.payload().has_value()) {
            auto b64 = apache::thrift::protocol::base64Encode(folly::ByteRange(
                reinterpret_cast<const uint8_t*>(req.payload()->data()),
                req.payload()->size()));
            while (!b64.empty() && b64.back() == '=') b64.pop_back();
            inner.AddMember("str", sonic_json::Node(b64, alloc), alloc);
        } else {
            inner.AddMember("str", sonic_json::Node("", alloc), alloc);
        }
        doc.AddMember("2", std::move(inner), alloc);
    }

    sonic_json::WriteBuffer wb;
    doc.Serialize(wb);
    return wb.ToString();
}

static std::string sonicSerializeResponse(const PerfTestResponse& resp) {
    sonic_json::Document doc;
    auto& alloc = doc.GetAllocator();
    doc.SetObject();

    {
        sonic_json::Node inner;
        inner.SetObject();
        inner.AddMember("dbl", sonic_json::Node(*resp.cpu_usage()), alloc);
        doc.AddMember("0", std::move(inner), alloc);
    }
    {
        sonic_json::Node inner;
        inner.SetObject();
        inner.AddMember("dbl", sonic_json::Node(*resp.mem_usage()), alloc);
        doc.AddMember("1", std::move(inner), alloc);
    }
    {
        sonic_json::Node inner;
        inner.SetObject();
        if (resp.payload().has_value()) {
            auto b64 = apache::thrift::protocol::base64Encode(folly::ByteRange(
                reinterpret_cast<const uint8_t*>(resp.payload()->data()),
                resp.payload()->size()));
            inner.AddMember("str", sonic_json::Node(b64, alloc), alloc);
        } else {
            inner.AddMember("str", sonic_json::Node("", alloc), alloc);
        }
        doc.AddMember("2", std::move(inner), alloc);
    }

    sonic_json::WriteBuffer wb;
    doc.Serialize(wb);
    return wb.ToString();
}

// ═══════════ Deserialize ═══════════

static Request sonicDeserializeRequest(std::string_view json) {
    sonic_json::Document doc;
    doc.Parse(std::string(json.data(), json.size()));
    if (doc.HasParseError())
        throw std::runtime_error("SonicJsonCodec parse error: " +
            std::to_string(static_cast<int>(doc.GetParseError())));

    Request req;
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        std::string key(it->name.GetString());
        auto& inner = it->value;
        auto jt = inner.MemberBegin();
        if (key == "1") {
            req.echo_attachment() = (jt->value.GetInt64() != 0);
        } else if (key == "2") {
            auto b64 = std::string(jt->value.GetString());
            auto decoded = apache::thrift::protocol::base64Decode(b64);
            req.payload() = decoded->moveToFbString().toStdString();
        }
    }
    return req;
}

static PerfTestResponse sonicDeserializeResponse(std::string_view json) {
    sonic_json::Document doc;
    doc.Parse(std::string(json.data(), json.size()));
    if (doc.HasParseError())
        throw std::runtime_error("SonicJsonCodec parse error: " +
            std::to_string(static_cast<int>(doc.GetParseError())));

    PerfTestResponse resp;
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        std::string key(it->name.GetString());
        auto& inner = it->value;
        auto jt = inner.MemberBegin();
        if (key == "0")
            resp.cpu_usage() = jt->value.GetDouble();
        else if (key == "1")
            resp.mem_usage() = jt->value.GetDouble();
        else if (key == "2") {
            auto b64 = std::string(jt->value.GetString());
            auto decoded = apache::thrift::protocol::base64Decode(b64);
            resp.payload() = decoded->moveToFbString().toStdString();
        }
    }
    return resp;
}

// ═══════════ Public API ═══════════

struct SonicJsonCodec {
    static std::string serialize(const Request& req) {
        return sonicSerializeRequest(req);
    }
    static std::string serialize(const PerfTestResponse& resp) {
        return sonicSerializeResponse(resp);
    }
    static Request deserializeRequest(std::string_view json) {
        return sonicDeserializeRequest(json);
    }
    static PerfTestResponse deserializeResponse(std::string_view json) {
        return sonicDeserializeResponse(json);
    }
};

} // namespace press_test
