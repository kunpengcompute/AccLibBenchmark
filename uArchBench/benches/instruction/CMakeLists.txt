cmake_minimum_required(VERSION 3.16)

add_library(instruction_bench STATIC
    arithmetic/add.S
    arithmetic/sub.S
    arithmetic/mul.S
    arithmetic/div.S
    arithmetic/mov.S
    memory/load.S
    memory/store.S
    branch/branch.S
    simd/neon.S
)

target_link_libraries(instruction_bench PUBLIC uarch_common)
target_include_directories(instruction_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
