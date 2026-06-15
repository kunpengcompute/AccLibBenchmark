cmake_minimum_required(VERSION 3.1)
project(rdma_performance C CXX)

option(LINK_SO "Whether examples are linked dynamically" OFF)
option(WITH_SONIC_JSON "Build with sonic-cpp JSON engine support" OFF)
set(SONIC_CPP_INCLUDE_DIR "" CACHE PATH "Path to sonic-cpp include directory")
set(SONIC_CPP_COMPILE_OPTIONS "" CACHE STRING "Extra compile options for sonic JSON source")

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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -O2 -pipe -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")

if(WITH_SONIC_JSON)
    if(NOT SONIC_CPP_INCLUDE_DIR)
        message(FATAL_ERROR "WITH_SONIC_JSON=ON but SONIC_CPP_INCLUDE_DIR is not set")
    endif()
    if(NOT EXISTS "${SONIC_CPP_INCLUDE_DIR}/sonic/sonic.h")
        message(FATAL_ERROR "WITH_SONIC_JSON=ON but sonic headers not found under ${SONIC_CPP_INCLUDE_DIR}")
    endif()
    include_directories(${SONIC_CPP_INCLUDE_DIR})
    add_definitions(-DBRPC_WITH_SONIC_JSON=1)
    if(SONIC_CPP_COMPILE_OPTIONS)
        separate_arguments(SONIC_CPP_COMPILE_OPTIONS_LIST NATIVE_COMMAND
                           "${SONIC_CPP_COMPILE_OPTIONS}")
    endif()
endif()

set(ABSL_ALL_LIBS "")
foreach(PREFIX_PATH ${CMAKE_PREFIX_PATH})
    file(GLOB ABSL_FILES
        "${PREFIX_PATH}/lib/libabsl_*.a"
        "${PREFIX_PATH}/lib64/libabsl_*.a"
        "${PREFIX_PATH}/lib/libabsl_*.so"
        "${PREFIX_PATH}/lib64/libabsl_*.so"
        "${PREFIX_PATH}/lib/libabsl_*.so.*"
        "${PREFIX_PATH}/lib64/libabsl_*.so.*"
    )
    list(APPEND ABSL_ALL_LIBS ${ABSL_FILES})
endforeach()
list(REMOVE_DUPLICATES ABSL_ALL_LIBS)

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

if(WITH_SONIC_JSON AND SONIC_CPP_COMPILE_OPTIONS_LIST)
    set_source_files_properties(
        server.cpp
        PROPERTIES COMPILE_OPTIONS "${SONIC_CPP_COMPILE_OPTIONS_LIST}")
endif()

target_link_libraries(client ${STATIC_GROUP_LIBS} ${DYNAMIC_LIB})
target_link_libraries(server ${STATIC_GROUP_LIBS} ${DYNAMIC_LIB})
