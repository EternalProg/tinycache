#include <spdlog/spdlog.h>
#include <algorithm>
#include <command.hpp>
#include <commandExecutor.hpp>
#include <shardRouter.hpp>
#include <string_view>
#include <utility>
#include <utils.hpp>
#include <vector>
#include "respValue.hpp"

namespace tinycache {

namespace {
RespValue processGetCommand(Command& cmd, LruShard& shard) {
  spdlog::debug("GET {}", cmd.args[0]);

  if (auto result = shard.get(cmd.args[0]); result.has_value()) {
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
RespValue processSetCommand(Command& cmd, LruShard& shard) {
  std::optional<std::size_t> expire_seconds = std::nullopt;

  const auto& key = cmd.args[0];

  if (cmd.args.size() == 3) {
    std::string& option = cmd.args[2];
    to_uppercase(option);

    spdlog::debug("SET {} {} {}", key, cmd.args[1], option);

    if (option == "NX") {
      if (shard.get(key).has_value()) {
        return RespValue(RespValue::Type::kNullBulkString, "");
      }
    } else if (option == "XX") {
      if (!shard.get(key).has_value()) {
        return RespValue(RespValue::Type::kNullBulkString, "");
      }
    } else {
      return RespValue(RespValue::Type::kError, "ERR syntax error");
    }
  } else if (cmd.args.size() == 4) {
    std::string& expire_option = cmd.args[2];
    to_uppercase(expire_option);

    spdlog::debug("SET {} {} {} {}", key, cmd.args[1], expire_option,
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
    shard.set(key, cmd.args[1], expire_seconds);
  } else {
    shard.set(key, cmd.args[1]);
  }

  return RespValue(RespValue::Type::kSimpleString, "OK");
}

std::int64_t processDelKeys(const std::vector<std::string_view>& keys,
                            LruShard& shard) {
  std::int64_t deleted_count = 0;
  for (auto key : keys) {
    if (shard.del(key)) {
      spdlog::debug("\tDEL {}", key);
      ++deleted_count;
    }
  }
  return deleted_count;
}

RespValue processExpireCommand(Command& cmd, LruShard& shard) {
  // EXPIRE key seconds
  try {
    std::size_t seconds = std::stoull(cmd.args[1]);

    const auto& key = cmd.args[0];
    auto result = shard.expire(key, seconds);
    spdlog::debug("EXPIRE {} {}", key, seconds);

    return RespValue(RespValue::Type::kInteger, result ? 1 : 0);
  } catch (const std::exception& e) {
    return RespValue(RespValue::Type::kError,
                     "EXPIRE: ERR value is not an integer or out of range");
  }
}

RespValue processTtlCommand(Command& cmd, LruShard& shard) {
  const auto& key = cmd.args[0];
  spdlog::debug("TTL {}", key);

  std::int64_t ttl_value = shard.ttl(key);
  return RespValue(RespValue::Type::kInteger, ttl_value);
}

RespValue processPingCommand(Command& cmd) {
  spdlog::debug("PING");
  std::string response_str = "PONG";
  if (cmd.args.size() == 1) {
    response_str = std::move(cmd.args[0]);
  }
  return RespValue(RespValue::Type::kBulkString, std::move(response_str));
}

RespValue processCommandCommand(Command& cmd) {
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

RespValue processUnknownCommand() {
  spdlog::warn("Unknown command");
  return RespValue(RespValue::Type::kError, "ERR unknown command");
}

}  // namespace

CommandExecutor::CommandExecutor(ShardPool& shard_pool)
    : shard_pool_(shard_pool) {}

asio::awaitable<RespValue> CommandExecutor::execute(Command& cmd) {
  auto shard_count = shard_pool_.size();
  switch (cmd.type) {
    case CommandType::kGet: {
      const auto& key = cmd.args[0];
      auto shard_index = shard_router::getShardIndex(key, shard_count);
      co_return co_await shard_pool_.run_on_shard(
          shard_index,
          [&](LruShard& shard) { return processGetCommand(cmd, shard); });
    }
    case CommandType::kSet: {
      const auto& key = cmd.args[0];
      auto shard_index = shard_router::getShardIndex(key, shard_count);
      co_return co_await shard_pool_.run_on_shard(
          shard_index,
          [&](LruShard& shard) { return processSetCommand(cmd, shard); });
    }
    case CommandType::kDel: {
      spdlog::debug("DEL {} keys", cmd.args.size());
      std::vector<std::vector<std::string_view>> keys_by_shard(shard_count);
      for (const auto& key : cmd.args) {
        auto shard_index = shard_router::getShardIndex(key, shard_count);
        keys_by_shard[shard_index].push_back(key);
      }

      std::int64_t deleted_count = 0;
      for (std::size_t i = 0; i < keys_by_shard.size(); ++i) {
        if (keys_by_shard[i].empty()) {
          continue;
        }
        auto shard_index = i;
        auto keys = keys_by_shard[i];
        auto count = co_await shard_pool_.run_on_shard(
            shard_index, [keys = std::move(keys)](LruShard& shard) {
              return processDelKeys(keys, shard);
            });
        deleted_count += count;
      }
      co_return RespValue(RespValue::Type::kInteger, deleted_count);
    }
    case CommandType::kExpire: {
      const auto& key = cmd.args[0];
      auto shard_index = shard_router::getShardIndex(key, shard_count);
      co_return co_await shard_pool_.run_on_shard(
          shard_index,
          [&](LruShard& shard) { return processExpireCommand(cmd, shard); });
    }
    case CommandType::kTtl: {
      const auto& key = cmd.args[0];
      auto shard_index = shard_router::getShardIndex(key, shard_count);
      co_return co_await shard_pool_.run_on_shard(
          shard_index,
          [&](LruShard& shard) { return processTtlCommand(cmd, shard); });
    }
    case CommandType::kPing:
      co_return processPingCommand(cmd);
    case CommandType::kCommand:
      co_return processCommandCommand(cmd);
    case CommandType::kUnknown:
    default:
      co_return processUnknownCommand();
  }
}

}  // namespace tinycache
