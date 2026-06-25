// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <gflags/gflags.h>
#include "butil/atomicops.h"
#include "butil/logging.h"
#include "butil/time.h"
#include "brpc/compress.h"
#include "brpc/global.h"
#include "brpc/server.h"
#include "brpc/rpc_pb_message_factory.h"
#include "bvar/variable.h"
#include "bvar/latency_recorder.h"
#include "butil/third_party/rapidjson/error/error.h"
#include "json2pb/rapidjson.h"
#include "press.pb.h"
#ifdef BRPC_WITH_SONIC_JSON
#include "sonic/sonic.h"
#endif

DEFINE_int32(port, 8002, "TCP Port of this server");
DEFINE_bool(use_rdma, true, "Use RDMA or not");
DEFINE_string(response_compress_type, "none",
              "Compression algorithm for response protobuf body: none/snappy/gzip/zlib");
DEFINE_int32(num_threads, 8, "Number of threads for server");
DEFINE_bool(enable_json_bench, false,
            "Enable JSON parse/serialize benchmark on server side");
DEFINE_string(json_engine, "sonic",
              "JSON engine for benchmark: sonic or rapidjson");

bvar::LatencyRecorder g_json_parse_latency("server_json_parse");
bvar::LatencyRecorder g_json_serialize_latency("server_json_serialize");
butil::atomic<uint64_t> g_last_time(0);
brpc::CompressType g_response_compress_type = brpc::COMPRESS_TYPE_NONE;

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string JoinSupportedCompressionNames() {
    std::vector<brpc::CompressHandler> handlers;
    brpc::ListCompressHandler(&handlers);
    std::string names = "none";
    for (size_t i = 0; i < handlers.size(); ++i) {
        names.append("/");
        names.append(handlers[i].name);
    }
    return names;
}

bool ValidateJsonBenchmarkOptions() {
    if (!FLAGS_enable_json_bench) {
        return true;
    }
    if (FLAGS_json_engine == "rapidjson") {
        return true;
    }
    if (FLAGS_json_engine == "sonic") {
#ifdef BRPC_WITH_SONIC_JSON
        return true;
#else
        LOG(ERROR) << "json_engine=sonic requires WITH_SONIC_JSON=ON";
        return false;
#endif
    }
    LOG(ERROR) << "Unknown json_engine: " << FLAGS_json_engine
               << ", expected rapidjson or sonic";
    return false;
}

bool ParseCompressionType(const std::string& name,
                          brpc::CompressType* type,
                          std::string* error) {
    const std::string normalized = ToLower(name);
    if (normalized == "none") {
        *type = brpc::COMPRESS_TYPE_NONE;
        return true;
    }
    if (normalized == "snappy") {
        *type = brpc::COMPRESS_TYPE_SNAPPY;
    } else if (normalized == "gzip") {
        *type = brpc::COMPRESS_TYPE_GZIP;
    } else if (normalized == "zlib") {
        *type = brpc::COMPRESS_TYPE_ZLIB;
    } else if (normalized == "lz4") {
        *type = brpc::COMPRESS_TYPE_LZ4;
    } else {
        *error = "Unknown compression type `" + name + "`, supported values: "
               + JoinSupportedCompressionNames();
        return false;
    }
    if (brpc::FindCompressHandler(*type) == NULL) {
        *error = "Compression type `" + normalized + "` is defined but not registered "
               "in this brpc build, supported values: " + JoinSupportedCompressionNames();
        return false;
    }
    return true;
}

namespace press_test {
class PerfTestServiceImpl : public PerfTestService {
public:
    PerfTestServiceImpl() {}
    ~PerfTestServiceImpl() {}

    void Test(google::protobuf::RpcController* cntl_base,
              const Request* request,
              PerfTestResponse* response,
              google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);
        brpc::Controller* cntl =
            static_cast<brpc::Controller*>(cntl_base);
        cntl->set_response_compress_type(g_response_compress_type);
        uint64_t last = g_last_time.load(butil::memory_order_relaxed);
        uint64_t now = butil::monotonic_time_us();
        if (now > last && now - last > 100000) {
            if (g_last_time.exchange(now, butil::memory_order_relaxed) == last) {
                response->set_cpu_usage(bvar::Variable::describe_exposed("process_cpu_usage"));
            } else {
                response->set_cpu_usage("");
            }
        } else {
            response->set_cpu_usage("");
        }
        response->set_server_request_decompress_avg_ns(
            brpc::GetRpcCompressStageLatency(brpc::RPC_COMPRESS_STAGE_SERVER_REQUEST));
        response->set_server_request_decompress_p99_ns(
            brpc::GetRpcCompressStageLatencyPercentile(
                brpc::RPC_COMPRESS_STAGE_SERVER_REQUEST, 0.99));
        response->set_server_response_compress_avg_ns(
            brpc::GetRpcCompressStageLatency(brpc::RPC_COMPRESS_STAGE_SERVER_RESPONSE));
        response->set_server_response_compress_p99_ns(
            brpc::GetRpcCompressStageLatencyPercentile(
                brpc::RPC_COMPRESS_STAGE_SERVER_RESPONSE, 0.99));

        if (FLAGS_enable_json_bench && !request->payload().empty()) {
            if (FLAGS_json_engine == "rapidjson") {
                int64_t parse_start = butil::cpuwide_time_ns();
                BUTIL_RAPIDJSON_NAMESPACE::Document doc;
                doc.Parse(request->payload().c_str());
                int64_t parse_end = butil::cpuwide_time_ns();
                if (doc.HasParseError()) {
                    cntl->SetFailed(brpc::EREQUEST,
                                    "rapidjson parse failed: %s, offset=%zu, payload_size=%zu",
                                    BUTIL_RAPIDJSON_NAMESPACE::GetParseError_En(
                                        doc.GetParseError()),
                                    doc.GetErrorOffset(),
                                    request->payload().size());
                    return;
                }
                BUTIL_RAPIDJSON_NAMESPACE::StringBuffer buffer;
                BUTIL_RAPIDJSON_NAMESPACE::Writer<
                    BUTIL_RAPIDJSON_NAMESPACE::StringBuffer> writer(buffer);
                int64_t ser_start = butil::cpuwide_time_ns();
                if (!doc.Accept(writer)) {
                    cntl->SetFailed(brpc::ERESPONSE,
                                    "rapidjson serialize failed, payload_size=%zu",
                                    request->payload().size());
                    return;
                }
                int64_t ser_end = butil::cpuwide_time_ns();
                g_json_parse_latency << (parse_end - parse_start);
                g_json_serialize_latency << (ser_end - ser_start);
                response->set_payload(buffer.GetString(), buffer.GetSize());
            }
#ifdef BRPC_WITH_SONIC_JSON
            else if (FLAGS_json_engine == "sonic") {
                int64_t parse_start = butil::cpuwide_time_ns();
                sonic_json::Document doc;
                doc.Parse(request->payload().data(), request->payload().size());
                int64_t parse_end = butil::cpuwide_time_ns();
                if (doc.HasParseError()) {
                    cntl->SetFailed(brpc::EREQUEST,
                                    "sonic parse failed: %s, payload_size=%zu",
                                    sonic_json::ErrorMsg(doc.GetParseError()),
                                    request->payload().size());
                    return;
                }
                sonic_json::WriteBuffer wb;
                int64_t ser_start = butil::cpuwide_time_ns();
                const sonic_json::SonicError err = doc.Serialize(wb);
                if (err != sonic_json::kErrorNone) {
                    cntl->SetFailed(brpc::ERESPONSE,
                                    "sonic serialize failed: %s, payload_size=%zu",
                                    sonic_json::ErrorMsg(err),
                                    request->payload().size());
                    return;
                }
                int64_t ser_end = butil::cpuwide_time_ns();
                g_json_parse_latency << (parse_end - parse_start);
                g_json_serialize_latency << (ser_end - ser_start);
                response->set_payload(wb.ToString(), wb.Size());
            }
#endif
        } else if (request->echo_attachment() && !request->payload().empty()) {
            response->set_payload(request->payload());
        }
        if (request->echo_attachment()) {
            cntl->response_attachment().append(cntl->request_attachment());
        }

        response->set_json_parse_avg_ns(
            static_cast<int64_t>(g_json_parse_latency.latency(10)));
        response->set_json_parse_p99_ns(
            static_cast<int64_t>(g_json_parse_latency.latency_percentile(0.99)));
        response->set_json_serialize_avg_ns(
            static_cast<int64_t>(g_json_serialize_latency.latency(10)));
        response->set_json_serialize_p99_ns(
            static_cast<int64_t>(g_json_serialize_latency.latency_percentile(0.99)));
    }
};
}

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::GlobalInitializeOrDie();

    std::string error_text;
    if (!ParseCompressionType(FLAGS_response_compress_type,
                              &g_response_compress_type, &error_text)) {
        LOG(ERROR) << error_text;
        return -1;
    }
    if (!ValidateJsonBenchmarkOptions()) {
        return -1;
    }

    brpc::Server server;
    press_test::PerfTestServiceImpl perf_test_service_impl;

    if (server.AddService(&perf_test_service_impl,
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }
    g_last_time.store(0, butil::memory_order_relaxed);

    brpc::ServerOptions options;
    options.use_rdma = FLAGS_use_rdma;
    options.num_threads = FLAGS_num_threads;
    options.rpc_pb_message_factory = brpc::GetArenaRpcPBMessageFactory();
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}
