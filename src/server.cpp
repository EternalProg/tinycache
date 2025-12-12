#include <spdlog/spdlog.h>
#include <iostream>
#include <server.hpp>

namespace tinycache {
Server::Server(std::uint16_t port)
    : acceptor_(io_context_, {boost::asio::ip::tcp::v4(), port}) {}

void Server::run() {
  co_spawn(io_context_, listener(), asio::detached);
  spdlog::info("Server is running");
  io_context_.run();
}

asio::awaitable<void> Server::listener() {
  for (;;) {
    asio::ip::tcp::socket socket =
        co_await acceptor_.async_accept(asio::use_awaitable);
    spdlog::info("New Connection");
    //co_spawn(io_context_, echo(std::move(socket)), asio::detached);
  }
}

}  // namespace tinycache
