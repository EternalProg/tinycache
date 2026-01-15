#include "respParser.hpp"
#include <boost/asio/buffers_iterator.hpp>
#include <cstdint>
#include <iterator>
#include <string>
#include "respValue.hpp"

static constexpr std::size_t kMaxBulkSize = 1024 * 1024 * 512;

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
    case RespValue::Type::kError: {
      result = parseError(it, end, consumed, outValue);
      break;
    }

    case RespValue::Type::kInteger: {
      result = parseInteger(it, end, consumed, outValue);
      break;
    }
    case RespValue::Type::kBulkString: {
      result = parseBulkString(it, end, consumed, outValue);
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

template <std::random_access_iterator Iterator>
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

/*
    $<length>\r\n<data>\r\n

    The dollar sign ($) as the first byte.
    One or more decimal digits (0..9) as the string's length, in bytes, as an unsigned, base-10 value.
    The CRLF terminator.
    The data.
    A final CRLF.
*/
template <std::random_access_iterator Iterator>
ParsingResult RespParser::parseBulkString(Iterator it, Iterator end,
                                          std::uint64_t& consumed,
                                          RespValue& outValue) {
  auto header_crlf = findCrlf(it, end);
  if (header_crlf == end) {
    return ParsingResult::kNeedMoreData;
  }

  std::string header_data = std::string(it, header_crlf);
  std::size_t header_size = header_data.size() + 2;
  std::int64_t length = 0;

  try {
    std::size_t pos = 0;
    length = std::stoll(header_data, &pos);

    if (pos != header_data.size()) {
      // length is specified incorrectly
      return ParsingResult::kError;
    }
  } catch (...) {
    return ParsingResult::kError;
  }

  // Null Bulk String
  if (length == -1) {
    outValue.type = RespValue::Type::kNullBulkString;
    consumed += header_size;
    return ParsingResult::kReady;
  }

  if (length < 0 || static_cast<std::size_t>(length) > kMaxBulkSize) {
    return ParsingResult::kError;
  }

  std::size_t total_needed = header_size + length + 2;
  if (std::distance(it, end) < static_cast<std::ptrdiff_t>(total_needed)) {
    return ParsingResult::kNeedMoreData;
  }

  Iterator data_begin = it + header_size;
  Iterator data_end = data_begin + length;

  if (*(data_end) != '\r' || *(data_end + 1) != '\n') {
    return ParsingResult::kError;
  }

  outValue.type = RespValue::Type::kBulkString;
  outValue.data = std::string(data_begin, data_end);

  consumed += total_needed;
  return ParsingResult::kReady;
}

/*
  :[<+|->]<value>\r\n

  The colon (:) as the first byte.
  An optional plus (+) or minus (-) as the sign.
  One or more decimal digits (0..9) as the integer's unsigned, base-10 value.
  The CRLF terminator.
  For example, :0\r\n and :1000\r\n are integer replies (of zero and one thousand, respectively).
   */
template <std::random_access_iterator Iterator>
ParsingResult RespParser::parseInteger(Iterator it, Iterator end,
                                       std::uint64_t& consumed,
                                       RespValue& outValue) {
  // :[<+|->]<value>\r\n
  Iterator crlf_pos = findCrlf(it, end);
  if (crlf_pos == end) {
    return ParsingResult::kNeedMoreData;
  }

  std::string data(it, crlf_pos);

  try {
    std::size_t pos = 0;
    std::int64_t value = std::stoll(std::string(data), &pos, 10);

    if (pos != data.size()) {
      return ParsingResult::kError;
    }

    outValue.type = RespValue::Type::kInteger;
    outValue.data = value;

  } catch (...) {
    return ParsingResult::kError;
  }

  consumed += std::distance(it, crlf_pos) + 2;

  return ParsingResult::kReady;
}

template <std::random_access_iterator Iterator>
template <std::forward_iterator Iterator>
ParsingResult RespParser::parseError(Iterator it, Iterator end,
                                     std::uint64_t& consumed,
                                     RespValue& outValue) {
  auto crlf_pos = findCrlf(it, end);
  if (crlf_pos == end) {
    return ParsingResult::kNeedMoreData;
  }

  std::string data(it, crlf_pos);

  outValue.type = RespValue::Type::kError;
  outValue.data = std::move(data);

  consumed += std::distance(it, crlf_pos) + 2;

  return ParsingResult::kReady;
}

}  // namespace tinycache
