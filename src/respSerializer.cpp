#include <respSerializer.hpp>
#include "respValue.hpp"

namespace tinycache {

namespace {
// TODO(eternal): get rid of recursion and use iterative approach to avoid stack overflow on large arrays
class RespSerializerImpl {
 public:
  static std::string serialize(const RespValue& value);

 private:
  static std::string serializeSimpleString(const RespValue& value);
  static std::string serializeError(const RespValue& value);
  static std::string serializeInteger(const RespValue& value);
  static std::string serializeBulkString(const RespValue& value);
  static std::string serializeNullBulkString();
  static std::string serializeArray(const RespValue& value);
};

std::string RespSerializerImpl::serializeSimpleString(const RespValue& value) {
  const auto& str = std::get<std::string>(value.data);
  return "+" + str + "\r\n";
}

std::string RespSerializerImpl::serializeError(const RespValue& value) {
  const auto& str = std::get<std::string>(value.data);
  return "-" + str + "\r\n";
}

std::string RespSerializerImpl::serializeInteger(const RespValue& value) {
  return ":" + std::to_string(std::get<std::int64_t>(value.data)) + "\r\n";
}

std::string RespSerializerImpl::serializeBulkString(const RespValue& value) {
  const auto& str = std::get<std::string>(value.data);
  return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string RespSerializerImpl::serializeNullBulkString() {
  return "$-1\r\n";
}

std::string RespSerializerImpl::serializeArray(const RespValue& value) {
  const auto& elements = std::get<std::vector<RespValue>>(value.data);
  std::string result = "*" + std::to_string(elements.size()) + "\r\n";
  for (const auto& elem : elements) {
    result += serialize(elem);
  }
  return result;
}

std::string RespSerializerImpl::serialize(const RespValue& value) {
  switch (value.type) {
    case RespValue::Type::kSimpleString:
      return serializeSimpleString(value);
    case RespValue::Type::kError:
      return serializeError(value);
    case RespValue::Type::kInteger:
      return serializeInteger(value);
    case RespValue::Type::kBulkString:
      return serializeBulkString(value);
    case RespValue::Type::kNullBulkString:
      return serializeNullBulkString();
    case RespValue::Type::kArray:
      return serializeArray(value);
    case RespValue::Type::kNullArray:
      return "*-1\r\n";
    default:
      return "-ERR unknown type\r\n";
  }
}

}  // namespace

std::string RespSerializer::serialize(const RespValue& value) {
  return RespSerializerImpl::serialize(value);
}

}  // namespace tinycache
