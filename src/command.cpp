#include <algorithm>
#include <cctype>
#include <command.hpp>
#include <optional>
#include <unordered_map>
#include "respValue.hpp"
#include "utils.hpp"

namespace tinycache {

namespace {

// Consider for future
// std::unordered_map<std::string_view, CommandType> command_map = {
//     {"GET", CommandType::kGet},
//     {"SET", CommandType::kSet},
//     {"DEL", CommandType::kDel},
//     {"EXPIRE", CommandType::kExpire},
//     {"TTL", CommandType::kTtl}};

CommandType determineCommandType(std::string_view type_sv) {
  if (type_sv == "GET")
    return CommandType::kGet;
  if (type_sv == "SET")
    return CommandType::kSet;
  if (type_sv == "DEL")
    return CommandType::kDel;
  if (type_sv == "EXPIRE")
    return CommandType::kExpire;
  if (type_sv == "TTL")
    return CommandType::kTtl;
  if (type_sv == "PING")
    return CommandType::kPing;
  if (type_sv == "COMMAND")
    return CommandType::kCommand;
  if (type_sv == "CONFIG")
    return CommandType::kConfig;

  return CommandType::kUnknown;
}

struct CommandSpec {
  std::size_t minArgs;
  std::size_t maxArgs;
};

constexpr std::size_t kMaxArgs = 10;

const std::unordered_map<CommandType, CommandSpec> kSpecs = {
    {CommandType::kGet, {1, 1}},
    {CommandType::kSet, {2, 4}},
    {CommandType::kDel, {1, kMaxArgs}},
    {CommandType::kExpire, {2, 2}},
    {CommandType::kTtl, {1, 1}},
    {CommandType::kPing, {0, 1}},
    {CommandType::kCommand, {0, kMaxArgs}},
    {CommandType::kConfig, {2, 2}}};

bool validateArgs(CommandType type, std::size_t argc) {
  // Case with unknown command type should be handled in toCommand, so we can assume the type is valid here.
  auto command_spec = kSpecs.find(type)->second;
  return argc >= command_spec.minArgs && argc <= command_spec.maxArgs;
}

}  // namespace

std::optional<Command> Command::toCommand(RespValue& value) {
  if (value.type != RespValue::Type::kArray) {
    return std::nullopt;
  }

  auto arr = std::move(std::get<std::vector<RespValue>>(value.data));

  if (arr.empty() || (arr[0].type != RespValue::Type::kBulkString &&
                      arr[0].type != RespValue::Type::kSimpleString)) {
    return std::nullopt;
  }

  auto type_str = std::move(std::get<std::string>(arr[0].data));
  to_uppercase(type_str);

  CommandType type = determineCommandType(type_str);

  if (type == CommandType::kUnknown) {
    return std::nullopt;
  }

  if (!validateArgs(type, arr.size() - 1)) {
    return std::nullopt;
  }

  std::vector<std::string> args;
  args.reserve(arr.size() - 1);

  for (std::size_t i = 1; i < arr.size(); ++i) {
    if (arr[i].type != RespValue::Type::kBulkString &&
        arr[i].type != RespValue::Type::kSimpleString) {
      return std::nullopt;
    }

    args.push_back(std::move(std::get<std::string>(arr[i].data)));
  }

  return Command{type, std::move(args)};
}

}  // namespace tinycache
