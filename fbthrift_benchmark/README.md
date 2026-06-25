# fbthrift RPC Benchmark — simdjson + sonic-cpp

基于 fbthrift 的 RPC 压测工具，支持三种序列化模式：

- `--mode native` — Thrift Binary 原生协议
- `--mode simdjson` — simdjson 高性能 JSON (mini_formatter + DOM parser)
- `--mode sonicjson` — 字节跳动 sonic-cpp JSON (Document DOM + WriteBuffer)
- `--mode universal` — 通用模式，支持 protobuf 序列化

## 环境依赖

- C++17, GCC 10+
- fbthrift, folly, fizz, wangle, fmt, glog, gflags, protobuf
- simdjson (ARM NEON / x86 AVX)
- sonic-cpp (ARM NEON / x86 AVX, header-only)

依赖放置到 `third/` 目录：

```
third/
├── folly/lib/cmake/folly
├── fizz/lib/cmake/fizz
├── wangle/lib/cmake/wangle
├── fbthrift/lib/cmake/fbthrift
├── protobuf/lib64/cmake/protobuf
├── fmt/lib/cmake/fmt
├── simdjson/{include/simdjson.h, lib64/libsimdjson.a}
└── sonic-cpp/include/sonic/
```

## 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 指定自定义依赖路径：
cmake .. -DSIMDJSON_ROOT=/opt/simdjson -DSONICCPP_ROOT=/opt/sonic-cpp
```

## 启动服务端

```bash
./press_server --num_io_threads=5 --num_cpu_threads=5 --port=12345
```

## 客户端压测

```bash
# simdjson 模式
./press_client --mode simdjson --port 12345 --test_seconds 10 \
    --connections 1000 --queue_depth 10 --payload_size 1024 \
    --io_threads 10 --cpu_threads 0 --rpc_timeout_ms 30000

# sonicjson 模式
./press_client --mode sonicjson --port 12345 --test_seconds 10 \
    --connections 1000 --queue_depth 10 --payload_size 1024 \
    --io_threads 10 --cpu_threads 0 --rpc_timeout_ms 30000

# native 模式（对比）
./press_client --mode native --port 12345 --test_seconds 10 \
    --connections 1000 --queue_depth 10 --payload_size 1024 \
    --io_threads 10 --cpu_threads 0
```

## 单元测试

```bash
./json_codec_ut
# 22 tests: simdjson / sonic-cpp / cross-codec compatibility
```

## Benchmark 结果

| 模式 | QPS | Throughput | Encode | Decode |
|------|-----|-----------|--------|--------|
| native | 178,820 | 174 MB/s | - | - |
| simdjson | 152,572 | 411 MB/s | 7.12us | 10.76us |
| sonicjson | 141,004 | 380 MB/s | 9.58us | 14.71us |
