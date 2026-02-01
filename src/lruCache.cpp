#include <lruCache.hpp>

namespace tinycache {

std::optional<std::string> LruCache::get(std::string_view key) {
  auto it = map_.find(std::string(key));
  if (it != map_.end()) {
    return std::string(it->second);
  }
  return std::nullopt;
}

void LruCache::set(std::string_view key, std::string_view value) {
  map_[std::string(key)] = std::string(value);
}

bool LruCache::del(std::string_view key) {
  return map_.erase(std::string(key)) > 0;
}

}  // namespace tinycache
