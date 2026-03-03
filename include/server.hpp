#ifndef TINYCACHE_SERVER_HPP
#define TINYCACHE_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <config.hpp>
#include "expirationController.hpp"
#include "lruCache.hpp"

namespace asio = boost::asio;

namespace tinycache {
class Server {
 public:
  explicit Server(Config& config);
  void run();

 private:
  LruCache cache_;
  ExpirationController expiration_controller_;
  asio::io_context io_context_;
  asio::ip::tcp::acceptor acceptor_;

  asio::awaitable<void> listener();

  asio::signal_set signals_;
};

};  // namespace tinycache

#endif
