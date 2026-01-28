#ifndef TINYCACHE_SESSION_HPP
#define TINYCACHE_SESSION_HPP

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <memory>
#include <string>
#include "commandExecutor.hpp"

namespace asio = boost::asio;

namespace tinycache {
enum class ReadResult { kNewMessage, kCloseConnection, kReadError };

class Session : std::enable_shared_from_this<Session> {
 public:
  explicit Session(asio::ip::tcp::socket socket);
  asio::awaitable<void> run();

 private:
  asio::ip::tcp::socket socket_;
  asio::streambuf buffer_;
  asio::strand<asio::ip::tcp::socket::executor_type> strand_;
  CommandExecutor executor_;

  [[nodiscard]] asio::awaitable<ReadResult> read();
  asio::awaitable<void> write(std::string message);
};

}  // namespace tinycache

#endif
