# Governed by docs/adr/ADR-0003-local-runtime-path.md.
#
# Build description for the orange loopback compute service, following
# docs/development/cpp-development-standard.md (Project Layout): targets stay
# small and explicit, and the `orange_http` library target arrives with the
# first production translation unit, not before.

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

# One executable per test suite; CTest judges each by its process exit status.
add_executable(http_message_tests tests/http_message_tests.cpp)
add_test(NAME http_message_tests COMMAND http_message_tests)
