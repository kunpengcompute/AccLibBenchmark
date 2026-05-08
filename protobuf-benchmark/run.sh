#! /bin/bash
cd `dirname $0`
set -e
source ./check-pb-home.sh
LD_LIBRARY_PATH=$PBLIB_HOME/lib taskset -c 0 ./bin/bm --benchmark_min_time=3s
