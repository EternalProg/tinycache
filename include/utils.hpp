#ifndef TINYCACHE_UTILS_HPP
#define TINYCACHE_UTILS_HPP

#include <boost/asio.hpp>

namespace tinycache {

inline constexpr auto kAsTuple =
    boost::asio::as_tuple(boost::asio::use_awaitable);

inline void to_uppercase(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}

}  // namespace tinycache

#endif
