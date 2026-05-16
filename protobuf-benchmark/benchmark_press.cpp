#include <benchmark/benchmark.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <random>
#include <vector>
#include <cstring>
#include <iostream>
#include "press.pb.h"

namespace {

std::string ReadFileToString(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        return "";
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        perror("Error getting file size");
        close(fd);
        return "";
    }

    std::string buffer(file_stat.st_size, '\0');
    if (read(fd, &buffer[0], file_stat.st_size) != file_stat.st_size) {
        perror("Error reading file");
        close(fd);
        return "";
    }
    close(fd);
    return buffer;
}

const std::string data = ReadFileToString("request_data.pb");

using namespace test;

static void test_req_serialize_with_arena(benchmark::State &state) {
    google::protobuf::Arena arena;
    Requests *request = google::protobuf::Arena::Create<Requests>(&arena);
    request->ParseFromString(data);
    std::string serialized_data;
    for (auto _ : state) {
        request->SerializeToString(&serialized_data);
    }
    state.SetBytesProcessed(state.iterations() * serialized_data.size());
}

static void test_req_serialize(benchmark::State &state) {
    Requests request;
    request.ParseFromString(data);
    std::string serialized_data;
    for (auto _ : state) {
        request.SerializeToString(&serialized_data);
    }
    state.SetBytesProcessed(state.iterations() * serialized_data.size());
}

static void test_req_deserialize(benchmark::State &state) {
    for (auto _ : state) {
        Requests request;
        request.ParseFromString(data);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}

static void test_req_deserialize_with_arena(benchmark::State &state) {
    google::protobuf::Arena arena;
    for (auto _ : state) {
        arena.Reset();
        Requests *request = google::protobuf::Arena::Create<Requests>(&arena);
        request->ParseFromString(data);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}

}  // namespace

BENCHMARK(test_req_serialize);
BENCHMARK(test_req_serialize_with_arena);
BENCHMARK(test_req_deserialize);
BENCHMARK(test_req_deserialize_with_arena);

BENCHMARK_MAIN();
