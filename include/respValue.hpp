#ifndef TINYCACHE_RESP_VALUE_HPP
#define TINYCACHE_RESP_VALUE_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace tinycache {
// https://redis.io/docs/latest/develop/reference/protocol-spec/
// RESP2 types

struct RespValue {
  enum class Type {
    kUnknown,
    kSimpleString,
    kError,
    kInteger,
    kBulkString,
    kNullBulkString,
    kArray,
  };

  using RespArray = std::vector<RespValue>;

  Type type = RespValue::Type::kUnknown;
  std::variant<std::string,   // Simple / Error / Bulk
               std::int64_t,  // Integer
               RespArray      // Array
               >
      data;
};

}  // namespace tinycache

#endif
