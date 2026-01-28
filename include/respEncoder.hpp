#ifndef TINYCACHE_RESP_ENCODER_HPP
#define TINYCACHE_RESP_ENCODER_HPP

#include <string>
#include "respValue.hpp"

namespace tinycache {

std::string encodeRespValue(const RespValue& value);

}  // namespace tinycache

#endif
