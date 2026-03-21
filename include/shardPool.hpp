#ifndef TINYCACHE_SHARD_POOL_HPP
#define TINYCACHE_SHARD_POOL_HPP

#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cassert>
#include <deque>
#include <functional>
#include <thread>
#include <type_traits>
#include <utility>
#include "expirationController.hpp"
#include "lruShard.hpp"

namespace asio = boost::asio;

namespace tinycache {

class ShardPool {
 public:
  ShardPool(std::size_t shard_count, std::size_t shard_capacity);
  ~ShardPool();

  void start();
  void stop();

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] asio::any_io_executor executor_for(std::size_t shard_index);
  [[nodiscard]] bool is_on_shard_thread(std::size_t shard_index) const;
  [[nodiscard]] LruShard& local_shard(std::size_t shard_index);

  template <typename Fn>
  asio::awaitable<std::invoke_result_t<Fn, LruShard&>> run_on_shard(
      std::size_t shard_index, Fn&& fn) {
    using Result = std::invoke_result_t<Fn, LruShard&>;
    assert(shard_index < workers_.size());

    auto& worker = workers_[shard_index];
    co_return co_await asio::co_spawn(
        worker.io_context,
        [fn = std::forward<Fn>(fn),
         &worker]() mutable -> asio::awaitable<Result> {
          if constexpr (std::is_void_v<Result>) {
            std::invoke(std::move(fn), worker.shard);
            co_return;
          }
          co_return std::invoke(std::move(fn), worker.shard);
        },
        asio::use_awaitable);
  }

 private:
  struct Worker {
    asio::io_context io_context;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    LruShard shard;
    ExpirationController expiration;
    std::thread thread;
    std::thread::id thread_id;

    explicit Worker(std::size_t capacity);
  };

  std::deque<Worker> workers_;
  bool started_ = false;
  bool stopped_ = false;
};

}  // namespace tinycache

#endif
