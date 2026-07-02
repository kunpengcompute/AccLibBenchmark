#pragma once

#include <simdjson.h>
#include <thrift/lib/cpp/protocol/TBase64Utils.h>
#include <string>
#include <string_view>
#include <stdexcept>
#include "gen-cpp2/press_types.h"

namespace press_test {

using Fmt = simdjson::internal::mini_formatter;

// ═══════════ Serialize: C++ struct → mini_formatter → JSON string ═══════════

namespace detail {
    

// Helper: start writing an object { ... }
inline void obj_begin(Fmt& f, bool& first) {
    f.start_object();
    first = true;
}
inline void obj_end(Fmt& f) {
    f.end_object();
}

// Helper: start writing an array [ ... ]
inline void arr_begin(Fmt& f, bool& first) {
    f.start_array();
    first = true;
}
inline void arr_end(Fmt& f) {
    f.end_array();
}

// Helper: write a key-value pair with comma management
inline void comma(Fmt& f, bool& first) {
    if (!first) f.comma();
    first = false;
}

inline void writeKeyValue(Fmt& f, bool& first, const char* key, const char* typeTag, auto&& writeVal) {
    comma(f, first);
    f.key(key);
    f.start_object();
    f.key(typeTag);
    writeVal();
    f.end_object();
}

inline void writeRequest(Fmt& f, const Request& req) {
    bool first;
    obj_begin(f, first);

    writeKeyValue(f, first, "1", "tf", [&]() {
        f.number(static_cast<int64_t>(*req.echo_attachment() ? 1 : 0));
    });

    writeKeyValue(f, first, "2", "str", [&]() {
        if (req.payload().has_value() && *req.payload()) {
            auto& buf = *req.payload();
            std::string b64;
            if (buf->isChained()) {
                auto coalesced = buf->cloneCoalesced();
                b64 = apache::thrift::protocol::base64Encode(
                    folly::ByteRange(coalesced->data(), coalesced->length()));
            } else {
                b64 = apache::thrift::protocol::base64Encode(
                    folly::ByteRange(buf->data(), buf->length()));
            }
            while (!b64.empty() && b64.back() == '=') b64.pop_back();
            f.string(b64);
        } else f.string("");
    });

    obj_end(f);
}

inline void writeResponse(Fmt& f, const PerfTestResponse& resp) {
    bool first;
    obj_begin(f, first);

    writeKeyValue(f, first, "0", "dbl", [&]() {
        f.number(*resp.cpu_usage());
    });

    writeKeyValue(f, first, "1", "dbl", [&]() {
        f.number(*resp.mem_usage());
    });

    writeKeyValue(f, first, "2", "str", [&]() {
        if (resp.payload().has_value() && *resp.payload()) {
            auto& buf = *resp.payload();
            if (buf->isChained()) {
                auto coalesced = buf->cloneCoalesced();
                f.string(apache::thrift::protocol::base64Encode(
                    folly::ByteRange(coalesced->data(), coalesced->length())));
            } else {
                f.string(apache::thrift::protocol::base64Encode(
                    folly::ByteRange(buf->data(), buf->length())));
            }
        } else f.string("");
    });

    obj_end(f);
}

} // namespace detail

// ═══════════ Public API ═══════════

struct SimdJsonCodec {

    static std::string serialize(const Request& req) {
        Fmt fmt;
        detail::writeRequest(fmt, req);
        auto sv = fmt.str();
        return std::string(sv.data(), sv.size());
    }

    static std::string serialize(const PerfTestResponse& resp) {
        Fmt fmt;
        detail::writeResponse(fmt, resp);
        auto sv = fmt.str();
        return std::string(sv.data(), sv.size());
    }

    static Request deserializeRequest(std::string_view json) {
        simdjson::dom::parser parser;
        auto r = parser.parse(std::string_view(json.data(), json.size()));
        if (r.error()) throw std::runtime_error(
            std::string("SimdJsonCodec parse: ") + simdjson::error_message(r.error()));

        auto obj = r.value_unsafe().get_object().value_unsafe();
        Request req;
        for (auto kv : obj) {
            auto io = kv.value.get_object().value_unsafe();
            auto tp = *io.begin();
            if (std::string(kv.key) == "1")
                req.echo_attachment() = (int64_t(tp.value) != 0);
            else if (std::string(kv.key) == "2") {
                auto sv = std::string_view(tp.value);
                req.payload() = apache::thrift::protocol::base64Decode(
                    folly::StringPiece(sv.data(), sv.size()));
            }
        }
        return req;
    }

    static PerfTestResponse deserializeResponse(std::string_view json) {
        simdjson::dom::parser parser;
        auto r = parser.parse(std::string_view(json.data(), json.size()));
        if (r.error()) throw std::runtime_error(
            std::string("SimdJsonCodec parse: ") + simdjson::error_message(r.error()));

        auto obj = r.value_unsafe().get_object().value_unsafe();
        PerfTestResponse resp;
        for (auto kv : obj) {
            auto io = kv.value.get_object().value_unsafe();
            auto tp = *io.begin();
            if (std::string(kv.key) == "0")
                resp.cpu_usage() = double(tp.value);
            else if (std::string(kv.key) == "1")
                resp.mem_usage() = double(tp.value);
            else if (std::string(kv.key) == "2") {
                auto sv = std::string_view(tp.value);
                resp.payload() = apache::thrift::protocol::base64Decode(
                    folly::StringPiece(sv.data(), sv.size()));
            }
        }
        return resp;
    }
};

} // namespace press_test
