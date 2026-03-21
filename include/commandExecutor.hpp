#ifndef TINYCACHE_COMMAND_EXECUTOR_HPP
#define TINYCACHE_COMMAND_EXECUTOR_HPP

#include <boost/asio/awaitable.hpp>
#include <command.hpp>
#include <optional>
#include <respValue.hpp>
#include <shardPool.hpp>
#include <utility>

namespace tinycache {

class CommandExecutor {
 public:
  explicit CommandExecutor(ShardPool& shard_pool);
  asio::awaitable<RespValue> execute(
      Command& cmd, std::optional<std::size_t> home_shard = std::nullopt);

 private:
  ShardPool& shard_pool_;
};

}  // namespace tinycache

#endif
