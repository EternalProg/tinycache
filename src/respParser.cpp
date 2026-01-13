#include "respParser.hpp"
#include <boost/asio/buffers_iterator.hpp>
#include <cstdint>
#include <iterator>
#include "respValue.hpp"

namespace tinycache {

namespace asio = boost::asio;

namespace {

RespValue::Type determineType(char type) {
  switch (type) {
    case '+':
      return RespValue::Type::kSimpleString;
    case '-':
      return RespValue::Type::kError;
    case ':':
      return RespValue::Type::kInteger;
    case '$':
      return RespValue::Type::kBulkString;
    case '*':
      return RespValue::Type::kArray;
    default:
      return RespValue::Type::kUnknown;
  }
}

template <typename Iterator>
Iterator findCrlf(Iterator it, Iterator end) {
  for (auto cur = it; cur != end; ++cur) {
    if (*cur == '\n') {
      if (cur != it && *(cur - 1) == '\r') {
        return cur - 1;  // points to '\r'
      }
    }
  }
  return end;
}

}  // namespace

ParsingResult RespParser::parse(asio::streambuf& buffer, RespValue& outValue) {
  if (buffer.size() == 0) {
    return ParsingResult::kNeedMoreData;
  }

  auto data = buffer.data();
  auto begin = boost::asio::buffers_begin(data);
  auto end = boost::asio::buffers_end(data);

  if (begin == end) {
    return ParsingResult::kNeedMoreData;
  }

  auto it = begin;

  auto type = determineType(*it);
  if (type == RespValue::Type::kUnknown) {
    return ParsingResult::kError;
  }
  ++it;

  // Because the first char is consumed to determine type
  std::uint64_t consumed = 1;

  ParsingResult result = ParsingResult::kError;
  switch (type) {
    case RespValue::Type::kSimpleString: {
      result = parseSimpleString(it, end, consumed, outValue);
      break;
    }
    default:
      break;
  }

  if (result != ParsingResult::kNeedMoreData) {
    buffer.consume(consumed);
  }

  return result;
}

template <std::forward_iterator Iterator>
ParsingResult RespParser::parseSimpleString(Iterator it, Iterator end,
                                            std::uint64_t& consumed,
                                            RespValue& outValue) {
  // CRLF (i.e., \r\n)
  auto crlf_pos = findCrlf(it, end);
  if (crlf_pos == end) {
    return ParsingResult::kNeedMoreData;
  }

  std::string data(it, crlf_pos);

  outValue.type = RespValue::Type::kSimpleString;
  outValue.data = std::move(data);

  consumed += std::distance(it, crlf_pos) + 2;

  return ParsingResult::kReady;
}


}  // namespace tinycache
