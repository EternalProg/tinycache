#ifndef TINYCACHE_COMMAND_HPP
#define TINYCACHE_COMMAND_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "respValue.hpp"

namespace tinycache {

enum class CommandType : std::uint8_t {
  kUnknown,
  kGet,
  kSet,
  kDel,
  kExpire,
  kTtl
};

struct Command {
  CommandType type;               // GET, SET
  std::vector<std::string> args;  // {"key", "value"}

  static std::optional<Command> toCommand(RespValue& value);
};

}  // namespace tinycache
#endif
