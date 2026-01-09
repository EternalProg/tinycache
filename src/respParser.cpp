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

// return iterator to
template <typename Iterator>
Iterator findCrlf(Iterator it, Iterator end) {
  while (it != end) {
    if (*it == '\n') {
      // return it that points to '\r'
      // so later I can read until it later and get the data
      return it - 2;
    }
    ++it;
  }

  return it;
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
  ++it;

  // Because It consumes the first char to determine type
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

  buffer.consume(consumed);

  return result;
}

template <std::forward_iterator Iterator>
ParsingResult RespParser::parseSimpleString(Iterator it, Iterator end,
                                            std::uint64_t& consumed,
                                            RespValue& outValue) {
  // CRLF (i.e., \r\n)
  auto crlf_pos = findCrlf(it, end);
  if (crlf_pos == it) {
    return ParsingResult::kNeedMoreData;
  }

  std::string data(it, crlf_pos - 2);

  outValue.type = RespValue::Type::kSimpleString;
  outValue.data = data;

  consumed += std::distance(it, crlf_pos) + 4;

  return ParsingResult::kReady;
}


}  // namespace tinycache
