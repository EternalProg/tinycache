#ifndef TINYCACHE_SERVER_HPP
#define TINYCACHE_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include "commandExecutor.hpp"

namespace asio = boost::asio;

namespace tinycache {
class Server {
 public:
  explicit Server(std::uint16_t port);
  void run();

 private:
  CommandExecutor executor_;  // stores the cache inside
  asio::io_context io_context_;
  asio::ip::tcp::acceptor acceptor_;

  asio::awaitable<void> listener();

  asio::signal_set signals_;
};

};  // namespace tinycache

#endif
