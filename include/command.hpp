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
  kTtl,
  kConfig
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
    std::to_array<CommandDefinition>({{CommandType::kPing, "PING", 1},
                                      {CommandType::kCommand, "COMMAND", -1},
                                      {CommandType::kGet, "GET", 2},
                                      {CommandType::kSet, "SET", -3},
                                      {CommandType::kDel, "DEL", -2},
                                      {CommandType::kExpire, "EXPIRE", 3},
                                      {CommandType::kTtl, "TTL", 2},
                                      {CommandType::kConfig, "CONFIG", -2}});

}  // namespace tinycache
#endif
