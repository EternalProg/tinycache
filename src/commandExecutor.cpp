#include "commandExecutor.hpp"
#include <spdlog/spdlog.h>

namespace tinycache {

RespValue CommandExecutor::execute(const Command& cmd) {
  switch (cmd.type) {
    case CommandType::kGet: {
      if (!cmd.args.empty()) {
        spdlog::debug("GET {}", cmd.args[0]);
      } else {
        spdlog::warn("GET missing key");
      }
      break;
    }
    case CommandType::kSet: {
      if (cmd.args.size() >= 2) {
        spdlog::debug("SET {}: {}", cmd.args[0], cmd.args[1]);
      } else {
        spdlog::warn("SET missing key/value");
      }
      break;
    }
    case CommandType::kDel: {
      spdlog::debug("DEL");
      break;
    }
    case CommandType::kExpire: {
      spdlog::debug("EXPIRE");
      break;
    }
    case CommandType::kTtl: {
      spdlog::debug("TTL");
      break;
    }
    case CommandType::kUnknown: {
      spdlog::warn("Unknown command");
      break;
    }
  }

  RespValue response;
  response.type = RespValue::Type::kSimpleString;
  response.data = std::string("OK");
  return response;
}

}  // namespace tinycache
