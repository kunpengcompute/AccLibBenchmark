# RDMA Performance Test (brpc)

本目录包含基于 brpc 的性能压测工具。为了支持压缩阶段的性能埋点（Latency Recorder）和 JSON 解析性能测试（sonic-cpp），需要对 brpc 源码进行环境适配及代码注入。

## 1. brpc 环境准备与安装

由于 brpc (v1.15.0) 使用了较旧的 CMake 风格，未自动处理库依赖，导致链接时可能找不到 `utf8_range` 和 `snappy`。需按以下步骤进行适配。

### 源码获取
```bash
git clone -b 1.15.0 https://github.com/apache/brpc.git /path/to/brpc-1.15.0
```

### 一键应用 brpc 覆盖补丁

本项目的 `RPC/brpc` 目录按 brpc 1.15.0 源码根目录的相对路径排列，用于覆盖到一份干净 brpc 1.15.0 源码树中。不要手动逐个复制文件，使用脚本统一应用：

```bash
cd /path/to/AccLibBenchmark/RPC

# 先预览会覆盖哪些文件
./apply_brpc_patch.sh --dry-run /path/to/brpc-1.15.0

# 确认后执行覆盖
./apply_brpc_patch.sh /path/to/brpc-1.15.0
```

路径对应关系示例：

```text
RPC/brpc/CMakeLists.txt      -> /path/to/brpc-1.15.0/CMakeLists.txt
RPC/brpc/src/CMakeLists.txt  -> /path/to/brpc-1.15.0/src/CMakeLists.txt
RPC/brpc/example/...         -> /path/to/brpc-1.15.0/example/...
RPC/brpc/test/...            -> /path/to/brpc-1.15.0/test/...
```

脚本只检查目标目录是否像 brpc 源码根目录；版本基线由前面的 `git clone -b 1.15.0` 保证。

覆盖补丁包含：

- brpc 根 `CMakeLists.txt`：增加 `WITH_SONIC_JSON`、`SONIC_CPP_INCLUDE_DIR`、`SONIC_CPP_COMPILE_OPTIONS`，补充 `utf8_range` / `Snappy::snappy` 链接依赖，并保持 brpc 1.15.0 基线。
- `src/CMakeLists.txt` / `test/CMakeLists.txt`：将 sonic 编译参数限制在 sonic 相关源文件，避免影响 brpc 无关源文件。
- `src/json2pb/*`：增加 `--pb2json_engine`、`--json2pb_engine` 和 sonic 分发逻辑。
- `src/google/protobuf/stubs/logging.h`：补充 protobuf 5.28.3 兼容所需的 logging shim。
- `example/rdma_performance/*`：增加 JSON payload 压测、rapidjson/sonic server 分支、JSON 延迟指标和 ksonic server 编译参数支持。
- `test/brpc_protobuf_json_unittest.cpp`：补充 json2pb/pb2json sonic 相关单测和性能用例。

### 获取 sonic-cpp 头文件
```bash
git clone https://github.com/bytedance/sonic-cpp.git /path/to/sonic-cpp
```

### 编译安装 brpc

**不带 sonic-cpp：**
```bash
cmake -B build -S . \
  -DCMAKE_PREFIX_PATH="${HOME}/.local" \
  -DCMAKE_INSTALL_PREFIX=${HOME}/.local/brpc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CPP_FLAGS="-Wno-deprecated-declarations"

cmake --build build --parallel $(nproc)
cmake --install build
```

**带 sonic-cpp：**
```bash
cmake -B build -S . \
  -DWITH_SONIC_JSON=ON \
  -DSONIC_CPP_INCLUDE_DIR=/path/to/sonic-cpp/include \
  -DCMAKE_PREFIX_PATH="${HOME}/.local" \
  -DCMAKE_INSTALL_PREFIX=${HOME}/.local/brpc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CPP_FLAGS="-Wno-deprecated-declarations"

cmake --build build --parallel $(nproc)
cmake --install build
```

**带 ksonic-cpp（SVE2 256）：**
```bash
cmake -B build -S . \
  -DWITH_SONIC_JSON=ON \
  -DSONIC_CPP_INCLUDE_DIR=/path/to/ksonic-cpp \
  "-DSONIC_CPP_COMPILE_OPTIONS=-march=armv8-a+sve+crc+sve2 -msve-vector-bits=256" \
  -DCMAKE_PREFIX_PATH="${HOME}/.local" \
  -DCMAKE_INSTALL_PREFIX=${HOME}/.local/brpc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CPP_FLAGS="-Wno-deprecated-declarations"

cmake --build build --parallel $(nproc)
cmake --install build
```

`SONIC_CPP_COMPILE_OPTIONS` 只作用于 sonic 相关源文件，避免 SVE2 等架构编译参数影响 brpc 无关源文件。ksonic-cpp 运行时仍使用 `--json2pb_engine=sonic` / `--pb2json_engine=sonic` 或 `--json_engine=sonic`，区别来自编译期头文件和编译参数。

---

## 2. 执行编译 (rdma_performance)

从 [AccLibBenchmark](https://gitcode.com/boostkit/AccLibBenchmark) 下载代码，进入对应的工具目录：

```bash
cd RPC/brpc/example/rdma_performance/
```

使用以下命令进行配置与编译。请确保路径指向你之前安装依赖的 `${HOME}/.local` 目录：

**不带 sonic-cpp：**
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="${HOME}/.local/brpc;${HOME}/.local/protobuf;${HOME}/.local/gflags;${HOME}/.local/leveldb;${HOME}/.local/snappy"
cmake --build build --parallel
```

**带 sonic-cpp：**
```bash
cmake -S . -B build \
  -DWITH_SONIC_JSON=ON \
  -DSONIC_CPP_INCLUDE_DIR=/path/to/sonic-cpp/include \
  -DCMAKE_PREFIX_PATH="${HOME}/.local/brpc;${HOME}/.local/protobuf;${HOME}/.local/gflags;${HOME}/.local/leveldb;${HOME}/.local/snappy"
cmake --build build --parallel
```

**带 ksonic-cpp（SVE2 256）：**
```bash
cmake -S . -B build \
  -DWITH_SONIC_JSON=ON \
  -DSONIC_CPP_INCLUDE_DIR=/path/to/ksonic-cpp \
  "-DSONIC_CPP_COMPILE_OPTIONS=-march=armv8-a+sve+crc+sve2 -msve-vector-bits=256" \
  -DCMAKE_PREFIX_PATH="${HOME}/.local/brpc;${HOME}/.local/protobuf;${HOME}/.local/gflags;${HOME}/.local/leveldb;${HOME}/.local/snappy"
cmake --build build --parallel
```

---

## 3. 压测运行示例

### 普通 RPC 压测

#### 启动 Server
```bash
taskset -c 304-311 ./build/server \
  --use_rdma=false \
  --port=8002 \
  --response_compress_type=snappy
```

#### 启动 Client
```bash
taskset -c 320-335 ./build/client \
  --use_rdma=false \
  --servers=127.0.0.1:8002 \
  --thread_num=16 \
  --queue_depth=10 \
  --attachment_size=1024 \
  --test_seconds=10 \
  --payload_mode=message \
  --request_compress_type=snappy \
  --data_file=./request_data.pb
```

### JSON 压测

#### 启动 Server（启用 JSON 压测）
```bash
taskset -c 304-311 ./build/server \
  --use_rdma=false \
  --port=8002 \
  --enable_json_bench=true \
  --json_engine=sonic
```

#### 启动 Client（JSON 载荷模式）
```bash
taskset -c 320-335 ./build/client \
  --use_rdma=false \
  --servers=127.0.0.1:8002 \
  --thread_num=16 \
  --queue_depth=10 \
  --test_seconds=10 \
  --payload_mode=json \
  --json_data_dir=/path/to/json/testdata
```

JSON 压测模式下，Client 会从 `--json_data_dir` 指定的目录中加载所有 `.json` 文件，轮询发送到 Server。Server 按 `--json_engine` 指定的 `rapidjson` 或 `sonic` 对 JSON 进行解析和序列化，并记录延迟指标。

#### JSON payload 模式说明

`rdma_performance` 的 `--payload_mode=json` 不是 HTTP raw JSON body，也不是 brpc 的 json2pb 自动转换路径。该模式下：

1. Client 从 `--json_data_dir` 加载所有 `.json` 文件。
2. 每个 JSON 文件内容被原样读成字符串。
3. Client 将该字符串写入 protobuf `Request.bytes payload` 字段。
4. 整个 `Request` 仍按 protobuf RPC 正常序列化、发送、反序列化。
5. Server 从 `request->payload()` 取出 JSON 字符串。
6. 仅当 `--enable_json_bench=true` 时，Server 才使用 `--json_engine` 指定的 `sonic` 或 `rapidjson` 手动解析并重新序列化 JSON。

因此该模式包含两层处理：

- 外层：protobuf `Request` 的序列化/反序列化。
- 内层：可选的 JSON parse/serialize benchmark。

`--json_data_dir` 下的 JSON 文件没有固定 schema 要求，只要是目标 JSON 引擎可解析的合法 JSON 即可。不同 JSON 数据集的结构、大小、嵌套深度、字符串比例、数字比例、数组/对象分布都会显著影响解析性能，因此 benchmark 结果必须和具体数据集一起说明。

注意：`--pb2json_engine` 和 `--json2pb_engine` 属于 brpc json2pb/pb2json 功能路径，用于 protobuf message 与 JSON 表示之间的字段映射转换；它们不是 `payload_mode=json` 的数据发送方式。

与 `RPC/brpc_bench --payload_format=raw_json` 不同，`rdma_performance --payload_mode=json` 不发送 HTTP raw body。前者是 HTTP/H2 body 直接承载 JSON，后者是 protobuf `bytes payload` 字段承载 JSON 文本。

当前 `rdma_performance` 的 client/server JSON payload 压测不会调用 `json2pb::JsonToProtoMessage` 或 `json2pb::ProtoMessageToJson`。brpc json2pb/pb2json 模块已在 `src/json2pb` 和 `brpc_protobuf_json_unittest` 中完成 rapidjson/sonic 切换适配；后续如需在 AccLibBenchmark client/server 端到端压测中覆盖 JSON 文本与 protobuf message 字段映射转换，可新增独立的 json2pb/pb2json payload 模式作为 TODO。

输出中会额外显示：
- `JsonParseAvg` / `JsonParseP99` — 服务端 JSON 解析延迟
- `JsonSerializeAvg` / `JsonSerializeP99` — 服务端 JSON 序列化延迟

### JSON 引擎 gflags 说明

| gflag | 作用域 | 默认值 | 说明 |
|-------|--------|--------|------|
| `--pb2json_engine` | brpc json2pb (pb→json) | `rapidjson` | 可选 `rapidjson` 或 `sonic` |
| `--json2pb_engine` | brpc json2pb (json→pb) | `rapidjson` | 可选 `rapidjson` 或 `sonic` |
| `--enable_json_bench` | rdma_performance server | `false` | 是否在服务端启用 JSON 解析/序列化压测 |
| `--json_engine` | rdma_performance server | `sonic` | 服务端 JSON 压测使用的引擎 |
| `--json_data_dir` | rdma_performance client | `""` | JSON 测试数据文件所在目录 |
| `--payload_mode` | rdma_performance client | `message` | 载荷模式：`message`/`attachment`/`json` |

### 性能观测
若需在终端实时查看埋点的压缩耗时明细，请在启动参数中加入：
`--log_rpc_compress_details=true`
