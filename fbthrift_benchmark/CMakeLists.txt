cmake_minimum_required(VERSION 3.14)
project(rpc_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third")

find_package(FBThrift REQUIRED)
find_package(fizz REQUIRED)
find_package(fmt REQUIRED)
find_package(folly REQUIRED)
find_package(proxygen REQUIRED)
find_package(wangle REQUIRED)

file(GLOB GEN_CPP_SOURCES CONFIGURE_DEPENDS "gen-cpp2/*.cpp")

# 公共链接库列表（所有依赖）
set(COMMON_LIBS
    FBThrift::thriftcpp2
    Folly::folly
    wangle::wangle
    fizz::fizz
    fmt::fmt
    proxygen::proxygen
)

# 创建静态库，封装 Thrift 生成的代码
add_library(thrift_generated STATIC ${GEN_CPP_SOURCES})

# 让静态库获得依赖的头文件路径，并传递给下游
target_link_libraries(thrift_generated PUBLIC ${COMMON_LIBS})

# 添加 gen-cpp2 自身的头文件路径（如果有必要的话）
target_include_directories(thrift_generated PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ------------------------------------------------------------
# 可执行文件（现在只需要链接 thrift_generated 和公共库）
# ------------------------------------------------------------

add_executable(press_server
    server/PerfTestService.cpp
    server/PressServer.cpp
    util/ResourceMonitor.cpp
)
target_link_libraries(press_server PRIVATE thrift_generated ${COMMON_LIBS} atomic)


add_executable(press_client
    client/PressTestClient.cpp
    util/ResourceMonitor.cpp
)
target_link_libraries(press_client PRIVATE thrift_generated ${COMMON_LIBS} atomic)
