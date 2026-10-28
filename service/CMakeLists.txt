# ADR: docs/adr/ADR-0008-http-parser-benchmarks.md
# ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
# ADR: service/docs/adr/ADR-0002-chunk-metadata-span-advancement.md
# ADR: service/docs/adr/ADR-0003-fragmented-head-offsets.md
# ADR: service/docs/adr/ADR-0004-owned-request-storage.md
# Governed by docs/adr/ADR-0003-local-runtime-path.md.
# Performance authorization: docs/reviews/2026-10-16-http-performance.md.
# Owned storage authorization: docs/reviews/2026-10-16-owned-request-storage.md.
#
# Build description for the orange loopback compute service, following
# docs/development/cpp-development-standard.md (Project Layout): targets stay
# small and explicit — the `orange_http` production library plus one
# executable per test suite.

cmake_minimum_required(VERSION 3.21)

project(orange_service LANGUAGES CXX)

# C++20 exactly: REQUIRED forbids a silent fallback to an older standard, and
# OFF extensions keeps GNU-dialect headers out of the compile line.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug by default so local verification runs carry debug information; a new
# warning in maintained code is a defect, not noise.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

enable_testing()

# Production library of the transport core, one capability per file, headers
# beside sources (cpp-development-standard.md, Project Layout). The PUBLIC
# include directory propagates to every target that links this library.
add_library(orange_http
    src/http_messages.cpp
    src/http_response.cpp
    src/http_server.cpp
    src/http_connection_store.cpp
    src/http_request_decoder.cpp
    src/http_body_framing.cpp
    src/http_chunk_line.cpp
    src/routes.cpp)
target_include_directories(orange_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# One executable per test suite; CTest judges each by its process exit status.
add_executable(http_message_tests tests/http_message_tests.cpp)
target_link_libraries(http_message_tests PRIVATE orange_http)
add_test(NAME http_message_tests COMMAND http_message_tests)

# Full-message tests keep body-framing cases separate from the head contract.
add_executable(http_request_decoder_tests tests/http_request_decoder_tests.cpp)
target_link_libraries(http_request_decoder_tests PRIVATE orange_http)
add_test(NAME http_request_decoder_tests COMMAND http_request_decoder_tests)

# Span advancement must agree with the existing metadata grammar at every split.
add_executable(http_chunk_line_tests tests/http_chunk_line_tests.cpp)
target_link_libraries(http_chunk_line_tests PRIVATE orange_http)
add_test(NAME http_chunk_line_tests COMMAND http_chunk_line_tests)

# Offset ownership is checked independently from complete-head parsing.
add_executable(http_head_fields_tests tests/http_head_fields_tests.cpp)
target_link_libraries(http_head_fields_tests PRIVATE orange_http)
add_test(NAME http_head_fields_tests COMMAND http_head_fields_tests)

# Owned request values must survive relocation with bounded materialization cost.
add_executable(http_request_storage_tests tests/http_request_storage_tests.cpp)
target_link_libraries(http_request_storage_tests PRIVATE orange_http)
add_test(NAME http_request_storage_tests COMMAND http_request_storage_tests)

# Response framing is the serializer side of the T-1 transport contract.
add_executable(http_response_tests tests/http_response_tests.cpp)
target_link_libraries(http_response_tests PRIVATE orange_http)
add_test(NAME http_response_tests COMMAND http_response_tests)

# The event loop is verified at the real socket boundary (ADR-0003 T-1).
add_executable(http_server_tests tests/http_server_tests.cpp)
target_link_libraries(http_server_tests PRIVATE orange_http)
add_test(NAME http_server_tests COMMAND http_server_tests)

add_executable(http_connection_store_tests tests/http_connection_store_tests.cpp)
target_link_libraries(http_connection_store_tests PRIVATE orange_http)
add_test(NAME http_connection_store_tests COMMAND http_connection_store_tests)

# ADR: service/docs/adr/ADR-0008-kqueue-response-scheduling.md
add_executable(http_server_scheduling_tests tests/http_server_scheduling_tests.cpp)
target_link_libraries(http_server_scheduling_tests PRIVATE orange_http)
add_test(NAME http_server_scheduling_tests COMMAND http_server_scheduling_tests)
add_executable(http_server_input_tests tests/http_server_input_tests.cpp)
target_link_libraries(http_server_input_tests PRIVATE orange_http)
add_test(NAME http_server_input_tests COMMAND http_server_input_tests)
add_executable(http_server_dispatch_tests tests/http_server_dispatch_tests.cpp)
target_link_libraries(http_server_dispatch_tests PRIVATE orange_http)
add_test(NAME http_server_dispatch_tests COMMAND http_server_dispatch_tests)

# Route dispatch is the first serving rung of ADR-0003 T-2.
add_executable(http_routes_tests tests/http_routes_tests.cpp)
target_link_libraries(http_routes_tests PRIVATE orange_http)
add_test(NAME http_routes_tests COMMAND http_routes_tests)

# Opt-in timings use the same optimized library as the correctness suites.
option(ORANGE_BUILD_BENCHMARKS "Build reproducible HTTP parser benchmarks" OFF)
if(ORANGE_BUILD_BENCHMARKS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "Parser benchmarks require CMAKE_BUILD_TYPE=Release")
    endif()
    add_executable(http_parser_bench bench/http_parser_bench.cpp)
    add_executable(http_parser_allocations bench/http_parser_allocations.cpp)
    target_link_libraries(http_parser_allocations PRIVATE orange_http)
    add_executable(http_parser_equivalence bench/http_parser_equivalence.cpp)
    target_link_libraries(http_parser_equivalence PRIVATE orange_http)
    target_link_libraries(http_parser_bench PRIVATE orange_http)
    set_property(TARGET http_parser_bench PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_compile_definitions(http_parser_bench PRIVATE
        ORANGE_BENCH_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        ORANGE_BENCH_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}"
        ORANGE_BENCH_ARCH="${CMAKE_SYSTEM_PROCESSOR}")
    add_test(NAME http_parser_bench_cli
        COMMAND ${CMAKE_COMMAND} -DBENCHMARK=$<TARGET_FILE:http_parser_bench>
                -DALLOCATIONS=$<TARGET_FILE:http_parser_allocations>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/bench/verify.cmake)
endif()
