#include <respSerializer.hpp>

#include <charconv>
#include <string_view>
#include <vector>

namespace tinycache {

namespace {

constexpr std::string_view kCrlf = "\r\n";
constexpr std::string_view kNullBulkString = "$-1\r\n";
constexpr std::string_view kNullArray = "*-1\r\n";
constexpr std::string_view kUnknownType = "-ERR unknown type\r\n";

template <typename Integer>
std::size_t integer_length(Integer value) {
  char buffer[32];
  auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (ec != std::errc{}) {
    return 0;
  }
  return static_cast<std::size_t>(ptr - buffer);
}

template <typename Integer>
void append_integer(std::string& out, Integer value) {
  char buffer[32];
  auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (ec == std::errc{}) {
    out.append(buffer, static_cast<std::size_t>(ptr - buffer));
  }
}

void append_prefixed_line(std::string& out, char prefix,
                          std::string_view text) {
  out.push_back(prefix);
  out.append(text);
  out.append(kCrlf);
}

void append_non_array(std::string& out, const RespValue& value) {
  switch (value.type) {
    case RespValue::Type::kSimpleString:
      append_prefixed_line(out, '+', std::get<std::string>(value.data));
      return;

    case RespValue::Type::kError:
      append_prefixed_line(out, '-', std::get<std::string>(value.data));
      return;

    case RespValue::Type::kInteger:
      out.push_back(':');
      append_integer(out, std::get<std::int64_t>(value.data));
      out.append(kCrlf);
      return;

    case RespValue::Type::kBulkString: {
      const auto& str = std::get<std::string>(value.data);
      out.push_back('$');
      append_integer(out, str.size());
      out.append(kCrlf);
      out.append(str);
      out.append(kCrlf);
      return;
    }

    case RespValue::Type::kNullBulkString:
      out.append(kNullBulkString);
      return;

    case RespValue::Type::kNullArray:
      out.append(kNullArray);
      return;

    default:
      out.append(kUnknownType);
      return;
  }
}

std::size_t estimate_serialized_size(const RespValue& root) {
  std::size_t total = 0;
  std::vector<const RespValue*> stack;
  stack.push_back(&root);

  while (!stack.empty()) {
    const RespValue* value = stack.back();
    stack.pop_back();

    switch (value->type) {
      case RespValue::Type::kSimpleString:
      case RespValue::Type::kError:
        total += 1 + std::get<std::string>(value->data).size() + kCrlf.size();
        break;

      case RespValue::Type::kInteger:
        total += 1 + integer_length(std::get<std::int64_t>(value->data)) +
                 kCrlf.size();
        break;

      case RespValue::Type::kBulkString: {
        const auto& str = std::get<std::string>(value->data);
        total += 1 + integer_length(str.size()) + kCrlf.size() + str.size() +
                 kCrlf.size();
        break;
      }

      case RespValue::Type::kNullBulkString:
        total += kNullBulkString.size();
        break;

      case RespValue::Type::kArray: {
        const auto& elements = std::get<RespValue::RespArray>(value->data);
        total += 1 + integer_length(elements.size()) + kCrlf.size();
        for (std::size_t i = elements.size(); i > 0; --i) {
          stack.push_back(&elements[i - 1]);
        }
        break;
      }

      case RespValue::Type::kNullArray:
        total += kNullArray.size();
        break;

      default:
        total += kUnknownType.size();
        break;
    }
  }

  return total;
}

std::string serialize_iterative(const RespValue& root) {
  struct ArrayFrame {
    const RespValue::RespArray* elements = nullptr;
    std::size_t next_index = 0;
  };

  std::string out;
  out.reserve(estimate_serialized_size(root));

  std::vector<ArrayFrame> stack;
  const RespValue* current = &root;

  for (;;) {
    if (current->type == RespValue::Type::kArray) {
      const auto& elements = std::get<RespValue::RespArray>(current->data);
      out.push_back('*');
      append_integer(out, elements.size());
      out.append(kCrlf);

      if (!elements.empty()) {
        stack.push_back(ArrayFrame{.elements = &elements, .next_index = 1});
        current = elements.data();
        continue;
      }
    } else {
      append_non_array(out, *current);
    }

    bool advanced = false;
    while (!stack.empty()) {
      auto& frame = stack.back();
      if (frame.next_index < frame.elements->size()) {
        current = &((*frame.elements)[frame.next_index]);
        ++frame.next_index;
        advanced = true;
        break;
      }
      stack.pop_back();
    }

    if (!advanced) {
      break;
    }
  }

  return out;
}

}  // namespace

std::string RespSerializer::serialize(const RespValue& value) {
  return serialize_iterative(value);
}

}  // namespace tinycache
