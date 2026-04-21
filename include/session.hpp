#ifndef TINYCACHE_SESSION_HPP
#define TINYCACHE_SESSION_HPP

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <commandExecutor.hpp>
#include <cstddef>
#include <memory>
#include "shardPool.hpp"

namespace asio = boost::asio;

namespace tinycache {
enum class ReadResult {
  kNewMessage,
  kCloseConnection,
  kReadError,
  kMessageTooLarge
};

class Session : public std::enable_shared_from_this<Session> {
 public:
  explicit Session(asio::ip::tcp::socket socket, ShardPool& shard_pool,
                   std::size_t home_shard, std::size_t max_message_size);
  asio::awaitable<void> run();

 private:
  asio::ip::tcp::socket socket_;
  asio::streambuf buffer_;
  asio::strand<asio::ip::tcp::socket::executor_type> strand_;
  CommandExecutor executor_;
  std::size_t home_shard_;
  std::size_t max_message_size_;

  [[nodiscard]] asio::awaitable<ReadResult> read();
  asio::awaitable<void> write(std::string message);
};

}  // namespace tinycache

#endif
