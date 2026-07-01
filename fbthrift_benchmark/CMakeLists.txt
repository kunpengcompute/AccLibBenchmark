cmake_minimum_required(VERSION 3.14)
project(rpc_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include(GNUInstallDirs)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/third/protobuf/${CMAKE_INSTALL_LIBDIR}/cmake"
)

find_package(Gflags REQUIRED)
find_package(Glog REQUIRED)
find_package(FBThrift REQUIRED)
find_package(fizz REQUIRED)
find_package(fmt REQUIRED)
find_package(folly REQUIRED)
find_package(wangle REQUIRED)
find_package(protobuf REQUIRED)

# simdjson（自动检测 lib/lib64）
include_directories(${SIMDJSON_ROOT}/include)
if(EXISTS "${SIMDJSON_ROOT}/lib64/libsimdjson.a")
    set(SIMDJSON_LIB "${SIMDJSON_ROOT}/lib64/libsimdjson.a")
else()
    set(SIMDJSON_LIB "${SIMDJSON_ROOT}/lib/libsimdjson.a")
endif()

# sonic-cpp（header-only）
include_directories(${SONICCPP_ROOT}/include)

set(COMMON_LIBS
    Folly::folly
    wangle::wangle
    fizz::fizz
    fmt::fmt
    ${SIMDJSON_LIB}
)

# ============================================================
# Thrift code generation
# ============================================================
include("${FBTHRIFT_INCLUDE_DIR}/thrift/ThriftLibrary.cmake")
set(THRIFT1 ${FBTHRIFT_COMPILER})
set(THRIFTCPP2 FBThrift::thriftcpp2)

set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gen-cpp2)
file(MAKE_DIRECTORY ${GEN_DIR})

# thrift_library emits #include "<prefix>/gen-cpp2/..."; use the current
# directory name as prefix so those includes resolve against the parent dir.
get_filename_component(MODULE_PREFIX ${CMAKE_CURRENT_SOURCE_DIR} NAME)

thrift_library(
  "press"
  "PerfTestService"
  "cpp2"
  ""
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${MODULE_PREFIX}"
)

thrift_library(
  "universal"
  "UniversalService"
  "cpp2"
  ""
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${MODULE_PREFIX}"
)

foreach(_lib press-cpp2-obj universal-cpp2-obj press-cpp2 universal-cpp2)
  target_include_directories(${_lib} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}        # source-level #include "gen-cpp2/..."
    ${CMAKE_CURRENT_SOURCE_DIR}/..     # generated  #include "<prefix>/gen-cpp2/..."
    ${FBTHRIFT_INCLUDE_DIR})           # thrift/lib/... headers
endforeach()

# OBJECT libs compile the generated .cpp; they need the full dependency
# include paths (folly, fizz, ...) which the macro only attaches to the wrapper.
foreach(_obj press-cpp2-obj universal-cpp2-obj)
  target_link_libraries(${_obj} PUBLIC FBThrift::thriftcpp2 ${COMMON_LIBS})
endforeach()

# thrift_library macro does not track *_processmap_*.cpp; compile them here.
set(PROCESSMAP_SOURCES
  ${GEN_DIR}/PerfTestService_processmap_binary.cpp
  ${GEN_DIR}/PerfTestService_processmap_compact.cpp
  ${GEN_DIR}/UniversalService_processmap_binary.cpp
  ${GEN_DIR}/UniversalService_processmap_compact.cpp
)
set_source_files_properties(${PROCESSMAP_SOURCES} PROPERTIES GENERATED TRUE)
add_library(thrift_processmap STATIC ${PROCESSMAP_SOURCES})
target_link_libraries(thrift_processmap PUBLIC press-cpp2 universal-cpp2 ${COMMON_LIBS})
target_include_directories(thrift_processmap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${FBTHRIFT_INCLUDE_DIR})
add_dependencies(thrift_processmap press-cpp2-target universal-cpp2-target)

# ============================================================
# Protobuf code generation
# ============================================================

add_library(press-protobuf STATIC ${CMAKE_CURRENT_SOURCE_DIR}/press.proto)
target_link_libraries(press-protobuf PUBLIC protobuf::libprotobuf)
target_include_directories(press-protobuf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

protobuf_generate(
  TARGET press-protobuf
  LANGUAGE cpp
  PROTOC_OUT_DIR ${GEN_DIR}
  IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================
# Executables
# ============================================================

add_executable(press_server
    server/PerfTestService.cpp server/PressServer.cpp util/ResourceMonitor.cpp
)
target_link_libraries(press_server PRIVATE
    press-cpp2 universal-cpp2 thrift_processmap press-protobuf
    ${COMMON_LIBS} atomic)

add_executable(press_client
    client/PressTestClient.cpp util/ResourceMonitor.cpp
)
target_link_libraries(press_client PRIVATE
    press-cpp2 universal-cpp2 thrift_processmap press-protobuf
    ${COMMON_LIBS} atomic)

add_executable(json_codec_ut json_codec_ut.cpp)
target_link_libraries(json_codec_ut PRIVATE
    press-cpp2 universal-cpp2 thrift_processmap press-protobuf
    ${COMMON_LIBS})
