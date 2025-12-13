cmake_minimum_required(VERSION 3.14)

project(TinyCache)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-g -Wall -Wextra -Wpedantic)

add_executable(${PROJECT_NAME} src/main.cpp src/server.cpp src/session.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
  include/
)

# Boost configuration
find_package(Boost CONFIG REQUIRED COMPONENTS system)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})

# SPDLOG configuration
find_package(spdlog REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system spdlog::spdlog_header_only)

# GTest configuration
include(FetchContent)
FetchContent_Declare(
  googletest
  https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

FetchContent_MakeAvailable(googletest)
enable_testing()
add_subdirectory(tests)
