#! /bin/bash
cd `dirname $0`
set -e

protocol="${1:-compact}"
if [[ "$protocol" != "compact" && "$protocol" != "binary" ]]; then
  echo "Usage: $0 [compact|binary]"
  exit 1
fi

./compile.sh
./run.sh "$protocol"
