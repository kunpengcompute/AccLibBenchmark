#pragma once

#include <atomic>
#include <cstdint>
#include "gen-cpp2/PerfTestService.h"
#include "util/LatencyStats.h"

namespace press_test {
void init_resource_collector();
class PerfTestServiceHandler
    : virtual public apache::thrift::ServiceHandler<PerfTestService> {
 public:
  void test(
      PerfTestResponse& response,
      std::unique_ptr<Request> req) override;

 private:
  std::atomic<uint64_t> last_time_{0};
  LatencyStats latency_stats_;
};

} // namespace press_test