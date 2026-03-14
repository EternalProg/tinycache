#ifndef TINYCACHE_LRU_SHARD_HPP
#define TINYCACHE_LRU_SHARD_HPP

#include <chrono>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tinycache {

using Key = std::string;
using TimePoint = std::chrono::steady_clock::time_point;

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
      : capacity_(capacity) {}

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
  void evict_lru();
  void remove_key(std::unordered_map<Key, Entry>::iterator it);

  std::unordered_map<Key, Entry> map_;
  std::list<Key> lru_list_;
  std::multimap<TimePoint, Key> expire_map_;
  std::size_t capacity_;
};

}  // namespace tinycache

#endif
