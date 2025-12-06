set(Target TinyCacheTests)

add_executable(${Target}
  test.cpp
)

target_link_libraries(${Target}
  PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${Target})
