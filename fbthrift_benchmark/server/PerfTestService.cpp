#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include "server/PerfTestService.h"
#include "util/ResourceMonitor.h"
#include "gen-cpp2/press.pb.h"

namespace press_test {

std::atomic<uint64_t> PerfTestServiceHandler::g_last_resource_update_time{0};

// ============ Handler 实现 ============

void PerfTestServiceHandler::native_test(PerfTestResponse &response,
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

struct alignas(64) UnivStatsSlot {
  util::HistogramStats<int64_t> serialize{100, 0, 5000000};
  util::HistogramStats<int64_t> deserialize{100, 0, 5000000};
};

static std::vector<std::unique_ptr<UnivStatsSlot>> g_univ_stats_slots;
static thread_local int g_univ_slot_idx = -1;

void initUnivStats(int num_threads) {
  g_univ_stats_slots.clear();
  int n = num_threads > 0 ? num_threads : 1;
  for (int i = 0; i < n; ++i) {
    g_univ_stats_slots.push_back(std::make_unique<UnivStatsSlot>());
  }
}

static int get_univ_slot_idx() {
  if (g_univ_slot_idx < 0) {
    static std::atomic<int> next_idx{0};
    int idx = next_idx.fetch_add(1, std::memory_order_relaxed);
    CHECK(idx < static_cast<int>(g_univ_stats_slots.size()))
        << "More threads than stats slots: " << idx + 1
        << " threads vs " << g_univ_stats_slots.size() << " slots";
    g_univ_slot_idx = idx;
  }
  return g_univ_slot_idx;
}

ServerUnivStats getServerUnivStats() {
  util::HistogramStats<int64_t> total_ser{100, 0, 5000000};
  util::HistogramStats<int64_t> total_deser{100, 0, 5000000};
  int num_slots = static_cast<int>(g_univ_stats_slots.size());
  for (int i = 0; i < num_slots; ++i) {
    total_ser.merge(g_univ_stats_slots[i]->serialize);
    total_deser.merge(g_univ_stats_slots[i]->deserialize);
  }
  return {
    total_ser.getAvg(),
    total_ser.getPercentile(0.50),
    total_ser.getPercentile(0.90),
    total_ser.getPercentile(0.99),
    total_ser.getPercentile(0.999),
    total_deser.getAvg(),
    total_deser.getPercentile(0.50),
    total_deser.getPercentile(0.90),
    total_deser.getPercentile(0.99),
    total_deser.getPercentile(0.999),
  };
}

void UniversalServiceHandler::universal_test(
    UniversalResponse& response,
    std::unique_ptr<UniversalRequest> request)
{
  switch (*request->type()) {
    case SerializationType::PROTOBUF: {
      press_test_proto::Request pb_req;
      const auto deser_start  = std::chrono::steady_clock::now();
      if (!pb_req.ParseFromString(*request->payload())) {
        LOG(ERROR) << "Failed to parse protobuf request";
        *response.type() = SerializationType::PROTOBUF;
        *response.payload() = std::string();
        return;
      }
      const auto deser_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - deser_start ).count();
      g_univ_stats_slots[get_univ_slot_idx()]->deserialize.addValue(deser_us);

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
      const auto ser_start = std::chrono::steady_clock::now();
      *response.payload() = pb_resp.SerializeAsString();
      const auto ser_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - ser_start).count();
      g_univ_stats_slots[get_univ_slot_idx()]->serialize.addValue(ser_us);
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