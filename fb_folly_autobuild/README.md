# fbthrift/Folly Benchmark 安装与运行说明

本目录用于在不同构建机上准备 fbthrift/Folly benchmark，并批量运行
`press_client`，最后把吞吐、延迟、QPS 和成功率保存为 CSV。

当前安装方式如下：

- Clang 16：优先复用系统或已有安装，找不到时仍可从本地 `tar.xz` 解压；
- Folly、Fizz、Wangle、fbthrift 和 fbBench：统一通过 HTTPS `git clone` 获取；
- 暂不提供上述源码仓库的本地 tar 安装分支。

目录中的主要文件：

- `install.py`：安装系统依赖、准备 Clang、通过 HTTPS clone 获取源码、应用补丁、生成 Thrift 代码并编译 benchmark；
- `run.py`：按照配置好的连接数、队列深度和数据类型运行 benchmark 矩阵；
- `fbthrift.patch`：构建 fbthrift 前检查并在需要时应用的补丁；
- `clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz`：机器没有可用 Clang 16 时使用的本地编译器归档。

当前配置示例使用 `apt-get`/`dnf`、`numactl` 和 Linux 风格的动态库路径，默认 Clang 归档也是 `aarch64-linux-gnu`。换到其他 Unix/Linux 平台时，应按目标平台调整包列表、运行命令和 Clang 归档。

脚本默认从 `/usr/include/glog` 和 `/usr/include/fmt` 准备 system header overlay，并让 CMake 忽略 `/usr/local` 中可能冲突的 glog、fmt 和动态库安装。如果目标平台的系统头文件或 CMake package 布局不同，需要同步调整 `prepare_dependency_overlays()`、`FMT_CMAKE_DIR_CANDIDATES` 和相关路径。

脚本不会根据平台自动决定是否启用 SVE2。如果目标平台或 CPU 不支持 ARM SVE2，把 `THRIFT_ENABLE_ARM_SVE2` 改为 `False`。

## 一、快速开始

### 1. 修改最少的配置

先打开 `install.py` 的配置区，至少确认 `WORK` 是目标机器上的可写目录：

```python
WORK = Path("/data/your-user/fbthrift-work")
INS = WORK / "ins"
```

`WORK` 会保存 Git 源码、CMake build 目录和 benchmark；`INS` 会保存 Folly、Fizz、Wangle 和 fbthrift 的安装结果。

首次安装应保持：

```python
AUTO_REENTRY = True
SKIP_SOURCE_CLONE = False
```

其中：

- `AUTO_REENTRY=True`：目录已经存在时复用已有源码，便于修改源码后重复构建；
- `SKIP_SOURCE_CLONE=False`：缺少源码目录时允许脚本执行 `git clone`。

当前脚本中的 `INSTALL_PACKAGES` 默认为 `False`。如果需要脚本自动使用 `apt-get` 或 `dnf` 安装依赖，改为：

```python
INSTALL_PACKAGES = True
```

如果继续保持 `False`，需要提前安装配置区列出的编译工具、Git、CMake、系统开发包以及 glog/fmt 头文件。

首次 clone 还要求构建机能够通过 HTTPS 访问以下站点：

- `gitcode.com`；
- `github.com`。

### 2. 准备 Clang 16

脚本会按以下顺序寻找 Clang 16：

1. `PATH` 中的 `clang-16` 与 `clang++-16`；
2. `CLANG16_DIR` 中已经解压的 Clang；
3. `CLANG16_TARBALL` 指向的本地 `tar.xz`。

如果机器没有可用的 Clang 16，把与目标平台和架构匹配的 `clang*.tar.xz` 放到本目录，或修改 `CLANG16_TARBALL`。当前默认值是：

```python
CLANG16_TARBALL = (
    SCRIPT_DIR / "clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz"
)
```

换到 x86_64 或其他系统时，应替换为对应归档。如果系统已经有可用的 Clang 16，则不需要提供这个文件。

Clang 16 是当前唯一保留的本地 tar 安装路径。Folly、Fizz、Wangle、fbthrift 和 benchmark 不再从本地源码归档安装。

### 3. 执行安装

```bash
cd /path/to/install
python3 install.py
```

一次完整的新安装会按配置执行以下步骤：

1. 当 `INSTALL_PACKAGES=True` 时，使用 `apt-get` 或 `dnf` 安装依赖；
2. 准备 system glog/fmt header overlay 和 system fmt CMake package；
3. 准备并测试 Clang 16；
4. 通过 HTTPS clone Folly、Fizz、Wangle 和 fbthrift；
5. 检查 fbthrift 是否已经包含 `fbthrift.patch`，未包含时使用 Git 应用补丁；
6. 配置、清理、编译并安装 Folly、Fizz、Wangle 和 fbthrift；
7. 通过 HTTPS clone fbBench 到 `WORK/fbthrift_folly_benchmark`；
8. 按需生成 benchmark 所需的 `gen-cpp2`；
9. 编译 `fbthrift_folly_benchmark/build/press_client`。

`clone_repo()` 会执行带 submodule 的 clone，并在配置了 ref 时限制到指定分支或标签。所有仓库地址必须以 `https://` 开头，否则脚本会直接报错。

### 4. 运行 benchmark

安装成功后：

```bash
python3 run.py
```

结果默认写到当前命令行目录下的 `benchmark_results.csv`。当前矩阵为 11 种数据设置 × 5 种连接设置 × 3 种队列深度，共 165 个 case。完整运行时间取决于 `--test_seconds=120`、机器性能和失败重试情况。

## 二、HTTPS Git 源码获取

### 当前仓库和版本

`install.py` 默认配置如下：

```python
FOLLY_URL = "https://gitcode.com/boostkit/folly.git"
FOLLY_REF = "dev_iouring"

FIZZ_URL = "https://github.com/facebookincubator/fizz.git"
FIZZ_REF = "v2022.11.14.00"

WANGLE_URL = "https://github.com/facebook/wangle.git"
WANGLE_REF = "v2022.11.14.00"

FBTHRIFT_URL = "https://gitcode.com/boostkit/fbthrift.git"
FBTHRIFT_REF = "dev_20221114"

BENCHMARK_URL = "https://gitcode.com/donghuanan/fbBench.git"
BENCHMARK_REF = "main"
```

对应关系：

| 组件 | HTTPS 仓库 | 分支或标签 |
| --- | --- | --- |
| Folly | `FOLLY_URL` | `dev_iouring` |
| Fizz | `FIZZ_URL` | `v2022.11.14.00` |
| Wangle | `WANGLE_URL` | `v2022.11.14.00` |
| fbthrift | `FBTHRIFT_URL` | `dev_20221114` |
| fbBench | `BENCHMARK_URL` | `main` |

`clone_repo()` 实际使用的形式相当于：

```bash
git clone --recurse-submodules \
  --branch <REF> \
  --single-branch \
  <HTTPS_URL> \
  <DESTINATION>
```

如果某个仓库把 `REF` 配置为 `None`，脚本会 clone 该仓库的默认分支，并省略 `--branch` 和 `--single-branch`。

### 更换源码版本

需要切换版本时，修改对应的 `*_REF`。例如：

```python
FIZZ_REF = "your-tag-or-branch"
WANGLE_REF = "your-tag-or-branch"
```

脚本不会在已经存在的源码目录中自动执行 `git fetch`、`git pull` 或 `git checkout`。修改 ref 后，需要先移动或删除对应源码目录，再运行 `install.py`：

```bash
rm -rf "$WORK/fizz"
python3 install.py
```

其中 `$WORK` 需要替换成 `install.py` 中实际配置的路径。删除前必须确认路径无误。

Fizz、Wangle、fbthrift 和 Folly 之间存在版本依赖关系。切换其中一个组件时，应同步确认其他组件的 API 和 CMake 配置仍然兼容。

### benchmark 源码目录处理

fbBench 仓库根目录本身就是当前 benchmark 工程，包含 `press.thrift`、CMake 配置以及 `press_client`/`press_server` 源码。脚本会直接 clone 到：

```text
WORK/fbthrift_folly_benchmark
```

因此最终可执行文件路径仍然是：

```text
WORK/fbthrift_folly_benchmark/build/press_client
```

虽然远程仓库名是 `fbBench`，本地 destination 仍保留 `fbthrift_folly_benchmark`，从而不需要同步修改现有 `run.py` 的执行路径。

## 三、重新进入、修改源码和重建

### 默认的可重入行为

脚本默认支持重复执行：

```python
AUTO_REENTRY = True
```

当 `WORK/folly`、`WORK/fizz`、`WORK/wangle`、`WORK/fbthrift` 或最终 benchmark 目录已经存在时，脚本会复用它们，不会再次 clone，也不会覆盖本地修改。因此可以直接修改源码后再次执行：

```bash
python3 install.py
```

对于 Folly、Fizz、Wangle 和 fbthrift，复用目录必须是 Git checkout，也就是目录中需要有 `.git`。旧版脚本解压得到、但没有 `.git` 的源码目录不会被当作当前 clone 模式的有效源码；应先将其移走或删除，再由新脚本重新 clone。

fbthrift 补丁会先执行反向检查。已经应用过时会自动跳过，因此重复执行不会因为重复打补丁而失败。已有的 `gen-cpp2` 默认也会复用，以保留对生成代码的手工修改。

如果希望发现“目录已经存在”就立即报错，把 `AUTO_REENTRY` 改为 `False`。这适合验证一次全新的安装流程，不适合日常源码迭代。

### 明确禁止新的 clone

当所有源码都已准备完成，并且希望缺少目录时立即报错，可以设置：

```python
SKIP_SOURCE_CLONE = True
```

此时：

- 已有源码目录仍会复用；
- 缺少任何需要 clone 的源码目录都会立即报错；
- 脚本不会访问远程 Git 仓库；
- CMake 配置、清理、编译和安装仍会继续执行。

常用组合是：

```python
AUTO_REENTRY = True
SKIP_SOURCE_CLONE = True
```

首次安装必须保持 `SKIP_SOURCE_CLONE=False`。

### CMake 的三个阶段

脚本中的主要构建函数是：

- `cmake(...)`：执行 `cmake -S ... -B ...`，只负责配置；
- `cmake_build(...)`：先执行 `cmake --build <dir> --target clean`，再编译；如果传入 install 目录，最后执行 `cmake --install`。

重复执行会保留 `CMakeCache.txt` 和 generator 配置，但会清掉旧的目标文件后重新编译，适合验证修改过的源码。

如果更换了编译器、CMake generator，或者出现 cache 冲突，只删除 build 目录，不要删除源码目录：

```bash
rm -rf "$WORK/folly/_build" \
       "$WORK/fizz/build_" \
       "$WORK/wangle/build_" \
       "$WORK/fbthrift/build_" \
       "$WORK/fbthrift_folly_benchmark/build"
```

然后再次执行：

```bash
python3 install.py
```

上面的 `$WORK` 需要替换为 `install.py` 中实际的 `WORK` 路径。删除前请确认路径无误。

### glog 和 fmt 覆盖层

`prepare_dependency_overlays()` 会：

1. 检查系统目录 `/usr/include/glog`；
2. 检查系统目录 `/usr/include/fmt`；
3. 在 `WORK/glog-system-overlay/glog` 和 `WORK/fmt-system-overlay/fmt` 创建符号链接；
4. 将两个 overlay 通过 `CFLAGS` 和 `CXXFLAGS` 放到头文件搜索路径前面；
5. 查找 system fmt 的 CMake package；
6. 通过 `CMAKE_IGNORE_PREFIX_PATH` 和 `CMAKE_IGNORE_PATH` 排除 `/usr/local` 下可能冲突的头文件、库和 CMake 配置。

这样可以避免 CMake 或编译器误用 `/usr/local` 中的另一套 glog/fmt。

如果 overlay 中目标路径已经是普通文件或普通目录，脚本会报错而不会覆盖。确认无用后应手动移走，或修改 overlay 配置。

### 重新生成 Thrift 代码

如果修改了 `press.thrift`，或希望重新生成 Thrift 代码，设置：

```python
REGENERATE_THRIFT_SOURCES = True
```

然后再次运行 `install.py`。

如果只修改 C++ 源码而没有改 `press.thrift`，保持 `False` 可以避免覆盖手工维护的 `gen-cpp2`。

### Clang 设置和源码修改

`get_clang16()` 会验证 C/C++ 编译器版本，并编译一个很小的 C 和 C++ 程序进行探测。

使用打包 Clang 时，脚本还会：

- 根据 `gcc -print-file-name=crtbeginS.o` 写入 `clang.cfg` 和 `clang++.cfg`；
- 禁用打包 Clang 中可能与系统库冲突的 bundled `libunwind`；
- 在每次复用已有 Clang 目录时重新执行上述配置和探测。

因此，修改 `WORK` 下的 Folly、Fizz、Wangle 或 fbthrift Git 源码不需要重新解压 Clang。保持 `AUTO_REENTRY=True`，必要时再打开 `SKIP_SOURCE_CLONE=True`，然后直接重建即可。

若目标 CPU 不支持 ARM SVE2，设置：

```python
THRIFT_ENABLE_ARM_SVE2 = False
```

如果 `CLANG16_DIR` 存在但里面没有可用的 Clang 16，脚本会拒绝覆盖这个疑似损坏的目录。请修复它、换一个 `CLANG16_DIR`，或在确认目录无用后手动移走再运行。

## 四、`install.py` 配置区常量说明

### 路径、重入和系统依赖

| 常量 | 含义 |
| --- | --- |
| `SCRIPT_DIR` | `install.py` 所在目录，由脚本自动计算；用于定位 Clang 归档和 `fbthrift.patch`。 |
| `WORK` | 工作根目录，保存 Git 源码、build 目录和 benchmark。需要可写且空间充足。 |
| `INS` | 依赖安装根目录，默认是 `WORK/ins`。CMake 会通过它寻找已安装库。 |
| `GLOG_OVERLAY` | glog 头文件 overlay 目录。 |
| `FMT_OVERLAY` | fmt 头文件 overlay 目录。 |
| `FMT_CMAKE_DIR_CANDIDATES` | system fmt CMake package 的候选目录。也可用环境变量 `FMT_CMAKE_DIR` 覆盖。 |
| `CMAKE_IGNORE_PREFIX_PATH` | CMake 要忽略的安装前缀，当前为 `/usr/local`。 |
| `CMAKE_IGNORE_PATH` | CMake 要忽略的具体头文件、库和 package 路径。 |
| `AUTO_REENTRY` | 为 `True` 时复用已存在源码；为 `False` 时遇到已存在目录就报错。 |
| `SKIP_SOURCE_CLONE` | 为 `True` 时禁止新的 clone，只接受已经存在的源码目录；缺目录会报错。 |
| `INSTALL_PACKAGES` | 为 `True` 时运行 `apt-get`/`dnf` 安装系统包；预装依赖时可设为 `False`。 |
| `REGENERATE_THRIFT_SOURCES` | 为 `True` 时重新运行 `thrift1 --gen mstch_cpp2 press.thrift`。 |

### Clang 和补丁

| 常量 | 含义 |
| --- | --- |
| `CLANG16_TARBALL` | 本地 Clang 16 `tar.xz` 路径；只有 PATH 和 `CLANG16_DIR` 都找不到可用 Clang 时才使用。 |
| `CLANG16_DIR` | Clang 解压目录以及后续复用目录。 |
| `FBTHRIFT_PATCH` | fbthrift 构建前检查并在需要时应用的 patch 文件。当前只对 Git checkout 使用 `git apply --3way`。 |

### Git 源码来源

| 常量 | 含义 |
| --- | --- |
| `FOLLY_URL` / `FOLLY_REF` | Folly 的 HTTPS clone 地址以及分支或标签。 |
| `FIZZ_URL` / `FIZZ_REF` | Fizz 的 HTTPS clone 地址以及分支或标签。 |
| `WANGLE_URL` / `WANGLE_REF` | Wangle 的 HTTPS clone 地址以及分支或标签。 |
| `FBTHRIFT_URL` / `FBTHRIFT_REF` | fbthrift 的 HTTPS clone 地址以及分支或标签。 |
| `BENCHMARK_URL` / `BENCHMARK_REF` | fbBench 的 HTTPS clone 地址以及分支或标签。 |

当前不再定义 `FOLLY_TARBALL`、`FIZZ_TARBALL`、`WANGLE_TARBALL`、`FBTHRIFT_TARBALL` 或 `BENCHMARK_TARBALL`，也不再提供通用源码归档解压函数。

### 源码和安装目录

以下路径默认都由 `WORK`/`INS` 派生；除非需要改变布局，否则不要单独改动：

| 常量 | 含义 |
| --- | --- |
| `FOLLY_DIR` / `FOLLY_INS` | Folly Git 源码目录 / 安装前缀。 |
| `FIZZ_DIR` / `FIZZ_INS` | Fizz Git 源码目录 / 安装前缀。实际 CMake source 是 `FIZZ_DIR/fizz`。 |
| `WANGLE_DIR` / `WANGLE_INS` | Wangle Git 源码目录 / 安装前缀。实际 CMake source 是 `WANGLE_DIR/wangle`。 |
| `FBTHRIFT_DIR` / `FBTHRIFT_INS` | fbthrift Git 源码目录 / 安装前缀。 |
| `BENCHMARK_DIR` | fbBench 的本地 Git checkout；可执行文件位于 `BENCHMARK_DIR/build/press_client`。 |

### 编译参数

| 常量 | 含义 |
| --- | --- |
| `BUILD_TYPE` | CMake 构建类型，当前为 `Release`。 |
| `JOBS` | 并行编译任务数，当前为 `min(CPU 数, 32)`。机器内存不足时可手动调小。 |
| `THRIFT_ENABLE_ARM_SVE2` | 传给 fbthrift 的 `-DTHRIFT_ENABLE_ARM_SVE2=ON/OFF`。 |
| `DEBIAN_PACKAGES` | `apt-get install` 使用的 Debian/Ubuntu 包列表。 |
| `OPENEULER_PACKAGES` | `dnf install` 使用的 openEuler/RPM 包列表。 |

## 五、`run.py` 配置、矩阵和数据采集

### 配置区

| 常量 | 含义 |
| --- | --- |
| `OUTPUT_CSV` | 输出文件名或路径。相对路径相对于运行 `python3 run.py` 时的当前目录。脚本以写模式打开它，每次运行会覆盖旧文件。 |
| `EXECUTABLE` | `press_client` 的完整路径；`PREFIX_CMD` 在模块加载时会使用这个值。 |
| `DRY_RUN` | 为 `True` 时只打印所有命令并写出 CSV 表头，不真正执行 benchmark，也不会写入测量行。 |
| `PREFIX_CMD` | 每个 case 都会使用的固定命令前缀和参数，包含 `numactl`、可执行文件及传输/压测选项。 |
| `CONN_SETUPS` | 连接设置列表，每项格式为 `[cpu_threads, io_threads, connections]`。 |
| `QD_SETUPS` | 队列深度列表，对应动态参数 `--queue_depth`。 |
| `DATA_SETUPS` | 数据设置列表，每项格式为 `[CSV中的显示名称, [传给 press_client 的参数]]`。 |

当前 `PREFIX_CMD` 的固定参数含义如下：

- `numactl -N 1`：绑定 NUMA node 1；只有一个 NUMA node 的机器通常应改成 `-N 0` 或删掉 `numactl`；
- `--mode=close_loop`：闭环压测模式；
- `--port=23456`：服务端端口；
- `--transport=rocket`：使用 Rocket transport；
- `--test_seconds=120`：每个 case 测试 120 秒；
- `--compression=none`：关闭压缩；
- `--rpc_timeout_ms=2000`：RPC 超时时间 2000 ms；
- `--echo_attachment=false`、`--use_io_uring=true`、`--random_fields=true`：分别控制 attachment 回显、io_uring 和随机字段。

### 修改测试矩阵

例如只测试两种连接设置、两个队列深度和一种 payload：

```python
CONN_SETUPS = [
    [8, 8, 500],
    [16, 16, 1000],
]

QD_SETUPS = [10, 50]

DATA_SETUPS = [
    ["binary-256B", ["--fields=none", "--payload_size=256"]],
]
```

如果换了 benchmark 输出路径，只需修改：

```python
EXECUTABLE = (
    "/data/fbthrift-work/"
    "fbthrift_folly_benchmark/build/press_client"
)
```

如果改写了整个 `PREFIX_CMD`，记得同时保留或更新其中的 `EXECUTABLE`。矩阵遍历顺序是 `DATA_SETUPS -> CONN_SETUPS -> QD_SETUPS`，也就是先固定一个数据类型，再遍历所有连接设置和队列深度。

### CSV 字段和解析规则

脚本使用分号 `;` 分隔 CSV，表头为：

```text
cmd;cpu_thread;io_thread;queue_dep;connection;payload;data_types;Throughput;latencyAvg;latencyP99;QPS/成功率
```

脚本从 `press_client` 的标准输出中寻找以下文本：

```text
Payload: 1024B
Throughput: 1330.80MB/s
Latency (us): ... avg=12.3 ... p99=30.1
QPS: 1000 Success: 999 Fail: 1
```

其中：

- `payload` 只保存数字部分，例如 `1024B` 变成 `1024`；
- `Throughput` 当前只匹配 `MB/s`；
- `latencyAvg`、`latencyP99` 保存数字；
- `QPS/成功率` 保存为类似 `1000/99.90%`，成功率由 `Success/(Success+Fail)` 计算；
- 某一项匹配不到时写成 `N/A`，同时把原始输出打印出来，便于更新正则表达式；
- 每完成一个成功或失败的 case 都会立即 flush CSV，并等待 5 秒再进入下一个 case。

因此，运行中断时通常仍能保留已经完成的行；但重新运行 `run.py` 会覆盖原文件。若要保留多次实验结果，请先修改 `OUTPUT_CSV` 或复制旧文件。

## 六、常见问题

### Q1：第一次运行提示 `SKIP_SOURCE_CLONE is enabled but source is missing`。

首次安装应设置：

```python
SKIP_SOURCE_CLONE = False
```

只有所有源码已经准备完成、并且明确不允许访问远程仓库时，才设为 `True`。

### Q2：第二次运行提示目录已经存在怎么办？

保持：

```python
AUTO_REENTRY = True
```

已有 Git checkout 会复用。如果希望全新安装，应先移动或删除对应源码目录，而不是把 `AUTO_REENTRY` 关掉后直接覆盖。

### Q3：旧源码目录报 `existing source is not a Git checkout`。

当前版本不再把本地源码 tar 作为安装来源。旧版脚本解压出的 Folly、Fizz、Wangle 或 fbthrift 目录没有 `.git`，不会被当前 clone 模式接受。

先备份本地修改，然后移走旧目录，再重新运行 `install.py` 让脚本通过 HTTPS clone：

```bash
mv "$WORK/fbthrift" "$WORK/fbthrift.old"
python3 install.py
```

### Q4：我修改了 clone 下来的 C++ 源码，怎样确保不会被覆盖？

保持 `AUTO_REENTRY=True`。脚本会复用已有 Git checkout，不会自动执行 `git reset`、`git pull` 或重新 clone。

还可以设置：

```python
SKIP_SOURCE_CLONE = True
```

这样缺少目录时也不会自动访问远程仓库。若修改的是 `gen-cpp2`，保持 `REGENERATE_THRIFT_SOURCES=False`。

### Q5：找不到 Clang 16，或 Clang 探测失败。

安装 `clang-16`/`clang++-16` 并确保它们在 `PATH`，或者把与目标平台和架构匹配的 `clang*.tar.xz` 放到 `CLANG16_TARBALL` 指定的位置。

脚本要求 C 与 C++ 编译器都为 major version 16，并且能通过最小编译探测。如果 probe 失败，先更换 Clang tarball，再移走旧的 `CLANG16_DIR` 后重试。

目标 CPU 不支持 SVE2 时，把 `THRIFT_ENABLE_ARM_SVE2` 设为 `False`。

### Q6：fbthrift patch 报错或已经应用过。

脚本会先运行反向检查，已应用时自动跳过。仍然失败时，通常是：

- `FBTHRIFT_REF` 与 patch 的上下文不匹配；
- fbthrift 源码已有不同的本地修改；
- patch 文件缺失或损坏；
- clone 得到的源码不是预期仓库。

检查 `FBTHRIFT_URL`、`FBTHRIFT_REF`、`fbthrift.patch`，以及 `thrift/lib/cpp2/server/BaseThriftServer.h` 附近的上下文。

### Q7：HTTPS clone 失败。

先手工验证目标机器能访问对应仓库：

```bash
git ls-remote https://gitcode.com/boostkit/folly.git

git ls-remote https://github.com/facebookincubator/fizz.git
```

常见原因包括 DNS、代理、企业网络策略、TLS 证书、GitCode/GitHub 访问限制或 submodule 下载失败。

如果 clone 中断后留下不完整目录，脚本不会覆盖它。确认目录无用后先删除，再重新运行。

### Q8：CMake 报 compiler 不匹配、旧 cache 或 generator 错误。

只删除对应的 build 目录，保留 Git 源码目录和 `INS`，再运行 `install.py`。如果更换了 `WORK`，需要重新确认所有 `*_DIR` 和 `*_INS` 仍然指向同一套布局。

### Q9：`numactl` 报 node 1 不存在。

当前默认固定到 NUMA node 1。单节点机器把 `PREFIX_CMD` 中的 `"-N", "1"` 改成 `"-N", "0"`，或删除 `numactl` 及其参数。

### Q10：CSV 中出现很多 `N/A`。

查看脚本打印的原始 benchmark 输出。通常是客户端输出格式或单位与 `parse_output()` 中的正则不一致，例如吞吐单位不是 `MB/s`、延迟标签变化，或者程序以非零状态退出。

根据实际输出修改 `parse_output()`，不要只修改 CSV 表头。

### Q11：运行时端口被占用或连接失败。

检查服务端是否监听 `23456`，并让服务端、客户端的端口和 transport 设置一致。如果要换端口，修改 `PREFIX_CMD` 中的 `--port=23456`。

### Q12：系统没有 root，安装依赖失败。

由管理员按 `DEBIAN_PACKAGES` 或 `OPENEULER_PACKAGES` 手动安装依赖，然后保持：

```python
INSTALL_PACKAGES = False
```

### Q13：编译内存不够或机器负载过高。

调小 `JOBS`，例如：

```python
JOBS = 8
```

并适当减少 `CONN_SETUPS`、`QD_SETUPS` 或 `DATA_SETUPS`，先做小矩阵验证。

### Q14：报 system glog/fmt headers not found 或版本冲突。

确认系统已安装对应开发头文件：

- Debian/Ubuntu：通常是 `libgoogle-glog-dev` 和 `libfmt-dev`；
- openEuler/RPM：通常是 `glog-devel` 和 `fmt-devel`。

同时检查 `/usr/include/glog`、`/usr/include/fmt` 和 system fmt CMake package 是否存在。

如果头文件或 CMake package 位于其他目录，修改 `prepare_dependency_overlays()`、`FMT_CMAKE_DIR_CANDIDATES`，或设置环境变量：

```bash
export FMT_CMAKE_DIR=/your/system/fmt/cmake
```

如果 overlay 目标已被手工创建成普通目录，先移走后再重试。CMake 仍选中错误版本时，检查 `CMAKE_IGNORE_PREFIX_PATH` 和 `CMAKE_IGNORE_PATH`。
