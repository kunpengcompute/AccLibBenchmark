cmake_minimum_required(VERSION 3.10...3.24)
project(ProtobufMatBenchmark)

if(NOT DEFINED ENV{PBLIB_HOME})
    message(FATAL_ERROR "Environment variable PBLIB_HOME is not set")
endif()

# 批量检查并生成 Protobuf 源文件
foreach(PROTO_FILE arr_double arr_data32 arr_data64 pae_data string)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${PROTO_FILE}.pb.h")
        message(STATUS "${PROTO_FILE}.pb.h not found. Generating...")
        execute_process(
            COMMAND $ENV{PBLIB_HOME}/bin/protoc --cpp_out=. ./${PROTO_FILE}.proto
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "protoc failed for ${PROTO_FILE}: ${error}")
        endif()
    endif()
endforeach()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

link_directories("$ENV{PBLIB_HOME}/lib64" "$ENV{PBLIB_HOME}/lib")
include_directories("$ENV{PBLIB_HOME}/include")

# 根据架构设置优化编译选项
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -march=native -no-pie")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -march=armv8.5-a+sve+sve2+sve2-bitperm")
endif()

add_executable(bm benchmark.cpp arr_double.pb.cc arr_data32.pb.cc arr_data64.pb.cc pae_data.pb.cc string.pb.cc)

target_link_libraries(bm 
    benchmark 
    pthread 
    protobuf 
    absl_log_internal_check_op 
    absl_log_internal_message 
    absl_log_internal_nullguard
    absl_raw_logging_internal
)