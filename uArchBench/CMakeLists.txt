cmake_minimum_required(VERSION 3.16)
project(uArchBench LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_INSTRUCTION_BENCH "Enable instruction latency/throughput benchmarks" ON)
option(ENABLE_MICROARCH_BENCH "Enable microarchitecture behavior benchmarks" ON)
option(ENABLE_DEBUG_BENCH "Enable debug benchmark executable" ON)

add_library(uarch_common INTERFACE)
target_include_directories(uarch_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(src)

if(ENABLE_INSTRUCTION_BENCH)
    add_subdirectory(benches/instruction)
endif()

if(ENABLE_MICROARCH_BENCH)
    add_subdirectory(benches/microarch)
endif()

if(ENABLE_DEBUG_BENCH)
    add_subdirectory(debug)
endif()
