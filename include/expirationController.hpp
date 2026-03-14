#ifndef TINYCACHE_EXPIRATION_CONTROLLER_HPP
#define TINYCACHE_EXPIRATION_CONTROLLER_HPP

#include <boost/asio.hpp>
#include "lruShard.hpp"

namespace asio = boost::asio;

namespace tinycache {

class ExpirationController {
 public:
  explicit ExpirationController(LruShard& cache);
  asio::awaitable<void> cleaning_loop();

 private:
  LruShard& cache_;
};

}  // namespace tinycache

#endif
