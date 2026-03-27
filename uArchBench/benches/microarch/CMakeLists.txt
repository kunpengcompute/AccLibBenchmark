cmake_minimum_required(VERSION 3.16)

add_library(microarch_bench STATIC
    cache/cache_latency.c
    cache/cache_bandwidth.c
    pipeline/pipeline_test.c
    prefetch/prefetch_test.c
    memory/store_to_load.c
    memory/store_to_load.S
)

target_link_libraries(microarch_bench PUBLIC uarch_common)
target_include_directories(microarch_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
