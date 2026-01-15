#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <ostream>
#include <respParser.hpp>

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

