#include <spdlog/spdlog.h>
#include <chrono>
#include <lruShard.hpp>

namespace tinycache {

namespace {
bool is_expired(
    const std::optional<std::chrono::steady_clock::time_point>& expire_at) {
  if (!expire_at.has_value()) {
    return false;  // No expiration set
  }

  return std::chrono::steady_clock::now() >= expire_at.value();
}

}  // namespace

void LruShard::remove_key(LruShard::Map::iterator it) {
  if (it != map_.end()) {
    Entry& entry = it->second;

    // Check if lru_it is valid before erasing
    if (entry.lru_it != lru_list_.end()) {
      lru_list_.erase(entry.lru_it);
    }

    if (entry.expire_it != expire_map_.end()) {
      expire_map_.erase(entry.expire_it);
    }
    map_.erase(it);
  }
}

std::optional<std::string> LruShard::get(std::string_view key) {
  auto it = map_.find(key);

  if (it != map_.end()) {
    Entry& entry = it->second;
    // Check if key has expired
    if (is_expired(entry.expire_at)) {
      remove_key(it);
      return std::nullopt;
    }

    // Move to front (most recently used)
    lru_list_.splice(lru_list_.begin(), lru_list_, entry.lru_it);
    return std::string(entry.value);
  }

  return std::nullopt;
}

void LruShard::set(std::string_view key, std::string_view value,
                   std::optional<std::size_t> expire_seconds) {
  auto expire_at =
      expire_seconds.has_value()
          ? std::optional(std::chrono::steady_clock::now() +
                          std::chrono::seconds(expire_seconds.value()))
          : std::nullopt;

  // Case 1: key already exists, update it and move to front
  auto it = map_.find(key);
  if (it != map_.end()) {
    Entry& entry = it->second;
    entry.value.assign(value.data(), value.size());

    // Any previous time to live associated with the key is discarded on successful SET operation
    if (entry.expire_it != expire_map_.end()) {
      expire_map_.erase(entry.expire_it);
    }

    if (expire_at.has_value()) {
      // Add new expiration
      entry.expire_it = expire_map_.emplace(expire_at.value(), it->first);
    } else {
      entry.expire_it = expire_map_.end();
    }
    entry.expire_at = expire_at;

    lru_list_.splice(lru_list_.begin(), lru_list_, entry.lru_it);
    return;
  }

  // Case 2: Insert new entry

  // Check if we need to evict
  if (map_.size() >= capacity_) {
    evict_lru();
  }

  Key key_str(key);
  lru_list_.push_front(key_str);
  auto expire_it = expire_map_.end();
  if (expire_at.has_value()) {
    expire_it = expire_map_.emplace(expire_at.value(), key_str);
  }

  map_.emplace(std::move(key_str), Entry{.value = std::string(value),
                                         .expire_at = expire_at,
                                         .lru_it = lru_list_.begin(),
                                         .expire_it = expire_it});
}

bool LruShard::del(std::string_view key) {
  auto it = map_.find(key);
  bool existed = it != map_.end();
  remove_key(it);

  return existed;
}

bool LruShard::expire(std::string_view key, std::size_t seconds) {
  auto it = map_.find(key);
  if (it != map_.end()) {
    Entry& entry = it->second;

    // Remove old expiration
    if (entry.expire_it != expire_map_.end()) {
      expire_map_.erase(entry.expire_it);
    }

    auto expire_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    auto expire_it = expire_map_.emplace(expire_at, it->first);

    entry.expire_at = expire_at;
    entry.expire_it = expire_it;

    return true;
  }

  return false;
}

std::int64_t LruShard::ttl(std::string_view key) {
  auto it = map_.find(key);

  // Key doesn't exist
  if (it == map_.end()) {
    return -2;
  }

  Entry& entry = it->second;

  // Check if key has expired
  if (is_expired(entry.expire_at)) {
    remove_key(it);
    return -2;
  }

  // Key has no expiration
  if (!entry.expire_at.has_value()) {
    return -1;
  }

  // Calculate remaining time to live
  auto now = std::chrono::steady_clock::now();
  auto remaining = entry.expire_at.value() - now;
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(remaining).count();

  return (seconds > 0) ? seconds : 0;
}

void LruShard::remove_expired_keys(TimePoint now) {
  if (expire_map_.empty()) {
    return;
  }

  auto it = expire_map_.begin();
  while (it != expire_map_.end() && it->first <= now) {
    Key key = it->second;
    it = expire_map_.erase(it);

    auto map_it = map_.find(key);
    if (map_it != map_.end()) {
      SPDLOG_DEBUG("Removing expired key: {}", key);
      Entry& entry = map_it->second;

      // Check if lru_it is valid before erasing
      if (entry.lru_it != lru_list_.end()) {
        lru_list_.erase(entry.lru_it);
      }

      // Clear the iterator to mark it as invalid
      entry.expire_it = expire_map_.end();

      map_.erase(map_it);
    }
  }
}

std::optional<TimePoint> LruShard::get_next_expire_time() {
  if (expire_map_.empty()) {
    return std::nullopt;
  }

  return expire_map_.begin()->first;
}

void LruShard::evict_lru() {
  if (lru_list_.empty()) {
    return;
  }

  Key lru_key = lru_list_.back();
  lru_list_.pop_back();
  auto it = map_.find(lru_key);
  if (it != map_.end()) {
    auto& entry = it->second;
    if (entry.expire_it != expire_map_.end()) {
      expire_map_.erase(entry.expire_it);
      entry.expire_it = expire_map_.end();
    }
    map_.erase(it);
  }
}

}  // namespace tinycache
