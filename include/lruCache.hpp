#ifndef TINYCACHE_LRU_CACHE_HPP
#define TINYCACHE_LRU_CACHE_HPP

#include <list>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tinycache {

using Key = std::string;

struct Entry {
  std::string value;
  std::list<Key>::iterator lru_it;
};

struct ExpireItem {
  Key key;
  std::size_t expire_time;
};

class LruCache {
 public:
  static constexpr std::size_t kDefaultCapacity = 1024;

  explicit LruCache(std::size_t capacity = kDefaultCapacity)
      : capacity_(capacity) {}

  [[nodiscard]] std::optional<std::string> get(std::string_view key);

  void set(std::string_view key, std::string_view value);

  [[nodiscard]] bool del(std::string_view key);

 private:
  void evict_lru();

  std::unordered_map<Key, Entry> map_;
  std::list<Key> lru_list_;
  std::priority_queue<ExpireItem> expire_queue_;
  std::size_t capacity_;

  std::mutex m_;
};

}  // namespace tinycache

#endif
