#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <ostream>
#include <respParser.hpp>
#include <variant>
#include "respValue.hpp"

using tinycache::ParsingResult;
using tinycache::RespParser;
using tinycache::RespValue;

// Helper functions
namespace {

void write(boost::asio::streambuf& buf, std::string_view s) {
  std::ostream os(&buf);
  os.write(s.data(), s.size());
}

}  // namespace

class RespParserTest : public ::testing::Test {
 protected:
  boost::asio::streambuf buffer_;
  RespValue value_;
};

TEST_F(RespParserTest, EmptyBufferNeedsMoreData) {
  auto res = RespParser::parse(buffer_, value_);

  EXPECT_EQ(res, ParsingResult::kNeedMoreData);
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, IncompleteSimpleStringDoesNotConsume) {
  write(buffer_, "+OK");  // missing CRLF

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kNeedMoreData);
  EXPECT_EQ(buffer_.size(), 3);
}

TEST_F(RespParserTest, ParsesSimpleString) {
  write(buffer_, "+OK\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(value_.data), "OK");
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesErrorString) {
  write(buffer_, "-ERR unknown command\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(value_.data), "ERR unknown command");
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesInteger) {
  write(buffer_, ":12345\r\n");
  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(value_.data), 12345);
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesIntegerWithPlus) {
  write(buffer_, ":+12345\r\n");
  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(value_.data), 12345);
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesNegativeInteger) {
  write(buffer_, ":-12345\r\n");
  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(value_.data), -12345);
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesBulkString) {
  write(buffer_, "$16\r\ntest_bulk_string\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(value_.data), "test_bulk_string");
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, IncompleteBulkStringDoesNotConsume) {
  write(buffer_, "$6\r\nfoo");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kNeedMoreData);
  EXPECT_GT(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesEmptyBulkString) {
  write(buffer_, "$0\r\n\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(value_.data), "");
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, ParsesNullBulkString) {
  write(buffer_, "$-1\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kNullBulkString);
  EXPECT_EQ(buffer_.size(), 0);
}

TEST_F(RespParserTest, BulkStringTooLargeIsError) {
  write(buffer_, "$536870913\r\n");  // 512MB + 1

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kError);
}

TEST_F(RespParserTest, BulkStringWithInternalCRLF) {
  write(buffer_, "$5\r\nhe\r\nl\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kReady);
  EXPECT_EQ(value_.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(value_.data), std::string("he\r\nl", 5));
}

TEST_F(RespParserTest, IncompleteBulkHeaderNeedsMoreData) {
  write(buffer_, "$1");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kNeedMoreData);
  EXPECT_GT(buffer_.size(), 0);
}

TEST_F(RespParserTest, InvalidBulkLengthIsError) {
  write(buffer_, "$abc\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kError);
}

TEST_F(RespParserTest, NegativeBulkLengthOtherThanMinusOneIsError) {
  write(buffer_, "$-5\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kError);
}

TEST_F(RespParserTest, ParsesArrayOfBulkStrings) {
  write(buffer_,
        "*2\r\n"
        "$3\r\nfoo\r\n"
        "$3\r\nbar\r\n");

  auto result = RespParser::parse(buffer_, value_);

  ASSERT_EQ(result, ParsingResult::kReady);
  ASSERT_EQ(value_.type, RespValue::Type::kArray);
  ASSERT_TRUE(std::holds_alternative<std::vector<RespValue>>(value_.data));

  auto elements = std::get<std::vector<RespValue>>(value_.data);

  ASSERT_EQ(elements.size(), 2);

  EXPECT_EQ(elements[0].type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(elements[0].data), "foo");
  EXPECT_EQ(elements[1].type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(elements[1].data), "bar");
}

TEST_F(RespParserTest, ParsesNestedArrays) {
  write(buffer_,
        "*2\r\n"
        "*1\r\n"
        ":1\r\n"
        "+OK\r\n");

  auto result = RespParser::parse(buffer_, value_);

  ASSERT_EQ(result, ParsingResult::kReady);
  ASSERT_EQ(value_.type, RespValue::Type::kArray);
  ASSERT_TRUE(std::holds_alternative<std::vector<RespValue>>(value_.data));

  auto elements = std::get<std::vector<RespValue>>(value_.data);

  ASSERT_EQ(elements.size(), 2);

  EXPECT_EQ(elements[0].type, RespValue::Type::kArray);

  auto nested_integer = std::get<std::vector<RespValue>>(elements[0].data)[0];
  EXPECT_EQ(nested_integer.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(nested_integer.data), 1);

  EXPECT_EQ(elements[1].type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(elements[1].data), "OK");
}

TEST_F(RespParserTest, ParsesEmptyArray) {
  write(buffer_, "*0\r\n");

  auto result = RespParser::parse(buffer_, value_);

  ASSERT_EQ(result, ParsingResult::kReady);
  ASSERT_EQ(value_.type, RespValue::Type::kArray);

  auto elements = std::get<std::vector<RespValue>>(value_.data);
  EXPECT_TRUE(elements.empty());
}

TEST_F(RespParserTest, ParsesArrayWithMixedTypes) {
  write(buffer_,
        "*4\r\n"
        "+OK\r\n"
        ":42\r\n"
        "$3\r\nfoo\r\n"
        "$-1\r\n");

  auto result = RespParser::parse(buffer_, value_);

  ASSERT_EQ(result, ParsingResult::kReady);

  auto elements = std::get<std::vector<RespValue>>(value_.data);
  ASSERT_EQ(elements.size(), 4);

  EXPECT_EQ(elements[0].type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(elements[0].data), "OK");

  EXPECT_EQ(elements[1].type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(elements[1].data), 42);

  EXPECT_EQ(elements[2].type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(elements[2].data), "foo");

  EXPECT_EQ(elements[3].type, RespValue::Type::kNullBulkString);
}

TEST_F(RespParserTest, ParsesArrayWithNestedEmptyArray) {
  write(buffer_,
        "*2\r\n"
        "*0\r\n"
        ":1\r\n");

  auto result = RespParser::parse(buffer_, value_);

  ASSERT_EQ(result, ParsingResult::kReady);

  auto elements = std::get<std::vector<RespValue>>(value_.data);
  ASSERT_EQ(elements.size(), 2);

  EXPECT_EQ(elements[0].type, RespValue::Type::kArray);
  EXPECT_TRUE(std::get<std::vector<RespValue>>(elements[0].data).empty());

  EXPECT_EQ(elements[1].type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(elements[1].data), 1);
}

TEST_F(RespParserTest, ParsesDeeplyNestedArrays) {
  write(buffer_,
        "*1\r\n"
        "*1\r\n"
        "*1\r\n"
        ":7\r\n");

  auto result = RespParser::parse(buffer_, value_);

  ASSERT_EQ(result, ParsingResult::kReady);

  auto lvl1 = std::get<std::vector<RespValue>>(value_.data);
  auto lvl2 = std::get<std::vector<RespValue>>(lvl1[0].data);
  auto lvl3 = std::get<std::vector<RespValue>>(lvl2[0].data);

  EXPECT_EQ(lvl3[0].type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(lvl3[0].data), 7);
}

TEST_F(RespParserTest, IncompleteArrayReturnsNeedMoreData) {
  write(buffer_,
        "*2\r\n"
        ":1\r\n");

  auto result = RespParser::parse(buffer_, value_);

  EXPECT_EQ(result, ParsingResult::kNeedMoreData);
}

TEST_F(RespParserTest, ExtraDataAfterArrayIsNotAnError) {
  write(buffer_,
        "*2\r\n"
        ":1\r\n"
        ":2\r\n"
        ":3\r\n");

  RespValue first;
  auto result1 = RespParser::parse(buffer_, first);

  ASSERT_EQ(result1, ParsingResult::kReady);
  ASSERT_EQ(first.type, RespValue::Type::kArray);

  auto elements = std::get<std::vector<RespValue>>(first.data);
  ASSERT_EQ(elements.size(), 2);

  RespValue second;
  auto result2 = RespParser::parse(buffer_, second);

  ASSERT_EQ(result2, ParsingResult::kReady);
  ASSERT_EQ(second.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(second.data), 3);
}
