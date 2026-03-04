#!/bin/bash

# --- 1. 基础配置 ---
INSTALL_DIR="/home/w00663106"
MY_OPENSSL="$INSTALL_DIR/bin/openssl"
SRC_DIR="openssl-3.0.17"

# 识别当前机器架构
ARCH=$(uname -m)

# 核心绑定逻辑 (根据架构调整)
if [ "$ARCH" = "aarch64" ]; then
    # 鲲鹏/ARM: 自动获取 192 个物理核
    PHYS_CORES=$(ls /sys/devices/system/cpu/cpu*/topology/thread_siblings_list | \
                 xargs -n 1 cat | cut -d'-' -f1 | cut -d',' -f1 | sort -un | head -n 192 | tr '\n' ',' | sed 's/,$//')
    CORE_COUNT=192
    CONF_TARGET="linux-aarch64"
    # 开启 ARM 加密扩展
    CFLAGS="-O3 -march=armv8-a+crypto"
elif [ "$ARCH" = "x86_64" ]; then
    # x86: 获取所有物理核心 (排除超线程)
    PHYS_CORES=$(lscpu -p | grep -v '^#' | awk -F, '{ if (!seen[$2]++) print $1 }' | tr '\n' ',' | sed 's/,$//')
    CORE_COUNT=$(echo $PHYS_CORES | tr ',' '\n' | wc -l)
    CONF_TARGET="linux-x86_64"
    # 开启 x86 的 AES-NI 和 AVX 优化
    CFLAGS="-O3 -march=native"
else
    echo "不支持的架构: $ARCH"
    exit 1
fi

# --- 2. 参数解析 (AES/RSA/ALL) ---
TEST_TYPE="all"
while getopts "t:" opt; do
  case $opt in
    t) TEST_TYPE=$OPTARG ;;
    *) echo "用法: $0 [-t aes|rsa|all]"; exit 1 ;;
  esac
done

# --- 3. 编译阶段 (保留逻辑) ---
if [ ! -f "$MY_OPENSSL" ]; then
    echo "=========================================================="
    echo "正在使用 Clang 16.0.6 编译高性能版 OpenSSL 3.0.17..."
    echo "=========================================================="
    
    if [ ! -d "$SRC_DIR" ]; then
        echo "错误: 未发现源码目录 $SRC_DIR"
        exit 1
    fi

    cd "$SRC_DIR"
    make clean > /dev/null 2>&1
    find . -name "configdata.pm" -delete
    
    # 自动选择编译器: 优先使用 Clang，如果没有则回退到 GCC
    if [ -x "/usr/bin/clang" ]; then
        CC_EXEC="/usr/bin/clang"
    else
        CC_EXEC="gcc"
    fi

    echo "使用编译器: $CC_EXEC | 优化参数: $CFLAGS"

    CC=$CC_EXEC ./Configure $CONF_TARGET \
        --prefix=$INSTALL_DIR \
        shared $CFLAGS
    
    make -j$(nproc) && make install
    cd ..
    export LD_LIBRARY_PATH="/home/w00663106/lib64:/home/w00663106/lib:$LD_LIBRARY_PATH"
    /home/w00663106/bin/openssl version -a
    echo "编译完成。"
fi

# 导出库路径 (兼容 lib 和 lib64)
export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$INSTALL_DIR/lib64:$LD_LIBRARY_PATH"

echo "=========================================================="
echo "   Kunpeng Benchmark (Mode: $TEST_TYPE | Cores: 192)      "
echo "=========================================================="

# --- 4. AES 对称加密测试函数 ---
run_aes() {
    echo ">>> 开始 AES 对称加密测试..."
    ALGS=("aes-128-xts" "aes-256-xts" "aes-128-gcm" "aes-256-gcm" "aes-128-cbc" "aes-256-cbc" "aes-128-ctr" "aes-256-ctr")
    
    printf "%-15s | %-10s | %-12s | %-12s | %-12s | %-12s | %-12s | %-12s\n" \
           "Algorithm" "Mode" "16B" "64B" "256B" "1024B" "8192B" "16384B"
    echo "----------------------------------------------------------------------------------------------------------------"

    for alg in "${ALGS[@]}"; do
        ALG_UPPER=$(echo $alg | tr '[:lower:]' '[:upper:]')
        
        # Single Core
        S_LINE=$(taskset -c 0 $MY_OPENSSL speed -elapsed -evp $alg 2>&1 | grep -E "^$ALG_UPPER" | head -n 1 | sed 's/k//g')
        printf "%-15s | %-10s | %-12s | %-12s | %-12s | %-12s | %-12s | %-12s\n" \
               "$alg" "Single" $(echo $S_LINE | awk '{printf "%.2f %.2f %.2f %.2f %.2f %.2f", $2,$3,$4,$5,$6,$7}')
        
        # Multi Core (192-Core)
        taskset -c $PHYS_CORES $MY_OPENSSL speed -elapsed -multi 192 -evp $alg > temp_speed.log 2>&1
        M_LINE=$(tail -n 20 temp_speed.log | grep -E "^$ALG_UPPER" | head -n 1 | sed 's/k//g')
        printf "%-15s | %-10s | %-12s | %-12s | %-12s | %-12s | %-12s | %-12s\n" \
               "$alg" "192-Core" $(echo $M_LINE | awk '{printf "%.2f %.2f %.2f %.2f %.2f %.2f", $2,$3,$4,$5,$6,$7}')
        echo "----------------------------------------------------------------------------------------------------------------"
        rm -f temp_speed.log
    done
}

# --- 5. RSA 非对称加密测试函数 ---
run_rsa() {
    echo -e "\n>>> 开始 RSA 非对称加密测试..."
    RSA_ALGS=("rsa2048" "rsa4096")
    
    printf "%-15s | %-10s | %-20s | %-20s\n" "Algorithm" "Mode" "Sign/s" "Verify/s"
    echo "----------------------------------------------------------------------------------------------------------------"

    for rsa in "${RSA_ALGS[@]}"; do
        case $rsa in
            "rsa2048") RSA_LABEL="rsa 2048" ;;
            "rsa4096") RSA_LABEL="rsa 4096" ;;
        esac

        # Single Core
        S_RSA_LINE=$(taskset -c 0 $MY_OPENSSL speed -elapsed $rsa 2>&1 | grep "^$RSA_LABEL" | tail -n 1)
        S_SIGN=$(echo $S_RSA_LINE | awk '{print $6}')
        S_VERIFY=$(echo $S_RSA_LINE | awk '{print $7}')
        printf "%-15s | %-10s | %-20s | %-20s\n" "$rsa" "Single" "$S_SIGN" "$S_VERIFY"

        # Multi Core (192-Core)
        taskset -c $PHYS_CORES $MY_OPENSSL speed -elapsed -multi 192 $rsa > rsa_run.log 2>&1
        M_RSA_LINE=$(grep "^$RSA_LABEL" rsa_run.log | tail -n 1)
        M_SIGN=$(echo $M_RSA_LINE | awk '{print $6}')
        M_VERIFY=$(echo $M_RSA_LINE | awk '{print $7}')
        printf "%-15s | %-10s | %-20s | %-20s\n" "$rsa" "192-Core" "$M_SIGN" "$M_VERIFY"
        echo "----------------------------------------------------------------------------------------------------------------"
        rm -f rsa_run.log
    done
}

# --- 6. 运行逻辑控制 ---
case $TEST_TYPE in
    "aes") run_aes ;;
    "rsa") run_rsa ;;
    "all") run_aes; run_rsa ;;
    *) echo "错误: 参数只能是 aes, rsa 或 all"; exit 1 ;;
esac

echo -e "\n[测试完成] 结果已输出至屏幕。若需保存请使用: ./kunpeng_bench.sh -t $TEST_TYPE > report.txt"
