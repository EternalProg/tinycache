#include <spdlog/spdlog.h>
#include <chrono>
#include <lruShard.hpp>

namespace tinycache {

namespace {

bool is_expired(const std::optional<TimePoint>& expire_at) {
  if (!expire_at.has_value()) {
    return false;
  }

  return std::chrono::steady_clock::now() >= *expire_at;
}

}  // namespace

std::size_t LruShard::payload_bytes(const Node& node) {
  return node.key.size() + node.value.size();
}

void LruShard::remove_key(Map::iterator it, RemovalReason reason) {
  if (it == map_.end()) {
    return;
  }

  auto node_it = it->second;
  const std::size_t removed_payload_bytes = payload_bytes(*node_it);

  if (node_it->expire_it != expire_map_.end()) {
    expire_map_.erase(node_it->expire_it);
    node_it->expire_it = expire_map_.end();
  }

  map_.erase(it);
  lru_list_.erase(node_it);

  if (used_payload_bytes_ >= removed_payload_bytes) {
    used_payload_bytes_ -= removed_payload_bytes;
  } else {
    used_payload_bytes_ = 0;
  }

  if (reason == RemovalReason::kEviction) {
    ++evictions_;
  } else if (reason == RemovalReason::kExpired) {
    ++expired_;
  }
}

void LruShard::enforce_max_memory() {
  while (used_payload_bytes_ > max_memory_bytes_ && !lru_list_.empty()) {
    evict_lru();
  }
}

std::optional<std::string> LruShard::get(std::string_view key) {
  auto it = map_.find(key);
  if (it == map_.end()) {
    return std::nullopt;
  }

  auto node_it = it->second;
  Node& node = *node_it;
  if (is_expired(node.expire_at)) {
    remove_key(it, RemovalReason::kExpired);
    return std::nullopt;
  }

  lru_list_.splice(lru_list_.begin(), lru_list_, node_it);
  return node.value;
}

void LruShard::set(std::string_view key, std::string_view value,
                   std::optional<std::size_t> expire_seconds) {
  auto expire_at = expire_seconds.has_value()
                       ? std::optional(std::chrono::steady_clock::now() +
                                       std::chrono::seconds(*expire_seconds))
                       : std::nullopt;

  auto it = map_.find(key);
  if (it != map_.end()) {
    auto node_it = it->second;
    Node& node = *node_it;

    const std::size_t old_payload = payload_bytes(node);
    node.value.assign(value.data(), value.size());
    const std::size_t new_payload = payload_bytes(node);
    if (new_payload >= old_payload) {
      used_payload_bytes_ += (new_payload - old_payload);
    } else {
      const auto diff = old_payload - new_payload;
      if (used_payload_bytes_ >= diff) {
        used_payload_bytes_ -= diff;
      } else {
        used_payload_bytes_ = 0;
      }
    }

    if (node.expire_it != expire_map_.end()) {
      expire_map_.erase(node.expire_it);
    }

    if (expire_at.has_value()) {
      node.expire_it = expire_map_.emplace(*expire_at, node_it);
    } else {
      node.expire_it = expire_map_.end();
    }
    node.expire_at = expire_at;

    lru_list_.splice(lru_list_.begin(), lru_list_, node_it);
    enforce_max_memory();
    return;
  }

  lru_list_.push_front(
      Node{Key(key), std::string(value), expire_at, expire_map_.end()});
  auto node_it = lru_list_.begin();
  map_.emplace(std::string_view{node_it->key}, node_it);
  used_payload_bytes_ += payload_bytes(*node_it);

  if (expire_at.has_value()) {
    node_it->expire_it = expire_map_.emplace(*expire_at, node_it);
  }

  enforce_max_memory();
}

bool LruShard::del(std::string_view key) {
  auto it = map_.find(key);
  bool existed = it != map_.end();
  remove_key(it, RemovalReason::kDelete);
  return existed;
}

LruShard::Stats LruShard::get_stats() const {
  return Stats{.used_payload_bytes = used_payload_bytes_,
               .key_count = map_.size(),
               .evictions = evictions_,
               .expired = expired_,
               .max_memory_bytes = max_memory_bytes_};
}

bool LruShard::expire(std::string_view key, std::size_t seconds) {
  auto it = map_.find(key);
  if (it == map_.end()) {
    return false;
  }

  auto node_it = it->second;
  Node& node = *node_it;

  if (node.expire_it != expire_map_.end()) {
    expire_map_.erase(node.expire_it);
  }

  auto expire_at =
      std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  node.expire_at = expire_at;
  node.expire_it = expire_map_.emplace(expire_at, node_it);

  return true;
}

std::int64_t LruShard::ttl(std::string_view key) {
  auto it = map_.find(key);
  if (it == map_.end()) {
    return -2;
  }

  auto node_it = it->second;
  Node& node = *node_it;
  if (is_expired(node.expire_at)) {
    remove_key(it, RemovalReason::kExpired);
    return -2;
  }

  if (!node.expire_at.has_value()) {
    return -1;
  }

  auto now = std::chrono::steady_clock::now();
  auto remaining = *node.expire_at - now;
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(remaining).count();
  return (seconds > 0) ? seconds : 0;
}

void LruShard::remove_expired_keys(TimePoint now) {
  auto it = expire_map_.begin();
  while (it != expire_map_.end() && it->first <= now) {
    auto node_it = it->second;
    it = expire_map_.erase(it);

    SPDLOG_DEBUG("Removing expired key: {}", node_it->key);
    node_it->expire_it = expire_map_.end();

    auto map_it = map_.find(node_it->key);
    if (map_it != map_.end()) {
      remove_key(map_it, RemovalReason::kExpired);
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

  auto lru_it = lru_list_.end();
  --lru_it;

  auto map_it = map_.find(lru_it->key);
  if (map_it != map_.end()) {
    remove_key(map_it, RemovalReason::kEviction);
    return;
  }

  lru_list_.erase(lru_it);
}

}  // namespace tinycache
