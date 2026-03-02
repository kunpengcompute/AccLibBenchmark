#!/bin/bash

# 检查输入参数
if [ $# -ne 1 ]; then
 echo "Usage: $0 <input_file>"
 exit 1
fi

input_file="$1"

# 使用awk提取usecs/call数据
awk '
# 跳过所有系统信息行（以!开头）
/^!/ { next }

# 跳过所有注释行（以#开头）
/^#/ { next }

# 跳过标题行
/^[[:space:]]*prc thr/ { next }

# 匹配 memcpy / memset / memmove / memcmp
/^(memcpy|memset|memmove|memcmp)_/ {
 test_name = $1
 usecs_per_call = $4
 printf "%s\t%s\n", test_name, usecs_per_call
}
' "$input_file"