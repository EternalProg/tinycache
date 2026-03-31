set(Target TinyCacheTests)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} TEST_SOURCES)

add_executable(${Target}
  ${TEST_SOURCES}
)
tinycache_apply_optimizations(${Target})

target_include_directories(${Target} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${Target}
  PRIVATE
    GTest::gtest_main
    ${TINYCACHE_LIB}
    tinycache_flags
)

include(GoogleTest)
gtest_discover_tests(${Target})
add_test(NAME ${Target} COMMAND ${Target})
