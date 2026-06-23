cmake_minimum_required(VERSION 3.14)
project(rpc_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 依赖路径（可通过 -D 覆盖）
set(FOLLY_ROOT   "${CMAKE_CURRENT_SOURCE_DIR}/third/folly/lib/cmake/folly"      CACHE PATH "folly cmake path")
set(FIZZ_ROOT    "${CMAKE_CURRENT_SOURCE_DIR}/third/fizz/lib/cmake/fizz"        CACHE PATH "fizz cmake path")
set(WANGLE_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/third/wangle/lib/cmake/wangle"    CACHE PATH "wangle cmake path")
set(FBTHRIFT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third/fbthrift/lib/cmake/fbthrift" CACHE PATH "fbthrift cmake path")
set(FMT_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}/third/fmt/lib/cmake/fmt"          CACHE PATH "fmt cmake path")
set(SIMDJSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third/simdjson"                  CACHE PATH "simdjson root")
set(SONICCPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third/sonic-cpp"                 CACHE PATH "sonic-cpp root")

list(PREPEND CMAKE_PREFIX_PATH
    "${FOLLY_ROOT}"
    "${FIZZ_ROOT}"
    "${WANGLE_ROOT}"
    "${FBTHRIFT_ROOT}"
    "${FMT_ROOT}"
)

find_package(Gflags REQUIRED)
find_package(Glog REQUIRED)
find_package(FBThrift REQUIRED)
find_package(fizz REQUIRED)
find_package(fmt REQUIRED)
find_package(folly REQUIRED)
find_package(wangle REQUIRED)

# simdjson（自动检测 lib/lib64）
include_directories(${SIMDJSON_ROOT}/include)
if(EXISTS "${SIMDJSON_ROOT}/lib64/libsimdjson.a")
    set(SIMDJSON_LIB "${SIMDJSON_ROOT}/lib64/libsimdjson.a")
else()
    set(SIMDJSON_LIB "${SIMDJSON_ROOT}/lib/libsimdjson.a")
endif()

# sonic-cpp（header-only）
include_directories(${SONICCPP_ROOT}/include)

file(GLOB GEN_CPP_SOURCES CONFIGURE_DEPENDS "gen-cpp2/*.cpp")

set(COMMON_LIBS
    FBThrift::thriftcpp2
    Folly::folly
    wangle::wangle
    fizz::fizz
    fmt::fmt
    ${SIMDJSON_LIB}
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

add_executable(json_codec_ut json_codec_ut.cpp)
target_link_libraries(json_codec_ut PRIVATE thrift_generated ${COMMON_LIBS})
