#include <spdlog/spdlog.h>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <server.hpp>
#include <session.hpp>
#include "utils.hpp"

namespace tinycache {

Server::Server(Config& config)
    : shard_pool_(config.shard_count, config.max_items_per_shard,
                  config.thread_affinity_enabled),
      acceptor_(io_context_),
      signals_(io_context_, SIGINT, SIGTERM) {

  auto addr = asio::ip::make_address(config.host);  // e.g. "0.0.0.0"
  boost::asio::ip::tcp::endpoint ep(addr, config.port);
  acceptor_.open(ep.protocol());
  acceptor_.set_option(asio::socket_base::reuse_address(true));
  acceptor_.bind(ep);
  acceptor_.listen(asio::socket_base::max_listen_connections);

  signals_.async_wait([&](auto, auto) {
    spdlog::info("Server is closing");
    shard_pool_.stop();
    io_context_.stop();
  });
}

void Server::run() {
  shard_pool_.start();
  co_spawn(io_context_, listener(), asio::detached);
  io_context_.run();
  shard_pool_.stop();
}

asio::awaitable<void> Server::listener() {
  spdlog::info("Server is running on port {}",
               acceptor_.local_endpoint().port());
  spdlog::info("Press Ctrl+C to stop the server");

  for (;;) {
    auto shard_index = next_shard_++ % shard_pool_.size();
    asio::ip::tcp::socket socket(shard_pool_.executor_for(shard_index));
    boost::system::error_code ec;
    co_await acceptor_.async_accept(
        socket, asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
      spdlog::error("Accept error: {}", ec.message());
      continue;
    }
    spdlog::info("New Connection");

    auto session =
        std::make_shared<Session>(std::move(socket), shard_pool_, shard_index);

    co_spawn(
        shard_pool_.executor_for(shard_index),
        [session]() -> asio::awaitable<void> { co_await session->run(); },
        asio::detached);
  }
}

}  // namespace tinycache
