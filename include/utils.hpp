#ifndef TINYCACHE_UTILS_HPP
#define TINYCACHE_UTILS_HPP

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace tinycache {

inline void to_uppercase(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
}

inline void to_lowercase(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
}

}  // namespace tinycache

#endif
