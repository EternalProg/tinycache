#ifndef TINYCACHE_LRU_SHARD_HPP
#define TINYCACHE_LRU_SHARD_HPP

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tinycache {

using Key = std::string;
using TimePoint = std::chrono::steady_clock::time_point;

struct TransparentKeyHash {
  using is_transparent = void;

  [[nodiscard]] std::size_t operator()(std::string_view key) const noexcept {
    return std::hash<std::string_view>{}(key);
  }

  [[nodiscard]] std::size_t operator()(const Key& key) const noexcept {
    return (*this)(std::string_view{key});
  }

  [[nodiscard]] std::size_t operator()(const char* key) const noexcept {
    return (*this)(std::string_view{key});
  }
};

struct TransparentKeyEqual {
  using is_transparent = void;

  [[nodiscard]] bool operator()(std::string_view lhs,
                                std::string_view rhs) const noexcept {
    return lhs == rhs;
  }
};

struct Entry {
  std::string value;
  std::optional<TimePoint> expire_at;
  std::list<Key>::iterator lru_it;
  std::multimap<TimePoint, Key>::iterator expire_it;
};

class LruShard {
 public:
  static constexpr std::size_t kDefaultCapacity = 1024;

  explicit LruShard(std::size_t capacity = kDefaultCapacity)
      : capacity_(capacity) {
    map_.max_load_factor(0.80F);
    map_.reserve(capacity_);
  }

  [[nodiscard]] std::optional<std::string> get(std::string_view key);

  void set(std::string_view key, std::string_view value,
           std::optional<std::size_t> expire_seconds = std::nullopt);

  // Set expiration time in seconds from now
  bool expire(std::string_view key, std::size_t seconds);

  // Get time-to-live in seconds (-1 if no expiration, -2 if key doesn't exist)
  [[nodiscard]] std::int64_t ttl(std::string_view key);

  [[nodiscard]] bool del(std::string_view key);

  [[nodiscard]] std::optional<TimePoint> get_next_expire_time();
  void remove_expired_keys(TimePoint now);

 private:
  using Map =
      std::unordered_map<Key, Entry, TransparentKeyHash, TransparentKeyEqual>;

  void evict_lru();
  void remove_key(Map::iterator it);

  Map map_;
  std::list<Key> lru_list_;
  std::multimap<TimePoint, Key> expire_map_;
  std::size_t capacity_;
};

}  // namespace tinycache

#endif
