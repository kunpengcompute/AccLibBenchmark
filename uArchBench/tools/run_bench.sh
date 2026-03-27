#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_DIR}/build"

if [[ ! -f "${BUILD_DIR}/src/uarch_bench" ]]; then
    echo "Error: Build not found. Run build.sh first."
    exit 1
fi

echo "Running uArchBench..."
echo "========================================"
"${BUILD_DIR}/src/uarch_bench" "$@"
