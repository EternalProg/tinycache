cmake_minimum_required(VERSION 3.14)

project(CacheServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-g -Wall -Wextra -Wpedantic)

add_executable(${PROJECT_NAME} src/main.cpp)

# Boost configuration
find_package(Boost CONFIG REQUIRED COMPONENTS system)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})

# GTest configuration
include(FetchContent)
FetchContent_Declare(
  googletest
  https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

FetchContent_MakeAvailable(googletest)
enable_testing()
add_subdirectory(tests)
