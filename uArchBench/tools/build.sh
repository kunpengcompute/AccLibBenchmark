#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_DIR}/build"
TOOLCHAIN=""

usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -t, --toolchain <name>   Toolchain to use"
    echo "  -c, --clean              Clean build directory before building"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Available toolchains:"
    echo "  aarch64-gcc    - GCC for AArch64 (default)"
    echo "  aarch64-clang  - Clang for AArch64"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--toolchain)
            TOOLCHAIN="$2"
            shift 2
            ;;
        -c|--clean)
            rm -rf "${BUILD_DIR}"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

TOOLCHAIN_FILE=""
case "${TOOLCHAIN}" in
    aarch64-gcc|"")
        TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/toolchains/aarch64-linux-gcc.cmake"
        ;;
    aarch64-clang)
        TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/toolchains/aarch64-linux-clang.cmake"
        ;;
    *)
        echo "Unknown toolchain: ${TOOLCHAIN}"
        usage
        exit 1
        ;;
esac

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
)

cmake "${PROJECT_DIR}" "${CMAKE_ARGS[@]}"
make -j$(nproc)

echo ""
echo "Build complete. Executable: ${BUILD_DIR}/src/uarch_bench"
