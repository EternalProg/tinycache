cmake_minimum_required(VERSION 3.14)

project(tinycache)

set(TINYCACHE_LIB tinycache_lib)
set(DEBUG_FLAGS -Wall -Wextra -Wpedantic -g)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

aux_source_directory(${PROJECT_SOURCE_DIR}/src SOURCES)

add_library(tinycache_flags INTERFACE)
target_compile_options(tinycache_flags INTERFACE -Wall -Wextra -Wpedantic)
target_compile_definitions(tinycache_flags INTERFACE
  $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
  $<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN>
  $<$<CONFIG:RelWithDebInfo>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN>
  $<$<CONFIG:MinSizeRel>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN>
)

add_library(${TINYCACHE_LIB}
  ${SOURCES}
)

target_include_directories(${TINYCACHE_LIB} PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

# Boost configuration
find_package(Boost CONFIG REQUIRED COMPONENTS system)

# SPDLOG configuration
find_package(spdlog REQUIRED)

# XXHASH configuration
find_package(PkgConfig REQUIRED)
pkg_check_modules(XXHASH REQUIRED libxxhash)

target_include_directories(${TINYCACHE_LIB} PRIVATE
  ${Boost_INCLUDE_DIRS}
  ${XXHASH_INCLUDE_DIRS}
)

include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

target_link_libraries(${TINYCACHE_LIB} PRIVATE
  Boost::system
  spdlog::spdlog_header_only
  tinycache_flags
  tomlplusplus::tomlplusplus
  ${XXHASH_LIBRARIES}
)

set(MAIN_FILE src/main.cpp)

add_executable(${PROJECT_NAME} ${MAIN_FILE})
target_link_libraries(${PROJECT_NAME} PRIVATE ${TINYCACHE_LIB} tinycache_flags)

# Testing and Benchmarking configuration

option(ENABLE_TESTS "Build Google Test tests" ON)
if(ENABLE_TESTS)
  # GTest configuration
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
  )
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()

# Option to enable benchmarks
option(ENABLE_BENCHMARKS "Build Google Benchmark benchmarks" OFF)
if(ENABLE_BENCHMARKS)
  # Configure benchmark before FetchContent_MakeAvailable() so it does not
  # try to build its own gtest-based tests when ENABLE_TESTS=OFF.
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_USE_BUNDLED_GTEST OFF CACHE BOOL "" FORCE)

  # Google Benchmark configuration
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
  )
  FetchContent_MakeAvailable(benchmark)
  add_subdirectory(benchmarks)
endif()
