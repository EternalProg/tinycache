#include <spdlog/spdlog.h>
#include <algorithm>
#include <command.hpp>
#include <commandExecutor.hpp>
#include <functional>
#include <unordered_map>
#include <utils.hpp>
#include "respValue.hpp"

namespace tinycache {

namespace {
RespValue processGetCommand(Command& cmd, LruCache& cache) {
  spdlog::debug("GET {}", cmd.args[0]);
  if (auto result = cache.get(cmd.args[0]); result.has_value()) {
    return RespValue(RespValue::Type::kBulkString, *result);
  }
  return RespValue(RespValue::Type::kNullBulkString, "");
}

/* TODO(eternal): Handle options like EX, PX, NX, XX
      EX seconds -- Set the specified expire time, in seconds.
      PX milliseconds -- Set the specified expire time, in milliseconds.
      NX -- Only set the key if it does not already exist.
      XX -- Only set the key if it already exist.
*/
RespValue processSetCommand(Command& cmd, LruCache& cache) {
  std::optional<std::size_t> expire_seconds = std::nullopt;

  if (cmd.args.size() == 3) {
    std::string& option = cmd.args[2];
    to_uppercase(option);

    spdlog::debug("SET {} {} {}", cmd.args[0], cmd.args[1], option);

    if (option == "NX") {
      if (cache.get(cmd.args[0]).has_value()) {
        return RespValue(RespValue::Type::kNullBulkString, "");
      }
    } else if (option == "XX") {
      if (!cache.get(cmd.args[0]).has_value()) {
        return RespValue(RespValue::Type::kNullBulkString, "");
      }
    } else {
      return RespValue(RespValue::Type::kError, "ERR syntax error");
    }
  } else if (cmd.args.size() == 4) {
    std::string& expire_option = cmd.args[2];
    to_uppercase(expire_option);

    spdlog::debug("SET {} {} {} {}", cmd.args[0], cmd.args[1], expire_option,
                  cmd.args[3]);

    if (expire_option == "EX") {
      try {
        expire_seconds = std::stoull(cmd.args[3]);
      } catch (const std::exception& e) {
        return RespValue(RespValue::Type::kError,
                         "ERR value is not an integer or out of range");
      }
    } else {
      return RespValue(RespValue::Type::kError, "ERR syntax error");
    }
  }

  if (expire_seconds.has_value()) {
    cache.set(cmd.args[0], cmd.args[1], expire_seconds);
  } else {
    cache.set(cmd.args[0], cmd.args[1]);
  }

  return RespValue(RespValue::Type::kSimpleString, "OK");
}

RespValue processDelCommand(Command& cmd, LruCache& cache) {
  spdlog::debug("DEL");
  std::int64_t deleted_count = 0;
  for (const auto& key : cmd.args) {
    if (cache.del(key)) {
      ++deleted_count;
    }
  }
  return RespValue(RespValue::Type::kInteger, deleted_count);
}

RespValue processExpireCommand(Command& cmd, LruCache& cache) {
  // EXPIRE key seconds
  try {
    std::size_t seconds = std::stoull(cmd.args[1]);
    auto result = cache.expire(cmd.args[0], seconds);
    spdlog::debug("EXPIRE {} {}", cmd.args[0], seconds);

    return RespValue(RespValue::Type::kInteger, result ? 1 : 0);
  } catch (const std::exception& e) {
    return RespValue(RespValue::Type::kError,
                     "ERR value is not an integer or out of range");
  }
}

RespValue processTtlCommand(Command& cmd, LruCache& cache) {
  spdlog::debug("TTL");

  std::int64_t ttl_value = cache.ttl(cmd.args[0]);
  return RespValue(RespValue::Type::kInteger, ttl_value);
}

RespValue processPingCommand(Command& cmd, [[maybe_unused]] LruCache& cache) {
  spdlog::debug("PING");
  std::string response_str = "PONG";
  if (cmd.args.size() == 1) {
    response_str = std::move(cmd.args[0]);
  }
  return RespValue(RespValue::Type::kBulkString, std::move(response_str));
}

RespValue processCommandCommand(Command& cmd,
                                [[maybe_unused]] LruCache& cache) {
  // Return the list of supported commands
  if (cmd.args.empty()) {
    spdlog::debug("COMMAND");
    RespValue::RespArray commands;
    for (const auto& def : kCommands) {
      RespValue::RespArray cmd_info;
      cmd_info.emplace_back(RespValue::Type::kBulkString,
                            std::string(def.name));
      cmd_info.emplace_back(RespValue::Type::kInteger, def.arity);
      commands.emplace_back(RespValue::Type::kArray, std::move(cmd_info));
    }
    return RespValue(RespValue::Type::kArray, std::move(commands));
  }

  std::string& subcommand = cmd.args[0];
  to_uppercase(subcommand);

  if (subcommand == "INFO") {
    // Returns @array-reply of details about multiple Redis commands.
    // Same result format as COMMAND except you can specify which commands get returned.
    spdlog::debug("COMMAND INFO");
    RespValue::RespArray commands;
    for (std::size_t i = 1; i < cmd.args.size(); ++i) {
      std::string& cmd_name = cmd.args[i];
      to_uppercase(cmd_name);
      const auto* it = std::find_if(kCommands.begin(), kCommands.end(),
                                    [cmd_name](const CommandDefinition& def) {
                                      return def.name == cmd_name;
                                    });

      if (it != kCommands.end()) {
        RespValue::RespArray cmd_info;
        cmd_info.emplace_back(RespValue::Type::kBulkString,
                              std::string(it->name));
        cmd_info.emplace_back(RespValue::Type::kInteger, it->arity);
        commands.emplace_back(RespValue::Type::kArray, std::move(cmd_info));
      }
    }
    return RespValue(RespValue::Type::kArray, std::move(commands));
  }

  if (subcommand == "COUNT") {
    // Returns @integer-reply of number of total commands in this Redis server.
    spdlog::debug("COMMAND COUNT");
    auto count = static_cast<std::int64_t>(kCommands.size());
    return RespValue(RespValue::Type::kInteger, count);
  }

  return RespValue(RespValue::Type::kError, "Unknown subcommand");
}

RespValue processUnknownCommand([[maybe_unused]] Command& cmd,
                                [[maybe_unused]] LruCache& cache) {
  spdlog::warn("Unknown command");
  return RespValue(RespValue::Type::kError, "ERR unknown command");
}

using CommandHandler = std::function<RespValue(Command&, LruCache&)>;
const std::unordered_map<CommandType, CommandHandler> kCommandHandlers = {
    {CommandType::kGet, processGetCommand},
    {CommandType::kSet, processSetCommand},
    {CommandType::kDel, processDelCommand},
    {CommandType::kExpire, processExpireCommand},
    {CommandType::kTtl, processTtlCommand},
    {CommandType::kPing, processPingCommand},
    {CommandType::kCommand, processCommandCommand},
    {CommandType::kUnknown, processUnknownCommand},
};

}  // namespace

CommandExecutor::CommandExecutor(LruCache& cache) : cache_(cache) {}

RespValue CommandExecutor::execute(Command& cmd) {
  auto it = kCommandHandlers.find(cmd.type);
  if (it != kCommandHandlers.end()) {
    return it->second(cmd, cache_);
  }

  // Fallback for unmapped command types
  spdlog::error("Command type not mapped in handler registry");
  return RespValue(RespValue::Type::kError,
                   "ERR internal error (unhandled command type)");
}

}  // namespace tinycache
