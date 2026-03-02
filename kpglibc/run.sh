#!/bin/bash
ARCH=$(uname -m)
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
echo $ROOT_DIR

rm -rf ./0.4.0-rh
rm -rf ./libMicro-0.4.0-rh
rm -rf ./kpglibc_result

#下载libMicro
wget --no-check-certificate https://codeload.github.com/redhat-performance/libMicro/zip/0.4.0-rh
unzip 0.4.0-rh
mkdir kpglibc_result
cd libMicro-0.4.0-rh/
cp ../memcmp.c .

SCRIPT_PATH="./bench.sh"
sed -i '133c\   (cd $TMPROOT && eval "numactl -C 30  bin/$A $B")' bench.sh
if command -v clang-16 &> /dev/null; then
    CC_COMPILER="clang-16"
else
    CC_COMPILER="gcc"
fi
sed -i "35s/^CC=.*$/CC=\t\t${CC_COMPILER}/; 37s/^CFLAGS=.*$/CFLAGS=\t-O3 -g -Wno-implicit-function-declaration/" Makefile.Linux
sed -i '71{h;s/^\([[:space:]]*\).*/\1memcmp\t\t\\/;G;s/\(.*\)\n\(.*\)/\2\n\1/}' Makefile.benchmarks
sed -i '116c\	ts->ts_dest = ts->ts_src + 64;' memmove.c

LINE_START=139
cp "$SCRIPT_PATH" "${SCRIPT_PATH}.bak"
sed -i.bak2 "${LINE_START},\$d" "$SCRIPT_PATH"  # 先删除 139 行到结尾
cat >> "$SCRIPT_PATH" <<'EOF'
memcpy		$OPTS -N "memcpy_10" 	-s 10 	-I 3
memcpy		$OPTS -N "memcpy_40"	    -s 40 	-I 3
memcpy		$OPTS -N "memcpy_64"	    -s 64  	-I 3
memcpy		$OPTS -N "memcpy_128"	-s 128	-I 3
memcpy		$OPTS -N "memcpy_180"	-s 180	-I 3
memcpy		$OPTS -N "memcpy_256"	-s 256	-I 10
memcpy		$OPTS -N "memcpy_300"	-s 300	-I 10
memcpy		$OPTS -N "memcpy_512"	-s 512	-I 10
memcpy		$OPTS -N "memcpy_1k"	    -s 1k	   -I 12
memcpy		$OPTS -N "memcpy_4k"    -s 4k    -I 62
memcpy		$OPTS -N "memcpy_10k"	-s 10k	-I 150
memcpy		$OPTS -N "memcpy_30k"	-s 30k	-I 150
memcpy		$OPTS -N "memcpy_64k"	-s 64k	-I 150
memcpy		$OPTS -N "memcpy_1m"	-s 1m	-I 200000
memcpy		$OPTS -N "memcpy_10m"	-s 10m -I 2000000
memcpy		$OPTS -N "memcpy_64m"	-s 64m -I 2000000

memmove		$OPTS -N "memmove_10"	-s 10 	-I 3
memmove		$OPTS -N "memmove_40"	-s 40	    -I 3
memmove		$OPTS -N "memmove_64"	-s 64	    -I 3
memmove		$OPTS -N "memmove_128"	-s 128	-I 3
memmove		$OPTS -N "memmove_180"	-s 180	-I 3
memmove		$OPTS -N "memmove_256"	-s 256	-I 10
memmove		$OPTS -N "memmove_300"	-s 300	-I 10
memmove		$OPTS -N "memmove_512"	-s 512	-I 10
memmove		$OPTS -N "memmove_1k"	-s 1k	 -I 12
memmove		$OPTS -N "memmove_4k"    -s 4k    -I 62
memmove		$OPTS -N "memmove_10k"	-s 10k	-I 150
memmove		$OPTS -N "memmove_30k"	-s 30k	-I 150
memmove		$OPTS -N "memmove_64k"	-s 64k	-I 150
memmove		$OPTS -N "memmove_1m"	-s 1m	-I 2000000
memmove		$OPTS -N "memmove_10m"	-s 10m -I 2000000
memmove		$OPTS -N "memmove_64m"	-s 64m -I 2000000


memset		$OPTS -N "memset_10"	    -s 10	    -I 3
memset		$OPTS -N "memset_40"	    -s 40	    -I 3
memset		$OPTS -N "memset_64"	    -s 64	    -I 3
memset		$OPTS -N "memset_128"	-s 128	-I 3
memset		$OPTS -N "memset_180"	-s 180	-I 3
memset		$OPTS -N "memset_256"	-s 256	-I 10
memset		$OPTS -N "memset_300"	-s 300	-I 10
memset		$OPTS -N "memset_512"	-s 512	-I 10
memset		$OPTS -N "memset_1k"	    -s 1k	   -I 12
memset		$OPTS -N "memset_4k"    -s 4k    -I 62
memset		$OPTS -N "memset_10k"	-s 10k	-I 150
memset		$OPTS -N "memset_30k"	-s 30k	-I 150
memset		$OPTS -N "memset_64k"	-s 64k	-I 150
memset		$OPTS -N "memset_1m"	    -s 1m	-I 200000
memset		$OPTS -N "memset_10m"	-s 10m  -I 2000000
memset		$OPTS -N "memset_64m"	-s 64m  -I 2000000

memcmp		$OPTS -N "memcmp_10"	-s 10	-I 1
memcmp		$OPTS -N "memcmp_40"	-s 40	-I 1
memcmp		$OPTS -N "memcmp_64"	-s 64	-I 1
memcmp		$OPTS -N "memcmp_128"	-s 128	-I 1
memcmp		$OPTS -N "memcmp_180"	-s 180	-I 3
memcmp		$OPTS -N "memcmp_256"	-s 256	-I 3
memcmp		$OPTS -N "memcmp_300"	-s 300	-I 5
memcmp		$OPTS -N "memcmp_512"	-s 512	-I 5
memcmp		$OPTS -N "memcmp_1k"	-s 1k	 -I 10
memcmp		$OPTS -N "memcmp_4k"    -s 4k    -I 30
memcmp		$OPTS -N "memcmp_10k"	-s 10k	-I 150
memcmp		$OPTS -N "memcmp_30k"	-s 30k	-I 300
memcmp		$OPTS -N "memcmp_64k"	-s 64k	-I 1000
memcmp		$OPTS -N "memcmp_1m"	-s 1m	-I 10000
memcmp		$OPTS -N "memcmp_10m"	-s 10m -I 100000
memcmp		$OPTS -N "memcmp_64m"	-s 64m -I 1000000
.
EOF

make clean
make -j

echo "SYS glibc func performance testing..."
sh bench.sh > ../kpglibc_result/base.out
sh ../extract_data.sh ../kpglibc_result/base.out > ../kpglibc_result/extract_base.out

# preload 测试950优化so
case "$ARCH" in
    aarch64)
        cd ..
        #下载kpglibc
        git clone https://gitcode.com/boostkit/kpglibc.git
        cd kpglibc
        sh build.sh
        cd ../libMicro-0.4.0-rh/

        echo "OPT glibc func performance testing..."
        export LD_LIBRARY_PATH=${ROOT_DIR}/kpglibc/output:$LD_LIBRARY_PATH
        sed -i '133c\   (cd $TMPROOT && eval "LD_PRELOAD='"${ROOT_DIR}"'/kpglibc/output/libkpglibc_sve_950.so numactl -C 30  bin/$A $B")' bench.sh
        sh bench.sh > ../kpglibc_result/kp_opt.out
        sh ../extract_data.sh ../kpglibc_result/kp_opt.out > ../kpglibc_result/extract_opt.out
        ;;
    x86_64)
        echo "检测到 x86 架构 ($ARCH)，跳过 ARM 优化测试。"
        ;;
    *)
        echo "未知架构 ($ARCH)，跳过 ARM 专用命令。"
        ;;
esac