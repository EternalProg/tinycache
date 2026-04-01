#include "bench_common.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

using tinycache::LruShard;
using tinycache::bench::Operation;
using tinycache::bench::WorkloadConfig;
using tinycache::bench::WorkloadCounters;
using tinycache::bench::WorkloadMix;
using tinycache::bench::ZipfSampler;

namespace {

class ReusableBarrier {
 public:
  void wait(int expected) {
    std::unique_lock<std::mutex> lock(mu_);
    if (arrived_ == 0) {
      expected_ = expected;
    }

    int gen = generation_;
    ++arrived_;
    if (arrived_ >= expected_) {
      arrived_ = 0;
      ++generation_;
      cv_.notify_all();
      return;
    }

    cv_.wait(lock, [&] { return generation_ != gen; });
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  int expected_ = 0;
  int arrived_ = 0;
  int generation_ = 0;
};

struct SharedRunState {
  ReusableBarrier barrier;
  std::unique_ptr<LruShard> cache;
  std::vector<std::string> keys;
  std::vector<std::string> values;
};

SharedRunState g_shared;

WorkloadMix mix_read_heavy() {
  return WorkloadMix{0.80, 0.15, 0.05, "read80"};
}

WorkloadMix mix_balanced() {
  return WorkloadMix{0.50, 0.40, 0.10, "balanced"};
}

WorkloadMix mix_write_heavy() {
  return WorkloadMix{0.20, 0.70, 0.10, "write70"};
}

WorkloadConfig cfg_small(const WorkloadMix& mix, bool zipf, double zipf_s) {
  return WorkloadConfig{/*cache_capacity=*/4096,
                        /*working_set=*/2048,
                        /*prefill=*/2048,
                        /*value_size=*/256,      mix,
                        /*use_zipf=*/zipf,
                        /*zipf_s=*/zipf_s};
}

WorkloadConfig cfg_pressure(const WorkloadMix& mix, bool zipf, double zipf_s) {
  return WorkloadConfig{/*cache_capacity=*/4096,
                        /*working_set=*/12288,
                        /*prefill=*/4096,
                        /*value_size=*/256,      mix,
                        /*use_zipf=*/zipf,
                        /*zipf_s=*/zipf_s};
}

void run_workload_mt(benchmark::State& state, const WorkloadConfig& config,
                     const char* label) {
  // Phase 1: synchronize start and build shared state once.
  g_shared.barrier.wait(state.threads());
  if (state.thread_index() == 0) {
    g_shared.cache = std::make_unique<LruShard>(config.cache_capacity);
    g_shared.keys = tinycache::bench::build_keys(config.working_set);
    g_shared.values =
        tinycache::bench::build_values(config.working_set, config.value_size);
    tinycache::bench::fill_cache(*g_shared.cache, g_shared.keys,
                                 g_shared.values, config.prefill);
  }
  std::atomic_thread_fence(std::memory_order_release);
  g_shared.barrier.wait(state.threads());
  std::atomic_thread_fence(std::memory_order_acquire);

  std::mt19937_64 rng(42U + static_cast<std::uint64_t>(state.thread_index()) *
                                0x9E3779B97F4A7C15ULL);
  ZipfSampler zipf(config.use_zipf ? config.working_set : 0, config.zipf_s,
                   rng);
  ZipfSampler* zipf_ptr = config.use_zipf ? &zipf : nullptr;
  std::uniform_real_distribution<double> op_dist(0.0, 1.0);

  WorkloadCounters counters;

  for (auto _ : state) {
    (void)_;  // benchmark loop token
    auto op = tinycache::bench::pick_operation(op_dist(rng), config.mix);
    std::size_t key_index =
        tinycache::bench::pick_key(rng, zipf_ptr, config.working_set);

    if (op == Operation::kGet) {
      auto result = g_shared.cache->get(g_shared.keys[key_index]);
      if (result.has_value()) {
        ++counters.get_hits;
      } else {
        ++counters.get_misses;
      }
      ++counters.get_ops;
      benchmark::DoNotOptimize(result);
    } else if (op == Operation::kSet) {
      g_shared.cache->set(g_shared.keys[key_index], g_shared.values[key_index]);
      ++counters.set_ops;
      benchmark::ClobberMemory();
    } else {
      [[maybe_unused]] auto deleted =
          g_shared.cache->del(g_shared.keys[key_index]);
      ++counters.del_ops;
    }
  }

  // Publish per-thread counters (summed across threads by the framework).
  state.counters["get_ops"] = static_cast<double>(counters.get_ops);
  state.counters["set_ops"] = static_cast<double>(counters.set_ops);
  state.counters["del_ops"] = static_cast<double>(counters.del_ops);
  state.counters["get_hits"] = static_cast<double>(counters.get_hits);
  state.counters["get_misses"] = static_cast<double>(counters.get_misses);

  // One iteration == one cache op.
  state.SetItemsProcessed(state.iterations());
  state.SetLabel(label);

  // Phase 2: teardown shared state after all threads finished.
  g_shared.barrier.wait(state.threads());
  if (state.thread_index() == 0) {
    g_shared.cache.reset();
    g_shared.keys.clear();
    g_shared.values.clear();
    g_shared.keys.shrink_to_fit();
    g_shared.values.shrink_to_fit();
  }
  std::atomic_thread_fence(std::memory_order_release);
  g_shared.barrier.wait(state.threads());
  std::atomic_thread_fence(std::memory_order_acquire);
}

void BM_LruMt_ReadHeavy_Uniform_Small(benchmark::State& state) {
  auto config = cfg_small(mix_read_heavy(), /*zipf=*/false, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "read80/uniform/small");
}

void BM_LruMt_ReadHeavy_Zipf_Small(benchmark::State& state) {
  auto config = cfg_small(mix_read_heavy(), /*zipf=*/true, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "read80/zipf/small");
}

void BM_LruMt_ReadHeavy_Uniform_Pressure(benchmark::State& state) {
  auto config = cfg_pressure(mix_read_heavy(), /*zipf=*/false, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "read80/uniform/pressure");
}

void BM_LruMt_ReadHeavy_Zipf_Pressure(benchmark::State& state) {
  auto config = cfg_pressure(mix_read_heavy(), /*zipf=*/true, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "read80/zipf/pressure");
}

void BM_LruMt_Balanced_Uniform_Small(benchmark::State& state) {
  auto config = cfg_small(mix_balanced(), /*zipf=*/false, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "balanced/uniform/small");
}

void BM_LruMt_Balanced_Zipf_Small(benchmark::State& state) {
  auto config = cfg_small(mix_balanced(), /*zipf=*/true, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "balanced/zipf/small");
}

void BM_LruMt_WriteHeavy_Uniform_Small(benchmark::State& state) {
  auto config = cfg_small(mix_write_heavy(), /*zipf=*/false, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "write70/uniform/small");
}

void BM_LruMt_WriteHeavy_Zipf_Small(benchmark::State& state) {
  auto config = cfg_small(mix_write_heavy(), /*zipf=*/true, /*zipf_s=*/1.2);
  run_workload_mt(state, config, "write70/zipf/small");
}

}  // namespace

BENCHMARK(BM_LruMt_ReadHeavy_Uniform_Small)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_ReadHeavy_Zipf_Small)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_ReadHeavy_Uniform_Pressure)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_ReadHeavy_Zipf_Pressure)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_Balanced_Uniform_Small)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_Balanced_Zipf_Small)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_WriteHeavy_Uniform_Small)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_LruMt_WriteHeavy_Zipf_Small)
    ->MinTime(0.1)
    ->ThreadRange(1, 16)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
