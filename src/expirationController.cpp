#include <spdlog/spdlog.h>
#include <expirationController.hpp>

namespace tinycache {

ExpirationController::ExpirationController(LruCache& cache) : cache_(cache) {
  spdlog::debug("Expiration controller started");
}

asio::awaitable<void> ExpirationController::cleaning_loop() {
  while (true) {
    auto next_expire_time = cache_.get_next_expire_time();

    if (next_expire_time.has_value()) {
      auto now = std::chrono::steady_clock::now();

      if (next_expire_time.value() > now) {
        auto wait_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                next_expire_time.value() - now);

        co_await asio::steady_timer(co_await asio::this_coro::executor,
                                    wait_duration)
            .async_wait(asio::use_awaitable);
      }
    } else {
      // No keys with expiration, wait for a fixed interval before checking again
      co_await asio::steady_timer(co_await asio::this_coro::executor,
                                  std::chrono::seconds(3))
          .async_wait(asio::use_awaitable);
    }

    cache_.remove_expired_keys(std::chrono::steady_clock::now());
  }
}

}  // namespace tinycache
