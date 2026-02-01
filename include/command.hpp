#ifndef TINYCACHE_COMMAND_HPP
#define TINYCACHE_COMMAND_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "respValue.hpp"

namespace tinycache {

enum class CommandType : std::uint8_t {
  kUnknown,
  kPing,
  kCommand,
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

struct CommandDefinition {
  CommandType type;
  std::string_view name;
  std::int32_t arity;  // number of arguments, -1 for variable length
};

static constexpr auto kCommands =
    std::to_array<CommandDefinition>({{CommandType::kPing, "ping", 1},
                                      {CommandType::kCommand, "command", -1},
                                      {CommandType::kGet, "get", 2},
                                      {CommandType::kSet, "set", -3},
                                      {CommandType::kDel, "del", -2},
                                      {CommandType::kExpire, "expire", 3},
                                      {CommandType::kTtl, "ttl", 2}});

}  // namespace tinycache
#endif
