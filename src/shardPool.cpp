#include <spdlog/spdlog.h>
#include <algorithm>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <shardPool.hpp>

namespace tinycache {

namespace {
#ifdef __linux__
#include <pthread.h>
#include <sched.h>

void pin_thread_to_core(unsigned int core) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  const int rc =
      pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  if (rc != 0) {
    SPDLOG_WARN("Failed to set affinity to core {}", core);
  }
}
#endif

}  // namespace

ShardPool::Worker::Worker(std::size_t capacity)
    : work_guard(asio::make_work_guard(io_context)),
      shard(capacity),
      expiration(shard) {}

ShardPool::ShardPool(std::size_t shard_count, std::size_t shard_capacity,
                     bool thread_affinity_enabled)
    : thread_affinity_enabled_(thread_affinity_enabled) {
  auto count = std::max<std::size_t>(1, shard_count);
  for (std::size_t i = 0; i < count; ++i) {
    workers_.emplace_back(shard_capacity);
  }
}

ShardPool::~ShardPool() {
  stop();
}

void ShardPool::start() {
  if (started_) {
    return;
  }
  started_ = true;

  for (std::size_t worker_i = 0; worker_i < workers_.size(); ++worker_i) {
    auto* worker_ptr = &workers_[worker_i];
    asio::co_spawn(worker_ptr->io_context,
                   worker_ptr->expiration.cleaning_loop(), asio::detached);
    worker_ptr->thread = std::thread([this, worker_ptr, worker_i]() {
      worker_ptr->thread_id = std::this_thread::get_id();
#ifdef __linux__
      if (thread_affinity_enabled_) {
        const auto cores = std::max(1U, std::thread::hardware_concurrency());
        pin_thread_to_core(static_cast<unsigned int>(worker_i % cores));
      }
#endif
      worker_ptr->io_context.run();
    });
  }
}

void ShardPool::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;

  for (auto& worker : workers_) {
    worker.work_guard.reset();
    worker.io_context.stop();
  }

  for (auto& worker : workers_) {
    if (worker.thread.joinable()) {
      worker.thread.join();
    }
  }
}

std::size_t ShardPool::size() const {
  return workers_.size();
}

asio::any_io_executor ShardPool::executor_for(std::size_t shard_index) {
  assert(shard_index < workers_.size());
  return workers_[shard_index].io_context.get_executor();
}

bool ShardPool::is_on_shard_thread(std::size_t shard_index) const {
  assert(shard_index < workers_.size());
  return workers_[shard_index].thread_id == std::this_thread::get_id();
}

LruShard& ShardPool::local_shard(std::size_t shard_index) {
  assert(shard_index < workers_.size());
  assert(is_on_shard_thread(shard_index));
  return workers_[shard_index].shard;
}

}  // namespace tinycache
