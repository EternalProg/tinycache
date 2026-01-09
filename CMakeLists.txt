cmake_minimum_required(VERSION 3.14)

project(TinyCache)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES src/main.cpp src/server.cpp src/session.cpp src/respParser.cpp)

add_compile_options(-g -Wall -Wextra -Wpedantic)

add_executable(${PROJECT_NAME} ${SOURCES})

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
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(googletest)
add_subdirectory(tests)
