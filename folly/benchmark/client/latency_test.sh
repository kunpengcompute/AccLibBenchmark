#!/bin/bash

# 打印 CSV 表头
echo "qps,p50,p95,p99,min,max,total_average"

for ((i=1; i<=300; i++))
do
    arg2=$((i * 2000))
    arg4=$((i * 2000 * 1))
    
    # 捕获 run.sh 的所有输出到变量中
    output=$(bash run.sh 1 "$arg2" 1 "$arg4")
    # 使用 awk 提取数值。
    # 逻辑：匹配关键字，然后打印该行最后一个空格或冒号后的数字部分。
    p50=$(echo "$output" | grep "p50" | awk -F': ' '{print $2}')
    p95=$(echo "$output" | grep "p95" | awk -F': ' '{print $2}')
    p99=$(echo "$output" | grep "p99" | awk -F': ' '{print $2}')
    min=$(echo "$output" | grep "min" | awk -F': ' '{print $2}')
    max=$(echo "$output" | grep "max" | awk -F': ' '{print $2}')
    avg=$(echo "$output" | grep "total average" | awk -F':' '{print $2}')
    # 按 CSV 格式输出：qps, p50, p95, p99, min, max, avg
    echo "$arg2,$p50,$p95,$p99,$min,$max,$avg"
done