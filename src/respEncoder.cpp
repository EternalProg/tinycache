#include "respEncoder.hpp"
#include <string_view>
#include <vector>

namespace tinycache {
namespace {

std::string encodeSimpleString(std::string_view data) {
  std::string output;
  output.reserve(data.size() + 3);
  output.push_back('+');
  output.append(data);
  output.append("\r\n");
  return output;
}

std::string encodeError(std::string_view data) {
  std::string output;
  output.reserve(data.size() + 3);
  output.push_back('-');
  output.append(data);
  output.append("\r\n");
  return output;
}

}  // namespace

std::string encodeRespValue(const RespValue& value) {
  if (value.type == RespValue::Type::kSimpleString &&
      std::holds_alternative<std::string>(value.data)) {
    return encodeSimpleString(std::get<std::string>(value.data));
  }

  if (value.type == RespValue::Type::kError &&
      std::holds_alternative<std::string>(value.data)) {
    return encodeError(std::get<std::string>(value.data));
  }

  if (value.type == RespValue::Type::kBulkString &&
      std::holds_alternative<std::string>(value.data)) {
    const auto& data = std::get<std::string>(value.data);
    std::string output;
    output.reserve(data.size() + 32);
    output.push_back('$');
    output.append(std::to_string(data.size()));
    output.append("\r\n");
    output.append(data);
    output.append("\r\n");
    return output;
  }

  if (value.type == RespValue::Type::kInteger &&
      std::holds_alternative<std::int64_t>(value.data)) {
    std::string output;
    output.reserve(32);
    output.push_back(':');
    output.append(std::to_string(std::get<std::int64_t>(value.data)));
    output.append("\r\n");
    return output;
  }

  if (value.type == RespValue::Type::kNullBulkString) {
    return "$-1\r\n";
  }

  if (value.type == RespValue::Type::kArray &&
      std::holds_alternative<std::vector<RespValue>>(value.data)) {
    const auto& elements = std::get<std::vector<RespValue>>(value.data);
    std::string output;
    output.reserve(32);
    output.push_back('*');
    output.append(std::to_string(elements.size()));
    output.append("\r\n");
    for (const auto& element : elements) {
      output.append(encodeRespValue(element));
    }
    return output;
  }

  return encodeError("ERR unsupported response");
}

}  // namespace tinycache
