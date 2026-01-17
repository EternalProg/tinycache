#ifndef TINYCACHE_UTILS_HPP
#define TINYCACHE_UTILS_HPP

#include <boost/asio.hpp>

namespace tinycache {

inline constexpr auto kAsTuple = boost::asio::as_tuple(boost::asio::use_awaitable);
}
#endif
