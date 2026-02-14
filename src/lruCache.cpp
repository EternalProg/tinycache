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

std::optional<std::string> LruCache::get(std::string_view key) {
  std::lock_guard lock(m_);
  auto it = map_.find(std::string(key));

  if (it != map_.end()) {
    // Check if key has expired
    if (is_expired(it->second.expire_at)) {
      map_.erase(it);
      lru_list_.erase(it->second.lru_it);
      return std::nullopt;
    }

    // Move to front (most recently used)
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_it);
    return std::string(it->second.value);
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
    expire_it = expire_map_.insert({expire_at.value(), key_str});
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
  if (it != map_.end()) {
    lru_list_.erase(it->second.lru_it);
    map_.erase(it);
    return true;
  }

  return false;
}

void LruCache::expire(std::string_view key, std::size_t seconds) {
  std::lock_guard lock(m_);

  auto it = map_.find(std::string(key));
  if (it != map_.end()) {
    it->second.expire_at =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  }
}

std::int64_t LruCache::ttl(std::string_view key) {
  std::lock_guard lock(m_);

  auto it = map_.find(std::string(key));

  // Key doesn't exist
  if (it == map_.end()) {
    return -2;
  }

  // Check if key has expired
  if (is_expired(it->second.expire_at)) {
    map_.erase(it);
    lru_list_.erase(it->second.lru_it);
    return -2;
  }

  // Key has no expiration
  if (!it->second.expire_at.has_value()) {
    return -1;
  }

  // Calculate remaining time to live
  auto now = std::chrono::steady_clock::now();
  auto remaining = it->second.expire_at.value() - now;
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(remaining).count();

  return (seconds > 0) ? seconds : 0;
}

std::vector<Key> LruCache::get_expired_keys() {
  std::lock_guard lock(m_);
  std::vector<Key> expired_keys;

  return expired_keys;
}

TimePoint LruCache::get_next_expire_time() {
  std::lock_guard lock(m_);
  TimePoint next_expire_time = TimePoint::max();

  for (const auto& [key, entry] : map_) {
    if (entry.expire_at.has_value() &&
        entry.expire_at.value() < next_expire_time) {
      next_expire_time = entry.expire_at.value();
    }
  }

  return next_expire_time;
}

void LruCache::evict_lru() {
  if (!lru_list_.empty()) {
    const Key& lru_key = lru_list_.back();
    map_.erase(lru_key);
    lru_list_.pop_back();
  }
}

}  // namespace tinycache
