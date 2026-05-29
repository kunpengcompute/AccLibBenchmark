#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include "server/PerfTestService.h"
#include "util/ResourceMonitor.h"
#include "gen-cpp2/press.pb.h"

namespace press_test {

std::atomic<uint64_t> PerfTestServiceHandler::g_last_resource_update_time{0};

// ============ Handler 实现 ============

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
    } else {
      *response.cpu_usage() = -1.0;
      *response.mem_usage() = -1.0;
    }
  } else {
    *response.cpu_usage() = -1.0;
    *response.mem_usage() = -1.0;
  }
}

} // namespace press_test


namespace universal {

std::atomic<uint64_t> UniversalServiceHandler::g_last_resource_update_time{0};

void UniversalServiceHandler::test(
    UniversalResponse& response,
    std::unique_ptr<UniversalRequest> request)
{
  switch (*request->type()) {
    case SerializationType::PROTOBUF: {
      press_test_proto::Request pb_req;
      if (!pb_req.ParseFromString(*request->payload())) {
        LOG(ERROR) << "Failed to parse protobuf request";
        *response.type() = SerializationType::PROTOBUF;
        *response.payload() = std::string();
        return;
      }

      press_test_proto::PerfTestResponse pb_resp;
      if (pb_req.echo_attachment()) {
        pb_resp.set_payload(pb_req.payload());
      }

      uint64_t last = g_last_resource_update_time.load(std::memory_order_relaxed);
      uint64_t now = util::ResourceCollector::monotonic_us();

      if (now > last && now - last > CPU_UPDATE_INTERVAL_US) {
        if (g_last_resource_update_time.compare_exchange_weak(
            last, now, std::memory_order_relaxed)) {
          auto snap = util::g_snapshot.load(std::memory_order_acquire);
          pb_resp.set_cpu_usage(std::to_string(snap.cpu_percent));
          pb_resp.set_mem_usage(std::to_string(snap.mem_percent));
        } else {
          pb_resp.set_cpu_usage("-1.0");
          pb_resp.set_mem_usage("-1.0");
        }
      } else {
        pb_resp.set_cpu_usage("-1.0");
        pb_resp.set_mem_usage("-1.0");
      }

      *response.type() = SerializationType::PROTOBUF;
      *response.payload() = pb_resp.SerializeAsString();
      break;
    }
    default: {
      LOG(ERROR) << "Unsupported serialization type: "
                 << static_cast<int>(*request->type());
      *response.type() = *request->type();
      *response.payload() = std::string();
      break;
    }
  }
}

} // namespace universal