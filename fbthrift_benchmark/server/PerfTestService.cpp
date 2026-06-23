#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include "server/PerfTestService.h"
#include "util/ResourceMonitor.h"
#include "SimdJsonCodec.h"
#include "SonicJsonCodec.h"
#include "SonicJsonCodec.h"

namespace press_test {

std::atomic<uint64_t> PerfTestServiceHandler::g_last_resource_update_time{0};

void PerfTestServiceHandler::test(PerfTestResponse &response,
                                  std::unique_ptr<Request> req)
{
  if (*req->echo_attachment()) {
    *response.payload() = *req->payload();
  }
  uint64_t last = g_last_resource_update_time.load(std::memory_order_relaxed);
  uint64_t now = util::ResourceCollector::monotonic_us();
  if (now > last && now - last > CPU_UPDATE_INTERVAL_US) {
    if (g_last_resource_update_time.compare_exchange_weak(
        last, now, std::memory_order_relaxed)) {
      auto snap = util::g_snapshot.load(std::memory_order_acquire);
      *response.cpu_usage() = snap.cpu_percent;
      *response.mem_usage() = snap.mem_percent;
    } else { *response.cpu_usage() = -1.0; *response.mem_usage() = -1.0; }
  } else { *response.cpu_usage() = -1.0; *response.mem_usage() = -1.0; }
}

void PerfTestServiceHandler::simdjson_call(::std::string& out, std::unique_ptr<::std::string> data) {
    auto req = SimdJsonCodec::deserializeRequest(std::string_view(data->data(), data->size()));
    PerfTestResponse resp;
    if (req.echo_attachment().has_value() && *req.echo_attachment())
        resp.payload() = req.payload().has_value() ? req.payload().value() : "";
    auto snap = util::g_snapshot.load(std::memory_order_acquire);
    resp.cpu_usage() = snap.cpu_percent;
    resp.mem_usage() = snap.mem_percent;
    out = SimdJsonCodec::serialize(resp);
}


void PerfTestServiceHandler::sonicjson_call(::std::string& out, std::unique_ptr<::std::string> data) {
    auto req = SonicJsonCodec::deserializeRequest(std::string_view(data->data(), data->size()));
    PerfTestResponse resp;
    if (req.echo_attachment().has_value() && *req.echo_attachment())
        resp.payload() = req.payload().has_value() ? req.payload().value() : "";
    auto snap = util::g_snapshot.load(std::memory_order_acquire);
    resp.cpu_usage() = snap.cpu_percent;
    resp.mem_usage() = snap.mem_percent;
    out = SonicJsonCodec::serialize(resp);
}

} // namespace press_test
