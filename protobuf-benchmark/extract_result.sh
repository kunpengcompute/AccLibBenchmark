#!/bin/bash

RUN_SCRIPT="./compile-and-run.sh"

if [ ! -x "$RUN_SCRIPT" ]; then
    echo "错误: 找不到 $RUN_SCRIPT 或没有执行权限。" >&2
    exit 1
fi

echo "正在编译并运行测试..." >&2

$RUN_SCRIPT 2>&1 | awk '
BEGIN {
    print "==================== 环境信息 ===================="
}

# 1. 提取 Protobuf 路径
/Current protobuf library home:/ {
    if (!pb_printed) {
        print $0
        pb_printed = 1
    }
}

# 2. 提取编译器版本信息
/-- The CXX compiler identification is/ {
    print "Compiler Version: " $NF
}

# 3. 提取运行时间戳和二进制路径
/^[0-9]{4}-[0-9]{2}-[0-9]{2}T/ { print "Timestamp: " $0 }
/Running \.\/bin\/bm/ { print "Target: " $0 }

# 4. 提取 CPU 信息
/Run on \(/ {
    print "CPU Info: " $0
}

# 5. 提取 CPU 缓存信息
/CPU Caches:/ {
    print $0
    capture_cache = 1
    next
}
capture_cache && /^[[:space:]]+L[1-3]/ {
    print $0
}
capture_cache && /Load Average:/ {
    print $0
    capture_cache = 0 
    print "==================== 测试结果 ===================="
    printf "Benchmark_Case\tCPU_Time_ns\tIterations\tBPS_Mi/s\n"
}

# 6. 处理 Benchmark 数据行
/bytes_per_second/ && !/warning:|error:/ {
    case_name = $1
    cpu_time = $4
    iterations = $6
    
    if (match($NF, /[0-9]+\.[0-9]+|[0-9]+/)) {
        val = substr($NF, RSTART, RLENGTH)
        unit = substr($NF, RSTART + RLENGTH)
        
        if (unit ~ /Gi\/s/) {
            bps_val = val * 1024
        } else if (unit ~ /ki\/s/) {
            bps_val = val / 1024
        } else {
            bps_val = val
        }
    }

    printf "%s\t%s\t%s\t%.3f\n", case_name, cpu_time, iterations, bps_val
}
'