#ifndef TINYCACHE_LRU_CACHE_HPP
#define TINYCACHE_LRU_CACHE_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tinycache {

// TODO(eternal): Implement LRU Cache
class LruCache {
 public:
  [[nodiscard]] std::optional<std::string> get(std::string_view key);

  void set(std::string_view key, std::string_view value);

  [[nodiscard]] bool del(std::string_view key);

 private:
  std::unordered_map<std::string_view, std::string_view> map_;
};

}  // namespace tinycache

#endif
