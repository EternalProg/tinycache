#ifndef TINYCACHE_COMMAND_EXECUTOR_HPP
#define TINYCACHE_COMMAND_EXECUTOR_HPP

#include "command.hpp"
#include "respValue.hpp"

namespace tinycache {

class CommandExecutor {
 public:
  RespValue execute(const Command& cmd);
};

}  // namespace tinycache

#endif
