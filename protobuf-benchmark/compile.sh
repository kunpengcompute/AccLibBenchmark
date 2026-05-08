#! /bin/bash
cd `dirname $0`
set -e
source ./check-pb-home.sh
./clean.sh
mkdir build
cd build
cmake ..
make -j
