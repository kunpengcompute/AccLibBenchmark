# RDMA Performance Test (brpc)

本目录包含基于 brpc 的性能压测工具。为了支持压缩阶段的性能埋点（Latency Recorder），需要对 brpc 源码进行环境适配及代码注入。

## 1. brpc 环境准备与安装

由于 brpc (v1.15.0) 使用了较旧的 CMake 风格，未自动处理库依赖，导致链接时可能找不到 `utf8_range` 和 `snappy`。需按以下步骤进行适配。

### 源码获取
```bash
git clone -b 1.15.0 https://github.com/apache/brpc.git
cd brpc
```

### 步骤 A：应用构建补丁
应用以下 Patch 以解决链接报错问题：

<details>
<summary>点击查看 CMakeLists.txt Patch 内容</summary>

```diff
diff --git a/CMakeLists.txt b/CMakeLists.txt
index 63f3ee06..f19b79d8 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -177,6 +177,8 @@ if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
 endif()
 
 find_package(Protobuf REQUIRED)
+find_package(utf8_range REQUIRED)
+find_package(Snappy REQUIRED)
 if(Protobuf_VERSION GREATER 4.21)
      # required by absl
      set(CMAKE_CXX_STANDARD 17)
@@ -295,7 +297,9 @@ include_directories(
 set(DYNAMIC_LIB
      ${GFLAGS_LIBRARY}
      ${PROTOBUF_LIBRARIES} ${protobuf_ABSL_USED_TARGETS}
+    utf8_range::utf8_validity
      ${LEVELDB_LIB}
+    Snappy::snappy
      ${PROTOC_LIB}
      ${CMAKE_THREAD_LIBS_INIT}
      ${THRIFT_LIB}
```
</details>

### 步骤 B：替换业务源码
请将本项目 `brpc/src/brpc` 目录下的源文件，复制并替换 brpc 源码中的对应文件：

1. **替换** `src/brpc/compress.h`
2. **替换** `src/brpc/compress.cpp`
3. **替换** `src/brpc/policy/baidu_rpc_protocol.cpp`

### 编译安装 brpc
```bash
# 建议安装至独立目录
cmake -B build -S . \
  -DCMAKE_PREFIX_PATH="${HOME}/.local" \
  -DCMAKE_INSTALL_PREFIX=${HOME}/.local/brpc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CPP_FLAGS="-Wno-deprecated-declarations"

cmake --build build --parallel $(nproc)
cmake --install build
```

---

## 2. 执行编译 (rdma_performance)

从 [AccLibBenchmark](https://gitcode.com/boostkit/AccLibBenchmark) 下载代码，进入对应的工具目录：

```bash
cd RPC/brpc/example/rdma_performance/
```

使用以下命令进行配置与编译。请确保路径指向你之前安装依赖的 `${HOME}/.local` 目录：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="${HOME}/.local/brpc;${HOME}/.local/protobuf;${HOME}/.local/gflags;${HOME}/.local/leveldb;${HOME}/.local/snappy"
cmake --build build --parallel
```

---

## 3. 压测运行示例

### 启动 Server
```bash
taskset -c 304-311 ./build/server \
  --use_rdma=false \
  --port=8002 \
  --response_compress_type=snappy
```

### 启动 Client
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

### 性能观测
若需在终端实时查看埋点的压缩耗时明细，请在启动参数中加入：
`--log_rpc_compress_details=true`