# uArchBench

微架构行为与指令时延/吞吐基准测试框架。

## 架构支持

| 架构 | 状态 | 说明 |
|------|------|------|
| **AArch64** | 主要支持 | ARM64 架构，支持 NEON 向量指令 |

## 项目结构

```
uArchBench/
├── benches/
│   ├── instruction/          # 指令时延/吞吐测试
│   │   ├── arithmetic/       # 算术指令
│   │   ├── memory/           # 内存指令
│   │   ├── branch/           # 分支指令
│   │   └── simd/             # 向量指令 (NEON)
│   └── microarch/            # 微架构行为测试
│       ├── cache/            # 缓存延迟与带宽
│       ├── memory/           # 内存行为测试
│       ├── pipeline/         # 流水线特性
│       └── prefetch/         # 预取效果
├── cmake/toolchains/         # 工具链配置
├── debug/                    # 单函数调试工具
├── include/                  # 公共头文件
├── src/                      # 主程序
├── results/                  # 测试结果归档目录
└── tools/                    # 构建与运行脚本
```

## 构建方法

### 使用构建脚本

```bash
# AArch64 默认构建
./tools/build.sh -t aarch64-gcc

# AArch64 使用 Clang
./tools/build.sh -t aarch64-clang

# 清理后重新构建
./tools/build.sh -c -t aarch64-gcc
```

### 手动 CMake 构建

```bash
mkdir build && cd build

# AArch64 GCC
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/aarch64-linux-gcc.cmake

# AArch64 Clang
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/aarch64-linux-clang.cmake

make -j$(nproc)
```

## 运行测试

```bash
# 运行测试，结果保存到 build/results/
./tools/run_bench.sh

# 指定结果输出目录
./build/src/uarch_bench -o /path/to/results

# 手动指定CPU频率
./build/src/uarch_bench -f 2.6
```

## 测试项目

### 指令测试

| 类别 | 测试项 | 说明 |
|------|--------|------|
| **算术指令** | ADD, SUB, MUL, DIV | 整数运算延迟与吞吐 |
| **内存指令** | LOAD, STORE | 内存访问延迟与吞吐 |
| **分支指令** | BRANCH (taken/not taken) | 分支预测开销 |
| **向量指令** | NEON ADD | SIMD 运算延迟与吞吐 |

### 微架构测试

| 类别 | 测试项 | 说明 |
|------|--------|------|
| **内存行为** | Store-to-Load Forwarding | 存储转发延迟 |
| **缓存** | Cache Latency/Bandwidth | 各级缓存性能 |
| **流水线** | Pipeline Characteristics | 流水线并行度 |
| **预取** | Prefetch Effect | 软件预取效果 |

## 测试原理

### Latency 测试
建立数据依赖链，测量单条指令执行延迟。例如：
- ADD: `add x0, x0, #1` (结果依赖上一条指令)
- LOAD: `ldr x1, [x1]` (地址依赖上一条加载)

### Throughput 测试
使用独立寄存器并行执行，测量指令吞吐量。例如：
- ADD: 16个不同寄存器并行执行 `add x0-x15, #1`

## ARM64 寄存器使用约定

汇编测试函数必须遵守 AArch64 调用约定，正确使用寄存器：

### 通用寄存器 (X0-X30)

| 寄存器 | 类型 | 说明 |
|--------|------|------|
| **x0-x7** | Caller-saved | 函数参数/返回值，可自由使用 |
| **x8-x17** | Caller-saved | 临时寄存器，可自由使用 |
| **x18-x30** | Callee-saved | **必须保存恢复**，否则会崩溃 |

特殊寄存器：
- `x29` = Frame Pointer (FP)
- `x30` = Link Register (LR) - 修改会导致 `ret` 指令跳转到错误地址

### NEON 向量寄存器 (V0-V31)

| 寄存器 | 类型 | 说明 |
|--------|------|------|
| **v0-v7** | Caller-saved | 可自由使用 |
| **v8-v15** | Callee-saved | **仅低64位需要保存** (d8-d15) |
| **v16-v31** | Caller-saved | 可自由使用 |

### 本项目使用的寄存器

Throughput 测试只使用 Caller-saved 寄存器：
- 通用寄存器: `x0-x17` (共18个，足够32次展开)
- NEON 寄存器: `v0-v7, v16-v31` (共24个，足够32次展开)

## 计时方式

- 使用 `clock_gettime(CLOCK_MONOTONIC_RAW)` 进行高精度计时
- 通过基准测试测量 CPU 频率，将纳秒转换为周期数

## Debug 调试工具

debug 目录提供单函数调试工具，用于：
- 单独测试某个指令的延迟/吞吐
- 使用 perf 采集 PMU 事件
- 使用 objdump 分析汇编代码

### 使用方法

```bash
# 构建后，debug_bench 位于 build/debug/
./build/debug/debug_bench

# 指定迭代次数
./build/debug/debug_bench 50000000

# 使用 perf 分析
perf stat -e cycles,instructions ./build/debug/debug_bench
perf record -g ./build/debug/debug_bench
perf annotate

# 查看汇编代码
objdump -d ./build/debug/debug_bench
```

### 切换测试函数

修改 `debug/debug_bench.c` 中的函数调用即可测试不同指令：

```c
// 将 bench_add_latency 改为其他函数
bench_sub_latency(iterations);    // 测试 SUB
bench_mul_latency(iterations);    // 测试 MUL
bench_load_latency(iterations, buffer);  // 测试 LOAD
```

## 编译选项

默认使用 `-O3` 优化等级，确保测试结果的准确性。

## 工具链

| 工具链 | 文件 | 用途 |
|--------|------|------|
| aarch64-gcc | `aarch64-linux-gcc.cmake` | AArch64 GCC 编译 |
| aarch64-clang | `aarch64-linux-clang.cmake` | AArch64 Clang 编译 |
