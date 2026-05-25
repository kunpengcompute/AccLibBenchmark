#include <glog/logging.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <thrift/lib/cpp2/server/ThriftServer.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketRoutingHandler.h>

#include "server/PerfTestService.h"

DEFINE_int32(port, 8002, "TCP Port of this server");
DEFINE_int32(num_threads, 8, "Number of IO and CPU worker threads");

using apache::thrift::RocketRoutingHandler;
using apache::thrift::ThriftServer;
using press_test::PerfTestServiceHandler;

std::unique_ptr<RocketRoutingHandler> createRocketRoutingHandler(
    std::shared_ptr<ThriftServer> server) {
  return std::make_unique<RocketRoutingHandler>(*server);
}

template <typename ServiceHandler>
std::shared_ptr<ThriftServer> newServer(int32_t port, int32_t num_threads) {
  auto handler = std::make_shared<ServiceHandler>();
  auto server = std::make_shared<ThriftServer>();
  server->setPort(port);
  server->setInterface(handler);
  server->setNumIOWorkerThreads(num_threads);
  server->setNumCPUWorkerThreads(num_threads);
  server->addRoutingHandler(createRocketRoutingHandler(server));
  return server;
}

int main(int argc, char** argv) {
  FLAGS_logtostderr = 1;
  folly::init(&argc, &argv);

  press_test::init_resource_collector();

  auto server = newServer<PerfTestServiceHandler>(FLAGS_port, FLAGS_num_threads);
  LOG(INFO) << "PerfTest Server running on port: " << FLAGS_port
            << " with " << FLAGS_num_threads << " IO threads (Rocket transport)";
  server->serve();

  return 0;
}