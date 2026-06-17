#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include <random>
#include <gflags/gflags.h>
#include <folly/SocketAddress.h>
#include <folly/init/Init.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/synchronization/Baton.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>
// #include <thrift/lib/cpp2/async/HTTPClientChannel.h>
#include <thrift/lib/cpp2/async/RocketClientChannel.h>
#include <thrift/lib/cpp2/transport/rocket/PayloadUtils.h>
#include <thrift/lib/cpp2/gen/client_cpp.h>
#include "gen-cpp2/PerfTestService.h"
#include "gen-cpp2/UniversalService.h"
#include "gen-cpp2/press.pb.h"
#include "util/LatencyStats.h"
#include "util/ResourceMonitor.h"
#include "IOBufZeroCopyStream.h"

DEFINE_string(host, "127.0.0.1", "Server host");
DEFINE_int32(port, 8002, "Server port");
DEFINE_string(transport, "rocket", "Transport protocol: rocket, header, http2");
DEFINE_int32(queue_depth, 1, "In-flight requests per connection");
DEFINE_int32(expected_qps, 0, "Expected QPS (0 = unlimited)");
DEFINE_int32(payload_size, -1, "Payload size (<0 = scan 1~1024)");
DEFINE_bool(echo_attachment, false, "Echo payload back");
DEFINE_int32(rpc_timeout_ms, 2000, "RPC timeout in ms");
DEFINE_int32(test_seconds, 20, "Test duration");
DEFINE_int32(test_iterations, 0, "Test iterations (0 = time-based)");
DEFINE_string(mode, "native", "Client mode: native, universal");
DEFINE_string(thrift_serialization, "thrift_binary", "Thrift serialization: thrift_binary, thrift_compact, thrift_json");
DEFINE_string(universal_serialization, "none", "Universal serialization: protobuf, simdjson");
DEFINE_string(compression, "none", "Compression algorithm: none, zlib, zstd, snappy, lz4");
DEFINE_int32(connections, 100, "Total number of connections");

// ============ New: Server-like thread config ============
DEFINE_int32(io_threads, 10, "Number of IO threads (EventBase count)");
DEFINE_int32(cpu_threads, 0, "Number of CPU worker threads (0 = use hardware_concurrency)");

using PerfTestClient = apache::thrift::Client<press_test::PerfTestService>;
using UniversalClient = apache::thrift::Client<universal::UniversalService>;
static std::atomic<uint64_t> g_client_last_update_time{0};

// ============ Global State ============

// Per-CPU-core stats (no lock needed, each callback thread writes to its own)
struct alignas(64) ThreadStats {
  util::HistogramStats<int64_t>  latency{100, 0, 5000000};
  util::HistogramStats<double>   server_cpu_usage{1, 0.0, 30000.0};
  util::HistogramStats<double>   client_cpu_usage{1, 0.0, 30000.0};
  util::HistogramStats<double>   server_mem_usage{1, 0.0, 100.0};
  util::HistogramStats<double>   client_mem_usage{1, 0.0, 100.0};
  util::HistogramStats<int64_t>  universal_serialize{100, 0, 5000000};
  util::HistogramStats<int64_t>  universal_deserialize{100, 0, 5000000};

  uint64_t success{0};
  uint64_t fail{0};
};

// CPU thread count determines stats array size
static std::vector<std::unique_ptr<ThreadStats>> g_stats_per_cpu;

std::atomic<bool> g_stop{false};
std::atomic<uint64_t> g_total_bytes{0};
std::atomic<uint64_t> g_total_cnt{0};

// Token bucket rate limiter
std::atomic<int64_t> g_token{0};

// ============ Utility Functions ============

int64_t monotonic_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string generate_payload(int size) {
  std::string payload;
  payload.resize(size);

  thread_local std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 255);

  for (int i = 0; i < size; ++i) {
    payload[i] = static_cast<char>(dist(rng));
  }
  return payload;
}

// ============ Token Bucket Thread ============

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
      g_token.fetch_add(delta, std::memory_order_relaxed);
      accumulative += delta;
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
  } else if (algo == "snappy") {
    codec.set_snappyConfig();
  } else if (algo == "lz4") {
    codec.set_lz4Config();
  } else {
    LOG(FATAL) << "Unsupported compression algorithm: " << algo;
  }
  return config;
}

apache::thrift::protocol::PROTOCOL_TYPES getProtocolId(const std::string& serialization) {
  if (serialization == "thrift_binary") {
    return apache::thrift::protocol::T_BINARY_PROTOCOL;
  } else if (serialization == "thrift_compact") {
    return apache::thrift::protocol::T_COMPACT_PROTOCOL;
  } else if (serialization == "thrift_json") {
    return apache::thrift::protocol::T_JSON_PROTOCOL;
  }
  LOG(FATAL) << "Unknown thrift serialization: " << serialization;
  return apache::thrift::protocol::T_BINARY_PROTOCOL;
}

// ============ Template Client Factory ============
// Must be called in the EventBase thread
template <typename ClientT>
std::unique_ptr<ClientT> newThriftClient(
    folly::EventBase* evb,
    const folly::SocketAddress& addr,
    const std::string& transport,
    apache::thrift::protocol::PROTOCOL_TYPES protocolId) {

  auto socket = folly::AsyncSocket::newSocket(evb, addr);

  if (transport == "rocket") {
    auto channel = apache::thrift::RocketClientChannel::newChannel(std::move(socket));
    channel->setProtocolId(protocolId);
    if (FLAGS_rpc_timeout_ms > 0) {
      channel->setTimeout(FLAGS_rpc_timeout_ms);
    }
    if (auto config = getCompressionConfig(FLAGS_compression)) {
      channel->setDesiredCompressionConfig(*config);
    }
    return std::make_unique<ClientT>(std::move(channel));

  } else if (transport == "header") {
    apache::thrift::HeaderClientChannel::Options options;
    options.setProtocolId(protocolId);
    auto channel = apache::thrift::HeaderClientChannel::newChannel(std::move(socket), std::move(options));
    if (FLAGS_rpc_timeout_ms > 0) {
      channel->setTimeout(FLAGS_rpc_timeout_ms);
    }
    if (auto config = getCompressionConfig(FLAGS_compression)) {
      channel->setDesiredCompressionConfig(*config);
    }
    return std::make_unique<ClientT>(std::move(channel));

  } else if (transport == "http2") {
    LOG(FATAL) << "http2 transport is not supported yet";
  }

  LOG(FATAL) << "Unknown transport: " << transport;
  return nullptr;
}

// Get thread-local stats index based on CPU thread pool thread id
static thread_local int g_stats_idx = -1;

static int get_stats_idx() {
  if (g_stats_idx < 0) {
    // First time in this thread, assign an index
    static std::atomic<int> next_idx{0};
    int idx = next_idx.fetch_add(1, std::memory_order_relaxed);
    CHECK(idx < static_cast<int>(g_stats_per_cpu.size()))
        << "More threads than stats slots: " << idx + 1
        << " threads vs " << g_stats_per_cpu.size() << " slots";
    g_stats_idx = idx;
  }
  return g_stats_idx;
}

// ============ Serialization Handler (for universal mode) ============
struct SerializationHandler {
  virtual ~SerializationHandler() = default;
  virtual std::unique_ptr<folly::IOBuf> serialize(const std::string& payload, bool echo_attachment) = 0;
  virtual void processResponse(const std::unique_ptr<folly::IOBuf>& payload, ThreadStats& stats) = 0;
  virtual universal::SerializationType getType() const = 0;
};

struct ProtobufHandler : SerializationHandler {
  std::unique_ptr<folly::IOBuf> serialize(const std::string& payload, bool echo_attachment) override {
    press_test_proto::Request req;
    req.set_echo_attachment(echo_attachment);
    req.set_payload(payload);
    size_t size_hint = req.ByteSizeLong();
    folly::IOBufQueue queue;
    press_test::IOBufAsZeroCopyOutputStream stream(&queue, size_hint);
    req.SerializeToZeroCopyStream(&stream);
    return queue.move();
  }

  void processResponse(const std::unique_ptr<folly::IOBuf>& payload, ThreadStats& stats) override {
    press_test_proto::PerfTestResponse resp;
    const auto deser_start = std::chrono::steady_clock::now();
    if (payload) {
      press_test::IOBufAsZeroCopyInputStream stream(payload.get());
      if (resp.ParseFromZeroCopyStream(&stream)) {
        stats.universal_deserialize.addValue(std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - deser_start).count());
        try {
          double cpu_val = std::stod(resp.cpu_usage());
          if (cpu_val > 0) {
            stats.server_cpu_usage.addValue(cpu_val);
          }
          double mem_val = std::stod(resp.mem_usage());
          if (mem_val > 0) {
            stats.server_mem_usage.addValue(mem_val);
          }
        } catch (...) {}
        g_total_bytes.fetch_add(resp.payload().size(), std::memory_order_relaxed);
      }
    }
  }

  universal::SerializationType getType() const override {
    return universal::SerializationType::PROTOBUF;
  }
};

struct SimdJsonHandler : SerializationHandler {
  std::unique_ptr<folly::IOBuf> serialize(const std::string& payload, bool echo_attachment) override {
    LOG(FATAL) << "SimdJson serialization not implemented yet";
    return nullptr;
  }

  void processResponse(const std::unique_ptr<folly::IOBuf>& payload, ThreadStats& stats) override {
    LOG(FATAL) << "SimdJson deserialization not implemented yet";
  }

  universal::SerializationType getType() const override {
    LOG(FATAL) << "SimdJson serialization type not defined";
    return universal::SerializationType::SIMD_JSON;
  }
};

std::unique_ptr<SerializationHandler> createSerializationHandler(const std::string& type) {
  if (type == "protobuf") {
    return std::make_unique<ProtobufHandler>();
  } else if (type == "simdjson") {
    return std::make_unique<SimdJsonHandler>();
  }
  LOG(FATAL) << "Unknown universal serialization: " << type;
  return nullptr;
}

// ============ Connection Context ============
using ClientVariant = std::variant<std::unique_ptr<PerfTestClient>,
                                   std::unique_ptr<UniversalClient>>;

struct ConnContext {
  folly::EventBase* evb;
  ClientVariant client;
  int evb_idx;
};

// Create client in the EventBase thread
template <typename ClientT>
void createClientInEvb(ConnContext* ctx,
                       const folly::SocketAddress& addr,
                       const std::string& transport,
                       apache::thrift::protocol::PROTOCOL_TYPES protocolId) {
  folly::Baton<> baton;
  ctx->evb->runInEventBaseThread([&]() {
    ctx->client = newThriftClient<ClientT>(ctx->evb, addr, transport, protocolId);
    baton.post();
  });
  baton.wait();
}

// ============ FIX: 把关键状态包装成 shared_ptr，彻底避免悬空引用 ============
struct BenchmarkState {
  std::vector<std::unique_ptr<ConnContext>> ctxs;
  press_test::Request request;
  std::unique_ptr<SerializationHandler> serialization_handler;
  bool is_native_mode;
  uint64_t start_time{0};
  std::atomic<int> total_inflight{0};
  std::atomic<uint64_t> total_processed{0};
};

// ============ Main thread drives all connections ============
// 返回 shared_ptr<BenchmarkState>，所有异步回调值捕获该 state，生命周期安全
std::shared_ptr<BenchmarkState> run_benchmark(
    const folly::SocketAddress& addr,
    const std::string& payload,
    const std::vector<folly::EventBase*>& evb_list,
    folly::Executor* cpu_executor) {

  int num_connections = FLAGS_connections;
  int num_evbs = static_cast<int>(evb_list.size());
  bool is_native_mode = (FLAGS_mode == "native");
  auto protocolId = getProtocolId(FLAGS_thrift_serialization);

  if (!is_native_mode && FLAGS_universal_serialization == "none") {
    LOG(FATAL) << "universal_serialization must be set when mode is universal";
  }

  auto state = std::make_shared<BenchmarkState>();
  state->is_native_mode = is_native_mode;

  if (!is_native_mode) {
    state->serialization_handler = createSerializationHandler(FLAGS_universal_serialization);
  }

  for (int i = 0; i < num_connections; ++i) {
    auto ctx = std::make_unique<ConnContext>();
    ctx->evb_idx = i % num_evbs;
    ctx->evb = evb_list[ctx->evb_idx];

    if (is_native_mode) {
      createClientInEvb<PerfTestClient>(ctx.get(), addr, FLAGS_transport, protocolId);
    } else {
      createClientInEvb<UniversalClient>(ctx.get(), addr, FLAGS_transport, protocolId);
    }

    state->ctxs.push_back(std::move(ctx));
  }

  state->request.echo_attachment() = FLAGS_echo_attachment;
  state->request.payload() = folly::IOBuf::copyBuffer(payload);
  state->start_time = monotonic_us();

  // ========================================================================
  // FIX: 用 shared_ptr 包装 send_request，值捕获 state，彻底避免悬空
  // ========================================================================
  auto send_request = std::make_shared<std::function<void(int)>>();

  // 值捕获 state（shared_ptr）和 send_request（shared_ptr），形成自引用链
  // 同时值捕获 cpu_executor 指针，保证 .via() 安全
  *send_request = [state, send_request, cpu_executor](int idx) {
    auto& ctx = *state->ctxs[idx];

    if (g_stop.load(std::memory_order_acquire)) {
      return;
    }

    // QPS rate limiting
    if (FLAGS_expected_qps > 0) {
      if (g_token.load(std::memory_order_relaxed) <= 0) {
        folly::futures::sleep(std::chrono::milliseconds(1))
            .via(cpu_executor)
            .thenTry([idx, send_request](folly::Try<folly::Unit>) {
              (*send_request)(idx);
            });
        return;
      }
      g_token.fetch_sub(1, std::memory_order_relaxed);
    }

    if (g_stop.load(std::memory_order_acquire)) {
      return;
    }

    state->total_inflight.fetch_add(1, std::memory_order_relaxed);

    auto on_response = [state, idx, send_request](int64_t send_time, int64_t recv_time, bool success) {
      int64_t latency_us = recv_time - send_time;

      int stats_idx = get_stats_idx();
      ThreadStats& stats = *g_stats_per_cpu[stats_idx];

      if (success) {
        stats.latency.addValue(latency_us);
        stats.success++;
      } else {
        stats.fail++;
      }

      uint64_t last = g_client_last_update_time.load(std::memory_order_relaxed);
      if (recv_time > last && recv_time - last > 100000) {
        if (g_client_last_update_time.compare_exchange_weak(
                last, recv_time, std::memory_order_relaxed)) {
          auto snap = util::g_snapshot.load(std::memory_order_acquire);
          stats.client_cpu_usage.addValue(snap.cpu_percent);
          stats.client_mem_usage.addValue(snap.mem_percent);
        }
      }

      g_total_cnt.fetch_add(1, std::memory_order_relaxed);
      state->total_inflight.fetch_sub(1, std::memory_order_relaxed);
      auto processed = state->total_processed.fetch_add(1, std::memory_order_relaxed) + 1;

      if (!g_stop.load(std::memory_order_acquire)) {
        if (FLAGS_test_iterations > 0 &&
            processed >= static_cast<uint64_t>(FLAGS_test_iterations)) {
          g_stop.store(true, std::memory_order_release);
        } else if (FLAGS_test_iterations == 0 &&
                    recv_time - state->start_time >= FLAGS_test_seconds * 1000000LL) {
          g_stop.store(true, std::memory_order_release);
        } else {
          (*send_request)(idx);
        }
      }
    };

    if (state->is_native_mode) {
      int64_t send_time = monotonic_us();
      std::get<std::unique_ptr<PerfTestClient>>(ctx.client)->semifuture_native_test(state->request)
          .via(cpu_executor)
          .thenTry([state, idx, send_time, on_response](folly::Try<press_test::PerfTestResponse>&& t) {
            int64_t recv_time = monotonic_us();
            bool success = t.hasValue();
            on_response(send_time, recv_time, success);
            if (success) {
              if (t.value().cpu_usage().value() > 0) {
                g_stats_per_cpu[get_stats_idx()]->server_cpu_usage.addValue(t.value().cpu_usage().value());
              }
              if (t.value().mem_usage().value() > 0) {
                g_stats_per_cpu[get_stats_idx()]->server_mem_usage.addValue(t.value().mem_usage().value());
              }
              auto& resp_payload = *t.value().payload();
              if (resp_payload) {
                g_total_bytes.fetch_add(resp_payload->computeChainDataLength(), std::memory_order_relaxed);
              }
            } else {
              LOG(ERROR) << "RPC failed: " << t.exception().what();
            }
          });
    } else {
      int64_t serialize_start = monotonic_us();
      std::string payload_input;
      auto& req_payload = *state->request.payload();
      if (req_payload) {
        if (!req_payload->isChained()) {
          payload_input.assign(reinterpret_cast<const char*>(req_payload->data()),
                               req_payload->length());
        } else {
          folly::io::Cursor cursor(req_payload.get());
          payload_input = cursor.readFixedString(req_payload->computeChainDataLength());
        }
      }
      std::unique_ptr<folly::IOBuf> serialized = state->serialization_handler->serialize(
          payload_input, *state->request.echo_attachment());
      int64_t univ_serialize_us = monotonic_us() - serialize_start;
      g_stats_per_cpu[get_stats_idx()]->universal_serialize.addValue(univ_serialize_us);

      universal::UniversalRequest univ_req;
      univ_req.type() = state->serialization_handler->getType();
      *univ_req.payload() = std::move(serialized);

      int64_t send_time = monotonic_us();
      std::get<std::unique_ptr<UniversalClient>>(ctx.client)->semifuture_universal_test(univ_req)
          .via(cpu_executor)
          .thenTry([state, idx, send_time, on_response](folly::Try<universal::UniversalResponse>&& t) {
            int64_t recv_time = monotonic_us();
            bool success = t.hasValue();
            on_response(send_time, recv_time, success);
            if (success) {
              state->serialization_handler->processResponse(*t.value().payload(), *g_stats_per_cpu[get_stats_idx()]);
            } else {
              LOG(ERROR) << "RPC failed: " << t.exception().what();
            }
          });
    }
  };

  // Initialize: distribute queue_depth across all connections
  for (int i = 0; i < num_connections; ++i) {
    for (int j = 0; j < FLAGS_queue_depth; ++j) {
      cpu_executor->add([i, send_request]() {
        (*send_request)(i);
      });
    }
  }

  // Main thread waits for completion (all responses received)
  while (!g_stop.load(std::memory_order_acquire) ||
        state->total_inflight.load(std::memory_order_acquire) > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return state;
}

// ============ Single Test Run ============
void test_once(int payload_size) {
  std::string payload = generate_payload(payload_size);

  int io_thread_num = FLAGS_io_threads > 0 ? FLAGS_io_threads : 10;
  int cpu_thread_num = FLAGS_cpu_threads > 0 ? FLAGS_cpu_threads : static_cast<int>(std::thread::hardware_concurrency());

  LOG(INFO) << "[IOThreads: " << io_thread_num
            << ", CPUThreads: " << cpu_thread_num
            << ", Connections: " << FLAGS_connections
            << ", Depth: " << FLAGS_queue_depth
            << ", Payload: " << payload_size << "B"
            << ", Transport: " << FLAGS_transport
            << ", Serialization: " << FLAGS_thrift_serialization
            << ", QPS: " << (FLAGS_expected_qps > 0 ? std::to_string(FLAGS_expected_qps) : "unlimited")
            << ", Duration: " << FLAGS_test_seconds << "s"
            << ", Echo: " << (FLAGS_echo_attachment ? "yes" : "no")
            << "]" ;

  g_stop.store(false, std::memory_order_release);
  g_total_bytes.store(0, std::memory_order_relaxed);
  g_total_cnt.store(0, std::memory_order_relaxed);

  g_token.store(0, std::memory_order_relaxed);

  // Initialize per-CPU stats
  g_stats_per_cpu.clear();
  for (int i = 0; i < cpu_thread_num; ++i) {
    g_stats_per_cpu.push_back(std::make_unique<ThreadStats>());
  }

  std::vector<folly::EventBase*> evbs;

  folly::SocketAddress addr(FLAGS_host, FLAGS_port);

  // Create IO thread pool
  auto io_executor = std::make_shared<folly::IOThreadPoolExecutor>(io_thread_num);
  for (int i = 0; i < io_thread_num; ++i) {
    evbs.push_back(io_executor->getEventBase());
  }

  // Create CPU thread pool
  auto cpu_executor = std::make_shared<folly::CPUThreadPoolExecutor>(cpu_thread_num);

  std::thread token_td;
  if (FLAGS_expected_qps > 0) {
    token_td = std::thread(token_thread, FLAGS_expected_qps);
  }

  auto test_start = std::chrono::steady_clock::now();

  // Main thread runs benchmark, gets state back (all async data safely inside)
  auto state = run_benchmark(addr, payload, evbs, cpu_executor.get());

  //  header compress Stats
  auto header_compress_avg = apache::thrift::transport::getHeaderCompressionAvg();
  auto header_compress_p50 = apache::thrift::transport::getHeaderCompressionP50();
  auto header_compress_p90 = apache::thrift::transport::getHeaderCompressionP90();
  auto header_compress_p99 = apache::thrift::transport::getHeaderCompressionP99();
  auto header_compress_p999 = apache::thrift::transport::getHeaderCompressionP999();

  //  header decompress Stats
  auto header_decompress_avg = apache::thrift::transport::getHeaderDecompressionAvg();
  auto header_decompress_p50 = apache::thrift::transport::getHeaderDecompressionP50();
  auto header_decompress_p90 = apache::thrift::transport::getHeaderDecompressionP90();
  auto header_decompress_p99 = apache::thrift::transport::getHeaderDecompressionP99();
  auto header_decompress_p999 = apache::thrift::transport::getHeaderDecompressionP999();

  //Rocket Compression Stats
  auto rocket_compress_avg = apache::thrift::rocket::getRocketCompressionAvg();
  auto rocket_compress_p50 = apache::thrift::rocket::getRocketCompressionP50();
  auto rocket_compress_p90 = apache::thrift::rocket::getRocketCompressionP90();
  auto rocket_compress_p99 = apache::thrift::rocket::getRocketCompressionP99();
  auto rocket_compress_p999 = apache::thrift::rocket::getRocketCompressionP999();

  //Rocket Decompression Stats
  auto rocket_decompress_avg = apache::thrift::rocket::getRocketDecompressionAvg();
  auto rocket_decompress_p50 = apache::thrift::rocket::getRocketDecompressionP50();
  auto rocket_decompress_p90 = apache::thrift::rocket::getRocketDecompressionP90();
  auto rocket_decompress_p99 = apache::thrift::rocket::getRocketDecompressionP99();
  auto rocket_decompress_p999 = apache::thrift::rocket::getRocketDecompressionP999();

  //  request serialize Stats
  auto request_serialize_avg = apache::thrift::getRequestSerializationAvg();
  auto request_serialize_p50 = apache::thrift::getRequestSerializationP50();
  auto request_serialize_p90 = apache::thrift::getRequestSerializationP90();
  auto request_serialize_p99 = apache::thrift::getRequestSerializationP99();
  auto request_serialize_p999 = apache::thrift::getRequestSerializationP999();

  //  response deserialize Stats
  auto response_deserialize_avg = apache::thrift::getResponseDeserializationAvg();
  auto response_deserialize_p50 = apache::thrift::getResponseDeserializationP50();
  auto response_deserialize_p90 = apache::thrift::getResponseDeserializationP90();
  auto response_deserialize_p99 = apache::thrift::getResponseDeserializationP99();
  auto response_deserialize_p999 = apache::thrift::getResponseDeserializationP999();

  // Signal stop to token thread
  g_stop.store(true, std::memory_order_release);
  if (token_td.joinable()) {
    token_td.join();
  }

  // CRITICAL: Wait for all callbacks to finish before destroying clients
  cpu_executor->stop();
  cpu_executor->join();

  // Now safe to destroy clients (no callbacks running)
  for (auto& ctx : state->ctxs) {
    folly::Baton<> baton;
    auto* evb = ctx->evb;
    std::visit([&](auto& client_ptr) {
      auto* raw = client_ptr.release();
      evb->runInEventBaseThread([raw, &baton]() {
        delete raw;
        baton.post();
      });
    }, ctx->client);
    baton.wait();
  }
  state->ctxs.clear();

  // Clear EventBase references before stopping IO threads
  evbs.clear();

  // Stop IO thread pool last
  io_executor->stop();
  io_executor->join();

  auto test_end = std::chrono::steady_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(test_end - test_start)
          .count();
  double elapsed_sec = elapsed_ms / 1000.0;

  // Aggregate stats from all CPU threads (no locks needed during aggregation)
  util::HistogramStats<int64_t> total_latency(100, 0, 5000000);
  util::HistogramStats<double> total_server_cpu(1, 0.0, 30000.0);
  util::HistogramStats<double> total_client_cpu(1, 0.0, 30000.0);
  util::HistogramStats<double> total_server_mem(1, 0.0, 100.0);
  util::HistogramStats<double> total_client_mem(1, 0.0, 100.0);
  util::HistogramStats<int64_t> total_univ_serialize(100, 0, 5000000);
  util::HistogramStats<int64_t> total_univ_deserialize(100, 0, 5000000);
  uint64_t total_success = 0;
  uint64_t total_fail = 0;

  for (auto& stats_ptr : g_stats_per_cpu) {
    ThreadStats& stats = *stats_ptr;
    total_latency.merge(stats.latency);
    total_server_cpu.merge(stats.server_cpu_usage);
    total_client_cpu.merge(stats.client_cpu_usage);
    total_server_mem.merge(stats.server_mem_usage);
    total_client_mem.merge(stats.client_mem_usage);
    total_univ_serialize.merge(stats.universal_serialize);
    total_univ_deserialize.merge(stats.universal_deserialize);
    total_success += stats.success;
    total_fail += stats.fail;
  }

  uint64_t total_cnt = total_success + total_fail;
  double throughput_mbps = (g_total_bytes.load() / 1024.0 / 1024.0) / elapsed_sec;

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

    LOG(INFO) << "Server CPU (%):"
              << " avg=" << std::fixed << std::setprecision(2) << total_server_cpu.getAvg()
              << " min=" << total_server_cpu.getMin()
              << " max=" << total_server_cpu.getMax()
              << " p50=" << total_server_cpu.getPercentile(0.50)
              << " p90=" << total_server_cpu.getPercentile(0.90)
              << " p99=" << total_server_cpu.getPercentile(0.99)
              << " p999=" << total_server_cpu.getPercentile(0.999);

    LOG(INFO) << "Client CPU (%):"
              << " avg=" << std::fixed << std::setprecision(2) << total_client_cpu.getAvg()
              << " min=" << total_client_cpu.getMin()
              << " max=" << total_client_cpu.getMax()
              << " p50=" << total_client_cpu.getPercentile(0.50)
              << " p90=" << total_client_cpu.getPercentile(0.90)
              << " p99=" << total_client_cpu.getPercentile(0.99)
              << " p999=" << total_client_cpu.getPercentile(0.999);

    LOG(INFO) << "Server Memory (%):"
              << " avg=" << std::fixed << std::setprecision(2) << total_server_mem.getAvg()
              << " min=" << total_server_mem.getMin()
              << " max=" << total_server_mem.getMax()
              << " p50=" << total_server_mem.getPercentile(0.50)
              << " p90=" << total_server_mem.getPercentile(0.90)
              << " p99=" << total_server_mem.getPercentile(0.99)
              << " p999=" << total_server_mem.getPercentile(0.999);

    LOG(INFO) << "Client Memory (%):"
              << " avg=" << std::fixed << std::setprecision(2) << total_client_mem.getAvg()
              << " min=" << total_client_mem.getMin()
              << " max=" << total_client_mem.getMax()
              << " p50=" << total_client_mem.getPercentile(0.50)
              << " p90=" << total_client_mem.getPercentile(0.90)
              << " p99=" << total_client_mem.getPercentile(0.99)
              << " p999=" << total_client_mem.getPercentile(0.999);
  }

  if (FLAGS_compression != "none" && FLAGS_transport == "header") {
    LOG(INFO) << "Header Compression (us):"
              << " avg=" << std::fixed << std::setprecision(2) << header_compress_avg
              << std::fixed << std::setprecision(0)
              << " p50=" << header_compress_p50
              << " p90=" << header_compress_p90
              << " p99=" << header_compress_p99
              << " p999=" << header_compress_p999;

    LOG(INFO) << "Header Decompression (us):"
              << " avg=" << std::fixed << std::setprecision(2) << header_decompress_avg
              << std::fixed << std::setprecision(0)
              << " p50=" << header_decompress_p50
              << " p90=" << header_decompress_p90
              << " p99=" << header_decompress_p99
              << " p999=" << header_decompress_p999;
  }

  if (FLAGS_compression != "none" && FLAGS_transport == "rocket") {
    LOG(INFO) << "Rocket Compression (us):"
              << " avg=" << std::fixed << std::setprecision(2) << rocket_compress_avg
              << std::fixed << std::setprecision(0)
              << " p50=" << rocket_compress_p50
              << " p90=" << rocket_compress_p90
              << " p99=" << rocket_compress_p99
              << " p999=" << rocket_compress_p999;

    LOG(INFO) << "Rocket Decompression (us):"
              << " avg=" << std::fixed << std::setprecision(2) << rocket_decompress_avg
              << std::fixed << std::setprecision(0)
              << " p50=" << rocket_decompress_p50
              << " p90=" << rocket_decompress_p90
              << " p99=" << rocket_decompress_p99
              << " p999=" << rocket_decompress_p999;
  }

  LOG(INFO) << "Native Request Serialization (us) :"
            << " avg=" << std::fixed << std::setprecision(2) << request_serialize_avg
            << std::fixed << std::setprecision(0)
            << " p50=" << request_serialize_p50
            << " p90=" << request_serialize_p90
            << " p99=" << request_serialize_p99
            << " p999=" << request_serialize_p999;

  LOG(INFO) << "Native Response Deserialization (us) :"
            << " avg=" << std::fixed << std::setprecision(2) << response_deserialize_avg
            << std::fixed << std::setprecision(0)
            << " p50=" << response_deserialize_p50
            << " p90=" << response_deserialize_p90
            << " p99=" << response_deserialize_p99
            << " p999=" << response_deserialize_p999;

  if (FLAGS_mode == "universal" && total_univ_serialize.getCount() > 0) {
    LOG(INFO) << "Universal Request Serialization (us) :"
              << " avg=" << std::fixed << std::setprecision(2) << total_univ_serialize.getAvg()
              << " min=" << total_univ_serialize.getMin()
              << " max=" << total_univ_serialize.getMax()
              << std::fixed << std::setprecision(0)
              << " p50=" << total_univ_serialize.getPercentile(0.50)
              << " p90=" << total_univ_serialize.getPercentile(0.90)
              << " p99=" << total_univ_serialize.getPercentile(0.99)
              << " p999=" << total_univ_serialize.getPercentile(0.999);

    LOG(INFO) << "Universal Response Deserialization (us) :"
              << " avg=" << std::fixed << std::setprecision(2) << total_univ_deserialize.getAvg()
              << " min=" << total_univ_deserialize.getMin()
              << " max=" << total_univ_deserialize.getMax()
              << std::fixed << std::setprecision(0)
              << " p50=" << total_univ_deserialize.getPercentile(0.50)
              << " p90=" << total_univ_deserialize.getPercentile(0.90)
              << " p99=" << total_univ_deserialize.getPercentile(0.99)
              << " p999=" << total_univ_deserialize.getPercentile(0.999);
  }

  LOG(INFO) << "================================"; 
}

// ============ main ============

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  const folly::Init init(&argc, &argv);

  if (FLAGS_mode != "native" && FLAGS_mode != "universal") {
    LOG(FATAL) << "Unknown mode: " << FLAGS_mode << ", supported modes: native, universal";
  }

  util::init_resource_collector();

  if (FLAGS_payload_size >= 0) {
    test_once(FLAGS_payload_size);
  } else {
    for (int i = 1; i <= 1024; i *= 4) {
      test_once(i);
    }
  }

  return 0;
}