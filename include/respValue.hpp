#ifndef TINYCACHE_RESP_VALUE_HPP
#define TINYCACHE_RESP_VALUE_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace tinycache {
// https://redis.io/docs/latest/develop/reference/protocol-spec/
// RESP2 types

struct [[nodiscard]] RespValue {
  enum class Type {
    kUnknown,
    kSimpleString,
    kError,
    kInteger,
    kBulkString,
    kNullBulkString,
    kArray,
    kNullArray,
  };
  using RespArray = std::vector<RespValue>;

  RespValue() = default;
  RespValue(const RespValue&) = default;
  RespValue(RespValue&&) noexcept = default;
  RespValue& operator=(const RespValue&) = default;
  RespValue& operator=(RespValue&&) noexcept = default;
  ~RespValue() = default;

  RespValue(Type t, std::string str) : type(t), data(std::move(str)) {}

  RespValue(Type t, std::int64_t integer) : type(t), data(integer) {}

  RespValue(Type t, RespArray array) : type(t), data(std::move(array)) {}

  explicit RespValue(std::int64_t integer)
      : type(Type::kInteger), data(integer) {}

  explicit RespValue(RespArray array)
      : type(Type::kArray), data(std::move(array)) {}

  Type type = RespValue::Type::kUnknown;
  std::variant<std::string,   // Simple / Error / Bulk
               std::int64_t,  // Integer
               RespArray      // Array
               >
      data;
};

}  // namespace tinycache

#endif
