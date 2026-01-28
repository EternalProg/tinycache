#include "command.hpp"
#include <algorithm>
#include <cctype>

namespace tinycache {
namespace {

void toUpper(std::string& value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
}

}  // namespace

std::optional<Command> Command::toCommand(const RespValue& value) {
  if (value.type != RespValue::Type::kArray) {
    return std::nullopt;
  }

  if (!std::holds_alternative<std::vector<RespValue>>(value.data)) {
    return std::nullopt;
  }

  const auto& elements = std::get<std::vector<RespValue>>(value.data);
  if (elements.empty()) {
    return std::nullopt;
  }

  if (elements[0].type != RespValue::Type::kBulkString) {
    return std::nullopt;
  }

  if (!std::holds_alternative<std::string>(elements[0].data)) {
    return std::nullopt;
  }

  std::string command_name = std::get<std::string>(elements[0].data);
  toUpper(command_name);
  Command command;
  command.args.reserve(elements.size() - 1);

  for (std::size_t i = 1; i < elements.size(); ++i) {
    if (elements[i].type != RespValue::Type::kBulkString ||
        !std::holds_alternative<std::string>(elements[i].data)) {
      return std::nullopt;
    }
    command.args.push_back(std::get<std::string>(elements[i].data));
  }

  if (command_name == "GET") {
    command.type = CommandType::kGet;
  } else if (command_name == "SET") {
    command.type = CommandType::kSet;
  } else if (command_name == "DEL") {
    command.type = CommandType::kDel;
  } else if (command_name == "EXPIRE") {
    command.type = CommandType::kExpire;
  } else if (command_name == "TTL") {
    command.type = CommandType::kTtl;
  } else {
    command.type = CommandType::kUnknown;
  }

  return command;
}

}  // namespace tinycache
