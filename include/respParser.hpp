#ifndef TINYCACHE_RESP_PARSER_HPP
#define TINYCACHE_RESP_PARSER_HPP

#include <boost/asio/streambuf.hpp>
#include <iterator>
#include "respValue.hpp"

namespace tinycache {
enum class ParsingResult { kReady, kNeedMoreData, kError };

class RespParser {
 public:
  /*
    try to parse buffer and fill the value
    return kReady if parsing is done
    return kNeedMoreData if not enough data
    return kError otherwise
  */
  static ParsingResult parse(boost::asio::streambuf& buffer,
                             RespValue& outValue);

 private:
  template <std::forward_iterator Iterator>
  static ParsingResult parseSimpleString(Iterator it, Iterator end,
                                         std::uint64_t& consumed,
                                         RespValue& outValue);

};

}  // namespace tinycache

#endif
