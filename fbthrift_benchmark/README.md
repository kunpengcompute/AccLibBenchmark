# fbthrift Echo 压测示例（C++）

这是一个基于 Facebook Thrift (fbthrift) 的简单 Echo RPC 压测工具示例，包含：

- Thrift IDL 定义
- Proto IDL 定义
- fbthrift 服务端（press_server）
- fbthrift 压测客户端（press_client）
- 支持 Rocket / Header 两种传输协议
- 支持 zlib / zstd / snappy / lz4 / none 压缩
- 支持 binary / compact / json / protobuf 序列化

---

## 一、环境依赖

- C++17 或更高
- fbthrift
- fizz
- fmt
- wangle
- folly
- glog
- proxygen
- protobuf

 依赖库安装到 third 目录下

```bash
third
├── fbthrift
├── fizz
├── fmt
├── folly
├── glog
├── proxygen
├── wangle
└── protobuf
```

## 二、编译构建

```bash
  # 代码增加了  rocket header 序列化耗时延迟统计,和序列化反序列化耗时统计功能
  # 需要对应修改版本的fbthrift源码

  mkdir build
  cd build
  cmake ..
  make -j
```

## 三、服务端启动

```bash

--port	8002	服务端监听的 TCP 端口号，客户端需要通过该端口连接。
--num_io_threads	5	服务端处理请求的 I/O 工作线程数量。这里设置为 5 个线程，用于并发处理客户端请求。
--num_cpu_threads	5	服务端处理请求的 CPU 工作线程数量。这里设置为 5 个线程，用于并发处理客户端请求。

./press_server  --num_io_threads=5  --num_cpu_threads=5  --port=8002
```

## 四、客户端压测

```bash

--host	127.0.0.1	服务端 IP 地址，本地回环地址。
--port	8002	服务端监听端口，与服务端启动时的 --port 一致。
--transport	header	传输协议，使用 Header 协议（另一种可选值为 rocket）。
--payload_size	1024	每个请求的 Payload 大小（字节）。这里固定为 1024 字节。
--io_threads	10	folly I/O 线程数数目，对应eventbase数目。
--cpu_threads	10	业务回调执行的线程数目。
--queue_depth	10	每个线程允许的最大 in-flight 请求数（即每个线程可以同时发起的未完成请求数量）。
--test_seconds	10	压测持续时间（秒），运行 10 秒后自动停止。
--compression	zstd	压缩算法，使用 Zstandard 压缩 Payload（可选 none / zlib / zstd）。
--rpc_timeout_ms	0	RPC 超时时间（毫秒），0 表示无超时限制。
--connections	1	客户端连接数，多个客户端取模分配到各个eventbase上。
--echo_attachment true	是否开启附件回显，true 表示开启，false 表示关闭。

./press_client  \
 --host=127.0.0.1 \
 --port=8002 \
 --transport=rocket \
 --payload_size=2048 \
 --queue_depth=1  \
 --test_seconds=10 \
 --compression=zstd \
 --rpc_timeout_ms=100000 \
 --io_threads=1 \
 --cpu_threads=1 \
 --connections=1 \
 --echo_attachment=true

```

## 五、示例压测结果

### client

```bash
I20260525 10:45:37.922885 147483 PressTestClient.cpp:361] [IOThreads: 1, CPUThreads: 1, Connections: 1, Depth: 1, Payload: 2048B, Transport: rocket, QPS: unlimited, Duration: 10s, Echo: yes]
I20260525 10:45:47.946425 147483 PressTestClient.cpp:515] ====== Benchmark Results =======
I20260525 10:45:47.946468 147483 PressTestClient.cpp:516] Total duration: 10.009s
I20260525 10:45:47.946508 147483 PressTestClient.cpp:518] Total requests: 25554
I20260525 10:45:47.946520 147483 PressTestClient.cpp:519] Success: 25554
I20260525 10:45:47.946523 147483 PressTestClient.cpp:520] Fail: 0
I20260525 10:45:47.946527 147483 PressTestClient.cpp:521] QPS: 2553.10
I20260525 10:45:47.946539 147483 PressTestClient.cpp:523] Throughput: 4.99MB/s
I20260525 10:45:47.946543 147483 PressTestClient.cpp:527] Latency (us): avg=390.17 min=289 max=11479 p50=373 p90=466 p99=730 p999=1077
I20260525 10:45:47.947530 147483 PressTestClient.cpp:536] Server CPU (%): avg=55.43 min=0.83 max=61.11 p50=55.95 p90=56.71 p99=57.00 p999=61.82
I20260525 10:45:47.948014 147483 PressTestClient.cpp:545] Client CPU (%): avg=50.59 min=0.30 max=52.03 p50=51.13 p90=51.82 p99=52.02 p999=52.90
I20260525 10:45:47.948453 147483 PressTestClient.cpp:554] Server Memory (%): avg=0.13 min=0.11 max=0.13 p50=0.13 p90=0.83 p99=0.98 p999=1.00
I20260525 10:45:47.948472 147483 PressTestClient.cpp:563] Client Memory (%): avg=0.12 min=0.07 max=0.12 p50=0.12 p90=0.82 p99=0.98 p999=1.00
I20260525 10:45:47.948495 147483 PressTestClient.cpp:592] Rocket Compression (us): avg=8.67 p50=7 p90=11 p99=19 p999=52
I20260525 10:45:47.948509 147483 PressTestClient.cpp:600] Rocket Decompression (us): avg=6.80 p50=6 p90=9 p99=14 p999=42
I20260525 10:45:47.948514 147483 PressTestClient.cpp:609] Request Serialization (us) : avg=1.44 p50=1 p90=8 p99=9 p999=16
I20260525 10:45:47.948518 147483 PressTestClient.cpp:617] Response Deserialization (us) : avg=3.08 p50=2 p90=8 p99=9 p999=19
I20260525 10:45:47.948522 147483 PressTestClient.cpp:625] ================================
```

### server 端口，等待client执行完毕后，ctrl c退出会显示压缩序列化耗时信息

```bash
I20260525 10:45:27.619612 147412 PressServer.cpp:161] PerfTest Server running on port: 8002 with 1 IO threads and 1 CPU threads (Rocket transport)
I20260525 10:45:27.621663 147412 ThriftServer.cpp:970] Resource pools check complete - allowed
I20260525 10:45:27.621758 147412 ThriftServer.cpp:655] Using thread manager (resource pools not enabled) thriftFlagNotSet,  on address/port 8002 enable gflag:0 disable gflag:0
^CI20260525 10:47:05.025774 147412 PressServer.cpp:118] ==================================
I20260525 10:47:05.025821 147412 PressServer.cpp:25] ====== Server Header Compression Stats =======
I20260525 10:47:05.025830 147412 PressServer.cpp:26] Header Compression (us): avg=0.00 p50=0 p90=0 p99=0 p999=0
I20260525 10:47:05.026643 147412 PressServer.cpp:33] Header Decompression (us): avg=0.00 p50=0 p90=0 p99=0 p999=0
I20260525 10:47:05.027030 147412 PressServer.cpp:43] ====== Server Rocket Compression Stats =======
I20260525 10:47:05.027035 147412 PressServer.cpp:44] Rocket Compression (us): avg=12.31 p50=11 p90=18 p99=23 p999=59
I20260525 10:47:05.028712 147412 PressServer.cpp:51] Rocket Decompression (us): avg=8.26 p50=7 p90=9 p99=17 p999=38
I20260525 10:47:05.030068 147412 PressServer.cpp:61] ====== Server Request Deserialization Stats =======
I20260525 10:47:05.030072 147412 PressServer.cpp:62] Request Deserialization (us): avg=2.09 p50=2 p90=8 p99=9 p999=16
I20260525 10:47:05.031405 147412 PressServer.cpp:72] ====== Server Response Serialization Stats =======
I20260525 10:47:05.031409 147412 PressServer.cpp:73] Response Serialization (us): avg=2.07 p50=2 p90=8 p99=9 p999=15
I20260525 10:47:05.032740 147412 PressServer.cpp:124] ==================================
I20260525 10:47:05.032744 147412 PressServer.cpp:125] Received signal 2, stopping server...
I20260525 10:47:05.036387 147412 PressServer.cpp:167] Server exited gracefully.
```
