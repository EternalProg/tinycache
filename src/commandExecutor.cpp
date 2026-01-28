#include "commandExecutor.hpp"
#include <spdlog/spdlog.h>

namespace tinycache {

namespace {

RespValue makeError(std::string message) {
  RespValue response;
  response.type = RespValue::Type::kError;
  response.data = std::move(message);
  return response;
}

RespValue makeOk() {
  RespValue response;
  response.type = RespValue::Type::kSimpleString;
  response.data = std::string("OK");
  return response;
}

}  // namespace

RespValue CommandExecutor::execute(const Command& cmd) {
  switch (cmd.type) {
    case CommandType::kGet: {
      if (cmd.args.size() != 1) {
        spdlog::warn("GET missing key");
        return makeError("ERR wrong number of arguments for 'get'");
      }
      spdlog::debug("GET {}", cmd.args[0]);
      return makeOk();
    }
    case CommandType::kSet: {
      if (cmd.args.size() < 2) {
        spdlog::warn("SET missing key/value");
        return makeError("ERR wrong number of arguments for 'set'");
      }
      spdlog::debug("SET {}: {}", cmd.args[0], cmd.args[1]);
      return makeOk();
    }
    case CommandType::kDel: {
      if (cmd.args.empty()) {
        spdlog::warn("DEL missing key");
        return makeError("ERR wrong number of arguments for 'del'");
      }
      spdlog::debug("DEL");
      return makeOk();
    }
    case CommandType::kExpire: {
      if (cmd.args.size() < 2) {
        spdlog::warn("EXPIRE missing key/timeout");
        return makeError("ERR wrong number of arguments for 'expire'");
      }
      spdlog::debug("EXPIRE");
      return makeOk();
    }
    case CommandType::kTtl: {
      if (cmd.args.size() != 1) {
        spdlog::warn("TTL missing key");
        return makeError("ERR wrong number of arguments for 'ttl'");
      }
      spdlog::debug("TTL");
      return makeOk();
    }
    case CommandType::kUnknown: {
      spdlog::warn("Unknown command");
      return makeError("ERR unknown command");
    }
    default:
      return makeError("ERR unknown command");
  }

  return makeError("ERR unknown command");
}

}  // namespace tinycache
