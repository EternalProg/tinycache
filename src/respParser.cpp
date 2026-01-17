#include "respParser.hpp"
#include <boost/asio/buffers_iterator.hpp>
#include <cstdint>
#include <iterator>
#include <string>
#include "respValue.hpp"

// 16Mb; Default RESP use 512Mb
static constexpr std::size_t kMaxBulkSize = 1024 * 1024 * 16 /*512*/;

namespace tinycache {

namespace asio = boost::asio;

namespace {

class RespParserImpl {
 public:
  template <std::random_access_iterator Iterator>
  static ParsingResult parseImpl(Iterator begin, Iterator end,
                                 std::uint64_t& consumed, RespValue& outValue);

 private:
  template <std::random_access_iterator Iterator>
  static ParsingResult dispatchParsing(RespValue::Type type, Iterator it,
                                       Iterator end, std::uint64_t& consumed,
                                       RespValue& outValue);

  static RespValue::Type determineType(char type);

  template <typename Iterator>
  static Iterator findCrlf(Iterator it, Iterator end);

  template <std::random_access_iterator Iterator>
  static ParsingResult parseSimpleString(Iterator it, Iterator end,
                                         std::uint64_t& consumed,
                                         RespValue& outValue);

  template <std::random_access_iterator Iterator>
  static ParsingResult parseBulkString(Iterator it, Iterator end,
                                       std::uint64_t& consumed,
                                       RespValue& outValue);

  template <std::random_access_iterator Iterator>
  static ParsingResult parseInteger(Iterator it, Iterator end,
                                    std::uint64_t& consumed,
                                    RespValue& outValue);

  template <std::random_access_iterator Iterator>
  static ParsingResult parseArray(Iterator it, Iterator end,
                                  std::uint64_t& consumed, RespValue& outValue);

  template <std::random_access_iterator Iterator>
  static ParsingResult parseError(Iterator it, Iterator end,
                                  std::uint64_t& consumed, RespValue& outValue);
};

// ParsingResult RespParserImpl::parseImpl(asio::streambuf& buffer,
//                                         RespValue& outValue) {
//   if (buffer.size() == 0) {
//     return ParsingResult::kNeedMoreData;
//   }

//   auto data = buffer.data();
//   auto begin = boost::asio::buffers_begin(data);
//   auto end = boost::asio::buffers_end(data);

//   if (begin == end) {
//     return ParsingResult::kNeedMoreData;
//   }

//   auto it = begin;

//   auto type = determineType(*it);
//   if (type == RespValue::Type::kUnknown) {
//     return ParsingResult::kError;
//   }
//   ++it;

//   // Because the first char is consumed to determine type
//   std::uint64_t consumed = 1;

//   ParsingResult result = dispatchParsing(type, it, end, consumed, outValue);

//   if (result != ParsingResult::kNeedMoreData) {
//     buffer.consume(consumed);
//   }

//   return result;
// }

template <std::random_access_iterator Iterator>
ParsingResult RespParserImpl::parseImpl(Iterator begin, Iterator end,
                                        std::uint64_t& consumed,
                                        RespValue& outValue) {
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
  std::uint64_t tmp_consumed = 1;

  ParsingResult result = dispatchParsing(type, it, end, consumed, outValue);

  if (result != ParsingResult::kNeedMoreData) {
    consumed += tmp_consumed;
  }

  return result;
}

template <std::random_access_iterator Iterator>
ParsingResult RespParserImpl::dispatchParsing(RespValue::Type type, Iterator it,
                                              Iterator end,
                                              std::uint64_t& consumed,
                                              RespValue& outValue) {
  switch (type) {
    case RespValue::Type::kSimpleString: {
      return parseSimpleString(it, end, consumed, outValue);
    }
    case RespValue::Type::kError: {
      return parseError(it, end, consumed, outValue);
    }
    case RespValue::Type::kInteger: {
      return parseInteger(it, end, consumed, outValue);
    }
    case RespValue::Type::kBulkString: {
      return parseBulkString(it, end, consumed, outValue);
    }
    case RespValue::Type::kArray: {
      return parseArray(it, end, consumed, outValue);
    }
    default:
      break;
  }

  return ParsingResult::kError;
}

RespValue::Type RespParserImpl::determineType(char type) {
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
Iterator RespParserImpl::findCrlf(Iterator it, Iterator end) {
  for (auto cur = it; cur != end; ++cur) {
    if (*cur == '\n') {
      if (cur != it && *(cur - 1) == '\r') {
        return cur - 1;  // points to '\r'
      }
    }
  }
  return end;
}

template <std::random_access_iterator Iterator>
ParsingResult RespParserImpl::parseSimpleString(Iterator it, Iterator end,
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
ParsingResult RespParserImpl::parseBulkString(Iterator it, Iterator end,
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
ParsingResult RespParserImpl::parseInteger(Iterator it, Iterator end,
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

/*
    *<number-of-elements>\r\n<element-1>...<element-n>

    An asterisk (*) as the first byte.
    One or more decimal digits (0..9) as the number of elements in the array as an unsigned, base-10 value.
    The CRLF terminator.
    An additional RESP type for every element of the array.
*/
template <std::random_access_iterator Iterator>
ParsingResult RespParserImpl::parseArray(Iterator it, Iterator end,
                                         std::uint64_t& consumed,
                                         RespValue& outValue) {
  auto header_crlf = findCrlf(it, end);
  if (header_crlf == end) {
    return ParsingResult::kNeedMoreData;
  }

  std::string header(it, header_crlf);
  std::size_t number_of_elements = 0;

  try {
    std::size_t pos = 0;
    number_of_elements = std::stoll(header, &pos, 10);

    if (pos != header.size()) {
      return ParsingResult::kError;
    }
  } catch (...) {
    return ParsingResult::kError;
  }

  outValue.type = RespValue::Type::kArray;
  std::vector<RespValue> elements;
  elements.reserve(number_of_elements);

  std::size_t tmp_consumed = std::distance(it, header_crlf) + 2;
  for (std::size_t i = 0; i < number_of_elements; ++i) {
    // Not sure if it's good to fill the elements, because there can be error at the end.
    // And also we go through all the elements every time
    RespValue tmp;
    auto result = parseImpl(it + tmp_consumed, end, tmp_consumed, tmp);
    if (result == ParsingResult::kReady) {
      elements.push_back(std::move(tmp));
    } else {
      return result;
    }
  }

  outValue.data = std::move(elements);
  consumed += tmp_consumed;

  return ParsingResult::kReady;
}

template <std::random_access_iterator Iterator>
ParsingResult RespParserImpl::parseError(Iterator it, Iterator end,
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

}  // namespace

ParsingResult RespParser::parse(asio::streambuf& buffer, RespValue& outValue) {
  auto data = buffer.data();
  auto begin = boost::asio::buffers_begin(data);
  auto end = boost::asio::buffers_end(data);

  RespValue tmp;
  std::size_t consumed = 0;

  auto result = RespParserImpl::parseImpl(begin, end, consumed, tmp);
  if (result == ParsingResult::kReady) {
    outValue = std::move(tmp);
    buffer.consume(consumed);
  }

  return result;
}

}  // namespace tinycache
