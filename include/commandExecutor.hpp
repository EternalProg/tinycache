#ifndef TINYCACHE_COMMAND_EXECUTOR_HPP
#define TINYCACHE_COMMAND_EXECUTOR_HPP

#include <boost/asio/awaitable.hpp>
#include <command.hpp>
#include <respValue.hpp>
#include <shardPool.hpp>

namespace tinycache {

class CommandExecutor {
 public:
  explicit CommandExecutor(ShardPool& shard_pool);
  asio::awaitable<RespValue> execute(Command& cmd);

 private:
  ShardPool& shard_pool_;
};

}  // namespace tinycache

#endif
