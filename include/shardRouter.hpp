#ifndef TINYCACHE_SHARDED_CACHE_HPP
#define TINYCACHE_SHARDED_CACHE_HPP

#include <xxhash.h>
#include <cassert>
#include <cstddef>
#include <string_view>

namespace tinycache::shard_router {

inline std::size_t getShardIndex(std::string_view key,
                                 std::size_t shard_count) {
  // Using XXH3_64bits for fast and good-quality hashing
  assert(shard_count > 0);
  return XXH3_64bits(key.data(), key.size()) % shard_count;
}

}  // namespace tinycache::shard_router

#endif
