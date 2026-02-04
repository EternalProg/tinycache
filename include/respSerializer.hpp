#ifndef TINYCACHE_RESP_SERIALIZER_HPP
#define TINYCACHE_RESP_SERIALIZER_HPP

#include <string>
#include "respValue.hpp"

namespace tinycache {

class RespSerializer {
 public:
  static std::string serialize(const RespValue& value);
};
}  // namespace tinycache
#endif
