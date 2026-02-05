#include <spdlog/spdlog.h>
#include <command.hpp>
#include <commandExecutor.hpp>
#include "respValue.hpp"

namespace tinycache {

// TODO(eternal): Implement command execution logic
RespValue CommandExecutor::execute(const Command& cmd) {
  RespValue response;
  switch (cmd.type) {
    case CommandType::kGet: {
      spdlog::debug("GET {}", cmd.args[0]);
      if (auto result = cache_.get(cmd.args[0]); result.has_value()) {
        response = RespValue(RespValue::Type::kBulkString, *result);
      } else {
        response = RespValue(RespValue::Type::kNullBulkString, "");
      }
      break;
    }
    case CommandType::kSet: {
      spdlog::debug("SET {}: {}", cmd.args[0], cmd.args[1]);
      /* TODO(eternal): Handle options like EX, PX, NX, XX
      EX seconds -- Set the specified expire time, in seconds.
      PX milliseconds -- Set the specified expire time, in milliseconds.
      NX -- Only set the key if it does not already exist.
      XX -- Only set the key if it already exist.
      */
      cache_.set(cmd.args[0], cmd.args[1]);
      response = RespValue(RespValue::Type::kSimpleString, "OK");
      break;
    }
    case CommandType::kDel: {
      spdlog::debug("DEL");
      std::int64_t deleted_count = 0;
      for (const auto& key : cmd.args) {
        if (cache_.del(key)) {
          ++deleted_count;
        }
      }
      response = RespValue(RespValue::Type::kInteger, deleted_count);
      break;
    }
    case CommandType::kExpire: {
      spdlog::debug("EXPIRE");
      // EXPIRE key seconds
      if (cmd.args.size() != 2) {
        response =
            RespValue(RespValue::Type::kError,
                      "ERR wrong number of arguments for 'expire' command");
        break;
      }

      try {
        std::size_t seconds = std::stoull(cmd.args[1]);
        cache_.expire(cmd.args[0], seconds);
        response = RespValue(RespValue::Type::kInteger, 1LL);
      } catch (const std::exception& e) {
        response = RespValue(RespValue::Type::kError,
                             "ERR value is not an integer or out of range");
      }
      break;
    }
    case CommandType::kTtl: {
      spdlog::debug("TTL");
      // TTL key
      if (cmd.args.size() != 1) {
        response = RespValue(RespValue::Type::kError,
                             "ERR wrong number of arguments for 'ttl' command");
        break;
      }

      std::int64_t ttl_value = cache_.ttl(cmd.args[0]);
      response = RespValue(RespValue::Type::kInteger, ttl_value);
      break;
    }
    case CommandType::kPing: {
      spdlog::debug("PING");
      std::string response_str = "PONG";
      if (cmd.args.size() == 1) {
        response_str = std::move(cmd.args[0]);
      }
      response =
          RespValue(RespValue::Type::kBulkString, std::move(response_str));
      break;
    }
    case CommandType::kCommand: {
      spdlog::debug("COMMAND");

      // return the list of supported commands
      if (cmd.args.empty()) {
        RespValue::RespArray commands;
        for (const auto& def : kCommands) {
          RespValue::RespArray cmd_info;
          cmd_info.emplace_back(RespValue::Type::kBulkString,
                                std::string(def.name));
          cmd_info.emplace_back(RespValue::Type::kInteger, def.arity);
          commands.emplace_back(RespValue::Type::kArray, std::move(cmd_info));
        }

        response = RespValue(RespValue::Type::kArray, std::move(commands));
        break;
      }

      std::string_view subcommand = cmd.args[0];
      if (subcommand == "INFO") {
        /*
           Returns @array-reply of details about multiple Redis commands.
            Same result format as COMMAND except you can specify which commands get returned.
           */
        RespValue::RespArray commands;
        for (std::size_t i = 1; i < cmd.args.size(); ++i) {
          std::string_view cmd_name = cmd.args[i];
          const auto* it =
              std::find_if(kCommands.begin(), kCommands.end(),
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
        response = RespValue(RespValue::Type::kArray, std::move(commands));
      } else if (subcommand == "COUNT") {
        // Returns @integer-reply of number of total commands in this Redis server.
        auto count = static_cast<std::int64_t>(kCommands.size());
        response = RespValue(RespValue::Type::kInteger, count);
      } else {
        response = RespValue(RespValue::Type::kError, "Unknown subcommand");
      }

      break;
    }
    case CommandType::kUnknown: {
      spdlog::warn("Unknown command");
      break;
    }
  }

  return response;
}

}  // namespace tinycache
