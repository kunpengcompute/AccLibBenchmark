// PressTestClient.cpp
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <random>
#include <folly/SocketAddress.h>
#include <folly/init/Init.h>
#include <folly/io/async/EventBase.h>
#include <gflags/gflags.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>
// #include <thrift/lib/cpp2/async/HTTPClientChannel.h>
#include <thrift/lib/cpp2/async/RocketClientChannel.h>

#include "gen-cpp2/PerfTestService.h"
#include "util/LatencyStats.h"

DEFINE_string(host, "127.0.0.1", "Server host");
DEFINE_int32(port, 8002, "Server port");
DEFINE_string(transport, "rocket", "Transport protocol: rocket, header, http2");
DEFINE_int32(thread_num, 0, "Thread num (0 = scan 1~max_thread_num)");
DEFINE_int32(max_thread_num, 16, "Max threads for scan mode");
DEFINE_int32(queue_depth, 1, "In-flight requests per thread");
DEFINE_int32(expected_qps, 0, "Expected QPS (0 = unlimited)");
DEFINE_int32(payload_size, -1, "Payload size (0 = scan 1~1024)");
DEFINE_bool(echo_attachment, false, "Echo payload back");
DEFINE_int32(rpc_timeout_ms, 2000, "RPC timeout in ms");
DEFINE_int32(test_seconds, 20, "Test duration");
DEFINE_int32(test_iterations, 0, "Test iterations (0 = time-based)");
DEFINE_string(compression, "none", "Compression algorithm: none, zlib, zstd");

using PerfTestClient = apache::thrift::Client<press_test::PerfTestService>;

// ============ 全局状态 ============

struct alignas(64) ThreadStats {
  LatencyStats latency;
  uint64_t success{0};
  uint64_t fail{0};
};

std::atomic<bool> g_stop{false};
std::atomic<uint64_t> g_total_bytes{0};
std::atomic<uint64_t> g_total_cnt{0};

// 令牌桶限流
std::mutex g_token_mtx;
std::condition_variable g_token_cv;
int64_t g_token{0};

// ============ 工具函数 ============

int64_t monotonic_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string generate_payload(int size) {
    std::string payload;
    payload.resize(size);

    // 使用线程局部随机引擎，避免多线程竞争
    thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 255);

    for (int i = 0; i < size; ++i) {
        payload[i] = static_cast<char>(dist(rng));
    }
    return payload;
}

// ============ 令牌桶线程 ============

void token_thread(int expected_qps) {
  if (expected_qps <= 0) return;

  auto start = std::chrono::steady_clock::now();
  int64_t accumulative = 0;

  while (!g_stop.load(std::memory_order_relaxed)) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();

    int64_t expected = static_cast<int64_t>(expected_qps) * elapsed_us / 1000000;
    if (accumulative < expected) {
      int64_t delta = expected - accumulative;
      {
        std::lock_guard<std::mutex> lock(g_token_mtx);
        g_token += delta;
        accumulative += delta;
      }
      g_token_cv.notify_all();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}


std::optional<apache::thrift::CompressionConfig> getCompressionConfig(
    const std::string& algo) {
  if (algo == "none" || algo.empty()) {
    return std::nullopt;
  }

  apache::thrift::CompressionConfig config;
  auto& codec = config.codecConfig().ensure();

  if (algo == "zstd") {
    codec.set_zstdConfig();
  } else if (algo == "zlib") {
    codec.set_zlibConfig();
  } else {
    LOG(FATAL) << "Unsupported compression algorithm: " << algo;
  }
  return config;
}

// ============ 协议工厂 ============
std::unique_ptr<PerfTestClient> newPerfTestClient(
    folly::EventBase* evb,
    const folly::SocketAddress& addr,
    const std::string& transport) {
  
  auto socket = folly::AsyncSocket::newSocket(evb, addr);

  if (transport == "rocket") {
    auto channel = apache::thrift::RocketClientChannel::newChannel(std::move(socket));
    if (FLAGS_rpc_timeout_ms > 0) {
      channel->setTimeout(FLAGS_rpc_timeout_ms);
    }
    if (auto config = getCompressionConfig(FLAGS_compression)) {
      channel->setDesiredCompressionConfig(*config);
    }
    return std::make_unique<PerfTestClient>(std::move(channel));
    
  } else if (transport == "header") {
    auto channel = apache::thrift::HeaderClientChannel::newChannel(std::move(socket));
    if (FLAGS_rpc_timeout_ms > 0) {
      channel->setTimeout(FLAGS_rpc_timeout_ms);
    }
    if (auto config = getCompressionConfig(FLAGS_compression)) {
      channel->setDesiredCompressionConfig(*config);
    }
    return std::make_unique<PerfTestClient>(std::move(channel));
    
  } else if (transport == "http2") {
    LOG(FATAL) << "http2 transport is not supported yet";
  }
  
  LOG(FATAL) << "Unknown transport: " << transport;
}
// ============ 工作线程 ============

void worker_thread(int tid, const folly::SocketAddress& addr,
                   const std::string& payload, ThreadStats& stats,
                   folly::EventBase* evb_ptr) {
  folly::EventBase& evb = *evb_ptr;

  auto client = newPerfTestClient(&evb, addr, FLAGS_transport);

  press_test::Request request;
  request.echo_attachment() = FLAGS_echo_attachment;
  request.payload() = payload;

  std::atomic<int> inflight{0};
  uint64_t start_time = monotonic_us();
  uint64_t sent_count = 0;

  std::function<void()> send_request;

  send_request = [&]() {
    if (g_stop.load(std::memory_order_relaxed)) {
      if (inflight.load(std::memory_order_relaxed) == 0) {
        evb.terminateLoopSoon();
      }
      return;
    }

    if (FLAGS_expected_qps > 0) {
      std::unique_lock<std::mutex> lock(g_token_mtx);
      g_token_cv.wait(lock, []() {
        return g_token > 0 || g_stop.load(std::memory_order_relaxed);
      });
      if (g_stop.load(std::memory_order_relaxed)) {
        if (inflight.load(std::memory_order_relaxed) == 0) {
          evb.terminateLoopSoon();
        }
        return;
      }
      g_token--;
    }

    inflight.fetch_add(1, std::memory_order_relaxed);
    sent_count++;

    int64_t send_time = monotonic_us();

    client->semifuture_test(request)
        .via(&evb)
        .thenTry([&, send_time](folly::Try<press_test::PerfTestResponse>&& t) {
          int64_t recv_time = monotonic_us();
          int64_t latency_us = recv_time - send_time;

          if (t.hasValue()) {
            stats.latency.addValue(latency_us);
            stats.success++;
            g_total_bytes.fetch_add(payload.size(), std::memory_order_relaxed);
          } else {
            stats.fail++;
            LOG(ERROR) << "RPC failed: " << t.exception().what();
          }

          g_total_cnt.fetch_add(1, std::memory_order_relaxed);
          inflight.fetch_sub(1, std::memory_order_relaxed);

          if (!g_stop.load(std::memory_order_relaxed)) {
            if (FLAGS_test_iterations > 0 && sent_count >= static_cast<uint64_t>(FLAGS_test_iterations)) {
              g_stop.store(true, std::memory_order_relaxed);
            } else if (FLAGS_test_iterations == 0 && recv_time - start_time > FLAGS_test_seconds * 1000000LL) {
              g_stop.store(true, std::memory_order_relaxed);
            } else {
              send_request();
              return;
            }
          }

          if (inflight.load(std::memory_order_relaxed) == 0) {
            evb.terminateLoopSoon();
          }
        });
  };

  for (int j = 0; j < FLAGS_queue_depth; ++j) {
    send_request();
  }

  evb.loop();
}

// ============ 单次测试 ============

void test_once(int thread_num, int payload_size) {
  std::string payload = generate_payload(payload_size);

  std::cout << "[Threads: " << thread_num
            << ", Depth: " << FLAGS_queue_depth
            << ", Payload: " << payload_size << "B"
            << ", Transport: " << FLAGS_transport
            << ", QPS: " << (FLAGS_expected_qps > 0 ? std::to_string(FLAGS_expected_qps) : "unlimited")
            << ", Duration: " << FLAGS_test_seconds << "s"
            << ", Echo: " << (FLAGS_echo_attachment ? "yes" : "no")
            << "]" << std::endl;

  g_stop.store(false, std::memory_order_relaxed);
  g_total_bytes.store(0, std::memory_order_relaxed);
  g_total_cnt.store(0, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lock(g_token_mtx);
    g_token = (FLAGS_expected_qps > 0) ? 10000 : 0;
  }

  std::vector<ThreadStats> stats(thread_num);
  std::vector<folly::EventBase*> evbs(thread_num, nullptr);
  std::vector<std::thread> workers;

  folly::SocketAddress addr(FLAGS_host, FLAGS_port);

  std::thread token_td;
  if (FLAGS_expected_qps > 0) {
    token_td = std::thread(token_thread, FLAGS_expected_qps);
  }

  auto test_start = std::chrono::steady_clock::now();

  for (int i = 0; i < thread_num; ++i) {
    evbs[i] = new folly::EventBase();
    workers.emplace_back(worker_thread, i, std::ref(addr),
                         std::ref(payload), std::ref(stats[i]), evbs[i]);
  }

  for (auto& t : workers) {
    t.join();
  }

  g_stop.store(true, std::memory_order_relaxed);
  g_token_cv.notify_all();
  if (token_td.joinable()) {
    token_td.join();
  }

  auto test_end = std::chrono::steady_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(test_end - test_start)
          .count();
  double elapsed_sec = elapsed_ms / 1000.0;

  LatencyStats total_latency;
  uint64_t total_success = 0;
  uint64_t total_fail = 0;

  for (int i = 0; i < thread_num; ++i) {
    total_latency.merge(stats[i].latency);
    total_success += stats[i].success;
    total_fail += stats[i].fail;
    delete evbs[i];
  }

  uint64_t total_cnt = total_success + total_fail;
  double throughput_mbps = (g_total_bytes.load() / 1048576.0) / elapsed_sec;

  LOG(INFO) << "====== Benchmark Results =======";
  LOG(INFO) << "Total duration: " << std::fixed << std::setprecision(3)
            << elapsed_sec << "s";
  LOG(INFO) << "Total requests: " << total_cnt;
  LOG(INFO) << "Success: " << total_success;
  LOG(INFO) << "Fail: " << total_fail;
  LOG(INFO) << "QPS: " << std::fixed << std::setprecision(2)
            << (total_success / elapsed_sec);
  LOG(INFO) << "Throughput: " << std::fixed << std::setprecision(2)
            << throughput_mbps << "MB/s";

  if (total_success > 0) {
    LOG(INFO) << "Latency (us):"
              << " avg=" << std::fixed << std::setprecision(2) << total_latency.getAvg()
              << " min=" << total_latency.getMin()
              << " max=" << total_latency.getMax()
              << " p50=" << total_latency.getPercentile(0.50)
              << " p90=" << total_latency.getPercentile(0.90)
              << " p99=" << total_latency.getPercentile(0.99)
              << " p999=" << total_latency.getPercentile(0.999);
  }

  LOG(INFO) << "================================";
}

// ============ main ============

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  const folly::Init init(&argc, &argv);

  if (FLAGS_thread_num > 0 && FLAGS_payload_size >= 0) {
    test_once(FLAGS_thread_num, FLAGS_payload_size);
  } else if (FLAGS_thread_num <= 0 && FLAGS_payload_size >= 0) {
    for (int i = 1; i <= FLAGS_max_thread_num; i *= 2) {
      test_once(i, FLAGS_payload_size);
    }
  } else if (FLAGS_thread_num > 0 && FLAGS_payload_size < 0) {
    for (int i = 1; i <= 1024; i *= 4) {
      test_once(FLAGS_thread_num, i);
    }
  } else {
    for (int j = 1; j <= 1024; j *= 4) {
      for (int i = 1; i <= FLAGS_max_thread_num; i *= 2) {
        test_once(i, j);
      }
    }
  }

  return 0;
}