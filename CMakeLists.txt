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

add_library(${TINYCACHE_LIB}
  ${SOURCES}
)

target_include_directories(${TINYCACHE_LIB} PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

# Boost configuration
find_package(Boost CONFIG REQUIRED COMPONENTS system)
target_include_directories(${TINYCACHE_LIB} PRIVATE ${Boost_INCLUDE_DIRS})

# SPDLOG configuration
find_package(spdlog REQUIRED)

target_link_libraries(${TINYCACHE_LIB} PRIVATE
  Boost::system
  spdlog::spdlog_header_only
  tinycache_flags
)

set(MAIN_FILE src/main.cpp)

add_executable(${PROJECT_NAME} ${MAIN_FILE})
target_link_libraries(${PROJECT_NAME} PRIVATE ${TINYCACHE_LIB} tinycache_flags)

# GTest configuration
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(googletest)
add_subdirectory(tests)


