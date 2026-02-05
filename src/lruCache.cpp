#include <lruCache.hpp>
#include <mutex>

namespace tinycache {

std::optional<std::string> LruCache::get(std::string_view key) {
  std::lock_guard lock(m_);
  auto it = map_.find(std::string(key));

  if (it != map_.end()) {
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
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_it);
    return;
  }

  // Check if we need to evict
  if (map_.size() >= capacity_) {
    evict_lru();
  }

  // Insert new entry at front
  lru_list_.push_front(key_str);
  map_[key_str] = Entry{std::string(value), lru_list_.begin()};
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

void LruCache::evict_lru() {
  if (!lru_list_.empty()) {
    const Key& lru_key = lru_list_.back();
    map_.erase(lru_key);
    lru_list_.pop_back();
  }
}

}  // namespace tinycache
