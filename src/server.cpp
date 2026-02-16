#include <spdlog/spdlog.h>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <server.hpp>
#include <session.hpp>
#include "expirationController.hpp"
#include "utils.hpp"

namespace tinycache {

Server::Server(std::uint16_t port)
    : expiration_controller_(cache_),
      acceptor_(io_context_, {boost::asio::ip::tcp::v4(), port}),
      signals_(io_context_, SIGINT, SIGTERM) {
  signals_.async_wait([&](auto, auto) {
    spdlog::info("Server is closing");
    io_context_.stop();
  });
}

void Server::run() {
  co_spawn(io_context_, listener(), asio::detached);
  co_spawn(io_context_, expiration_controller_.cleaning_loop(), asio::detached);
  io_context_.run();
}

asio::awaitable<void> Server::listener() {
  spdlog::info("Server is running on port {}",
               acceptor_.local_endpoint().port());
  spdlog::info("Press Ctrl+C to stop the server");

  for (;;) {
    auto [ec, socket] = co_await acceptor_.async_accept(kAsTuple);
    if (ec) {
      spdlog::error("Accept error: {}", ec.message());
      continue;
    }
    spdlog::info("New Connection");

    auto session =
        std::make_shared<Session>(std::move(socket), CommandExecutor(cache_));

    co_spawn(
        io_context_,
        [session]() -> asio::awaitable<void> { co_await session->run(); },
        asio::detached);
  }
}

}  // namespace tinycache
