cmake_minimum_required(VERSION 3.10...3.24)
project(ProtobufMatBenchmark)

if(NOT DEFINED ENV{PBLIB_HOME})
    message(FATAL_ERROR "Environment variable PBLIB_HOME is not set")
endif()

# List of all proto files for the new test suite
set(PROTO_FILES
    # Scalar types (packed=false)
    scalar_int32
    scalar_uint32
    scalar_sint32
    scalar_int64
    scalar_uint64
    scalar_sint64
    scalar_bool
    scalar_enum
    scalar_double
    scalar_float
    # Repeated types (packed=true)
    repeated_int32
    repeated_uint32
    repeated_sint32
    repeated_int64
    repeated_uint64
    repeated_sint64
    repeated_double
    repeated_float
    # String and bytes
    string
    bytes_data
)

# Generate Protobuf source files if needed
foreach(PROTO_FILE ${PROTO_FILES})
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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

link_directories("$ENV{PBLIB_HOME}/lib64" "$ENV{PBLIB_HOME}/lib")
include_directories("$ENV{PBLIB_HOME}/include")

# Architecture-specific optimization flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -march=native -no-pie")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -march=armv8.5-a+sve+sve2+sve2-bitperm")
endif()

# Generate list of protobuf .cc files
set(PROTO_SOURCES "")
foreach(PROTO_FILE ${PROTO_FILES})
    list(APPEND PROTO_SOURCES "${PROTO_FILE}.pb.cc")
endforeach()

# Build the benchmark executable
add_executable(bm
    benchmark_main.cpp
    benchmark_scalar.cpp
    benchmark_repeated.cpp
    benchmark_string.cpp
    ${PROTO_SOURCES}
)

target_link_libraries(bm
    benchmark
    pthread
    protobuf
    utf8_range
    utf8_validity
    -Wl,--start-group
    absl_log_internal_check_op
    absl_log_internal_message
    absl_log_internal_nullguard
    absl_log_internal_format
    absl_log_internal_globals
    absl_log_internal_log_sink_set
    absl_log_internal_proto
    absl_log_severity
    absl_log_sink
    absl_log_entry
    absl_log_globals
    absl_log_initialize
    absl_log_flags
    absl_vlog_config_internal
    absl_log_internal_fnmatch
    absl_log_internal_conditions
    absl_status
    absl_statusor
    absl_strerror
    absl_raw_logging_internal
    absl_hash
    absl_flags_reflection
    absl_flags_commandlineflag
    absl_flags_commandlineflag_internal
    absl_flags_config
    absl_flags_internal
    absl_flags_marshalling
    absl_flags_parse
    absl_flags_private_handle_accessor
    absl_flags_program_name
    absl_flags_usage
    absl_flags_usage_internal
    absl_strings
    absl_strings_internal
    absl_string_view
    absl_str_format_internal
    absl_base
    absl_synchronization
    absl_time
    absl_time_zone
    absl_civil_time
    absl_stacktrace
    absl_symbolize
    absl_debugging_internal
    absl_demangle_internal
    absl_cord
    absl_cord_internal
    absl_cordz_functions
    absl_cordz_handle
    absl_cordz_info
    absl_cordz_sample_token
    absl_crc_cord_state
    absl_crc32c
    absl_crc_cpu_detect
    absl_crc_internal
    absl_raw_hash_set
    absl_hashtablez_sampler
    absl_exponential_biased
    absl_throw_delegate
    absl_int128
    absl_bad_optional_access
    absl_bad_variant_access
    absl_low_level_hash
    absl_city
    absl_spinlock_wait
    absl_malloc_internal
    absl_kernel_timeout_internal
    absl_graphcycles_internal
    absl_examine_stack
    absl_failure_signal_handler
    absl_periodic_sampler
    absl_die_if_null
    -Wl,--end-group
)
