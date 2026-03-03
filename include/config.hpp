#ifndef TINYCACHE_CONFIG_HPP
#define TINYCACHE_CONFIG_HPP

#include <cstdint>
#include <string>

namespace tinycache {

struct Config {
  std::string_view host = "0.0.0.0";
  std::uint16_t port = 8080;
  std::uint16_t max_items = 1024;
  std::uint16_t shard_count = 1;
};

Config get_config();

}  // namespace tinycache

#endif
