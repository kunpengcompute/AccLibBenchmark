cmake_minimum_required(VERSION 3.14)
project(rpc_benchmark LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
list(PREPEND CMAKE_PREFIX_PATH
    "/home/a00970558/ins/folly/lib/cmake/folly"
    "/home/a00970558/ins/fizz/lib/cmake/fizz"
    "/home/a00970558/ins/wangle/lib/cmake/wangle"
    "/home/a00970558/ins/fbthrift/lib/cmake/fbthrift"
    "/usr/lib64/cmake/fmt"
)

find_package(Gflags REQUIRED)
find_package(Glog REQUIRED)
find_package(FBThrift REQUIRED)
find_package(fizz REQUIRED)
find_package(fmt REQUIRED)
find_package(folly REQUIRED)
find_package(wangle REQUIRED)

set(SIMDJSON_ROOT "/home/a00970558/ins/simdjson")
include_directories(${SIMDJSON_ROOT}/include)
set(SONICCPP_ROOT "/home/a00970558/sonic-cpp")
include_directories(${SONICCPP_ROOT}/include)

file(GLOB GEN_CPP_SOURCES CONFIGURE_DEPENDS "gen-cpp2/*.cpp")

set(COMMON_LIBS
    FBThrift::thriftcpp2
    Folly::folly
    wangle::wangle
    fizz::fizz
    fmt::fmt
        ${SIMDJSON_ROOT}/lib64/libsimdjson.a
)

add_library(thrift_generated STATIC ${GEN_CPP_SOURCES})
target_link_libraries(thrift_generated PUBLIC ${COMMON_LIBS})
target_include_directories(thrift_generated PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(press_server
    server/PerfTestService.cpp server/PressServer.cpp util/ResourceMonitor.cpp
)
target_link_libraries(press_server PRIVATE thrift_generated ${COMMON_LIBS} atomic)

add_executable(press_client
    client/PressTestClient.cpp util/ResourceMonitor.cpp
)
target_link_libraries(press_client PRIVATE thrift_generated ${COMMON_LIBS} atomic)
