#include <chrono>
#include <lruCache.hpp>
#include <mutex>

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

void LruCache::remove_key(std::unordered_map<Key, Entry>::iterator it) {
  if (it != map_.end()) {
    Entry& entry = it->second;
    lru_list_.erase(entry.lru_it);
    expire_map_.erase(entry.expire_it);
    map_.erase(it);
  }
}

std::optional<std::string> LruCache::get(std::string_view key) {
  std::lock_guard lock(m_);
  auto it = map_.find(std::string(key));

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

void LruCache::set(std::string_view key, std::string_view value,
                   std::optional<std::size_t> expire_seconds) {
  std::lock_guard lock(m_);

  Key key_str(key);

  auto expire_at =
      expire_seconds.has_value()
          ? std::optional(std::chrono::steady_clock::now() +
                          std::chrono::seconds(expire_seconds.value()))
          : std::nullopt;

  // Case 1: key already exists, update it and move to front
  auto it = map_.find(key_str);
  if (it != map_.end()) {
    Entry& entry = it->second;
    entry.value = std::string(value);

    // Any previous time to live associated with the key is discarded on successful SET operation
    if (entry.expire_at.has_value()) {
      // Remove old expiration
      expire_map_.erase(entry.expire_it);
    }

    if (expire_at.has_value()) {
      // Add new expiration
      entry.expire_it = expire_map_.emplace(expire_at.value(), key_str);
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

  lru_list_.push_front(key_str);
  auto expire_it = expire_map_.end();
  if (expire_at.has_value()) {
    expire_it = expire_map_.emplace(expire_at.value(), key_str);
  }

  auto new_entry = Entry{.value = std::string(value),
                         .expire_at = expire_at,
                         .lru_it = lru_list_.begin(),
                         .expire_it = expire_it};

  map_[key_str] = new_entry;
}

bool LruCache::del(std::string_view key) {
  std::lock_guard lock(m_);

  auto it = map_.find(std::string(key));
  remove_key(it);

  return it != map_.end();
}

bool LruCache::expire(std::string_view key, std::size_t seconds) {
  std::lock_guard lock(m_);

  auto it = map_.find(std::string(key));
  if (it != map_.end()) {
    Entry& entry = it->second;

    // Remove old expiration
    if (entry.expire_at.has_value()) {
      expire_map_.erase(entry.expire_it);
    }

    auto expire_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    auto expire_it = expire_map_.emplace(expire_at, std::string(key));

    entry.expire_at = expire_at;
    entry.expire_it = expire_it;

    return true;
  }

  return false;
}

std::int64_t LruCache::ttl(std::string_view key) {
  std::lock_guard lock(m_);

  auto it = map_.find(std::string(key));

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

void LruCache::remove_expired_keys(TimePoint now) {
  std::lock_guard lock(m_);
  if (expire_map_.empty()) {
    return;
  }

  for (auto& [expire_time, key] : expire_map_) {
    if (expire_time <= now) {
      auto it = map_.find(key);
      remove_key(it);
    } else {
      break;
    }
  }
}

std::optional<TimePoint> LruCache::get_next_expire_time() {
  std::lock_guard lock(m_);
  if (expire_map_.empty()) {
    return std::nullopt;
  }

  return expire_map_.begin()->first;
}

void LruCache::evict_lru() {
  if (!lru_list_.empty()) {
    const Key& lru_key = lru_list_.back();
    lru_list_.pop_back();
    expire_map_.erase(map_[lru_key].expire_it);
    map_.erase(lru_key);
  }
}

}  // namespace tinycache
