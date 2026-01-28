#include <gtest/gtest.h>
#include <respEncoder.hpp>
#include <string>
#include <vector>
#include "respValue.hpp"

using tinycache::encodeRespValue;
using tinycache::RespValue;

namespace {

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
