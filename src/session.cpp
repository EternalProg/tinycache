#include <spdlog/spdlog.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <session.hpp>
#include "utils.hpp"

namespace tinycache {

inline constexpr std::size_t kMaxMessageSize = 1024;

Session::Session(asio::ip::tcp::socket socket)
    : socket_(std::move(socket)),
      strand_(asio::make_strand(socket_.get_executor())) {}

asio::awaitable<void> Session::run() {
  co_await read();
  co_await write();
}

asio::awaitable<void> Session::read() {
  // auto buffer = buffer_.prepare(kMaxMessageSize);
  auto [ec, nbytes] = co_await asio::async_read_until(
      socket_, buffer_, "\r\n", asio::bind_executor(strand_, kAsTuple));
  if (ec) {
    spdlog::error("Read: {}", ec.message());
  }
  spdlog::debug("Read message: {}", nbytes);
  // buffer_.commit(nbytes);
}

asio::awaitable<void> Session::write() {
  std::string message = "OK\n";
  co_await asio::async_write(socket_, asio::buffer(message),
                             asio::bind_executor(strand_, kAsTuple));
  spdlog::debug("Response sent: {}", message);
}

}  // namespace tinycache
