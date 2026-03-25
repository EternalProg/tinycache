#ifndef TINYCACHE_CONFIG_HPP
#define TINYCACHE_CONFIG_HPP

#include <cstdint>
#include <string>

namespace tinycache {

struct Config {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
  std::uint16_t max_items_per_shard = 1024;
  std::uint16_t shard_count = 1;
  bool thread_affinity_enabled = false;
};

Config get_config();

}  // namespace tinycache

#endif
