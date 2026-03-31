# Benchmarks configuration
set(BENCHMARK_ENABLE_TESTING OFF)  # Don't build benchmark's internal tests
set(BENCHMARK_ENABLE_GTEST_TESTS OFF)  # Don't require GTest for benchmark itself

# Create benchmark executables
add_executable(tinycache_bench_read_heavy
  bench_read_heavy.cpp
)
tinycache_apply_optimizations(tinycache_bench_read_heavy)

add_executable(tinycache_bench_balanced
  bench_balanced.cpp
)
tinycache_apply_optimizations(tinycache_bench_balanced)

add_executable(tinycache_bench_write_heavy
  bench_write_heavy.cpp
)
tinycache_apply_optimizations(tinycache_bench_write_heavy)

add_executable(tinycache_bench_resp
  bench_resp.cpp
)
tinycache_apply_optimizations(tinycache_bench_resp)

add_executable(tinycache_bench_lru_mt
  bench_lru_mt.cpp
)
tinycache_apply_optimizations(tinycache_bench_lru_mt)

# Link dependencies
target_link_libraries(tinycache_bench_read_heavy PRIVATE
  ${TINYCACHE_LIB}
  benchmark::benchmark
  tinycache_flags
)

target_link_libraries(tinycache_bench_balanced PRIVATE
  ${TINYCACHE_LIB}
  benchmark::benchmark
  tinycache_flags
)

target_link_libraries(tinycache_bench_write_heavy PRIVATE
  ${TINYCACHE_LIB}
  benchmark::benchmark
  tinycache_flags
)

target_link_libraries(tinycache_bench_resp PRIVATE
  ${TINYCACHE_LIB}
  benchmark::benchmark
  tinycache_flags
)

target_link_libraries(tinycache_bench_lru_mt PRIVATE
  ${TINYCACHE_LIB}
  benchmark::benchmark
  tinycache_flags
)

# Ensure we build in Release mode for accurate benchmarks
target_compile_options(tinycache_bench_read_heavy PRIVATE -O3)
target_compile_options(tinycache_bench_balanced PRIVATE -O3)
target_compile_options(tinycache_bench_write_heavy PRIVATE -O3)
target_compile_options(tinycache_bench_resp PRIVATE -O3)
target_compile_options(tinycache_bench_lru_mt PRIVATE -O3)

# Add custom target to run benchmarks easily
add_custom_target(run_benchmarks
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tinycache_bench_read_heavy
  DEPENDS tinycache_bench_read_heavy
  COMMENT "Running TinyCache read-heavy benchmarks..."
)

# Also allow running with custom flags
add_custom_target(run_benchmarks_verbose
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tinycache_bench_read_heavy --benchmark_verbose
  DEPENDS tinycache_bench_read_heavy
  COMMENT "Running TinyCache read-heavy benchmarks (verbose mode)..."
)
