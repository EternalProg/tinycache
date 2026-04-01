#ifndef TINYCACHE_UTILS_HPP
#define TINYCACHE_UTILS_HPP

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace tinycache {

inline void to_uppercase(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}

}  // namespace tinycache

#endif
