#include <algorithm>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <shardPool.hpp>

namespace tinycache {

ShardPool::Worker::Worker(std::size_t capacity)
    : work_guard(asio::make_work_guard(io_context)),
      shard(capacity),
      expiration(shard) {}

ShardPool::ShardPool(std::size_t shard_count, std::size_t shard_capacity) {
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

  for (auto& worker : workers_) {
    asio::co_spawn(worker.io_context, worker.expiration.cleaning_loop(),
                   asio::detached);
    auto* worker_ptr = &worker;
    worker.thread =
        std::thread([worker_ptr]() { worker_ptr->io_context.run(); });
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

}  // namespace tinycache
