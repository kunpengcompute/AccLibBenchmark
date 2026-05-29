#pragma once

#include <atomic>
#include <cstdint>
#include "gen-cpp2/PerfTestService.h"
#include "gen-cpp2/UniversalService.h"
#include "util/LatencyStats.h"

namespace press_test {

class PerfTestServiceHandler
    : virtual public apache::thrift::ServiceHandler<PerfTestService> {
 public:
  void test(
      PerfTestResponse& response,
      std::unique_ptr<Request> req) override;

 private:
  static std::atomic<uint64_t> g_last_resource_update_time;
  static constexpr uint64_t CPU_UPDATE_INTERVAL_US = 100000;  // 100ms
};

} // namespace press_test


namespace universal {

class UniversalServiceHandler
    : virtual public apache::thrift::ServiceHandler<UniversalService> {
 public:
  void test(
      UniversalResponse& response,
      std::unique_ptr<UniversalRequest> request) override;

 private:
  static std::atomic<uint64_t> g_last_resource_update_time;
  static constexpr uint64_t CPU_UPDATE_INTERVAL_US = 100000;
};

} // namespace universal