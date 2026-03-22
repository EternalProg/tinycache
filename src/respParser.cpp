#include "respParser.hpp"

#include <boost/asio/buffers_iterator.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace tinycache {

namespace asio = boost::asio;

namespace {

// Keep parser limits conservative to prevent oversized allocations.
constexpr std::uint64_t kMaxBulkSize = 16ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaxArrayLength = 1ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaxNestingDepth = 4096;

class RespParserImpl {
 public:
  template <std::random_access_iterator Iterator>
  static ParsingResult parseImpl(Iterator begin, Iterator end,
                                 std::size_t& consumed, RespValue& outValue);

 private:
  struct ArrayFrame {
    std::size_t remaining = 0;
    RespValue::RespArray elements;
  };

  template <std::random_access_iterator Iterator>
  static std::size_t findCrlf(Iterator begin, std::size_t size,
                              std::size_t start);

  template <std::random_access_iterator Iterator>
  static bool parseInt64Token(Iterator begin, std::size_t token_start,
                              std::size_t token_end, std::int64_t& out);

  static void attachValue(RespValue value, std::vector<ArrayFrame>& stack,
                          RespValue& root_value, bool& root_ready);
};

template <std::random_access_iterator Iterator>
ParsingResult RespParserImpl::parseImpl(Iterator begin, Iterator end,
                                        std::size_t& consumed,
                                        RespValue& outValue) {
  consumed = 0;

  const auto size = static_cast<std::size_t>(std::distance(begin, end));
  if (size == 0) {
    return ParsingResult::kNeedMoreData;
  }

  std::size_t pos = 0;
  std::vector<ArrayFrame> stack;
  stack.reserve(8);

  RespValue root_value;
  bool root_ready = false;

  while (!root_ready) {
    if (pos >= size) {
      return ParsingResult::kNeedMoreData;
    }

    const char type_byte = *(begin + pos);
    ++pos;

    switch (type_byte) {
      case '+':
      case '-': {
        const auto line_end = findCrlf(begin, size, pos);
        if (line_end == std::numeric_limits<std::size_t>::max()) {
          return ParsingResult::kNeedMoreData;
        }

        RespValue value;
        value.type = type_byte == '+' ? RespValue::Type::kSimpleString
                                      : RespValue::Type::kError;
        value.data = std::string(begin + pos, begin + line_end);
        pos = line_end + 2;

        attachValue(std::move(value), stack, root_value, root_ready);
        break;
      }

      case ':': {
        const auto line_end = findCrlf(begin, size, pos);
        if (line_end == std::numeric_limits<std::size_t>::max()) {
          return ParsingResult::kNeedMoreData;
        }

        std::int64_t number = 0;
        if (!parseInt64Token(begin, pos, line_end, number)) {
          return ParsingResult::kError;
        }

        RespValue value;
        value.type = RespValue::Type::kInteger;
        value.data = number;
        pos = line_end + 2;

        attachValue(std::move(value), stack, root_value, root_ready);
        break;
      }

      case '$': {
        const auto line_end = findCrlf(begin, size, pos);
        if (line_end == std::numeric_limits<std::size_t>::max()) {
          return ParsingResult::kNeedMoreData;
        }

        std::int64_t bulk_length = 0;
        if (!parseInt64Token(begin, pos, line_end, bulk_length)) {
          return ParsingResult::kError;
        }

        pos = line_end + 2;

        if (bulk_length == -1) {
          RespValue value;
          value.type = RespValue::Type::kNullBulkString;
          attachValue(std::move(value), stack, root_value, root_ready);
          break;
        }

        if (bulk_length < 0 ||
            static_cast<std::uint64_t>(bulk_length) > kMaxBulkSize) {
          return ParsingResult::kError;
        }

        const auto data_len = static_cast<std::size_t>(bulk_length);
        if (size - pos < data_len + 2) {
          return ParsingResult::kNeedMoreData;
        }

        if (*(begin + pos + data_len) != '\r' ||
            *(begin + pos + data_len + 1) != '\n') {
          return ParsingResult::kError;
        }

        RespValue value;
        value.type = RespValue::Type::kBulkString;
        value.data = std::string(begin + pos, begin + pos + data_len);
        pos += data_len + 2;

        attachValue(std::move(value), stack, root_value, root_ready);
        break;
      }

      case '*': {
        const auto line_end = findCrlf(begin, size, pos);
        if (line_end == std::numeric_limits<std::size_t>::max()) {
          return ParsingResult::kNeedMoreData;
        }

        std::int64_t array_len = 0;
        if (!parseInt64Token(begin, pos, line_end, array_len)) {
          return ParsingResult::kError;
        }

        pos = line_end + 2;

        if (array_len == -1) {
          RespValue value;
          value.type = RespValue::Type::kNullArray;
          attachValue(std::move(value), stack, root_value, root_ready);
          break;
        }

        if (array_len < 0 ||
            static_cast<std::uint64_t>(array_len) > kMaxArrayLength) {
          return ParsingResult::kError;
        }

        if (array_len == 0) {
          RespValue value;
          value.type = RespValue::Type::kArray;
          value.data = RespValue::RespArray{};
          attachValue(std::move(value), stack, root_value, root_ready);
          break;
        }

        if (stack.size() + 1 > kMaxNestingDepth) {
          return ParsingResult::kError;
        }

        ArrayFrame frame;
        frame.remaining = static_cast<std::size_t>(array_len);
        frame.elements.reserve(frame.remaining);
        stack.push_back(std::move(frame));
        break;
      }

      default:
        return ParsingResult::kError;
    }
  }

  consumed = pos;
  outValue = std::move(root_value);
  return ParsingResult::kReady;
}

template <std::random_access_iterator Iterator>
std::size_t RespParserImpl::findCrlf(Iterator begin, std::size_t size,
                                     std::size_t start) {
  if (start >= size) {
    return std::numeric_limits<std::size_t>::max();
  }

  for (std::size_t i = start; i + 1 < size; ++i) {
    if (*(begin + i) == '\r' && *(begin + i + 1) == '\n') {
      return i;
    }
  }

  return std::numeric_limits<std::size_t>::max();
}

template <std::random_access_iterator Iterator>
bool RespParserImpl::parseInt64Token(Iterator begin, std::size_t token_start,
                                     std::size_t token_end, std::int64_t& out) {
  if (token_start >= token_end) {
    return false;
  }

  std::size_t pos = token_start;
  bool negative = false;

  const char sign = *(begin + pos);
  if (sign == '+' || sign == '-') {
    negative = (sign == '-');
    ++pos;
    if (pos >= token_end) {
      return false;
    }
  }

  constexpr auto kInt64Max =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  constexpr std::uint64_t kInt64MinAbs = kInt64Max + 1ULL;
  const std::uint64_t limit = negative ? kInt64MinAbs : kInt64Max;

  std::uint64_t acc = 0;
  for (; pos < token_end; ++pos) {
    const char c = *(begin + pos);
    if (c < '0' || c > '9') {
      return false;
    }

    const auto digit = static_cast<std::uint64_t>(c - '0');
    if (acc > (limit - digit) / 10ULL) {
      return false;
    }
    acc = acc * 10ULL + digit;
  }

  if (!negative) {
    out = static_cast<std::int64_t>(acc);
    return true;
  }

  if (acc == kInt64MinAbs) {
    out = std::numeric_limits<std::int64_t>::min();
    return true;
  }

  out = -static_cast<std::int64_t>(acc);
  return true;
}

void RespParserImpl::attachValue(RespValue value,
                                 std::vector<ArrayFrame>& stack,
                                 RespValue& root_value, bool& root_ready) {
  RespValue current = std::move(value);

  while (true) {
    if (stack.empty()) {
      root_value = std::move(current);
      root_ready = true;
      return;
    }

    auto& frame = stack.back();
    frame.elements.push_back(std::move(current));
    --frame.remaining;

    if (frame.remaining > 0) {
      return;
    }

    current.type = RespValue::Type::kArray;
    current.data = std::move(frame.elements);
    stack.pop_back();
  }
}

}  // namespace

ParsingResult RespParser::parse(asio::streambuf& buffer, RespValue& outValue) {
  auto data = buffer.data();
  auto begin = asio::buffers_begin(data);
  auto end = asio::buffers_end(data);

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
