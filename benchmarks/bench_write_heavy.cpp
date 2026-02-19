#include "bench_common.hpp"

using tinycache::LruCache;
using tinycache::bench::LatencyCollector;
using tinycache::bench::Operation;
using tinycache::bench::WorkloadConfig;
using tinycache::bench::WorkloadCounters;
using tinycache::bench::WorkloadMix;
using tinycache::bench::ZipfSampler;

namespace {

WorkloadMix mix_write_heavy() {
  return WorkloadMix{0.20, 0.70, 0.10, "write70"};
}

void run_workload(benchmark::State& state, const WorkloadConfig& config) {
  LruCache cache(config.cache_capacity);
  auto keys = tinycache::bench::build_keys(config.working_set);
  auto values =
      tinycache::bench::build_values(config.working_set, config.value_size);
  tinycache::bench::fill_cache(cache, keys, values, config.prefill);

  std::mt19937_64 rng(42);
  ZipfSampler zipf(config.use_zipf ? config.working_set : 0, config.zipf_s,
                   rng);
  ZipfSampler* zipf_ptr = config.use_zipf ? &zipf : nullptr;
  std::uniform_real_distribution<double> op_dist(0.0, 1.0);

  WorkloadCounters counters;
  LatencyCollector latency;

  for (auto _ : state) {
    (void)_;
    auto op = tinycache::bench::pick_operation(op_dist(rng), config.mix);
    std::size_t key_index =
        tinycache::bench::pick_key(rng, zipf_ptr, config.working_set);

    auto start = std::chrono::steady_clock::now();
    if (op == Operation::kGet) {
      auto result = cache.get(keys[key_index]);
      if (result.has_value()) {
        ++counters.get_hits;
      } else {
        ++counters.get_misses;
      }
      ++counters.get_ops;
      benchmark::DoNotOptimize(result);
    } else if (op == Operation::kSet) {
      cache.set(keys[key_index], values[key_index]);
      ++counters.set_ops;
      benchmark::ClobberMemory();
    } else {
      [[maybe_unused]] auto deleted = cache.del(keys[key_index]);
      ++counters.del_ops;
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();
    latency.add(static_cast<std::uint64_t>(elapsed));
  }

  tinycache::bench::apply_counters(state, counters, latency);
  state.SetLabel(config.mix.name);
}

void BM_WriteHeavy_Uniform_Small(benchmark::State& state) {
  WorkloadConfig config{/*cache_capacity=*/4096,
                        /*working_set=*/2048,
                        /*prefill=*/2048,
                        /*value_size=*/256,
                        mix_write_heavy(),
                        /*use_zipf=*/false,
                        /*zipf_s=*/1.2};
  run_workload(state, config);
}

void BM_WriteHeavy_Zipf_Small(benchmark::State& state) {
  WorkloadConfig config{/*cache_capacity=*/4096,
                        /*working_set=*/2048,
                        /*prefill=*/2048,
                        /*value_size=*/256,
                        mix_write_heavy(),
                        /*use_zipf=*/true,
                        /*zipf_s=*/1.2};
  run_workload(state, config);
}

}  // namespace

BENCHMARK(BM_WriteHeavy_Uniform_Small)
    ->MinTime(0.1)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_WriteHeavy_Zipf_Small)
    ->MinTime(0.1)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
