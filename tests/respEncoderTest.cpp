#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "respValue.hpp"

using tinycache::RespValue;

namespace {

std::string encodeSimpleString(std::string_view data) {
  std::string output;
  output.reserve(data.size() + 3);
  output.push_back('+');
  output.append(data);
  output.append("\r\n");
  return output;
}

std::string encodeError(std::string_view data) {
  std::string output;
  output.reserve(data.size() + 3);
  output.push_back('-');
  output.append(data);
  output.append("\r\n");
  return output;
}

std::string encodeRespValue(const RespValue& value) {
  if (value.type == RespValue::Type::kSimpleString &&
      std::holds_alternative<std::string>(value.data)) {
    return encodeSimpleString(std::get<std::string>(value.data));
  }

  if (value.type == RespValue::Type::kError &&
      std::holds_alternative<std::string>(value.data)) {
    return encodeError(std::get<std::string>(value.data));
  }

  if (value.type == RespValue::Type::kBulkString &&
      std::holds_alternative<std::string>(value.data)) {
    const auto& data = std::get<std::string>(value.data);
    std::string output;
    output.reserve(data.size() + 32);
    output.push_back('$');
    output.append(std::to_string(data.size()));
    output.append("\r\n");
    output.append(data);
    output.append("\r\n");
    return output;
  }

  if (value.type == RespValue::Type::kInteger &&
      std::holds_alternative<std::int64_t>(value.data)) {
    std::string output;
    output.reserve(32);
    output.push_back(':');
    output.append(std::to_string(std::get<std::int64_t>(value.data)));
    output.append("\r\n");
    return output;
  }

  if (value.type == RespValue::Type::kNullBulkString) {
    return "$-1\r\n";
  }

  if (value.type == RespValue::Type::kArray &&
      std::holds_alternative<std::vector<RespValue>>(value.data)) {
    const auto& elements = std::get<std::vector<RespValue>>(value.data);
    std::string output;
    output.reserve(32);
    output.push_back('*');
    output.append(std::to_string(elements.size()));
    output.append("\r\n");
    for (const auto& element : elements) {
      output.append(encodeRespValue(element));
    }
    return output;
  }

  return encodeError("ERR unsupported response");
}

}  // namespace

TEST(RespEncoderTest, EncodesSimpleString) {
  RespValue value;
  value.type = RespValue::Type::kSimpleString;
  value.data = std::string("OK");

  EXPECT_EQ(encodeRespValue(value), "+OK\r\n");
}

TEST(RespEncoderTest, EncodesError) {
  RespValue value;
  value.type = RespValue::Type::kError;
  value.data = std::string("ERR nope");

  EXPECT_EQ(encodeRespValue(value), "-ERR nope\r\n");
}

TEST(RespEncoderTest, EncodesBulkString) {
  RespValue value;
  value.type = RespValue::Type::kBulkString;
  value.data = std::string("foo");

  EXPECT_EQ(encodeRespValue(value), "$3\r\nfoo\r\n");
}

TEST(RespEncoderTest, EncodesInteger) {
  RespValue value;
  value.type = RespValue::Type::kInteger;
  value.data = static_cast<std::int64_t>(42);

  EXPECT_EQ(encodeRespValue(value), ":42\r\n");
}

TEST(RespEncoderTest, EncodesNullBulkString) {
  RespValue value;
  value.type = RespValue::Type::kNullBulkString;
  value.data = std::string();

  EXPECT_EQ(encodeRespValue(value), "$-1\r\n");
}

TEST(RespEncoderTest, EncodesArray) {
  RespValue first;
  first.type = RespValue::Type::kSimpleString;
  first.data = std::string("OK");

  RespValue second;
  second.type = RespValue::Type::kInteger;
  second.data = static_cast<std::int64_t>(1);

  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data = std::vector<RespValue>{first, second};

  EXPECT_EQ(encodeRespValue(value), "*2\r\n+OK\r\n:1\r\n");
}
