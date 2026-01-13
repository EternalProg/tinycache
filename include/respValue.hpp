#ifndef TINYCACHE_RESP_VALUE_HPP
#define TINYCACHE_RESP_VALUE_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace tinycache {

struct RespValue {
  enum class Type {
    kUnknown,
    kSimpleString,
    kError,
    kInteger,
    kBulkString,
    kArray,
  };

  Type type;
  std::variant<std::string,            // Simple / Error / Bulk
               std::int64_t,           // Integer
               std::vector<RespValue>  // Array
               >
      data;
};

}  // namespace tinycache

#endif
