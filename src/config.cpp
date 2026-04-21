#include <spdlog/spdlog.h>
#include <config.hpp>
#include <toml++/toml.hpp>

namespace tinycache {

Config get_config() {
  auto toml_config = toml::parse_file("config.toml");
  Config config;
  config.host = toml_config["server"]["host"].value_or("0.0.0.0");
  config.port = toml_config["server"]["port"].value_or(8080);
  config.thread_affinity_enabled =
      toml_config["server"]["thread_affinity_enabled"].value_or(false);

  std::int64_t max_message_size =
      toml_config["server"]["max_message_size"].value_or(1024);
  if (max_message_size <= 0) {
    max_message_size = 1024;
  }
  config.max_message_size = static_cast<std::size_t>(max_message_size);

  config.max_items_per_shard =
      toml_config["cache"]["max_items_per_shard"].value_or(1024);

  std::int32_t shard_count = toml_config["cache"]["shard_count"].value_or(1);
  if (shard_count <= 0) {
    shard_count = 1;
  }
  config.shard_count = shard_count;

  SPDLOG_DEBUG(
      "Configuration loaded: host={}, port={}, max_message_size={}, "
      "max_items={}, shard_count={}, thread_affinity={}",
      config.host, config.port, config.max_message_size,
      config.max_items_per_shard, config.shard_count,
      config.thread_affinity_enabled);

  return config;
}

}  // namespace tinycache
