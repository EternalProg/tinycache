#ifndef TINYCACHE_CONFIG_HPP
#define TINYCACHE_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace tinycache {

struct Config {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
  std::size_t max_message_size = 1024;
  std::uint64_t max_memory_bytes_per_shard = 1024;
  std::size_t preallocated_map_capacity_per_shard = 0;
  std::uint16_t shard_count = 1;
  bool thread_affinity_enabled = false;
};

Config get_config();

}  // namespace tinycache

#endif
