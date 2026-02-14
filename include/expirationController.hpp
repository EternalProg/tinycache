#ifndef TINYCACHE_EXPIRATION_CONTROLLER_HPP
#define TINYCACHE_EXPIRATION_CONTROLLER_HPP

#include <boost/asio.hpp>
#include "lruCache.hpp"

namespace asio = boost::asio;

namespace tinycache {

class ExpirationController {
 public:
  explicit ExpirationController(LruCache& cache);
  asio::awaitable<void> cleaning_loop();

 private:
  LruCache& cache_;
};

}  // namespace tinycache

#endif
