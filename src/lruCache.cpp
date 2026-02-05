#include <chrono>
#include <lruCache.hpp>
#include <mutex>

namespace tinycache {

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

void LruCache::set(std::string_view key, std::string_view value) {
  std::lock_guard lock(m_);

  Key key_str(key);

  // If key already exists, update it and move to front
  auto it = map_.find(key_str);
  if (it != map_.end()) {
    it->second.value = std::string(value);
    it->second.expire_at = std::nullopt;  // Clear expiration on update
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_it);
    return;
  }

  // Check if we need to evict
  if (map_.size() >= capacity_) {
    evict_lru();
  }

  // Insert new entry at front
  lru_list_.push_front(key_str);
  map_[key_str] = Entry{std::string(value), lru_list_.begin(), std::nullopt};
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

void LruCache::evict_lru() {
  if (!lru_list_.empty()) {
    const Key& lru_key = lru_list_.back();
    map_.erase(lru_key);
    lru_list_.pop_back();
  }
}

bool LruCache::is_expired(
    const std::optional<std::chrono::steady_clock::time_point>& expire_at)
    const {
  if (!expire_at.has_value()) {
    return false;  // No expiration set
  }

  return std::chrono::steady_clock::now() >= expire_at.value();
}

}  // namespace tinycache
