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
  void native_test(
      PerfTestResponse& response,
      std::unique_ptr<Request> req) override;

 private:
  static std::atomic<uint64_t> g_last_resource_update_time;
  static constexpr uint64_t CPU_UPDATE_INTERVAL_US = 100000;  // 100ms
};

} // namespace press_test


namespace universal {

struct ServerUnivStats {
  double serialize_avg;
  int64_t serialize_p50, serialize_p90, serialize_p99, serialize_p999;
  double deserialize_avg;
  int64_t deserialize_p50, deserialize_p90, deserialize_p99, deserialize_p999;
};

ServerUnivStats getServerUnivStats();
void initUnivStats(int num_threads);

class UniversalServiceHandler
    : virtual public apache::thrift::ServiceHandler<UniversalService> {
 public:
  void universal_test(
      UniversalResponse& response,
      std::unique_ptr<UniversalRequest> request) override;

 private:
  static std::atomic<uint64_t> g_last_resource_update_time;
  static constexpr uint64_t CPU_UPDATE_INTERVAL_US = 100000;
};

} // namespace universal