# fbthrift Benchmark

这个项目基于 `fbthrift` 对序列化/反序列化进行 benchmark，当前程序为：

- `bm_arr`：数组类数据（`varint` / `double` / `string`）的序列化与反序列化基准

支持两种 fbthrift 协议：

- `compact`
- `binary`

## 1. 环境要求

- Linux/macOS Bash 环境（脚本为 `sh/bash`）
- `cmake`
- C++ 编译器（支持 C++17）
- `thrift1`（或 `thrift`）可执行程序
- 已编译安装的 fbthrift 库（如 `thriftcpp2` / `thriftprotocol` / `folly`）

需要设置环境变量：

- `FBTHRIFT_HOME`：fbthrift 根目录（包含 `bin`、`include`、`lib`）

示例：

```bash
export FBTHRIFT_HOME=/path/to/fbthrift
```

> 你当前机器上可参考：`C:\Users\gxt\code\fbthrift\fbthrift`

## 2. 项目结构说明

- `arr_data.thrift`：数组类 benchmark 的 thrift 定义（`Varint32Data` / `Varint64Data` / `DoubleData` / `StringData`）
- `benchmark_for_arr_data.cpp`：`bm_arr` 源码
- `CMakeLists.txt`：编译规则（自动调用 thrift 生成 `gen-cpp2` 代码）
- `compile.sh`：清理并编译
- `run.sh`：运行 `bm_arr`，支持协议参数
- `clean.sh`：清理构建产物

## 3. 编译方式

在项目根目录执行：

```bash
./compile.sh
```

它会执行以下流程：

1. 校验 `FBTHRIFT_HOME`
2. 清理旧产物（`build`、`gen-cpp2`、`bin`）
3. `cmake ..`
4. `make -j`

编译成功后，二进制输出在：

- `bin/bm_arr`

## 4. 运行方式

### 4.1 运行数组类型 benchmark（bm_arr）

`run.sh` 支持协议参数：

```bash
./run.sh [compact|binary]
```

参数定义：

- `compact`：使用 `CompactSerializer`（默认）
- `binary`：使用 `BinarySerializer`

示例：

```bash
# 默认 compact
./run.sh

# 显式 compact
./run.sh compact

# binary 协议
./run.sh binary
```

`run.sh` 默认运行 `bm_arr`，也可直接执行：

```bash
FBTHRIFT_PROTOCOL=compact LD_LIBRARY_PATH=$FBTHRIFT_HOME/lib ./bin/bm_arr
```

或：

```bash
FBTHRIFT_PROTOCOL=binary LD_LIBRARY_PATH=$FBTHRIFT_HOME/lib ./bin/bm_arr
```

## 5. 参数与行为说明

### 5.1 协议参数

- 参数来源：
  - `run.sh` 的第一个参数（`compact|binary`）
  - 或手动设置 `FBTHRIFT_PROTOCOL`
- 默认值：`compact`

### 5.2 数据类型覆盖（`bm_arr`）

- `varint32`：使用 thrift `list<i32>`（按 1~5 字节均匀采样，单个字节档位各 20%）
- `varint64`：使用 thrift `list<i64>`（按 1~10 字节均匀采样）
- `double`：使用 thrift `list<double>`
- `string`：使用 thrift `list<string>`

每种类型都包含：

- 序列化 benchmark
- 反序列化 benchmark

## 6. 一键编译+运行

已有脚本：

```bash
./compile-and-run.sh
```

该脚本默认走 `./run.sh` 的默认协议（`compact`）。

如果你需要 `binary`，建议手动：

```bash
./compile.sh
./run.sh binary
```

## 7. 常见问题

- 提示 `environment FBTHRIFT_HOME must exist!`
  - 说明没有设置 `FBTHRIFT_HOME`，先 `export` 后再执行脚本
- 提示找不到 `cmake`
  - 安装 `cmake` 并确保在 `PATH` 中
- 提示找不到 `thrift1/thrift`
  - 确保 `${FBTHRIFT_HOME}/bin` 下有 thrift 编译器，或系统 `PATH` 可找到

