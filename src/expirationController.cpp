#include <spdlog/spdlog.h>
#include <expirationController.hpp>

namespace tinycache {

ExpirationController::ExpirationController(LruShard& cache) : cache_(cache) {}

asio::awaitable<void> ExpirationController::cleaning_loop() {
  spdlog::debug("ExpirationController: cleaning_loop started");
  auto executor = co_await asio::this_coro::executor;
  asio::steady_timer timer(executor);

  while (true) {
    auto next_expire_time = cache_.get_next_expire_time();

    if (next_expire_time.has_value()) {
      auto now = std::chrono::steady_clock::now();

      if (next_expire_time.value() > now) {
        timer.expires_at(next_expire_time.value());
        co_await timer.async_wait(asio::use_awaitable);
      }
    } else {
      timer.expires_after(std::chrono::seconds(3));
      co_await timer.async_wait(asio::use_awaitable);
    }

    cache_.remove_expired_keys(std::chrono::steady_clock::now());
  }
}

}  // namespace tinycache
