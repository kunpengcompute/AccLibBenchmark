cmake_minimum_required(VERSION 3.16)

add_executable(uarch_bench 
    main.cpp 
    cpu_freq.c
    measure.c
    output.cpp
)
target_link_libraries(uarch_bench PRIVATE uarch_common)

if(ENABLE_INSTRUCTION_BENCH)
    target_link_libraries(uarch_bench PRIVATE instruction_bench)
    target_compile_definitions(uarch_bench PRIVATE UARCH_ENABLE_INSTRUCTION_BENCH)
endif()

if(ENABLE_MICROARCH_BENCH)
    target_link_libraries(uarch_bench PRIVATE microarch_bench)
    target_compile_definitions(uarch_bench PRIVATE UARCH_ENABLE_MICROARCH)
endif()
