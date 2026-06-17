cmake_minimum_required(VERSION 3.1)
project(rdma_performance C CXX)

option(LINK_SO "Whether examples are linked dynamically" OFF)

find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER press.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})

find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
if((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    message(FATAL_ERROR "Fail to find gflags")
endif()
include_directories(${GFLAGS_INCLUDE_PATH})

find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)
if(LINK_SO)
    find_library(BRPC_LIB NAMES brpc)
else()
    find_library(BRPC_LIB NAMES libbrpc.a brpc)
endif()
if((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
    message(FATAL_ERROR "Fail to find brpc")
endif()
include_directories(${BRPC_INCLUDE_PATH})

find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()
include_directories(${LEVELDB_INCLUDE_PATH})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_path(RDMA_INCLUDE_PATH NAMES infiniband/verbs.h)
find_library(RDMA_LIB NAMES ibverbs)
if ((NOT RDMA_INCLUDE_PATH) OR (NOT RDMA_LIB))
    message(FATAL_ERROR "Fail to find ibverbs")
endif()

include(FindThreads)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -O2 -pipe -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer -march=armv8.5-a+sve+sve2+sve2-bitperm")

set(ABSL_ALL_LIBS "")
foreach(PREFIX_PATH ${CMAKE_PREFIX_PATH})
    file(GLOB ABSL_FILES 
        "${PREFIX_PATH}/lib/libabsl_*.a"
        "${PREFIX_PATH}/lib64/libabsl_*.a"
    )
    list(APPEND ABSL_ALL_LIBS ${ABSL_FILES})
endforeach()

find_library(SNAPPY_LIB NAMES snappy)
find_library(UTF8_RANGE_LIB NAMES utf8_range)
find_library(UTF8_VALIDITY_LIB NAMES utf8_validity)

set(STATIC_GROUP_LIBS
    -Wl,--start-group
    ${BRPC_LIB}
    ${Protobuf_LIBRARIES}
    ${LEVELDB_LIB}
    ${SNAPPY_LIB}
    ${ABSL_ALL_LIBS}
    ${UTF8_RANGE_LIB}
    ${UTF8_VALIDITY_LIB}
    -Wl,--end-group
)

set(DYNAMIC_LIB
    ${CMAKE_THREAD_LIBS_INIT}
    ${GFLAGS_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    dl
    z
    rt
    ${RDMA_LIB}
)

add_executable(client client.cpp ${PROTO_SRC})
add_executable(server server.cpp ${PROTO_SRC})

target_compile_definitions(client PRIVATE BRPC_WITH_RDMA)
target_compile_definitions(server PRIVATE BRPC_WITH_RDMA)

target_link_libraries(client ${STATIC_GROUP_LIBS} ${DYNAMIC_LIB})
target_link_libraries(server ${STATIC_GROUP_LIBS} ${DYNAMIC_LIB})
