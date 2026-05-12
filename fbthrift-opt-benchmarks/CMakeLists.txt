cmake_minimum_required(VERSION 3.10)
project(fbthrift_benchmark_datagen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -DNDEBUG -march=armv8-a+sve2-bitperm")

set(FBTHRIFT_HOME_DEFAULT "/usr/local")
if(DEFINED ENV{FBTHRIFT_HOME})
  set(FBTHRIFT_HOME "$ENV{FBTHRIFT_HOME}")
else()
  set(FBTHRIFT_HOME "${FBTHRIFT_HOME_DEFAULT}")
endif()


list(APPEND CMAKE_PREFIX_PATH
  "/home/g00570279/ins/folly/lib/cmake/folly"
  "/home/g00570279/folly/build/installed/fmt/lib/cmake/fmt"
        "${FBTHRIFT_HOME}"
  "/home/g00570279/ins"
)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find dependencies in an order compatible with FBThrift imported targets.
set(FOLLY_ROOT "/home/g00570279/ins/folly")
find_package(folly REQUIRED)
find_package(fmt REQUIRED)
find_package(wangle REQUIRED)
find_package(fizz REQUIRED)
find_package(FBThrift REQUIRED)

find_package(gflags QUIET)
if(NOT gflags_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    include(FindPkgConfig)
    pkg_check_modules(GFLAGS gflags)
  endif()
endif()

find_program(
  FBTHRIFT_THRIFT1
  NAMES thrift1 thrift
  PATHS "${FBTHRIFT_HOME}/bin"
  NO_DEFAULT_PATH
)
if(NOT FBTHRIFT_THRIFT1)
  find_program(FBTHRIFT_THRIFT1 NAMES thrift1 thrift)
endif()
if(NOT FBTHRIFT_THRIFT1)
  message(FATAL_ERROR "Cannot find thrift compiler (thrift1/thrift).")
endif()

set(THRIFT_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gen-cpp2")
set(ARR_THRIFT "arr_data.thrift")
set(THRIFT_GEN_SOURCES
  "${THRIFT_GEN_DIR}/arr_data_types.cpp"
  "${THRIFT_GEN_DIR}/arr_data_data.cpp"
)

add_custom_command(
  OUTPUT
    "${THRIFT_GEN_DIR}/arr_data_data.h"
    "${THRIFT_GEN_DIR}/arr_data_data.cpp"
    "${THRIFT_GEN_DIR}/arr_data_types.h"
    "${THRIFT_GEN_DIR}/arr_data_types.cpp"
  COMMAND "${FBTHRIFT_THRIFT1}" --gen mstch_cpp2:no_metadata -o "." "${ARR_THRIFT}"
  DEPENDS "${ARR_THRIFT}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating fbthrift code for arr_data.thrift"
)

add_custom_target(
  thrift_codegen ALL
  DEPENDS
    "${THRIFT_GEN_DIR}/arr_data_data.h"
    "${THRIFT_GEN_DIR}/arr_data_data.cpp"
    "${THRIFT_GEN_DIR}/arr_data_types.h"
    "${THRIFT_GEN_DIR}/arr_data_types.cpp"
)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${FBTHRIFT_HOME}/include"
)

add_executable(bm_arr
  benchmark_for_arr_data.cpp
  #test_fbthrift.cpp
  ${THRIFT_GEN_SOURCES}
)
add_dependencies(bm_arr thrift_codegen)

target_link_libraries(bm_arr
  benchmark
  pthread
  FBThrift::thriftcpp2
  Folly::folly
  wangle::wangle
  fizz::fizz
  fmt::fmt
)

if(TARGET gflags::gflags)
  target_link_libraries(bm_arr gflags::gflags)
elseif(GFLAGS_FOUND)
  target_include_directories(bm_arr PRIVATE ${GFLAGS_INCLUDE_DIRS})
  target_link_libraries(bm_arr ${GFLAGS_LIBRARIES})
else()
  target_link_libraries(bm_arr gflags)
endif()

message(STATUS "FBThrift Benchmark Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  C++ Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "  FBTHRIFT_HOME: ${FBTHRIFT_HOME}")
message(STATUS "  Thrift Compiler: ${FBTHRIFT_THRIFT1}")
message(STATUS "  Thrift Generated Dir: ${THRIFT_GEN_DIR}")
