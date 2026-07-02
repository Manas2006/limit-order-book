cmake_minimum_required(VERSION 3.20)

project(limit_order_book
    VERSION 0.1.0
    DESCRIPTION "C++20 limit order book and matching engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOB_BUILD_TESTS "Build unit tests" ON)
option(LOB_BUILD_BENCHMARKS "Build benchmark executable" ON)

add_library(lob
    src/csv_replay.cpp
    src/matching_engine.cpp)

target_include_directories(lob
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include)

target_compile_features(lob PUBLIC cxx_std_20)

target_compile_options(lob
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>)

add_executable(lob_replay src/main.cpp)
target_link_libraries(lob_replay PRIVATE lob)

if(LOB_BUILD_BENCHMARKS)
    add_executable(lob_bench bench/benchmark_main.cpp)
    target_link_libraries(lob_bench PRIVATE lob)
endif()

include(CTest)
if(BUILD_TESTING AND LOB_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1)
    FetchContent_MakeAvailable(Catch2)

    add_executable(lob_tests tests/matching_engine_tests.cpp)
    target_link_libraries(lob_tests PRIVATE lob Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(lob_tests)
endif()
