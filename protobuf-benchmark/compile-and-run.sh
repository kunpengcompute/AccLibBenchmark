#! /bin/bash
cd `dirname $0`
set -e
./compile.sh
./run.sh
