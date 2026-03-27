# Page Boundary Load Performance Test

测试页边界Load指令的性能，支持普通load和SVE掩码load。

## 编译

```bash
gcc -O2 -march=armv8-a+sve -o page_boundary_load page_boundary_load.c
```

或使用Makefile：

```bash
make
```

## 运行

```bash
./page_boundary_load [iterations] [offset]
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| iterations | 测试迭代次数 | 10000000 |
| offset | 相对于base的偏移量 | 4095 (PAGE_SIZE-1) |

### 示例

```bash
# 使用默认值 (1000万次迭代, offset=4095)
./page_boundary_load

# 指定迭代次数
./page_boundary_load 5000000

# 指定迭代次数和偏移量
./page_boundary_load 10000000 4095   # 页边界前1字节
./page_boundary_load 10000000 4096   # 跨页到第二页
./page_boundary_load 10000000 0      # 页起始位置

# 查看帮助
./page_boundary_load --help
```

## 测试项目

1. **Normal load** - 普通ldr指令，作为基准
2. **Normal ldrb** - 普通ldrb指令，在指定偏移处加载1字节
3. **SVE load** - 使用ptrue掩码的SVE ld1b指令
4. **SVE load with whilelo** - 使用whilelo生成掩码的SVE ld1b指令

## 设计说明

- 使用`posix_memalign`分配页对齐内存（2页大小）
- 使用`clock_gettime(CLOCK_MONOTONIC_RAW)`计时
- 页大小假设为4KB
