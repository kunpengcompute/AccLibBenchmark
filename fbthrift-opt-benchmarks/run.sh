#! /bin/bash
cd `dirname $0`
set -e
source ./check-pb-home.sh

protocol="${1:-compact}"
if [[ "$protocol" != "compact" && "$protocol" != "binary" ]]; then
  echo "Usage: $0 [compact|binary]"
  exit 1
fi

FBTHRIFT_PROTOCOL="$protocol" LD_LIBRARY_PATH=$FBTHRIFT_HOME/lib taskset -c 105 ./bin/bm_arr "${@:2}"
