# Governed by docs/adr/ADR-0003-local-runtime-path.md.
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
    src/http_request_decoder.cpp
    src/http_body_framing.cpp
    src/http_chunk_line.cpp)
target_include_directories(orange_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# One executable per test suite; CTest judges each by its process exit status.
add_executable(http_message_tests tests/http_message_tests.cpp)
target_link_libraries(http_message_tests PRIVATE orange_http)
add_test(NAME http_message_tests COMMAND http_message_tests)

# Full-message tests keep body-framing cases separate from the head contract.
add_executable(http_request_decoder_tests tests/http_request_decoder_tests.cpp)
target_link_libraries(http_request_decoder_tests PRIVATE orange_http)
add_test(NAME http_request_decoder_tests COMMAND http_request_decoder_tests)
