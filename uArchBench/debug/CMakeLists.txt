cmake_minimum_required(VERSION 3.16)

add_executable(debug_bench 
    debug_bench.c
    ../src/cpu_freq.c
)

target_link_libraries(debug_bench PRIVATE uarch_common instruction_bench)
