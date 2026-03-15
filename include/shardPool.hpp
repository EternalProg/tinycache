#ifndef TINYCACHE_SHARD_POOL_HPP
#define TINYCACHE_SHARD_POOL_HPP

#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cassert>
#include <deque>
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

    auto executor = co_await asio::this_coro::executor;

    auto token = asio::use_awaitable;
    co_return co_await asio::async_initiate<void(Result)>(
        [this, shard_index, fn = std::forward<Fn>(fn),
         executor](auto handler) mutable {
          auto& worker = workers_[shard_index];
          asio::post(worker.io_context, [this, shard_index, fn = std::move(fn),
                                         handler = std::move(handler),
                                         executor]() mutable {
            Result result = fn(workers_[shard_index].shard);
            asio::post(executor, [handler = std::move(handler),
                                  result = std::move(result)]() mutable {
              handler(std::move(result));
            });
          });
        },
        token);
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
