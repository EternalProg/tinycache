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
