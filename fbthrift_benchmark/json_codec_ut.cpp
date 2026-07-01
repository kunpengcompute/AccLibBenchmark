// Unit test for SimdJsonCodec and SonicJsonCodec
// Tests serialize/deserialize roundtrip, cross-codec compatibility, edge cases.
#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <folly/io/IOBuf.h>
#include "SimdJsonCodec.h"
#include "SonicJsonCodec.h"
#include "gen-cpp2/press_types.h"

static int g_passed = 0, g_failed = 0;

#define CHECK(cond, name) do { \
    if (cond) { g_passed++; std::cout << "  [PASS] " << name << std::endl; } \
    else { g_failed++; std::cout << "  [FAIL] " << name << std::endl; } \
} while(0)

// Helper: compare two IOBuf payloads by content
static bool iobufEqual(const std::unique_ptr<folly::IOBuf>& a, const std::unique_ptr<folly::IOBuf>& b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    auto ac = a->cloneCoalesced();
    auto bc = b->cloneCoalesced();
    return ac->length() == bc->length() &&
           std::memcmp(ac->data(), bc->data(), ac->length()) == 0;
}

// ─── SimdJsonCodec Tests ───

void test_simdjson_request_roundtrip() {
    std::cout << "SimdJSON Request roundtrip:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer("hello simdjson test payload!");
    auto json = press_test::SimdJsonCodec::serialize(req);
    auto req2 = press_test::SimdJsonCodec::deserializeRequest(json);
    CHECK(req.echo_attachment() == req2.echo_attachment(), "echo_attachment match");
    CHECK(iobufEqual(*req.payload(), *req2.payload()), "payload match");
}

void test_simdjson_response_roundtrip() {
    std::cout << "SimdJSON Response roundtrip:" << std::endl;
    press_test::PerfTestResponse resp;
    resp.cpu_usage() = 1.5;
    resp.mem_usage() = 2.75;
    resp.payload() = folly::IOBuf::copyBuffer("response data");
    auto json = press_test::SimdJsonCodec::serialize(resp);
    auto resp2 = press_test::SimdJsonCodec::deserializeResponse(json);
    CHECK(*resp.cpu_usage() == *resp2.cpu_usage(), "cpu_usage match");
    CHECK(*resp.mem_usage() == *resp2.mem_usage(), "mem_usage match");
    CHECK(iobufEqual(*resp.payload(), *resp2.payload()), "payload match");
}

void test_simdjson_empty_payload() {
    std::cout << "SimdJSON Empty payload:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer("");
    auto json = press_test::SimdJsonCodec::serialize(req);
    auto req2 = press_test::SimdJsonCodec::deserializeRequest(json);
    CHECK(req2.payload().has_value(), "payload exists");
}

void test_simdjson_large_payload() {
    std::cout << "SimdJSON Large payload (1024B):" << std::endl;
    press_test::Request req;
    req.echo_attachment() = false;
    req.payload() = folly::IOBuf::copyBuffer(std::string(1024, 'X'));
    auto json = press_test::SimdJsonCodec::serialize(req);
    auto req2 = press_test::SimdJsonCodec::deserializeRequest(json);
    CHECK(req.echo_attachment() == req2.echo_attachment(), "echo_attachment(false)");
    CHECK((*req.payload())->length() == (*req2.payload())->length(), "payload size=1024");
}

void test_simdjson_special_chars() {
    std::cout << "SimdJSON Special characters:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer(std::string("\x00\x01\x02\xff\xfe中文\n\t\"\\", 16));
    auto json = press_test::SimdJsonCodec::serialize(req);
    auto req2 = press_test::SimdJsonCodec::deserializeRequest(json);
    CHECK(iobufEqual(*req.payload(), *req2.payload()), "binary special chars preserved");
}

// ─── SonicJsonCodec Tests ───

void test_sonicjson_request_roundtrip() {
    std::cout << "SonicJSON Request roundtrip:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer("hello sonic-cpp test payload!");
    auto json = press_test::SonicJsonCodec::serialize(req);
    auto req2 = press_test::SonicJsonCodec::deserializeRequest(json);
    CHECK(req.echo_attachment() == req2.echo_attachment(), "echo_attachment match");
    CHECK(iobufEqual(*req.payload(), *req2.payload()), "payload match");
}

void test_sonicjson_response_roundtrip() {
    std::cout << "SonicJSON Response roundtrip:" << std::endl;
    press_test::PerfTestResponse resp;
    resp.cpu_usage() = 3.14;
    resp.mem_usage() = 0.001;
    resp.payload() = folly::IOBuf::copyBuffer("sonic response");
    auto json = press_test::SonicJsonCodec::serialize(resp);
    auto resp2 = press_test::SonicJsonCodec::deserializeResponse(json);
    CHECK(*resp.cpu_usage() == *resp2.cpu_usage(), "cpu_usage match");
    CHECK(*resp.mem_usage() == *resp2.mem_usage(), "mem_usage match");
    CHECK(iobufEqual(*resp.payload(), *resp2.payload()), "payload match");
}

void test_sonicjson_empty_payload() {
    std::cout << "SonicJSON Empty payload:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = false;
    req.payload() = folly::IOBuf::copyBuffer("");
    auto json = press_test::SonicJsonCodec::serialize(req);
    auto req2 = press_test::SonicJsonCodec::deserializeRequest(json);
    CHECK(req2.payload().has_value(), "payload exists");
}

void test_sonicjson_large_payload() {
    std::cout << "SonicJSON Large payload (1024B):" << std::endl;
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer(std::string(1024, 'Z'));
    auto json = press_test::SonicJsonCodec::serialize(req);
    auto req2 = press_test::SonicJsonCodec::deserializeRequest(json);
    CHECK((*req.payload())->length() == (*req2.payload())->length(), "payload size=1024");
}

// ─── Cross-Codec Compatibility Tests ───

void test_cross_simd_to_sonic() {
    std::cout << "Cross-codec: SimdJSON serialize -> SonicJSON deserialize:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer("cross-codec test payload");
    auto json = press_test::SimdJsonCodec::serialize(req);
    auto req2 = press_test::SonicJsonCodec::deserializeRequest(json);
    CHECK(req.echo_attachment() == req2.echo_attachment(), "echo_attachment match");
    CHECK(iobufEqual(*req.payload(), *req2.payload()), "payload match");
}

void test_cross_sonic_to_simd() {
    std::cout << "Cross-codec: SonicJSON serialize -> SimdJSON deserialize:" << std::endl;
    press_test::Request req;
    req.echo_attachment() = false;
    req.payload() = folly::IOBuf::copyBuffer("reverse cross-codec test");
    auto json = press_test::SonicJsonCodec::serialize(req);
    auto req2 = press_test::SimdJsonCodec::deserializeRequest(json);
    CHECK(req.echo_attachment() == req2.echo_attachment(), "echo_attachment match");
    CHECK(iobufEqual(*req.payload(), *req2.payload()), "payload match");
}

void test_cross_response() {
    std::cout << "Cross-codec Response:" << std::endl;
    press_test::PerfTestResponse resp;
    resp.cpu_usage() = 0.99;
    resp.mem_usage() = 0.5;
    resp.payload() = folly::IOBuf::copyBuffer("cross response");
    // SimdJSON serialize -> SonicJSON deserialize
    auto json = press_test::SimdJsonCodec::serialize(resp);
    auto resp2 = press_test::SonicJsonCodec::deserializeResponse(json);
    CHECK(*resp.cpu_usage() == *resp2.cpu_usage(), "cpu cross-codec");
    // SonicJSON serialize -> SimdJSON deserialize
    auto json2 = press_test::SonicJsonCodec::serialize(resp);
    auto resp3 = press_test::SimdJsonCodec::deserializeResponse(json2);
    CHECK(iobufEqual(*resp.payload(), *resp3.payload()), "payload cross-codec");
}

// ─── Performance benchmarks (quick) ───

void perf_bench(int n, const char* label,
    std::string (*ser)(const press_test::Request&),
    press_test::Request (*deser)(std::string_view)) {
    press_test::Request req;
    req.echo_attachment() = true;
    req.payload() = folly::IOBuf::copyBuffer(std::string(128, 'A'));
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; i++) { auto j = ser(req); auto r = deser(j); }
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "  " << label << ": " << n << " roundtrips in " << us / 1000.0 << "ms ("
              << us * 1000.0 / n << "ns/op)" << std::endl;
}

// ─── Main ───

int main() {
    std::cout << "=== JsonCodec Unit Tests ===" << std::endl << std::endl;

    // SimdJSON tests
    test_simdjson_request_roundtrip();
    test_simdjson_response_roundtrip();
    test_simdjson_empty_payload();
    test_simdjson_large_payload();
    test_simdjson_special_chars();

    // SonicJSON tests
    test_sonicjson_request_roundtrip();
    test_sonicjson_response_roundtrip();
    test_sonicjson_empty_payload();
    test_sonicjson_large_payload();

    // Cross-codec tests
    test_cross_simd_to_sonic();
    test_cross_sonic_to_simd();
    test_cross_response();

    // Performance
    std::cout << std::endl << "=== Quick Performance ===" << std::endl;
    perf_bench(10000, "SimdJSON",
        press_test::SimdJsonCodec::serialize,
        press_test::SimdJsonCodec::deserializeRequest);
    perf_bench(10000, "SonicJSON",
        press_test::SonicJsonCodec::serialize,
        press_test::SonicJsonCodec::deserializeRequest);

    std::cout << std::endl << "=== Results: " << g_passed << "/" << (g_passed + g_failed) << " passed ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
