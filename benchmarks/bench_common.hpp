#ifndef BENCH_COMMON_HPP
#define BENCH_COMMON_HPP

#include <benchmark/benchmark.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <lruShard.hpp>
#include <random>
#include <string>
#include <vector>

namespace tinycache::bench {

enum class Operation { kGet, kSet, kDel };

struct WorkloadMix {
  double get_ratio;
  double set_ratio;
  double del_ratio;
  const char* name;
};

struct WorkloadConfig {
  std::size_t cache_capacity;
  std::size_t working_set;
  std::size_t prefill;
  std::size_t value_size;
  WorkloadMix mix;
  bool use_zipf;
  double zipf_s;
};

struct WorkloadCounters {
  std::uint64_t get_ops = 0;
  std::uint64_t set_ops = 0;
  std::uint64_t del_ops = 0;
  std::uint64_t get_hits = 0;
  std::uint64_t get_misses = 0;
};

class ZipfSampler {
 public:
  ZipfSampler(std::size_t n, double s, std::mt19937_64& rng)
      : dist_(0.0, 1.0), rng_(rng), cdf_(n) {
    double normalizer = 0.0;
    for (std::size_t i = 1; i <= n; ++i) {
      normalizer += 1.0 / std::pow(static_cast<double>(i), s);
    }
    double cumulative = 0.0;
    for (std::size_t i = 1; i <= n; ++i) {
      cumulative += 1.0 / std::pow(static_cast<double>(i), s);
      cdf_[i - 1] = cumulative / normalizer;
    }
  }

  std::size_t sample() {
    double r = dist_(rng_);
    auto it = std::lower_bound(cdf_.begin(), cdf_.end(), r);
    return static_cast<std::size_t>(std::distance(cdf_.begin(), it));
  }

 private:
  std::uniform_real_distribution<double> dist_;
  std::mt19937_64& rng_;
  std::vector<double> cdf_;
};

struct LatencyCollector {
  std::vector<std::uint64_t> nanos;

  void add(std::uint64_t value) { nanos.push_back(value); }

  std::uint64_t percentile(double p) const {
    if (nanos.empty()) {
      return 0;
    }
    std::vector<std::uint64_t> sorted = nanos;
    std::sort(sorted.begin(), sorted.end());
    double rank = p * static_cast<double>(sorted.size() - 1);
    auto idx = static_cast<std::size_t>(rank);
    return sorted[idx];
  }
};

inline std::vector<std::string> build_keys(std::size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    keys.push_back("key_" + std::to_string(i));
  }
  return keys;
}

inline std::vector<std::string> build_values(std::size_t count,
                                             std::size_t size) {
  std::vector<std::string> values;
  values.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    values.emplace_back(size, static_cast<char>('a' + (i % 26)));
  }
  return values;
}

inline Operation pick_operation(double r, const WorkloadMix& mix) {
  if (r < mix.get_ratio) {
    return Operation::kGet;
  }
  if (r < mix.get_ratio + mix.set_ratio) {
    return Operation::kSet;
  }
  return Operation::kDel;
}

inline std::size_t pick_key(std::mt19937_64& rng, ZipfSampler* zipf,
                            std::size_t key_count) {
  if (zipf) {
    return zipf->sample();
  }
  std::uniform_int_distribution<std::size_t> dist(0, key_count - 1);
  return dist(rng);
}

inline void fill_cache(tinycache::LruShard& cache,
                       const std::vector<std::string>& keys,
                       const std::vector<std::string>& values,
                       std::size_t prefill) {
  for (std::size_t i = 0; i < prefill && i < keys.size(); ++i) {
    cache.set(keys[i], values[i]);
  }
}

inline void apply_counters(benchmark::State& state,
                           const WorkloadCounters& counters,
                           const LatencyCollector& latency) {
  state.counters["p50_ns"] =
      benchmark::Counter(static_cast<double>(latency.percentile(0.50)));
  state.counters["p95_ns"] =
      benchmark::Counter(static_cast<double>(latency.percentile(0.95)));
  state.counters["p99_ns"] =
      benchmark::Counter(static_cast<double>(latency.percentile(0.99)));

  auto total_ops = static_cast<double>(counters.get_ops + counters.set_ops +
                                       counters.del_ops);
  if (total_ops > 0.0) {
    state.counters["get_ratio"] =
        benchmark::Counter(static_cast<double>(counters.get_ops) / total_ops);
    state.counters["set_ratio"] =
        benchmark::Counter(static_cast<double>(counters.set_ops) / total_ops);
    state.counters["del_ratio"] =
        benchmark::Counter(static_cast<double>(counters.del_ops) / total_ops);
  }

  auto hit_total = static_cast<double>(counters.get_hits + counters.get_misses);
  if (hit_total > 0.0) {
    state.counters["hit_rate"] =
        benchmark::Counter(static_cast<double>(counters.get_hits) / hit_total);
  }
}

}  // namespace tinycache::bench

#endif  // BENCH_COMMON_HPP
