#include <signal.h>
#include <atomic>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <thrift/lib/cpp2/server/ThriftServer.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketRoutingHandler.h>
#include <thrift/lib/cpp2/gen/client_cpp.h>

#include "server/PerfTestService.h"
#include "util/ResourceMonitor.h"

DEFINE_int32(port, 8002, "TCP Port of NativeService (PerfTestService)");
DEFINE_int32(universal_port, 8003, "TCP Port of UniversalService");
DEFINE_int32(num_io_threads, 1, "Number of IO worker threads");
DEFINE_int32(num_cpu_threads, 1, "Number of CPU worker threads");

using apache::thrift::RocketRoutingHandler;
using apache::thrift::ThriftServer;
using press_test::PerfTestServiceHandler;
using universal::UniversalServiceHandler;

std::shared_ptr<ThriftServer> g_server;
std::shared_ptr<ThriftServer> g_universal_server;
std::atomic<bool> g_stop_requested{false};

void printHeaderCompressionStats(){
  LOG(INFO) << "====== Server Header Compression Stats =======";
  LOG(INFO) << "Header Compression (us):"
            << " avg=" << std::fixed << std::setprecision(2) << apache::thrift::transport::getHeaderCompressionAvg()
            << std::fixed << std::setprecision(0)
            << " p50=" << apache::thrift::transport::getHeaderCompressionP50()
            << " p90=" << apache::thrift::transport::getHeaderCompressionP90()
            << " p99=" << apache::thrift::transport::getHeaderCompressionP99()
            << " p999=" << apache::thrift::transport::getHeaderCompressionP999();
  LOG(INFO) << "Header Decompression (us):"
            << " avg=" << std::fixed << std::setprecision(2) << apache::thrift::transport::getHeaderDecompressionAvg()
            << std::fixed << std::setprecision(0)
            << " p50=" << apache::thrift::transport::getHeaderDecompressionP50()
            << " p90=" << apache::thrift::transport::getHeaderDecompressionP90()
            << " p99=" << apache::thrift::transport::getHeaderDecompressionP99()
            << " p999=" << apache::thrift::transport::getHeaderDecompressionP999();
}

void printRocketCompressionStats(){
  LOG(INFO) << "====== Server Rocket Compression Stats =======";
  LOG(INFO) << "Rocket Compression (us):"
            << " avg=" << std::fixed << std::setprecision(2) << apache::thrift::rocket::getRocketCompressionAvg()
            << std::fixed << std::setprecision(0)
            << " p50=" << apache::thrift::rocket::getRocketCompressionP50()
            << " p90=" << apache::thrift::rocket::getRocketCompressionP90()
            << " p99=" << apache::thrift::rocket::getRocketCompressionP99()
            << " p999=" << apache::thrift::rocket::getRocketCompressionP999();
  LOG(INFO) << "Rocket Decompression (us):"
            << " avg=" << std::fixed << std::setprecision(2) << apache::thrift::rocket::getRocketDecompressionAvg()
            << std::fixed << std::setprecision(0)
            << " p50=" << apache::thrift::rocket::getRocketDecompressionP50()
            << " p90=" << apache::thrift::rocket::getRocketDecompressionP90()
            << " p99=" << apache::thrift::rocket::getRocketDecompressionP99()
            << " p999=" << apache::thrift::rocket::getRocketDecompressionP999();
}

void printRequestDeserializationStats(){
  LOG(INFO) << "====== Native Request Deserialization Stats =======";
  LOG(INFO) << "Request Deserialization (us):"
            << " avg=" << std::fixed << std::setprecision(2) << apache::thrift::getRequestDeserializationAvg()
            << std::fixed << std::setprecision(0)
            << " p50=" << apache::thrift::getRequestDeserializationP50()
            << " p90=" << apache::thrift::getRequestDeserializationP90()
            << " p99=" << apache::thrift::getRequestDeserializationP99()
            << " p999=" << apache::thrift::getRequestDeserializationP999();
}

void printResponseSerializationStats(){
  LOG(INFO) << "====== Native Response Serialization Stats =======";
  LOG(INFO) << "Response Serialization (us):"
            << " avg=" << std::fixed << std::setprecision(2) << apache::thrift::getResponseSerializationAvg()
            << std::fixed << std::setprecision(0)
            << " p50=" << apache::thrift::getResponseSerializationP50()
            << " p90=" << apache::thrift::getResponseSerializationP90()
            << " p99=" << apache::thrift::getResponseSerializationP99()
            << " p999=" << apache::thrift::getResponseSerializationP999();
}

void printUniversalSerializationStats()
{
  auto s = universal::getServerUnivStats();
  LOG(INFO) << "====== Universal Serialize/Deserialize Stats =======";
  LOG(INFO) << "Universal Serialization (us):"
            << " avg=" << std::fixed << std::setprecision(2) << s.serialize_avg
            << std::fixed << std::setprecision(0)
            << " p50=" << s.serialize_p50
            << " p90=" << s.serialize_p90
            << " p99=" << s.serialize_p99
            << " p999=" << s.serialize_p999;
  LOG(INFO) << "Universal Deserialization (us):"
            << " avg=" << std::fixed << std::setprecision(2) << s.deserialize_avg
            << std::fixed << std::setprecision(0)
            << " p50=" << s.deserialize_p50
            << " p90=" << s.deserialize_p90
            << " p99=" << s.deserialize_p99
            << " p999=" << s.deserialize_p999;
}


void signalHandler(int sig) {
  if (!g_stop_requested.exchange(true)) {
    LOG(INFO) << "==================================";
    printHeaderCompressionStats();
    printRocketCompressionStats();
    printRequestDeserializationStats();
    printResponseSerializationStats();
    printUniversalSerializationStats();
    LOG(INFO) << "==================================";
    LOG(INFO) << "Received signal " << sig << ", stopping server...";
    if (g_server) {
      g_server->stop();
    }
    if (g_universal_server) {
      g_universal_server->stop();
    }
  }
}

std::unique_ptr<RocketRoutingHandler> createRocketRoutingHandler(
    std::shared_ptr<ThriftServer> server) {
  return std::make_unique<RocketRoutingHandler>(*server);
}

template <typename ServiceHandler>
std::shared_ptr<ThriftServer> newServer(int32_t port, int32_t num_io_threads, int32_t num_cpu_threads) {
  auto handler = std::make_shared<ServiceHandler>();
  auto server = std::make_shared<ThriftServer>();

  server->setPort(port);
  server->setInterface(handler);
  server->setNumIOWorkerThreads(num_io_threads);
  server->setNumCPUWorkerThreads(num_cpu_threads);
  server->addRoutingHandler(createRocketRoutingHandler(server));

  return server;
}

int main(int argc, char** argv) {
  FLAGS_logtostderr = 1;
  folly::init(&argc, &argv);

  util::init_resource_collector();

  universal::initUnivStats(FLAGS_num_cpu_threads);

  g_server = newServer<PerfTestServiceHandler>(FLAGS_port, FLAGS_num_io_threads, FLAGS_num_cpu_threads);
  g_universal_server = newServer<UniversalServiceHandler>(FLAGS_universal_port, FLAGS_num_io_threads, FLAGS_num_cpu_threads);

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  LOG(INFO) << "PerfTest Server running on port: " << FLAGS_port
            << " with " << FLAGS_num_io_threads << " IO threads and "
            << FLAGS_num_cpu_threads << " CPU threads";
  LOG(INFO) << "Universal Server running on port: " << FLAGS_universal_port
            << " with " << FLAGS_num_io_threads << " IO threads and "
            << FLAGS_num_cpu_threads << " CPU threads";

  std::thread universal_td([&]() {
    g_universal_server->serve();
  });

  g_server->serve();

  universal_td.join();

  LOG(INFO) << "Server exited gracefully.";
  return 0;
}