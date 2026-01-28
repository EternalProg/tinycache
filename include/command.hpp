#ifndef TINYCACHE_COMMAND_HPP
#define TINYCACHE_COMMAND_HPP

#include <optional>
#include <string>
#include <vector>
#include "respValue.hpp"

namespace tinycache {

enum class CommandType { kGet, kSet, kDel, kExpire, kTtl, kUnknown };

struct Command {
  CommandType type{CommandType::kUnknown};
  std::vector<std::string> args;

  static std::optional<Command> toCommand(const RespValue& value);
};

}  // namespace tinycache

#endif
