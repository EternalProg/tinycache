#ifndef TINYCACHE_COMMAND_EXECUTOR_HPP
#define TINYCACHE_COMMAND_EXECUTOR_HPP

#include <spdlog/spdlog.h>
#include <command.hpp>
#include <lruCache.hpp>
#include <respValue.hpp>

namespace tinycache {

class CommandExecutor {
 public:
  RespValue execute(Command& cmd);

 private:
  LruCache cache_;
};

}  // namespace tinycache

#endif
